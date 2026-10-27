# Variáveis de ambiente

`scripts/windows/env.ps1` define os padrões com que o jogo roda. Qualquer variável definida antes de chamar `play.ps1`/`autotest.ps1` tem prioridade. Os números entre parênteses são os itens do [diário](REPORT.md).

A interface antiga não é compilada nem vinculada ao executável. `DES_UI`, `DES_UI_START`, `DES_UI_OPEN`, `DES_UI_KEYS` e `DES_UI_SOUND` não reativam os menus; `settings.ini` não é carregado. Configure os parâmetros de `play.ps1` ou o ambiente antes de iniciar. `Jogar.bat` chama esse script diretamente; `launcher.ps1` permanece apenas como referência da interface anterior.

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
| `DES_FPS` | 60 via scripts | Controle único: `0` original 30, `30` tempo real, `60`, `61..240`, `uncapped` experimental (vblank até 240 Hz). |
| `DES_FPS_UNLOCK` | `1` | Compatibilidade com scripts antigos: só escolhe o padrão quando `DES_FPS` não está definido. |
| `DES_VSYNC` | `0` | VSync alterável em tempo de execução. Usa `LD_VSYNC` como padrão legado. |
| `LD_PRESENT_WORKER` | `1` no runtime | Apresenta frames normais e executa seu limitador no worker `present_worker`, sobrepondo a próxima cena offscreen. No máximo um trabalho pendente; drena antes de escolher/reutilizar o backbuffer e nas transições de vídeo/encerramento. `0` compara o caminho serial. Overlay/vídeos usam apresentação serial; FG usa seu controle próprio. |
| `LD_ASYNC_PRESENT` | `1` | Sobreposição CPU/GPU. O controle do título aplica o limite de present conforme `DES_FPS`, inclusive nas cutscenes (30). |
| `LD_PRESENT_MIN_US` / `GCM_FLIP_MIN_US` | não definidos pelos scripts | Opções legadas do runtime. Não definir um segundo limitador fixo ao usar `DES_FPS`. |
| `DES_AUDIO_NOTIFY_FIX` | `1` | Recupera a chave de notificação publicada pela thread filha do libmixer, no ponto BLUS30443 verificado. `0` só para A/B. |
| `DES_AUDIO_NOTIFY_TEST_ZERO` | desligado | Injeta a falha de chave zero exclusivamente para regressão; nunca usar ao jogar. |
| `SPU_RDCH_FAST` | `1` | Leituras MFC inline. `0` compara com as leituras completas, sem ativar logs de canais. |
| `PS3_SHORT_SLEEP_PAUSES` | `16` | PAUSEs antes do yield nas esperas curtas; `0` compara com o comportamento anterior (máximo 64). |
| `SPU_PROGRESS_ALWAYS` | `0` | `1` restaura o contador global a cada escrita SPU para A/B. Normalmente ele só é necessário com `PS3_USLEEP_MODE=progress` ou `spinwake`. |
| `SPU_TASKSET_SCAN` | desligado | Definir força a varredura antiga dos 64 slots HLE em cada commit SPU. Normalmente a varredura só começa depois do primeiro registro de tarefa HLE bloqueada; a ativação é permanente para preservar notificações. |

## Nome do personagem

No Windows, selecionar o campo de nome na criação abre uma janela de texto.
Digite pelo teclado do PC e use **Enter/Confirmar** ou **Esc/Cancelar**.
O limite vem do jogo; nomes vazios não podem ser confirmados. A entrada usa
Unicode e devolve UTF-16 ao jogo, cuja fonte pode não ter todos os símbolos.
Enquanto a janela está aberta, as teclas não viram comandos do personagem.

Na BLUS30443 testada, o limite foi 16. A exibição original ainda omitiu o
`ã` de `João Silva` (apareceu `Joo Silva`), embora a entrada/devolução UTF-16
preservem esse caractere. Nome sem acento (`Lucas Teste`) foi exibido inteiro.
Essa limitação de exibição/conversão do jogo ainda não foi corrigida.

`PS3_OSK_TEXT=<texto UTF-8>` continua disponível para testes: preenche o nome
automaticamente, respeitando o limite. Remova essa variável para usar a janela.
`PS3_NO_USER_INPUT=1` mantém a resposta automática dos diagnósticos (texto
inicial ou `Slayer` se estiver vazio). Ausente/vazia/`0` usa a janela normalmente.

## Renderizador

