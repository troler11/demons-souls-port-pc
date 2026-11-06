# Fluidez no modo 60 fps: o que foi medido (28–29/10/2026)

Continuação de [CHARACTER_FLICKER_FIX.md](CHARACTER_FLICKER_FIX.md). Todas as
medidas foram feitas em Boletaria, na rota fixa de 400 s (save
`save_boletaria`, `PAD_PATH` andando a partir de 330 s), 1080p, escala 1 e
frame generation desligado. Trechos por tempo de parede, contados a partir do
fim da rodada; as rodadas variam muito entre si, por isso cada modo teve 2
rodadas.

## Quadros longos (o "teleporte" do inimigo)

O registro `[spike]` (`LD_SPIKE_LOG_MS`, ligado por padrão) marca cada quadro
do jogo apresentado 40 ms ou mais depois do anterior.

- Cada quadro longo é **um** quadro do jogo (`flips=1` em 117 de 118), e não
  um quadro perdido na tela. Com o 60 fps desbloqueado o jogo avança o tempo
  real daquele intervalo (até 50 ms): um inimigo rápido no ar anda ~3 quadros
  de uma vez.
- Só ~5% coincidem com shader ou PSO novo.
- São quadros com ~1,7× mais draws que um normal (~950 contra ~570), o
  tempo dividido entre o renderizador processando o FIFO e o renderizador
  esperando o jogo.
- 46% contêm um flush de ring cheio (submissão + espera da GPU). Era
  correlação: com rings 2× e 4× maiores esses flushes caíram de ~34 para ~10
  por rodada e os quadros longos não diminuíram.

## Experimentos descartados

| Experimento | Resultado |
|---|---|
| Hash de textura com fase por textura (em vez de todas no mesmo quadro a cada 16) | Empate |
| Limitador de apresentação desligado | Trecho pesado +0,5 fps (ruído) |
| Reciclar rings sem esperar a GPU (guarda por fence) | Pior: quadros longos ~72 → ~110; 42% das reciclagens ainda esperavam |
| Rings 2× / 4× maiores | Mesmos quadros longos, fps igual ou menor |
| Rastrear páginas escritas (`MEM_WRITE_WATCH`) para pular hashes | Não implementado: 0,5 µs por consulta e 0,76 µs na primeira escrita de cada página, contra ~1,3 µs de hash por draw |
| Um present por atualização do monitor (pela CPU) | Repetições −1 ponto, quadros nunca exibidos iguais (~2,5%), quadros longos mais |
| `THREAD_PRIORITY_HIGHEST` nas threads críticas | Ruído |
| Draws pulados por inteiro (teto do ganho de um renderizador mais barato), com `LD_ZPASS=0` nos dois braços | Trecho pesado **46 fps contra 55**, quadros longos iguais ou mais |

O último é o que decide: nem com o custo dos draws zerado o jogo fica mais
rápido. O limite do trecho pesado está na cadeia do jogo (HighGraphics ↔ job
SPURS em `spu1000_0`, a ~99% em todas as rodadas), não no renderizador.

## Qual thread limita: teste causal (29/10)

Sem como acelerar uma thread de graça, o teste faz o inverso: `DES_SLOW_THREAD` tira 20% do tempo de uma thread (suspensa 0,4 ms a cada 2 ms) e mede o fps. Controle com o mesmo auxiliar a 0%.

| Braço | Trecho pesado (fps) | Andando |
|---|---:|---:|
| Controle | 55,8 / 53,4 | 58,6 / 57,9 |
| `spu1000_0` −20% | 49,5 / 52,5 | 53,0 / 57,3 |
| `HighGraphics` −20% | 53,7 / 55,4 | 58,3 / 57,2 |

- `HighGraphics` (PPU do jogo) não limita: otimizar o código PPU não renderia fps.
- `spu1000_0` (kernel SPURS + job que monta os comandos do quadro) limita em parte: −3,6 fps em média (−6,6%) para −20% de tempo, com variação grande entre rodadas. Parte do tempo dela é o kernel ocioso procurando trabalho. Acelerá-la ~20% renderia, por simetria, algo como +2 a +4 fps no trecho pesado (estimativa).

