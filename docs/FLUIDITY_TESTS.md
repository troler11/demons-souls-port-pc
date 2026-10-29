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
