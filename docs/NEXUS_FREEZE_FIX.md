# Congelamento do Nexus: causa e correção (27/10/2026)

Continuação de [STUTTER_FOLLOWUP.md](STUTTER_FOLLOWUP.md) e
[SPU_EVENT_FOLLOWUP.md](SPU_EVENT_FOLLOWUP.md), que deixaram o congelamento
do Nexus em aberto.

## Onde parava

`PS3_HANG_DUMP=5` (registro de todas as threads quando nenhum quadro sai por
5 s) pegou o congelamento em 4 de 4 rodadas da rota do Nexus
(`work/perf19_20261026/seeds/nexus_original`, câmera por `PAD_PATH`). Em
todas:

- `HighGraphics` (thread de desenho, guest tid 6) parada em
  `sys_event_queue_receive`, fila de eventos 22, chamada de
  `cellSpursQueuePopBody` (import NID `0x91066667`, `libsre_func_30016CFC`).
- A `CellSpursQueue` em `0x480C5080` (elementos de 16 bytes, profundidade 250)
  com itens: 12 e 14 nas duas primeiras capturas. A palavra de leitura
  (`+0x0`) estava complementada, a marca de "PPU esperando".
- Fila de eventos 22 com 0 de 1 eventos pendentes: nenhum aviso foi enviado.

## A corrida

`HANG_WATCH_LINE=480C5080 HANG_WATCH_Q=22` (anel em memória, sem deixar o jogo
lento) registrou a sequência final, idêntica nas duas capturas:

1. SPU: GETLLAR + PUTLLC marca a palavra de escrita (`+0x4`) como "em
   andamento" (complementada).
2. PPU: `stwcx` consome um item (palavra de leitura positiva).
3. SPU: GETLLAR + PUTLLC conclui o item. Vê a palavra de leitura positiva,
   portanto ninguém esperando, e não manda evento.
4. PPU: `stwcx` grava a marca de espera. Ela tinha decidido "vazio" com a
   leitura antiga da palavra de escrita, de antes do passo 3.
5. PPU: `sys_event_queue_receive`, dormindo para sempre com um item na fila.
   Os itens seguintes não geram evento, porque o SPU só avisa na passagem de
   vazio para não vazio (inferência pelo comportamento).

No Cell, a reserva do `lwarx` vale para a linha de 128 bytes: o PUTLLC do
passo 3 escreve na mesma linha e faria o `stwcx` do passo 4 falhar. A PPU
tentaria de novo e veria o item. O runtime conferia a reserva só pela
palavra exata, e o CAS só compara essa palavra, que não tinha mudado.

## Correção

- `ppu_resv_break_line` (`ppu_loader.cpp`): cancela toda reserva PPU dentro de
  uma linha. É chamada por `spu_coh_notify_write*` (`spu_coherency.c`), ou seja,
  em toda escrita confirmada numa linha reservada por SPU (PUTLLC, PUTLLUC,
  DMA, store coerente da PPU), sob o lock da linha.
- `ppu_stwcx32` / `ppu_stdcx64` pegam o lock da linha **antes** de conferir a
  própria reserva, para não sobrar janela entre conferir e gravar.

## Resultado

| Rota do Nexus (110 s cada) | Rodadas | Congelaram |
|---|---:|---:|
| Antes da correção | 4 | 4 (entre 1 e 2 min) |
| Com a correção | 6 | 0 (~60 fps até o fim) |

## Custo e filtro

A primeira versão percorria os contextos de todas as threads PPU a cada
escrita confirmada de SPU. O kernel SPURS faz milhões de atômicos por segundo,
e isso custou ~5 fps em Boletaria (trecho pesado, sem frame generation: 53,5
com a correção contra 58,4 sem, média de 2 rodadas cada). Agora um bitmap marca
as linhas em que a PPU já fez `stwcx`/`stdcx` (fixo depois de marcado, como o de
coerência dos SPUs), e o cancelamento só percorre as threads nessas linhas.

| Boletaria, trecho pesado (A/B no mesmo exe, `PPU_RESV_LINE=0` desliga) | fps médio | mínimo |
|---|---:|---:|
| Correção ligada, rodada 1 | 57,0 | 54,4 |
| Correção desligada, rodada 1 | 57,1 | 54,6 |
| Correção ligada, rodada 2 | 56,9 | 53,8 |
| Correção desligada, rodada 2 | 57,5 | 55,0 |

A diferença fica dentro do ruído. Com o filtro, a rota do Nexus foi 3 de 3
rodadas sem congelar.

Limite do filtro: o primeiro `lwarx`/`stwcx` de uma linha, antes de o bit ser
marcado, ainda não é protegido.

Evidências: `work/diagnostics/nxq_*`, `nxr_*` (antes), `nxfix_*` e `nxfilt_*` (depois); `work/logs/resv1_*` (sem filtro) e `resv_*` (com filtro).