## Runtime SPU: primeira etapa (29/10)

- `MFC_Cmd` do código SPU direto para o motor de atômicos/DMA (`spu_mfc_cmd`, `SPU_MFC_FAST=0` volta ao caminho antigo); checagens de diagnóstico do caminho quente (`hang_watch_line`, `ps3_progress_bump`) trocadas por leitura de variável.
- A/B, 3 rodadas por braço: 57,8 contra 57,3 fps no trecho pesado (+0,5, dentro do ruído); quadros longos sem melhora. Rota do Nexus 2/2 sem congelar. Mantido por ser equivalente em lógica.
- Coerente com o teste causal: cada 5% de tempo cortado na `spu1000_0` vale ~1,5% de fps, abaixo do ruído das rodadas. Ganho visível exigiria cortar uma fatia grande do runtime SPU de uma vez.

## Ideias do port de God of War: Ascension (05–06/11)

Do repositório `arnaldolima1/gow-ascension-pc` (pinagem de threads, PGO, RSX dormindo, blocos SPU fundidos). Boletaria, 3 rodadas por braço, Ryzen 5 5500 (6 núcleos/12 threads, sem AVX-512):

| Teste | Resultado | Decisão |
|---|---|---|
| `frame_clock` dormindo em vez de girar (`FC_SPIN_US=0`) | 57,8 fps nos dois; ~10 pontos de CPU a menos no renderizador; quadros longos iguais ou menos | Padrão mantido girando: 1 congelamento do Nexus em 12 rodadas dormindo (0 em 3 girando, 11 em 11 antes) — poucas rodadas para descartar |
| `spu1000_0` sozinha num núcleo físico (pinagem) | 58,0 contra 58,1 fps | Removido |
| PGO no runtime + código SPU (`-DDES_PGO=gen|use`) | 58,1 contra 58,8 fps | Opção no CMake, desligada |
| Blocos SPU fundidos (`-DPS3RECOMP_SPU_MERGE=ON`) | 57,8 contra 57,6 fps | Opção no CMake, desligada |
| Busca de export do HLE por índice (`prx_resolve_export` fazia busca linear a cada chamada) | sem efeito medível no fps | Mantido (equivalente, mais barato) |

O trecho pesado desta rota já roda a 57–59 fps, perto do limite de 60: sobra pouca margem para medir ganhos nele.

### Aberto: uma task SPURS interpretada

`SPU_INTERP_PROF=1` mostra a task do EBOOT na LS `0xF2FC` (ELF `0x01832680`, imagem `380810790`) passando ~2 s de cada 5 s no interpretador, na `spu1000_3` (não na `spu1000_0`, que limita). A task retoma no meio de uma função; uma vez no interpretador, `spu_lifted_lookup` procura só na imagem base (não na task adotada) e nunca volta ao código gerado. Fazer o `spu_lifted_lookup` consultar a task tirou o tempo do interpretador, mas um salto no boot para LS `0x32890` (retorno `0x05F60`) que encerra um job apareceu em 6 de 6 rodadas assim, contra 2 de ~10 com o executável antigo nesta série e 0 em 38 antes. Como esse job pode ser a própria task, a correção foi revertida até se saber o que é esse salto.

## Opções que ficaram (desligadas por padrão)

- `DES_FPS_DT_MAX_MS=<ms>`: limite menor para o passo de um quadro (ex.: 33).
  O inimigo salta menos nos quadros longos; o jogo desacelera um instante.
- `DES_FPS_DT_SMOOTH=<n>`: passo = média dos últimos n quadros (ex.: 4).
  Movimento mais regular, tempo total preservado.

Ambas rodaram sem erro e com o mesmo fps; o efeito é visual e precisa ser
avaliado jogando.

Ferramentas: `work/qa_20261028/fcsamp/fc_sampler.c` (amostrador externo de uma
thread por nome, com chamadores na pilha), `work/qa_20261028/disp_cadence.py`
(cadência da tela a partir de `LD_DISPLAY_LOG`).