| Variável | O que faz |
|---|---|
| `DES_WINDOW=1080p\|1440p\|4k\|LxA` | Tamanho da janela; o quadro é escalado na GPU. |
| `LD_RES_SCALE=1..4` | Resolução interna: a cena renderiza k× 1280×720. `play.ps1 -Scale 1` também limpa uma escala maior herdada. |
| `DES_FULLSCREEN=1` | Começa em tela cheia sem bordas (F11 / Alt+Enter alternam). |
| `LD_FSR=1` | AMD FSR 1 quando a cena é menor que a janela: EASU (ampliação que segue as bordas) + RCAS (nitidez). |
| `LD_FXAA=1` | Anti-aliasing FXAA na imagem do jogo, na resolução interna. |
| `LD_FRAMEGEN=1` | Geração experimental por busca de blocos, sem vetores/depth do jogo. Tenta intercalar um quadro e acrescenta atraso; o ganho visível depende do FPS base, GPU e monitor. Correspondências incertas usam a imagem atual para reduzir imagens duplas. `2` envia somente os quadros gerados. Não é uma integração de FSR 3. |
| `LD_FRAMEGEN_ASYNC=0` | Compara com a apresentação serial anterior. Padrão: worker `present_worker` com lista de cópia, allocator e fence próprios. Um par pendente no máximo; a thread RSX grava a próxima cena enquanto o worker apresenta/espera. Vídeos e encerramento drenam o trabalho antes de reutilizar os recursos. Com overlay ativo, usa o caminho serial. |
| `LD_FRAMEGEN_OVERLAP=0` | Volta ao caminho anterior do frame generation, em que a thread RSX desenha no back buffer e espera o par anterior ser apresentado. Padrão: ela desenha em alvos próprios e o `present_worker` copia para a swap chain, então só espera antes de entregar o próximo par (Boletaria, trecho pesado: 41–46 → ~55 fps do jogo com frame generation). |
| `LD_FRAMEGEN_FILL=0` | Volta ao agendamento fixo de meio intervalo. Padrão: agendamento pelos vblanks do monitor (horário do DWM). O quadro real sempre vai para a tela, e o gerado só entra quando há uma atualização sobrando para ele. Num monitor de 60 Hz em Boletaria, a parte interpolada do que aparece caiu de 63–76% para 11–24%, com imagem nova em 94–98% das atualizações. |
| `LD_FRAMEGEN_FILL_SLACK_US=<us>` | Folga na previsão do próximo quadro real (padrão 3000). Maior mostra mais quadros gerados e repete menos imagens; 0 prioriza só quadros reais. |
| `LD_DISPLAY_LOG=<arquivo>` | Registra o que chega à tela: cada `Present` do `present_worker` (contagem DXGI, gerado/real/normal, horário) e, depois dele, as estatísticas de quadro da swap chain (qual `Present` estava na tela e em qual vblank). Para medir quadros descartados e repetidos. |
| `DES_WINDOW_THREAD=0` | Volta a criar e atender a janela na thread `frame_clock`. Padrão: a janela tem thread própria (`window`), e arrastar/redimensionar, o menu da janela (Alt+Espaço) ou um Alt sozinho não param mais o jogo (antes: até 30–50 s parado). |
| `LD_FRAMEGEN_EARLY_EXIT=0` | Comparação com busca exaustiva sem rejeição antecipada de candidatos; mesmos vetores, mais trabalho por candidato. |
| `LD_PRESENT_PROF=1` | Divide o tempo de apresentação guest em espera pela trava, preparo, espera pelo worker, cópia/comandos, flush, writeback, overlay, limitador, DXGI e cauda. Mostra tempo de parede e CPU da thread chamadora; não inclui a CPU do worker nem representa tempo real de GPU. Desligado por padrão; desligar no A/B final. |
| `LD_PRESENT_STATS=1` | Distribuição dos intervalos entre Presents registrados: FPS, p50, p95, p99, máximo e contagens `over33` (>33⅓ ms)/`over50` (>50 ms) por janela de cerca de 5 s. `elapsed` é o tempo desde o primeiro Present. Reusa o QPC já coletado; sem alocação por frame. Mede submissões, sem medir scanout/atraso de entrada. Com FG registra apenas o primeiro Present do par. Desligado por padrão. |
| `LD_POST_PROF=1` | Tempos reais na GPU da cadeia de saída e da interpolação, coletados sem esperar pela GPU (`[gpu-post]`). Os contadores de framegen medem chamadas bem-sucedidas a Present, não quadros efetivamente vistos no monitor. |
| `LD_PSO_HASH=0` | Compara o cache de pipelines com a busca linear antiga. Padrão: índice por hash, com igualdade da chave completa. |
| `LD_SHADER_META=0` | Desliga o novo cache de análise de shaders para A/B. Ligado por padrão; valida instruções completas e relê constantes a cada draw. Snapshots/offsets limitados a 8 MiB, além das tabelas fixas. `LD_FLUSH_PROF=1` mostra acertos/falhas e memória retida. |
| `LD_TEX_LOOKUP_FAST=0` | Compara com a busca linear de texturas. Padrão: dica de índice por hash, sempre validada contra o descritor completo da entrada; colisões/evicções voltam à busca linear. A checagem de conteúdo e a atualização de texturas dinâmicas permanecem ativas. |
| `LD_SHARPEN=0..1` | Força do RCAS (1 = máxima). Padrão 0,8 com FSR e 0 sem; funciona também sem FSR. |
| `LD_ZPACK_SELF=0` | Desliga a profundidade para draws que a leem e testam ao mesmo tempo (névoa, partículas suaves) (57). |
| `LD_ZPASS=0` | Volta a responder "visível" a toda consulta de oclusão (flares atravessam paredes) (58). |
| `LD_ZPASS_START=0` | Começa com a contagem de oclusão desligada. |
| `LD_GEOM_CACHE`, `LD_VERTEX_BATCH`, `LD_TEX_REHASH_LAZY` | Caches do renderizador (ligados). |
| `LD_GEOM_XXH3=0` | Usa o hash anterior das malhas para A/B. Padrão: XXH3-128 para regiões de 64 bytes ou mais; ambos leem todos os bytes e mantêm chave de 128 bits. |
| `LD_INDEX_RANGE_SIMD=0` | Compara a varredura escalar min/max de índices com AVX2. Padrão: AVX2 quando compilado com suporte; processa 16 índices BE16 ou 8 BE32 por bloco, respeita restart e trata a cauda sem ler além do buffer. |
| `LD_INDEX_RANGE_CACHE=1` | Experimento desligado por padrão: guarda min/max com snapshots completos e valida bytes/largura/restart em cada consulta. Orçamento de 8 MiB, além das tabelas fixas; fallback para varredura se não couber/alocar. A primeira medição com ~74% de acertos custou mais por consulta que varrer. |
| `LD_INDEX_RANGE_PROF=1` + `LD_FLUSH_PROF=1` | Mede o tempo de min/max e mostra contadores do cache. Acrescenta temporizadores por consulta; desligar na comparação final de FPS. |
| `GCM_FIFO_IDLE=1` | Experimento desligado por padrão: evita repetir o trabalho de um FIFO já drenado, mantendo trava e referências. Exige backend live, contexto normal, reciclagem do título desativada no host e worker FIFO desligado; confere PUT/GET guest, contexto/call e mapeamento. Diagnósticos FIFO e modo syscall conservam o caminho completo. Não mostrou ganho consistente de FPS/CPU no A/B. |
| `GCM_FIFO_IDLE_PROF=1` | Conta consultas vazias/ativas, progresso/paradas, falta de contexto, atalhos e referências publicadas pelo walker. Desligado por padrão. |
| `GCM_FIFO_IDLE_TIMING=1` + `GCM_FIFO_IDLE_PROF=1` | Mede parede por consulta e espera pela trava. Inclui temporizadores e altera o custo do polling; desligar ambos no A/B final de FPS/CPU. |
| `LD_FIFO_BATCH=0` | Volta ao envio de uma palavra por chamada. Padrão: agrupar pacotes NV4097 elegíveis, ler cada argumento uma vez e tomar a trava do renderizador uma vez por pacote. Controles/labels/reports/transferências conservam o caminho ordenado. |
| `LD_FIFO_ASYNC=1` | Experimento: worker D3D12 com três blocos de 2.048 comandos copiados. Desligado por padrão; a primeira medição piorou o FPS devido às drenagens frequentes. Requer batching; não habilita snapshots de quadros como `RF_ASYNC` do inFAMOUS. |
| `LD_FIFO_PROF=1` | No worker experimental, conta comandos/blocos/barreiras com trabalho pendente e tempos de espera/execução. Tempos de parede, não amostras de CPU. |
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
| `PAD_KEYMAP=<arquivo>` | Mapa de controles (o launcher grava `work\input.ini`). `[keyboard]`: código de tecla virtual por controle do PS3 (botões, `ls_*`/`rs_*` para os analógicos; botões do mouse valem). `[controller]`: botão XInput (`A`…`RIGHT`, `NONE`). Sem arquivo, o layout embutido. DualShock 4/DualSense por HID não mudam. |
| `PAD_NO_HID=1` | Não lê DualShock 4 / DualSense direto (por HID). |
| `PAD_NO_HOST=1` | Mantém neutra a entrada física nos testes; `PAD_SCRIPT`, `PAD_PATH` e o hook de menus continuam funcionando. |
| `PAD_SCRIPT_MOVIE_HOLD=1` | O `PAD_SCRIPT` não aperta nada enquanto um vídeo toca (assistir inteiro). Até 256 eventos. |
| `DES_LAUNCHER_SHOT=<png>` | `DesLauncher.exe` desenha uma página num PNG e sai (teste de layout). `DES_LAUNCHER_PAGE=0-5` escolhe a página, `DES_LAUNCHER_DROP=<n>` abre a lista da opção n, `DES_LAUNCHER_SCROLL=<px>` rola. |

