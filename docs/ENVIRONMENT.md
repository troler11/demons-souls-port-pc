# Variáveis de ambiente

`scripts/windows/env.ps1` define os padrões com que o jogo roda. Qualquer variável definida antes de chamar `play.ps1`/`autotest.ps1` tem prioridade. Os números entre parênteses são os itens do [diário](REPORT.md).

## O que o jogo precisa (padrões)

| Variável | Padrão | O que faz |
|---|---|---|
| `RSX_LIVE_DRAW` | `1` | Renderizador Direct3D 12. |
| `DES_LIBSRE` | `work\lle\out\libsre_image.bin` | Roda o kernel SPURS real do firmware em vez do HLE. |
| `DES_ONE_LOG` | `1` | Log enxuto. |
| `PS3_DEV_FLASH` | `<rpcs3>\dev_flash` | Serve `/dev_flash` (o codec ATRAC do áudio). |
| `PS3_GCM_BEGIN_OFFSET` | `1000` | O SPU publica o put do GCM a partir de 4 KB no IO. |
| `PS3_GCM_NO_TITLE_RECYCLE` | `1` | O jogo percorre os próprios segmentos de comando. |
| `YDKJ_FORCE_HLE` | `8A85674D,011EE38B` | LFQueue push/init no lado HLE (descompressão no host). |
| `SAIL_HOST_PLAY` | `1` | Toca os vídeos `.pam` no host com o ffmpeg (som estéreo e 5.1). `0` pula as cinemáticas. |
| `SAIL_FFMPEG` | ffmpeg do PATH ou `third_party\ffmpeg\bin` | Qual ffmpeg usar. |
| `SAIL_OPEN_DONE_EVENT` / `SAIL_CLOSE_EVENT` | `9` / `5` | Eventos que a tarefa de vídeo espera. |
| `LD_SURF_WRITEBACK` | `1` | Copia os alvos de luminância de volta para a VRAM do jogo (adaptação de exposição). |
| `DES_FPS_UNLOCK` | `1` | 60 fps (patch Unlock FPS). `0` = 30 fps original. |
| `LD_VSYNC` / `LD_ASYNC_PRESENT` / `LD_PRESENT_MIN_US` | `0` / `1` / `16683` | Modo 60: present sem vsync, CPU e GPU sobrepostas, limite de 59,94 fps (60). |
| `GCM_FLIP_MIN_US` | `33300` no modo 30 | Segura os flips a 30 por segundo (55). |

## Renderizador

| Variável | O que faz |
|---|---|
| `DES_WINDOW=1080p\|1440p\|4k\|LxA` | Tamanho da janela; o quadro é escalado na GPU. |
| `LD_RES_SCALE=2..4` | Resolução interna: a cena renderiza k× 1280×720. |
| `DES_FULLSCREEN=1` | Começa em tela cheia sem bordas (F11 / Alt+Enter alternam). |
| `LD_FSR=1` | AMD FSR 1 quando a cena é menor que a janela: EASU (ampliação que segue as bordas) + RCAS (nitidez). |
| `LD_FXAA=1` | Anti-aliasing FXAA na imagem do jogo, na resolução interna. |
| `LD_FRAMEGEN=1` | Frame generation: a GPU estima o movimento entre dois quadros (busca por blocos, sem vetores do jogo) e mostra um quadro intermediário. 60 → 120 na tela, sem custo de CPU; meio quadro de atraso. `2` mostra só os quadros gerados (para avaliar). |
| `LD_SHARPEN=0..1` | Força do RCAS (1 = máxima). Padrão 0,8 com FSR e 0 sem; funciona também sem FSR. |
| `LD_ZPACK_SELF=0` | Desliga a profundidade para draws que a leem e testam ao mesmo tempo (névoa, partículas suaves) (57). |
| `LD_ZPASS=0` | Volta a responder "visível" a toda consulta de oclusão (flares atravessam paredes) (58). |
| `LD_ZPASS_START=0` | Começa com a contagem de oclusão desligada. |
| `LD_GEOM_CACHE`, `LD_VERTEX_BATCH`, `LD_TEX_REHASH_LAZY` | Caches do renderizador (ligados). |
| `LD_SHADOW` | Comparação de sombra (ligada). |

## Áudio

