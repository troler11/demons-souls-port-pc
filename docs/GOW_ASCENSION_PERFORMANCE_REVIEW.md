# Melhorias do Ascension aplicáveis ao DeS — 24/10/2026

Rodada de teste posterior à revisão: o gate dos watches PPU foi implementado,
validado e ativado por padrão. No A/B 0/1/1/0, FPS +10,6%/+10,5% contra o
caminho completo. Opções de compilação VMX passaram em 2,4 milhões de
comparações isoladas, mas continuam fora das flags do jogo. Resultados,
integração e limites em [`PERFORMANCE.md`](PERFORMANCE.md).

Revisão dos fontes públicos de
[`arnaldolima1/gow-ascension-pc`](https://github.com/arnaldolima1/gow-ascension-pc/tree/6e503ad0761b9ccea0f7ea8948e6833cbe16f43b),
commit `6e503ad0761b9ccea0f7ea8948e6833cbe16f43b`, comparados ao runtime e
ao código gerado presentes neste port. Cópia para consulta em
`work/reviews/gow-ascension-20261024`. Nenhum código externo foi executado,
nenhum fonte do jogo/runtime ou opção de build foi alterado nesta revisão.
Não houve benchmark de uma otimização nova.

## Prioridades

| Ordem | Ideia | Situação no DeS | Próxima experiência |
| --- | --- | --- | --- |
| 1 | Evitar diagnóstico de escrita inativo; depois ampliar stores inline | `barrier_watch_hit` ocupou 15,94% das amostras diretas da HighGraphics; o header do PPU principal não contém stores inline, enquanto `work/lle/recomp/libsre_recomp.h` contém | Primeiro um gate barato no helper, sem regenerar o PPU; depois avaliar stores inline com fallback correto |
| 2 | Registradores PPU em variáveis locais | Nosso `ppu_lifter.py` não oferece `--reg-locals`; o regcache SPU que já temos é outra transformação | Desenvolver/adaptar em funções quentes isoladas e comparar estado antes de regenerar tudo |
| 3 | Opções de geração do PPU/VMX | SPU/runtime já usam AVX2/FMA; a configuração específica dos fontes PPU principais ainda só acrescenta `/w;/bigobj` no ramo MSVC | Comparar funções isoladas com `-fno-math-errno`, AVX2/FMA e contração implícita desativada; inspecionar o assembly |
| 4 | Captura/replay diferencial de jobs SPU | Não encontramos o coletor `spu_jobcap.c` no SDK local | Capturar entradas/LS/GPR/DMA do job quente e validar versões nativas fora do jogo antes de habilitar |
| 5 | Afinidade por núcleo físico e perfil por dono | Ainda não temos os controles `SPURS_PIN`/`PS3_PIN_PPU` do donor | Experiência separada, baseada na topologia real, com HighGraphics/SPU/render/present/áudio considerados juntos |

As prioridades são inferências do código e do perfil local, não estimativas
de ganho de FPS. Os percentuais de HighGraphics são amostras diretas de uma
thread em uma janela, não CPU total. Ver limites em `PERFORMANCE.md`.

## 1. Escritas de memória: a aplicação mais imediata

O [patch 0038](https://github.com/arnaldolima1/gow-ascension-pc/blob/6e503ad0761b9ccea0f7ea8948e6833cbe16f43b/patches/ps3recomp-0038-inline-guest-stores.patch)
gera stores inline com seleção por diagnósticos, limites de memória e teste
das linhas de reserva SPU. Aqui a ideia foi aplicada ao header da libsre,
mas não ao `work/game/recompiled/ppu_recomp.h` do jogo principal. O comentário
histórico sobre stores inline não significa que todas as threads PPU os usem.

No perfil local, `vm_write64` e `vm_write32` somaram mais 8,21% das amostras
fora de `barrier_watch_hit`. Não somar esses números como ganho garantido:
o trabalho real da escrita e da coerência continua necessário. Um gate
para os watches desligados é um primeiro experimento menor e não exige
recompilar os grandes arquivos PPU. A ampliação dos stores inline é posterior.

Não copiar o patch literalmente: nosso runtime tem outras guardas e modos
diagnósticos. Preservar endian, atomics/alinhamento, limites, página nula,
registradores raw SPU, watch dinâmico e sincronização de reservas. Escritas
que cruzam uma linha exigem examinar ambas; checagem de bitmap sem protocolo
correto pode correr contra a criação de uma reserva. O fato de o donor usar
um caminho não prova sua correção no nosso runtime concorrente.

## 2. PPU local e geração de código

O [commit de registradores locais](https://github.com/arnaldolima1/gow-ascension-pc/commit/14758c3277b7b52c5e62fc8de74e81eef043e223)
acrescenta `--reg-locals` ao relift e `-fno-math-errno` ao build PPU.
O script informa que a transformação mantém registradores locais e uma
visão dos globais de memória. Isso pode reduzir tráfego em `ctx` e problemas
de alias. É diferente do nosso `scripts/spu_regcache.py`, exclusivo de SPU.

O repositório contém o comando que invoca a transformação, mas não a
implementação `_localize_regs` do `ps3kit/tools/ppu_lifter.py`. Não basta
adicionar o argumento ao nosso lifter. Antes de uma implementação, definir
como publicar/recarregar registradores em chamadas, syscalls, HLE, saltos
indiretos, interrupções, entradas no meio de funções e hooks existentes.

O [CMake do donor](https://github.com/arnaldolima1/gow-ascension-pc/blob/6e503ad0761b9ccea0f7ea8948e6833cbe16f43b/CMakeLists.txt)
também oferece AVX2/BMI2/FMA para o PPU e passa `-ffp-contract=off` ao clang-cl.
Nosso ramo MSVC do PPU principal não acrescenta essas opções. A oportunidade
merece teste porque `vmx_vmaddfp`/`vmx_vaddfp` somaram 7,27% das amostras locais.
Não equivale a simplesmente trocar por `fast-math`: manter FMA explícito onde
requerido, arredondamento, VSCR, NaN e denormais. Confirmar o comando efetivo
do compilador e os resultados antes de alterar flags do projeto inteiro;
esse passo pode recompilar gigabytes de código gerado.

## 3. Jobs nativos: aproveitar o método

Os scripts
[`replay.ps1`](https://github.com/arnaldolima1/gow-ascension-pc/blob/6e503ad0761b9ccea0f7ea8948e6833cbe16f43b/scripts/windows/replay.ps1)
e [`postbench.ps1`](https://github.com/arnaldolima1/gow-ascension-pc/blob/6e503ad0761b9ccea0f7ea8948e6833cbe16f43b/scripts/windows/postbench.ps1)
comparam jobs capturados, LS/registradores e saídas. Os scripts apontam para
`runtime/spu/spu_jobcap.c`, `libs/spu_hle/gowa_drawcmd` e `gowa_post` no SDK
separado. Essas implementações não estão entre os arquivos rastreados do
repositório analisado; sua disponibilidade em outros lugares não foi estabelecida.

Não há substituto pronto para o job de geometria de DeS nessa árvore. As
rotinas/endereço/ABI de Ascension não identificam as funções equivalentes
de outro jogo. A ideia útil é gravar entradas do job DeS que monta comandos,
substituir um trecho por vez e comparar todos os efeitos observáveis.
Comparações por ABI só podem ignorar registradores comprovadamente mortos;
o próprio postbench alerta que DMA concorrente pode contaminar o estado
capturado. Replay isolado não valida sozinho a sincronização durante gameplay.

## 4. Medição e afinidade

[`host/ascension_defaults.cpp`](https://github.com/arnaldolima1/gow-ascension-pc/blob/6e503ad0761b9ccea0f7ea8948e6833cbe16f43b/host/ascension_defaults.cpp)
seleciona `SPURS_PIN` e `PS3_PIN_PPU`. São configurações úteis como ideia de
experimento; não existem com esses nomes no nosso código local e copiá-las
para o ambiente não implementa afinidade. DeS também executa libsre recompilada;
ajustes de workloads do scheduler HLE deles não necessariamente controlam
esse caminho. Não copiar IDs, máximos de concorrência ou waits do Ascension.

As ferramentas novas `prof_by_owner.py`, `frame_layout.py` e `thread_cpu.py`
são boas referências para separar custo do runtime/código gerado/DLL e
correlacionar SPURS com progresso do frame. Dependem dos formatos de log do
donor; `frame_layout.py` ainda usa endereço de frame específico de Ascension.
`maprva.py` usa símbolo anterior e escolhe um alias: manter nossa resolução
com mapa exato, aliases preservados e `.pdata`, sem reintroduzir atribuições
enganosas a funções guest vazias.

## Não priorizar como melhoria nova

- AVX2 e remoção de hooks LS no SPU, FP rápido, stores inline da libsre,
  ring TLS de diagnóstico e upload reserve já têm equivalentes locais.
- Patch 0043 evita uma varredura de 32 slots de código residente; nossa
  resolução observada percorre quatro. Não importar sua geração global sem
  perfil que justifique e sem tratar reutilização/reset de contextos.
- Patches 0040/0044 registram que merge/promote não trouxeram ganho naquele
  teste; fusão de chunks já teve tentativa histórica sem ganho aqui.
- `SPURS_KERNEL_SPIN` aumenta atividade de CPU e o patch 0041 não demonstrou
  ganho no cenário deles. Não é resposta automática para o nosso uso de CPU.
- O backend Vulkan é o RSX do RPCS3, com integração e licença próprias;
  não é uma chave que transforma o nosso D3D12. O perfil CPU atual favorece
  atacar memória/PPU/SPU antes de uma troca de backend.

Código eventualmente adaptado deve conservar licença/créditos aplicáveis.
O repositório analisado publica licença MIT; a integração RSX do RPCS3 tem
requisitos próprios, conforme o README do donor.