## Diagnóstico

| Variável | O que faz |
|---|---|
| `LD_FRAME_DUMP=<pasta>` (+`_EVERY`, `_FROM`) | Grava a tela em `.ppm` a cada N frames. |
| `LD_DRAW_DUMP=<pasta>` (+`_EVERY`, `_FRAMES`, `_AT_MS`) | Lê de volta o alvo antes de cada draw de um frame (quanto cada draw pinta). |
| `RSX_DRAW_CSV=<arquivo>` (+`_FROM`, `_GATE`) | Estado de cada draw em CSV. |
| `LD_HLSL_DUMP=<pasta>` | Grava os shaders HLSL gerados. |
| `RSX_FP_SRC_SAT=0` | Compara o tradutor antigo, que ignorava o modificador SAT dos operandos FP. A correção fica ligada por padrão: limita o operando a 0–1 antes de abs/negação, conforme a instrução. Reiniciar para trocar; não é um ajuste de brilho. |
| `LD_USED_TEXTURES=1` | Experimento: prepara somente as unidades de textura lidas pelo programa FP, incluindo caminhos condicionais. Desligado por padrão: menor custo local, sem ganho consistente de FPS/CPU/p99 no percurso testado. Diagnósticos `LD_PS_TEX0`/`LD_PS_DIAG` forçam o caminho completo. |
| `LD_HITCH_PROF=1` | Registra preparação de PSOs e flushes acima de 1 ms, com frame, compilação VS/PS, criação de pipeline e espera da GPU. Desligado por padrão; logs/temporizadores perturbam a cadência e devem ficar fora do A/B final. |
| `PS3_PERF_TRACE=1` | Trace diagnóstico Windows, desligado por padrão: eventos de VFS (prefault/trava/leitura), inflate/sinalização, texturas, PSO, flush e gaps ≥33⅓ ms entre submissões. Timestamps QPC absolutos em µs, campos `a`/`b` dependem do evento. O log pode causar atraso; não usar como benchmark nem somar eventos sobrepostos como CPU exclusiva. |
| `PS3_PERF_TRACE_START_US=<QPC µs>` | Início absoluto do recorte. O script calcula a partir de `Stopwatch.GetTimestamp()/Frequency`, normalmente +60 s. Ausente/zero começa imediatamente. |
| `PS3_PERF_TRACE_MIN_US=<µs>` | Limiar de duração dos eventos, padrão 250. Teto de 20.000 eventos por unidade de compilação; gaps usam o limiar próprio de 33⅓ ms. |
| `LD_SHADER_PRELOAD=0` | Desliga o pré-carregamento de shaders, ligado por padrão (variável ausente/vazia também usa o padrão). Usa uma thread de baixa prioridade e um pack versionado no diretório de cache. Confere os bytes completos do HLSL; faltas/erros continuam no carregador normal. Reiniciar ao trocar. |
| `LD_SHADER_PRELOAD_MB=<MiB>` | Limite dos registros retidos pelo preload: padrão 64, faixa 1–256. Índice, blobs quentes do renderizador e outras alocações são adicionais. Ao atingir o limite, preserva o pack completo anterior e usa o carregador normal para as faltas. |
| `LD_PSO_LIBRARY=0` | Desliga a biblioteca persistente de PSOs D3D12, ligada por padrão (variável ausente/vazia também usa o padrão). É separada do cache de bytecode e do cache PSO em RAM. Cache incompatível, não suportado ou ausente recua à criação normal. Usa o mesmo diretório de shaders; `RSX_NO_SHADER_DISK_CACHE` também impede sua inicialização. |
| `LD_PSO_LIBRARY_MB=<MiB>` | Limite do blob serializado de pipelines: padrão 128, faixa 1–256. Não é um limite de toda a memória interna do driver. Ao atingir o teto, para de acrescentar pipelines; os desenhos continuam normalmente. |
| `LD_PSO_LIBRARY_SAVE_MS=<ms>` | Intervalo de gravação em uma thread de baixa prioridade: padrão 10000, faixa 1000–60000. Shutdown sinaliza/junta a thread antes da gravação final e liberação dos recursos. |
| `LD_PSO_WARMUP=1` | Experimental, desligado por padrão (ausente/vazia/`0`). Prepara objetos PSO conhecidos de sessões anteriores em uma thread de baixa prioridade; o renderizador só consulta objetos prontos e nunca espera essa thread. Liga a gravação de receitas por padrão. Exige a biblioteca/root compatíveis e usa o diretório de shaders; sem suporte/receita usa criação normal. Reiniciar ao trocar. |
| `LD_PSO_WARMUP_RECORD=1` | Grava receitas para a próxima sessão, mesmo com `LD_PSO_WARMUP=0`; `=0` impede acrescentar receitas ao pack. Ausente/vazia acompanha `LD_PSO_WARMUP`. Writer a cada 10 s e no shutdown normal, substituição atômica; limite/cancelamento da leitura preserva o pack completo anterior. Sem receitas, a primeira sessão continua criando os PSOs pelo caminho normal. |
| `LD_PSO_WARMUP_MB=<MiB>` | Teto de entradas/receitas retidas: padrão 64, faixa 1–256. Objetos/alocações do driver e buffers temporários são adicionais. |
| `LD_PSO_WARMUP_MAX=<n>` | Máximo de objetos/receitas antecipados: padrão 512, faixa 1–2048. Faltas ou teto atingido usam criação normal. |
| `LD_PSO_WARMUP_TIME_MS=<ms>` | Orçamento suave desde o início da leitura/preparação: padrão 5000, faixa 100–30000. Verificado entre objetos; não aborta uma chamada do driver em andamento. Receitas restantes usam fallback normal, sem espera pelo worker. |
| `LD_FLUSH_PROF=1`, `PS3_FC_PROF=1` | Onde vai o tempo do renderizador e do relógio de frames. |
| `RSX_D3D_DEBUG=1`, `RSX_D3D_GBV=1` | Ativa validação D3D12 e, opcionalmente, validação pela GPU (lenta). Na build da etapa 14, variável ausente/vazia ou `0` desliga; GBV depende do debug ligado. Builds anteriores usavam presença, incluindo string vazia/`0`, e podiam ativar validação ao tentar desligá-la. |
| `PS3_SAMPLE=<ms>` + `PS3_SAMPLE_MAIN=1` [+`PS3_SAMPLE_THREAD=<nome>`] | Amostrador de CPU de uma thread (`HighGraphics`, `spu1000_0`...). Gera um `.map` para os nomes. |
| `PS3_SAMPLE_FUNCTIONS=1` | Agrupa RVAs pelo início da função nativa antes do corte de 25 entradas. Evita dispersar helpers grandes em vários buckets. Resolver com `scripts/profile_symbols.py <run.err> <map-do-executável>`. Usar somente na investigação; sampler suspende a thread e altera tempos de jogo. |
| `SYSCALL_TRACE_THREAD=<nome>` / `SYSCALL_TRACE_TID` | Syscalls lentas de uma thread. |
| `SYSCALL_TRACE_SC=<n>` [+`SYSCALL_TRACE_A0=<hex>`] | Toda chamada de uma syscall, de qualquer thread. |
| `RECV_TRACE_TID`, `COND_PROF` | Recepções de eventos; esperas em condition variables. |
| `SAIL_DUMP=<pasta>` | Grava 1 quadro dos vídeos a cada 300. |
| `SAIL_AUDIO_RAW=<arquivo>` | Grava o som dos vídeos que vai para os alto-falantes (s16le estéreo 48 kHz). |
| `DES_WATCH=<ea>:<len>:<s>`, `DES_DUMP=<ea>:<len>:<s>` | Vigia/despeja memória do jogo. |
| `SPU_LLAR_BACKOFF=1` | Experimento (mais lento, desligado). |
| `SPU_WRCH_DIRECT=1` | Experimento de escritas simples de canais fora da função de DMA/diagnósticos. Desligado por padrão: A/B não mostrou ganho de FPS. Presença de `SPU_CHHIST`, inclusive `0`, força o caminho completo. Comandos, tag update, mailboxes e decrementer mantêm o runtime original. |
| `SPU_CH_BLOCK=0` | SPUs voltam a girar esperando canais (padrão: dormem; com 6 threads, 13–25 → 45–49 fps em Boletaria). |
| `FC_SPIN_US` | Giro do `frame_clock` enquanto o jogo desenha (µs). Padrão: 100000 com 10+ processadores lógicos, 0 com menos. |
| `PS3_USLEEP_MODE=spin\|sleep\|progress` | Como um `usleep` curto espera (padrão `spin`). |
| `SPU_LLAR_WAIT=1` | SPU que relê a mesma linha atômica sem mudança dorme até alguém gravar nela (desligado: sem ganho medido). |
| `SPU_FAST_FIX=0` | Volta ao conserto antigo (lento) de soma/multiplicação de float SPU fora da faixa normal. |
| `SPU_DRAIN_SLOW=1` | Despachante SPU com todas as checagens a cada salto (o padrão pula as de diagnóstico). |
| `PPU_WRITE_WATCH_FAST=0` | Volta às verificações completas de watch em cada store PPU. Padrão: evita o helper pesado quando nenhum diagnóstico de escrita foi selecionado no início do processo; o watch dinâmico de barreira continua consultado em cada escrita. Não desliga coerência, reservas, atomics ou guardas de memória. `PPU_WWATCH`, `PPU_WVAL` e seus modificadores presentes conservam o caminho completo, inclusive com texto `0`. Reiniciar para trocar. |
| `SPU_REGCACHE=0` | Seleciona as funções SPU originais para A/B. Padrão: versões com registradores locais, geradas com `PORT_SPU_REGCACHE=ON`. Seleção fixa na primeira execução SPU; reiniciar para trocar. `SPU_LS_WATCH`, `SPU_LS_LOWREAD` ou `SPU_SMC_WATCH` presentes selecionam originais para preservar observação do contexto. |
| `SPU_RCV_RATE=1` | Eventos por segundo que cada thread SPU do lv2 recebe. |
| `PS3RECOMP_SPU_MERGE=1` (no build) | Lifter SPU com `--merge-chunks` (laços em C). Experimental: trava em Boletaria. |
| `LD_GPU_PROF=1` | A cada 5 s: tempo de GPU por quadro (média, máximo, acima de 16,7 ms) e por motivo de flush. |
| `LD_SWAP_COUNT=3` | Três buffers no swap chain (padrão 2; sem ganho medido). |
| `LD_SRV_KEEP=0` | Cada draw volta as superfícies que leu para alvo de renderização logo depois (padrão: ficam em leitura enquanto os draws seguintes as leem). |
| `LD_ASYNC_SHADERS=0` | Compila shaders fora do cache na thread do renderizador, como antes (padrão: em segundo plano; os draws que dependem dele esperam 1–3 quadros). |
| `LD_GEOM_ARENA=0` | Geometria do cache volta a ser copiada para os anéis a cada draw (padrão: área persistente). |
| `LD_CB_REUSE=0`, `LD_SRV_REUSE=0` | Sempre reenviar as constantes de vértice / a tabela de descritores (padrão: reaproveitar quando não mudaram). |
| `LD_DESCRIPTOR_CACHE=1` | Índices experimentais de tabelas SRV/samplers, desligados por padrão: não houve ganho consistente de FPS sem diagnóstico. Chave completa dos 16 slots; mudanças de descritores CPU e reciclagem dos anéis invalidam as entradas. Fence sem reciclagem preserva cópias imutáveis. Arrays fixos de 698.880 bytes, sem ampliar os heaps GPU. `LD_SRV_REUSE=0` também desliga o novo índice SRV. Reiniciar ao trocar; contadores com `LD_FLUSH_PROF=1`. |
| `PS3_USLEEP_MODE=spinwake` | `usleep` curto que gira mas sai quando um SPU grava na memória. |
| `PS3_THREAD_CPU=<s>` | A cada s segundos, quanto de um núcleo cada thread gastou. |
| `SPU_LOCK_STRIPES=0` | Volta à trava global única para as atômicas dos SPUs (padrão: 64 travas por linha, +4 fps). |
| `SPU_LOCK_PROF=1` | A cada 5 s: aquisições/s, % com espera, núcleos esperando/segurando, e quem chama. |