| Variável | O que faz |
|---|---|
| `AUDIO_CLOCK_PACE=0` | Mistura guiada pela placa de som (o comportamento antigo, picotado) (56). |
| `AUDIO_DEVICE_MS` | Buffer da placa de som em ms (padrão 60). |
| `AUDIO_LEAD_BLOCKS` | Blocos de folga do índice de leitura publicado (padrão 3). |
| `AUDIO_RATE=1` | A cada 5 s: blocos/s, % de blocos mudos, frames descartados. |
| `AUDIO_WAV=<arquivo>` | Grava a mistura final (f32le, estéreo, 48 kHz). |
| `AUDIO_SEQ_LOG=1`, `AUDIO_NOTIFY_LOG=1` | Quais blocos de cada porta tinham dados; eventos de notificação. |

## Entrada automática

| Variável | O que faz |
|---|---|
| `PAD_SCRIPT=<s>:<botões>,...` | Aperta botões em segundos fixos. `0x4000` ✕, `0x0800` R1, `0x0008` Start. |
| `PAD_PATH=<t0>-<t1>:lx,ly,rx,ry;...` | Segura os analógicos (0-255, 128 = centro) em cada intervalo. Para frente: `128,0,128,128`. |
| `PAD_NO_KEYBOARD=1` | Desliga o teclado como controle. |
| `PAD_SCRIPT_MOVIE_HOLD=1` | O `PAD_SCRIPT` não aperta nada enquanto um vídeo toca (assistir inteiro). Até 256 eventos. |

## Diagnóstico

| Variável | O que faz |
|---|---|
| `LD_FRAME_DUMP=<pasta>` (+`_EVERY`, `_FROM`) | Grava a tela em `.ppm` a cada N frames. |
| `LD_DRAW_DUMP=<pasta>` (+`_EVERY`, `_FRAMES`, `_AT_MS`) | Lê de volta o alvo antes de cada draw de um frame (quanto cada draw pinta). |
| `RSX_DRAW_CSV=<arquivo>` (+`_FROM`, `_GATE`) | Estado de cada draw em CSV. |
| `LD_HLSL_DUMP=<pasta>` | Grava os shaders HLSL gerados. |
| `LD_FLUSH_PROF=1`, `PS3_FC_PROF=1` | Onde vai o tempo do renderizador e do relógio de frames. |
| `PS3_SAMPLE=<ms>` + `PS3_SAMPLE_MAIN=1` [+`PS3_SAMPLE_THREAD=<nome>`] | Amostrador de CPU de uma thread (`HighGraphics`, `spu1000_0`...). Gera um `.map` para os nomes. |
| `SYSCALL_TRACE_THREAD=<nome>` / `SYSCALL_TRACE_TID` | Syscalls lentas de uma thread. |
| `SYSCALL_TRACE_SC=<n>` [+`SYSCALL_TRACE_A0=<hex>`] | Toda chamada de uma syscall, de qualquer thread. |
| `RECV_TRACE_TID`, `COND_PROF` | Recepções de eventos; esperas em condition variables. |
| `SAIL_DUMP=<pasta>` | Grava 1 quadro dos vídeos a cada 300. |
| `SAIL_AUDIO_RAW=<arquivo>` | Grava o som dos vídeos que vai para os alto-falantes (s16le estéreo 48 kHz). |
| `DES_WATCH=<ea>:<len>:<s>`, `DES_DUMP=<ea>:<len>:<s>` | Vigia/despeja memória do jogo. |
| `SPU_LLAR_BACKOFF=1` | Experimento (mais lento, desligado). |
| `SPU_CH_BLOCK=0` | SPUs voltam a girar esperando canais (padrão: dormem; com 6 threads, 13–25 → 45–49 fps em Boletaria). |
| `FC_SPIN_US` | Giro do `frame_clock` enquanto o jogo desenha (µs). Padrão: 100000 com 10+ processadores lógicos, 0 com menos. |
| `PS3_USLEEP_MODE=spin\|sleep\|progress` | Como um `usleep` curto espera (padrão `spin`). |
| `PS3_THREAD_CPU=<s>` | A cada s segundos, quanto de um núcleo cada thread gastou. |
| `SPU_LOCK_STRIPES=0` | Volta à trava global única para as atômicas dos SPUs (padrão: 64 travas por linha, +4 fps). |
| `SPU_LOCK_PROF=1` | A cada 5 s: aquisições/s, % com espera, núcleos esperando/segurando, e quem chama. |
