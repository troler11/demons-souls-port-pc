> Diário técnico do port, mantido durante o desenvolvimento. Os números de item seguem a ordem em que as coisas foram feitas, e os caminhos (`des_port/`, `p2/`, `lle/`) são os da árvore de trabalho original: neste repositório eles correspondem a `port/`, `work/game/` e `work/lle/`.

# Demon's Souls (BLUS-30443 v01.00) — recompilação estática, fase 0

## 1. ISO
- `Demon's Souls (USA).iso`, 8.759.410.688 bytes, `PS3VOLUME`, 12.512 arquivos.
- O header tem a tabela de regiões do Redump (3 regiões em claro e 2 "encriptadas"), mas **os setores já estão decriptados**: a dkey não é necessária.
- A extração foi feita com um script local (não incluído, porque usa chaves da Sony). Use o PS3Dec ou o RPCS3 (veja o README).
- No Git Bash, rode com `MSYS_NO_PATHCONV=1`, senão os caminhos `/PS3_GAME/...` são convertidos para caminhos do Windows.

## 2–3. Executável
- `dump/PS3_GAME/USRDIR/EBOOT.BIN` é um SELF de 28 MB. Decriptado com `rpcs3 --decrypt` (firmware 4.xx instalado no RPCS3) → `EBOOT.elf`.
- ELF64 big-endian, PPC64 (e_machine 0x15), entry 0x1916138.
- Não há SPU em arquivo separado nem `.sprx` no disco: tudo está dentro do EBOOT.

## 4. Análise (`scripts/elf_report.py`, saída em `elf_report.txt`, imports em `imports.json`)
| Métrica | Valor |
|---|---|
| Segmento de código+rodata | 25,4 MB (até ~6,3 M instruções; inclui dados) |
| `blr` (aprox. nº de funções) | 81.702 → estimo **60–80 mil funções** |
| Bibliotecas importadas | 25 |
| Funções importadas (HLE) | 389 |
| Imagens SPU embutidas | 12 |
| Havok | sim (física; usa SPU) |
| Edge (geometria SPU) | provável (strings "EDGE") |

Imports principais: cellSail 45 (player de vídeo), cellSpurs 37, sceNp 34 + sceNp2 26 (online), sys_net 29, sysPrxForUser 29, cellSysutil 27, cellGcmSys 21 (RSX), sys_fs 20, cellHttp 18, sys_io 18, cellAudio 12, cellAtrac 12, sceNpTrophy 10.

## 5. Recompilador: ps3recomp (sp00nznet, MIT, v0.15.0 de out/2026)
- Lifter PPU→C++ e SPU→C++ em Python, runtime LV2, RSX com backend D3D12 (Vulkan é opt-in), ~93 módulos HLE (cellSpurs, cellGcmSys, cellSail parcial, sys_net, cellHttp...).
- Estado: Simpsons Arcade jogável; Tornado Outbreak in-game a 10–16 fps; o resto ainda está em menus ou boot. O maior alvo até agora é Scott Pilgrim, com 59 mil funções, e só está "lifted".
- O boot PPU ainda é feito à mão para cada jogo.

## 6. Veredito
**Tecnicamente possível, mas muito difícil.** Demon's Souls seria provavelmente o maior alvo do ps3recomp (mais funções que Scott Pilgrim), com SPURS + Havok + Edge pesados no SPU e cellSail parcial.
Na prática: meses até ver o menu e mais de um ano até ficar jogável, contribuindo para o próprio ps3recomp no caminho.

Riscos:
1. Lifter não dar conta de 80 mil funções (jump tables, ponteiros de função, VMX).
2. SPU/SPURS do Havok.
3. Shaders RSX.
4. Online (sceNp) precisa ser desligado por stub.

## Fase 1, passo 2: ps3recomp no EBOOT (feito, exceto a compilação)
O venv com as dependências está em `tools/venv`, o clone em `tools/ps3recomp` e a saída em `p2/`.
- `ppu_loader.py`: 77.283 descritores OPD, **73.295 funções únicas**, 389 imports (bate com `elf_report.py`).
- `ppu_lifter.py` (~1 min, 12 workers):
  - **104.131 funções** convertidas (19.310 funções fundidas foram separadas, 11.136 wrappers de entrada no meio de função, 703 jump tables).
  - Resultado: 48 arquivos `.cpp`, **2,4 GB** e ~28 M linhas.
  - **Nenhum opcode desconhecido.** Só 16 avisos: funções no fim do segmento de código, sem continuação.
- Cobertura HLE (`scripts/hle_coverage.py` → `p2/hle_coverage.txt`): **307/389 (78%)**.
  - Faltam: cellSail 25 (vídeo; dá para pular as cutscenes FMV), online (sceNp 9, cellHttp 8, sys_net 6, sceNp2 2), cellSysutil 6, cellSpurs 5 (filas, importante), cellAtrac 5 (áudio), cellSaveData 1 (salvar!), cellL10n 1, cellGcmSys 1, cellAudio 1, cellKbRead.
- Compilação: o projeto `des_port/` foi criado do template e o CMake configurou, mas o **MSVC `cl` não é suportado** (`__int128`, `__atomic_*`). É preciso o **clang-cl** (componente "C++ Clang Compiler for Windows" do VS Installer, ou LLVM).

### Compilação e primeiro boot (feito)
- clang-cl 22.1.3 (VS 2026), `des_port/configure.bat` + `des_port/nin.bat` (ninja com vcvars).
- 1 chunk: 216 s, pico de 2,6 GB de RAM. Build completo com `-j8`: **17 min**.
- Link: o template do ps3recomp não inclui o `dxguid` (IID_IDXGIFactory6). Corrigido no `des_port/CMakeLists.txt`. Resultado: `MyGameRecomp.exe` de 419 MB.
- `stubs.cpp`: `sys_prx_register_library` (0x42B23552) → CELL_OK.
- Jogo extraído inteiro em `dump/PS3_GAME` (8,5 GB). Rodar com: `build\MyGameRecomp.exe dump\PS3_GAME\USRDIR\EBOOT.elf` (o VFS root é deduzido do caminho).
- **Boot:** TLS ok, carrega 6 módulos (RTC, L10N, FS, SPURS, RESC, NET), D3D12 abre a janela (RX 7600).
- **Primeira falha:** no init, `func_00B0AF28`, chamada por `func_00A99A90` ← `func_0001B078` ← `func_00015220` ← `func_000125F0` (crt/main), grava em NULL+0x2F, e depois vem uma cascata de NULL. r3/r31 contêm ASCII ("StNa", "onDi"), então provavelmente é um alocador ou singleton do engine devolvendo NULL. Próximo alvo de debug.

### Sessão de debug: do ponteiro nulo até frames na tela (18/10/2026)
Cada bloqueio, a causa e o conserto:

1. **r2 (TOC) errado → NULL.** Causa: bug do lifter. Os trampolins do linker que trocam de TOC (`std r2,0x28(r1); addis/addi r2; b alvo`, 16 bytes entre funções, sem OPD) eram tratados como "non-code": o `bl` sumia e o `ld r2,0x28(r1)` lia lixo.
   - Conserto: `scripts/add_toc_trampolines.py` adiciona os 1022 trampolins ao `functions.json` (`p2/out/EBOOT.functions_tramp.json`), depois relift.
2. **NID `cellSpursTaskAttributeSetExitCodeContainer`** → stub em `stubs.cpp`.
3. **Programas SPU.** 12 imagens ELF embutidas são convertidas pelo `PORT_SPU_INPUTS`. O job SPURS cru (2064 bytes em `0x0177B600`) foi despejado com `SPU_DUMP_MISS`, embrulhado com `wrap_spu_elf.py` (base 0) e entra como input extra; o fp do `.text` bate com o do job.
4. **Thread gráfica esperando o label RSX 255.** O SPU monta comandos na FIFO e publica `put` supondo que o buffer começa em `ioAddress+0x1000` (como no hardware). O ps3recomp começava em `ioAddress`, então o put ficava 0x1000 além e o último "label ← 2" nunca rodava.
   - Conserto: `PS3_GCM_BEGIN_OFFSET` (patch em `libs/video/cellGcmSys.c`). Para DeS: `PS3_GCM_BEGIN_OFFSET=1000`.
5. **Arquivos não encontrados.** O ISO9660 troca o 2º ponto por `_` (`systex_tex.tpf.dcx` → `SYSTEX_TEX_TPF.DCX`). Os nomes reais estão no UDF.
   - Conserto: montar a ISO (Windows lê UDF) e copiar com robocopy para `dump/PS3_GAME`.

**Estado atual:**
- O engine (Dantelion2) sobe ~23 threads: recursos, Havok, áudio MO, online Nexus, save.
- Abre e lê os arquivos do disco.
- O RSX desenha (draw_arrays, shaders, texturas) e **apresenta 36 frames (~7 fps) nos primeiros 5 s**; depois os flips param.

**Próximo bloqueio:** uma thread espera o bit 0x4 do event flag SPURS `0x48CBBD00`. As 5 tasks da imagem `412186069` (`spu_89175d51…`, fp `4F421747C0F7966A`, args `0x48CBC080…`) rodam, mas nunca setam o flag.

**Diagnósticos adicionados na cópia local do ps3recomp:**
- `PS3_SAMPLE_CHAIN=1`: cadeia de chamadas no sampler.
- `COND_BT_EVERY=n`: pilha guest no `sys_cond_wait`.
- `DES_DUMP=ea:len:s` (em `stubs.cpp`): dump de memória guest.

**Como rodar:** `PS3_GCM_BEGIN_OFFSET=1000`, depois `des_port/run.ps1 -Seconds N -Tag nome`.

### Sessão 2 (19/10/2026): carregador de arquivos

6. **`_cellSpursLFQueuePushBody` não existia** (retornava 0 sem fazer nada), e o init de LFQueue do ps3recomp usava um layout chutado.
   - Conserto em `stubs.cpp`: init e push com o layout/algoritmo real do `CellSyncLFQueue`, seguindo o cellSync do RPCS3. Cada CAS roda sob o lock de linha do SPU, e o sinal vai para a task via `_cellSpursSendSignal`.
   - Resultado: os workers consomem a fila.
7. **Descompressor `.dcx` (Edge zlib) travava às vezes.** Um dos 5 workers SPU (imagem 412186069) entrava no inflate e nunca voltava (segmento diferente a cada execução), então o contador do stream `zlibStm` nunca zerava.
   - O protocolo foi lido do código SPU: item `{src, dst, srcLen, dstLen, stream|1, flag, bits<<16}`; depois de escrever, faz decremento atômico do contador e, se era 1, seta `bits` no flag.
   - Conserto: **inflate no host** (`stbi_zlib_decode_noheader_buffer`, deflate cru) para as filas do Decompressor (init com lr `0x00C98D8C`), com o mesmo protocolo de conclusão. `DES_SPU_INFLATE=1` volta ao SPU.
   - Sem mais travas de flag em 3/3 execuções.
8. **Cache em `/dev_hdd1`**: é preciso existir `dump/cache`. O jogo cria as subpastas sozinho.

**Novo bloqueio:** a HighGraphics (`func_009F94A8`) espera o label RSX 255 alcançar o contador. O ring SPU está vazio (consumido) e `get=put=0x8000`, mas há comandos novos em `0x49F08000` que o RSX não executou: o `put` não avança depois do wrap do anel de comandos. Suspeita: o recycle/wrap da FIFO do ps3recomp vs. o anel de segmentos que o SPU monta.

**Detalhe do bloqueio da FIFO:**
- O jogo **não** usa a recycle do runtime (nenhum `[REC]`); ele mesmo gerencia os segmentos.
- O segmento do PPU (`0x8000–0x10000`) termina com `label255 ← N`, `0x1D74`, **`JUMP 0x10000`**. O segmento `0x10000+` é montado pelo SPU (draws), e em `0xFFE8` há `JUMP 0x8000`.
- O label para em valores variados (4 ou 0x26, dependendo da execução), com `get=put=0x8000`.
- Hipótese: o walker do ps3recomp (`cellGcm_rsx_process_fifo`) trata `put` atrás de `get` / JUMP para fora do anel de modo diferente do RSX real e para no segmento errado.
- Próximo passo: logar o walk (JUMPs seguidos, onde para) com `GCM_DRAINDBG` + trace de JUMP.

**Aprofundado (sessão 3):**
- A thread gráfica espera o label 255 ≥ contador (ex.: 0x27). Os labels **voltam a 0** (0..N, depois 0,1,2…): o RSX reexecuta o bloco de init que o SPU escreveu no início do anel.
- Layout: anel A = `[base+0x7000, base+0xF000)` com `JUMP base+0x7000` no fim (`+0xEFE8`). A área do SPU começa em `base+0xF000`. O SPU publica `put` relativo a uma base IO `0x1000` abaixo da do runtime.
- `PS3_GCM_BEGIN_OFFSET=1000` deixa tudo coerente, mas desloca a base. A alternativa equivalente é `PS3_GCM_BEGIN_OFFSET=0` + `SPU_CTRL_PUT_ADJ=1000` (patch em `spu_dma.h`). Nos dois casos o reset do label acontece no wrap.
- **Causa provável:** ordem de escrita/publicação no wrap. O RSX segue `JUMP início` e acha `put` adiante (publicado pelo SPU na área B) antes que o início do anel tenha sido reescrito. Falta descobrir quem escreve o início do anel após o wrap e por que o conteúdo ainda é o velho.
- Ferramentas: `SPU_CTRL_PUT_LOG=1`, `DES_SCAN=<word>:<ea>:<len>`.

### Sessão 3 (19/10/2026): frames contínuos

9. **FIFO resolvida.** Quem gravava o `JUMP begin` no fim do segmento era a heurística "recycled the title's own ring" do runtime (`cellGcmSys.c`): ela acha que o jogo estourou o segmento de 32 KB, mas o callback do próprio jogo já passa para o próximo segmento.
   - Conserto: `PS3_GCM_NO_TITLE_RECYCLE=1` (patch local, ligado no `run.ps1`).
   - Resultado: **~10 fps contínuos, 800+ frames**, label 255 sempre crescente.
10. **Renderizador live draw** ligado no `main.cpp` (como no `boot_main.cpp`), com `RSX_LIVE_DRAW=1`. Permite `LD_FRAME_DUMP=<pasta>` (PPM).
    - Os frames saem pretos: tela limpa + 1 quad. O jogo ainda está no carregamento inicial.
11. **Bug do runtime: flags do `cellFsOpen`.** Em `ppu_fs.cpp`, `O_CREAT`/`O_TRUNC`/`O_APPEND` estavam com valores errados, e o mesmo em `sys_fs.h`. Os valores do SDK são octais: `CREAT 0x40`, `EXCL 0x80`, `TRUNC 0x200`, `APPEND 0x400`.
    - O jogo abre com `0x41` e caía em `ENOENT`, então nunca gravava o cache em `/dev_hdd1`. Corrigido; o cache agora é gravado.
12. `cellSpursLFQueueDetachLv2EventQueue` → stub OK.

**Bloqueio atual:** o carregador para num ponto **aleatório** (5, 10 ou até 15 arquivos). As threads `Core.Res.*` ficam ociosas em `cond_wait` e a main continua no laço de frame.
- `COND_TRACE=1` (novo) não mostra sinal perdido no `sys_cond`.
- `PS3_COND_SPURIOUS_MS` estabiliza em 10 arquivos, mas o engine dá panic (`DLConditionSignal.cpp(392)`) e o heap corrompe com wakeups espúrios. `PS3_COND_SPURIOUS_OK_MS` também quebra. **Não usar.**
- Próxima suspeita: atômicos PPU (`lwarx/stwcx`) nas filas lock-free do Dantelion, ou outra primitiva (lwcond/evento).

**Sessão 4: diagnóstico da trava aleatória**
- A main (tid 1) fica bloqueada no `cond 13` (Havok, criado em `0x0132A684`), que só a "Havok PPU Thread" (tid 11) sinaliza.
- A tid 11 espera eventos na fila 5. Esses eventos vêm de **tasks SPURS do Havok** (imagem `352410531`, taskset `0x478ED800`) via porta SPU (`src 0xFFFFFFFF53505501`, `d2=0x300000003`).
- Numa execução travada: 9 tasks lançadas, todas "RETURNED", mas só 4 passaram pela syscall de saída e só 4 eventos chegaram.
- O escalonador SPURS HLE termina algumas tasks sem o caminho de saída real, de forma dependente de timing. É o problema clássico que `docs/FIRMWARE_LLE.md` descreve.
- **Próximo passo recomendado:** LLE do `libsre.sprx` (firmware no RPCS3: `dev_flash/sys/external/libsre.sprx`).
  - Decriptar com `rpcs3 --decrypt`.
  - Depois `tools/lift_prx.py --base 0x02000000`, `ppu_lifter --raw` e carga/ligação em `ps3_load_prx_modules()` (`stubs.cpp`).
- Testado e descartado:
  - `PPU_RESV_STORE=1`: sem efeito.
  - Wakeups espúrios: o engine dá panic.
- Achado extra: o grupo SPU `_synth2` (áudio) é lançado com a imagem errada (295448077) e dá FAULT. Falta também `cellAudioSetPortLevel`.

### Sessão 5 (19/10/2026): SPURS real (LLE do libsre)
13. **libsre LLE funcionando.** Passos:
    - `rpcs3 --decrypt` em `dev_flash/sys/external/libsre.sprx` (pasta `lle/`).
    - `tools/lift_prx.py --base 0x30000000` (TOC `0x30039AB0`; 267 exports, 32 imports).
    - `ppu_lifter --raw --toc 0x30039AB0 --hle-stubs libsre_hle_stubs.json --symbol-prefix libsre_`: 440 funções.
    - Ligação: `lle/libsre_glue.cpp` (`prx_load_module` + exports gerados em `libsre_exports.inc`), carregado em `ps3_load_prx_modules()` com `DES_LIBSRE=1`. `libsre.prx` também entra em `PORT_SPU_INPUTS` (2 imagens SPU: kernel e política).
    - `YDKJ_FORCE_HLE=8A85674D,011EE38B,73E06F91` mantém o push/init de LFQueue e o detach do port.
    - Resultado: **0 chamadas no SPURS HLE** e acabaram as travas do Havok.
14. **Flag SPU→PPU no inflate do host.** O `cellSpursEventFlagSet` real do PPU recusa flags SPU2PPU (`0x80410909` PERM). Implementei o set "do lado SPU": CAS de 64 bits no `ctrl`, `pendingRecv`, `ppuPendingRecv` e evento na `eventQueueId`, seguindo o layout do RPCS3.
15. **Inflate assíncrono** (thread host + cópia do item). O síncrono gerava panics em `Decompressor.cpp(241/421)`.
16. **Imagem SPU por conteúdo** (`spu_lifted_thread.c`): o `_synth2` (áudio) e o kernel do libsre têm a mesma entrada `0x100`, e o runtime escolhia a errada (FAULT). Agora escolhe pela impressão digital do ELF importado, e o áudio roda.
17. **NP offline coerente** (`stubs.cpp`): `GetNpId`/`OnlineName`/`OnlineId`/`AvatarUrl` → `SCE_NP_ERROR_OFFLINE`, convites → `ID_NOT_FOUND`. Stub de `cellAudioSetPortLevel`.

**Estado:** o carregamento passa por fontes, msg, params, sons, sfx, `menu/*.tpf`, `chr/c0000` e `facegen` (até 254 arquivos numa execução; com cache limpo, tipicamente 29–54 do disco), com ~10 fps contínuos e sem panics. Os frames ainda saem pretos e ninguém lê o controle (ainda não chegou à tela inicial).
- A parada atual varia: o FileLoader fica em `func_00BF62B0` → `0x00AFACB0`, Decompressor/MOStreaming em waits do libsre.
- Cuidado: o cache em `dump/cache` gerado por execuções com bug pode atrapalhar; limpe-o ao testar.

**Diagnósticos novos:**
- `EVT_TRACE=<fila>`: todo evento empurrado na fila.
- `COND_TRACE=1`: waits e signals do `sys_cond`.
- `PS3_SYNCLOG` agora loga também os `destroy`.
- `RSX_LABEL_LOG=n`: cap do log de labels.
- `PS3_THREAD_DUMP=s`: pilha de todas as threads guest (runtime).
- `DES_LFQ_WATCH=s`: estado das filas, do último item, do contador e do flag.
- `DES_ATOM_FOLLOW=obj|flag`: trace de atômicos na linha dinâmica.
- `SPU_ATOMTRACE_IMG`, `SPU_GET_DUMP_LSA`, `SPU_MFC_TRACE=3`.
- `DES_DUMP` com vários intervalos.

### Primeira imagem (19/10/2026)

O jogo mostra o aviso de auto-save com o botão OK, igual ao RPCS3 no mesmo ponto (`des_port/fd/frame_000900.ppm`). O RPCS3 da pasta do usuário serviu de oráculo: em 100–240 s ele abre os mesmos arquivos (o último é `facegen.fgbnd`) e para no mesmo diálogo, esperando o botão. Ou seja, o carregamento não estava travado; faltava a imagem.

18. **`sys_spu_thread_group_disconnect_event_all_threads` (syscall 252)** (`lv2_register.c`). Era stub. Cada abertura do decompressor EDGE anexa uma event queue, e o detach nunca liberava a porta. Depois de ~48 arquivos o attach dava `0x8041090A` (BUSY), e vinham os panics `Decompressor.cpp(230/241/421)`. Com a 252 implementada: zero panics.
19. **Argumentos na pilha para funções LLE** (`ppu_hle.cpp`). O runtime chama o export do libsre com `r1` deslocado 0x1000, então do 9º argumento em diante o callee lia lixo. `_cellSpursJobChainAttributeInitialize` recebia `maxSizeJobDescriptor=0` e retornava INVAL. Todo upload para a VRAM (`func_00AFC2D0`: ucode de FP, LUTs) é um job chain SPURS de memcpy, então os fragment programs ficavam zerados e tudo saía preto. Agora a área de parâmetros (`r1+0x30..0x100`) é copiada para o frame novo.
20. **Job SPURS de memcpy** (`p2/jobs/spujob_memcpy_177B480.elf`, 0x180 bytes em `0x0177B480`, logo antes do job já lifted). Embrulhado com `wrap_spu_elf.py` e adicionado ao `PORT_SPU_INPUTS`.
21. **Textura 1D** (`rsx_live_draw.c`). O passe final lê uma LUT de gama 256×1 declarada como 1D; o decoder só aceitava 2D e usava a textura branca de fallback (tela branca). 1D agora vira 2D de altura 1.
22. **Trava de E/S no `ppu_fs.cpp`** e `cellFsAioRead` absoluto (salva e restaura a posição), com conclusão assíncrona opcional (`PS3_FS_AIO_ASYNC=1`). Não era a causa dos panics, mas evita corrida.

**Diagnósticos novos:** `DES_WATCH=ea:len[:s]` (protege a página e loga cada escrita com a função lifted que escreveu), `DES_SAVE=ea:len:arquivo`, `DES_A2O_LR=lo:hi`, `FP_ADDR_DBG=1`, linhas `[fp-map]` no `RSX_DRAW_CSV`, `ppu_guest_func_for_rip()` no `ppu_loader.cpp`. Os `fprintf` `[des-dec230]`, `[des-mdec]`, `[des-edge]`, `[des-xfer]`, `[des-jc]` e `[des-jcattr]` estão nos `.cpp` gerados (`p2/recompiled/ppu_recomp_005.cpp`, `lle/recomp/libsre_recomp_000.cpp`) e somem quando eles forem regenerados.

### Do diálogo ao carregamento do mapa (20/10/2026)

A entrada já funcionava (XInput, teclado: Z=X, X=O, Enter=START, setas; `PAD_SCRIPT="seg:máscara,..."` para testes sem janela). Sequência alcançada: auto-save OK → "Checking SAVE DATA" → logos ATLUS/SCE/Japan Studio/FromSoftware/Havok → vídeo (pulado) → título "PRESS START BUTTON" → NEW GAME → avisos de servidor/offline → Create Profile → loading "Selen Vinland"/"Saint Urbain" e carga do mapa m08 (tutorial).

23. **cellSail (vídeos)** (`libs/codec/cellSail.c`). A tarefa de vídeo do jogo (tabela de estados em `0x018716C0`) só chama `PlayerStart` depois do evento major 9 com arg0=0 após `OpenStream`, e só sai do fechamento depois do evento 5 (stream closed, arg0 = descritor) e do estado CLOSED. `SAIL_OPEN_DONE_EVENT=9` e `SAIL_CLOSE_EVENT=5` ligam esses eventos. O vídeo não é decodificado: o player diz que terminou na hora.
24. **Teclado virtual** (`libs/misc/cellOskDialog.c`). `LoadAsync` exigia um `Init` que o jogo não importa; agora não exige, manda `LOADED`/`FINISHED` (0x502/0x503) e `UNLOADED` (0x504) via `cellSysutilQueueEventAll` (novo, `cellSysutil.c`) e responde `PS3_OSK_TEXT` (padrão "Slayer").
25. **Detach de LFQueue no libsre real** (`stubs.cpp`, `run.ps1`). O stub de `cellSpursLFQueueDetachLv2EventQueue` não liberava a porta SPU que o attach real (libsre) ocupa; no mapa acabavam as 64 portas e voltavam os panics `Decompressor.cpp(230)`. O detach agora vai ao libsre (stub só com `DES_LFQ_DETACH_STUB=1`). Restam ~20 portas presas sem causa conhecida (`SPU_PORT_LOG=1` loga connect/disconnect).

26. **Vídeos tocam no host** (`libs/codec/sail_host_movie.c`, `SAIL_HOST_PLAY=1`). PAMF = cabeçalho de 0x800 + MPEG-PS: vídeo MPEG-2 1280×720 29,97 fps decodificado por `ffmpeg` (pipe RGBA → `rsx_live_draw_present_rgba` em modo filme), áudio LPCM estéreo 48 kHz s16 BE (private stream 0xBD sub-id 0x40, cabeçalho de 4 bytes) demuxado à mão → `waveOut`. `PlayerStart` só reporta o fim quando o filme acaba; `PlayerStop` (pulo) interrompe. A abertura `ds_advertise.pam` toca inteira (3896 frames ≈ 2:10) e depois `ds_title.pam`. `SAIL_FFMPEG` aponta outro ffmpeg; `SAIL_DUMP=<dir>` grava 1 frame a cada 300.

27. **Tabela de saltos não detectada no Havok** (`tools/ppu_lifter.py`, `JT_EXTRA`). A carga do m08 parava depois de `h1000b0.hkx`/`h1020b0.hkx` (colisão Havok): `func_01275EC0` tem um `switch` (bctr em `0x127612C`, tabela relativa de 108 entradas em `0x1276130`, base guardada na pilha em `0x388(r1)`) que a descoberta automática não viu; os 34 casos ficaram sem rótulo e o salto virou `unresolved indirect call -> 0x0127697C`. `JT_EXTRA=disp:base:n` injeta a tabela; o EBOOT foi re-lifted com `JT_EXTRA=0x127612C:0x1276130:108` (704 tabelas).
28. **`sys_memory_get_user_memory_size`** (`ppu_loader.cpp`): o atalho do syscall 352 respondia 256/192 MB fixos; agora delega para `sys_memory.c` (213 MB, disponível = total − alocado), como no PS3. Não era a causa da trava (o heap de carga `0x1AD65C0` tem 12,25 MB fixos e é temporário: os pedidos que falham são refeitos pela `Core.Res.TaskManager` e acabam passando).
29. **FPS**: com `PS3_VERBOSE=0` os menus rodam a ~30 fps (antes ~15) — o log por evento custava metade. Um `static int _n; if (_n++ < 8)` em `spu_drain.c` estourava (2³¹ chamadas) e voltava a logar sem parar; corrigido. `LD_VSYNC=0` desliga a espera de vsync no Present.

### Dentro do jogo (20/10/2026)

Com o save do perfil "Slayer" copiado para o RPCS3 (`dev_hdd0/home/00000001/savedata/BLUS30443DEMONSS005`) e o RPCS3 dirigido por `PostMessage` (script no scratchpad), ele entra no tutorial pedindo **a mesma lista de arquivos** que o port — que termina em `h1020b0.hkx` e `m08_*.tpf.dcx`. Ou seja, a "trava" do m08 era o fim normal do carregamento: o port já está **dentro do jogo**, a ~19 fps, com ~100 draws por frame e nenhum descarte. O problema agora é renderização: as superfícies da cena (`0x7A0000`, `0x4260000`, `0x5870000`) só mostram a cor de fundo e um polígono, e os display buffers saem pretos (nem o HUD aparece). Muitas texturas caem no fallback branco (`binds white` > `real`) e 3 decodificações falham (`fmt=0x9E`).

### Passe final preto (20/10/2026, em andamento)

Ordem de um frame no jogo (`LD_DRAW_DUMP` + `LD_BIND_LOG_FRAME`): sombras/blur pequenos → cena e HUD em `0x7A0000` (S3, ~80 draws) → **passe final** (PSO `b199c35a804f3e9f`, desenhado duas vezes, em `0x10000` e `0x3D0000`, os dois display buffers) → mapa de sombra `0x0` 2048² → próximo frame (limpa `0x7A0000` para cinza 32). O passe final lê a unidade 0 = S3 (`0x7A0000`) e a unidade 1 = LUT 256×1 (`0x3E0`).

- Logo antes do passe final, `0x7A0000` tem o HUD sobre fundo cinza (barras de vida, itens, "Tutorial"), mas a cena 3D é só fundo + um polígono (segundo problema, depois).
- `LD_PS_TEX0_TC=1` (saída = TC0): gradiente na tela. As coordenadas e a escrita no display buffer funcionam.
- `LD_PS_TEX0_TC=2` (metade `Sample`, metade `Load`, alfa 1): **as duas metades pretas**. Logo não é blend, sampler nem LUT: o descritor t0 que esse passe recebe aponta para um recurso vazio/nulo.
- O HLSL do PSO está certo (`Texture2D rsx_tex0 : register(t0)`). Não há MSAA. Layout do heap de CPU sem sobreposição. O caminho de gravação do draw (barreiras, `SetDescriptorHeaps`, tabela, root sig) parece correto.
- Rodando: `RSX_D3D_DEBUG=1 RSX_D3D_GBV=1` (validação na GPU, novo) para ver o descritor/estado do recurso no draw. Log em `des_port/gbv1.err`, linhas `[d3d-debug]`.

Suspeitas restantes: declarações `TextureCube` (t8, t11–t14) com SRVs 2D no mesmo descriptor table; estado do recurso S3 diferente de RENDER_TARGET na hora da barreira; algo entre a cópia da tabela e o draw.

Diagnósticos novos: `LD_PS_TEX0_TC=1|2`, `LD_DRAW_DUMP_FROM=<frame>`, `LD_BIND_LOG_FRAME=<frame>` (linha `[bind]` por draw: alvo, PSO, o que cada unidade resolveu, `S<n>` = superfície), `RSX_D3D_GBV=1`, info queue limitada a 6 mensagens por ID. No jogo o frame ~11500–12000 já está no tutorial (a ~545 s de execução).

**Causa do preto (20/10, tarde):** não é o renderer. Com `LD_FORCE_SLOT0=b199c35a804f3e9f:9` o passe final mostra outra superfície normalmente; GPU-based validation (`RSX_D3D_GBV=1`) não acusa nada; um flush antes do passe não muda nada. A leitura de S3 no instante do passe final (`LD_BIND_LOG_PSO` + `LD_DUMP_UNIT0`) dá RGB 0 em todos os frames do jogo. O último draw em S3 antes do passe final é um quad de tela cheia (PSO `a3aefd322c272622`, posição 0..0x2800 × 0..0x1680 = 1280×720 em 1/8 de pixel, cor de vértice `00 00 00 FF`, blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA, máscara RGB): **é o fade do próprio jogo, preso em alfa 1**, desenhado por cima de cena e HUD. Hipótese: o jogo continua no estado de "entrando no mapa" (mesma causa provável da cena 3D vazia). `LD_TRACE_FADE=1` (com `LD_TRACE_PSO`) loga a cor do fade ao longo do tempo.

30. **Deadlock do som (MultiStream) — causa do fade preso** (`lv2_register.c`, `spu_channels.c`, `sys_event.c`). Comparando a lista de arquivos com o RPCS3, o port nunca abria `sound/ds_bgm_*.mowb`. `PS3_MUTEX_DBG=1` (novo) mostrou `MOMainThread` esperando o mutex 0x45E, que estava preso por `MOMixerThread`, bloqueado em `sys_event_queue_receive` (fila 15) à espera da SPU de síntese (`_synth2`). Eram três falhas:
    - `sys_spu_thread_bind_queue` era stub. O `sys_spu_thread_receive_event` da SPU (stop 0x110) não tinha serviço para threads lifted, e a SPU repetia o pedido ~3000×/s. Agora o bind/unbind é real e `spu_stop` atende 0x110/0x111 no lugar (`g_spu_stop_service_hook`): bloqueia na fila vinculada e responde `{CELL_OK, d1, d2, d3}`.
    - O evento de volta (`send_event` na spup 0x3A, ligada por `sys_spu_thread_connect_event` só àquela thread) era descartado, porque a rota só olhava as portas do grupo. Agora há fallback para o vínculo por thread.
    - `/dev_flash` apontava para um caminho inexistente e o codec ATRAC (`flashATRAC.pic`) não carregava. O `run.ps1` passou a definir `PS3_DEV_FLASH` com o `dev_flash` do RPCS3 local.

    Com isso `ds_bgm_other.mowb` abre como no RPCS3.

31. **Fade sai; HUD aparece** depois do item 30. O mundo ainda saía preto. Investigação (`LD_BIND_LOG_PSO=depth`, `LD_BIND_DUMP_TGT`, `LD_TRACE_PSO=depth`, `LD_TRACE_VS_DIR`, `tools/vp_eval.py`):
    - Os vértices transformam certo: o personagem cai no centro, em NDC ≈ (0,16; 0,03; 0,9975).
    - O cull não importa. Com a profundidade desligada a geometria cobre a tela, mas em preto.
    - Atenção: `LD_FORCE_GREEN` quebra a compilação de shaders com `discard`, e um teste chegou a descartar 216 mil draws. Os testes de "verde" ficaram inválidos.
32. **TC8/TC9** (`rsx_vp_decompiler.c`, `rsx_fp_decompiler.c`). Os pixel shaders do mundo fazem `cor × TC8 + TC9`, a dispersão atmosférica: extinção ≈ 0,99 em `o[15]` e in-scatter ≈ 0,001 em `o[6]`, o registrador de point size. O VS só exportava TC0–7, e o FP lia as entradas 0xC/0xD como `float4(0,0,0,0)`. Agora o VS sempre exporta `t8=o[15]` e `t9=o[6]`, e o PS declara `tc8/tc9` só quando usa, para não quebrar a assinatura nos outros backends.
    - Diagnóstico de tela dividida (`LD_PS_DIAG=1`): col0, tc8 e tc9 estão corretos, e aparecem o personagem, o arco e o castelo. Mas a cor iluminada antes da dispersão (`h[1]`) continua 0. Próximo passo: constantes do FP (`[pso-fpc]` no trace), textura difusa e fator de sombra.
33. **Áudio pendente.** O `cellAtrac` não implementa streaming (`GetStreamDataInfo`/`AddStreamData`), então a BGM sai como ruído e o `.mowb` é reaberto a cada trecho.

34. **Exposição = 0 → writeback de superfícies** (`rsx_live_draw.c`). Os shaders do mundo terminam em `cor × c16.x / c17.y`, e `c16.x` (exposição) é gravado no microcódigo do FP pelo próprio jogo via NV308A. O valor vem da adaptação de luminância: a GPU renderiza uma cadeia 64×64 → 16×16 em superfícies **X32 float**, e a CPU/SPU lê o resultado na VRAM do guest. Correções:
    - Superfícies `F_X32` e `F_W32Z32Y32X32` passam a ser R32F/RGBA32F; antes eram RGBA8, que corta o valor. O formato do RTV no PSO acompanha (`ld_surface_dxgi`).
    - `LD_SURF_WRITEBACK=1` (padrão no `run.ps1`): a cada present, as superfícies ≤ 4096 texels desenhadas no frame são lidas e gravadas de volta na VRAM do guest, em big-endian e com o `SURFACE_PITCH` do guest.

    **Resultado: o tutorial aparece**, com o personagem de armadura no corredor, a abertura ao fundo e o HUD. A imagem ainda está muito escura (precisa de ~6× para ver bem) e borrada.

35. **Adaptação de olho congelada** (`rsx_live_draw.c`, busca de superfície para textura). Com os formatos float, cada endereço da cadeia de luminância ganhou duas superfícies: a RGBA8 antiga dos menus e a R32F nova. A busca pegava a primeira do mesmo tamanho, que era a antiga. O pior caso era o draw de adaptação, que lê o próprio alvo (`novo = velho + (alvo − velho)·(1 − 2^(−dt·k))`): em vez da cópia de feedback, ele recebia a gêmea antiga, e o valor ficava preso em 0,603. Agora a busca prefere a superfície desenhada mais recentemente e ignora as que estão no mesmo endereço do alvo. A adaptação converge para 2,222 (chave 0,18 / luminância limitada a 0,08) e o brilho da cena vai de 5 para 18,5, contra 27 no RPCS3, no mesmo ponto. Ainda faltam: o DOF desfoca a tela inteira, a abertura ao fundo sai preta (céu/castelo), e faltam o escudo e a névoa.

36. **Profundidade lida como cor (DOF)** (`rsx_live_draw.c`, `zdepth_packed`). A composição da profundidade de campo amostra o zeta (0xB50000, tile 2) como textura A8R8G8B8, um truque comum no PS3. O z de 24 bits chega em A/R/G e o stencil em B, como numa palavra D24S8 big-endian. Como a profundidade só existe no host, a leitura vinha da VRAM do guest e a máscara mandava desfocar tudo. Agora um compute shader empacota o snapshot R32F numa textura RGBA8 (slots `SRV_ZPACK_BASE`/`UAV_ZPACK_BASE`, reempacotada quando o snapshot muda), e o bind usa essa textura quando um fetch A8R8G8B8 cai no endereço de um zeta. **A imagem ficou nítida** e muito próxima da referência do RPCS3. Ainda faltam o céu/castelo visto pelo arco, o escudo e a névoa.

37. **Textura D8R8G8B8 (0x9E)** (`rsx_live_draw.c`). Eram seis texturas com `decode failed` (cubemaps de ambiente de 32×32 e 64×64, mais uma de 256×256) que caíam no branco de fallback. O formato agora é tratado como A8R8G8B8 com alfa = 1. **O céu e o castelo aparecem pelo arco** do tutorial, como no RPCS3. Ainda faltam o escudo do personagem e a névoa/feixes de luz.

### Desempenho (21/10/2026)
38. **Módulos de política do SPURS rodavam interpretados** (~9 fps). O kernel SPURS do libsre carrega três policy modules crus em LS 0xA00. Eles foram extraídos e empacotados (`p2/jobs/sre_pm{A,B,C}_*.elf`, entrada em `configure.bat`), e as fontes foram registradas como overlay em `stubs.cpp` (`spu_overlay_register_source`): 0x30021480 → pmA, 0x30023680 → pmB (taskset), 0x3002A180 → pmC. Resultado: **~28 fps**. Os profilers novos são `SPU_INTERP_PROF=1` e `SPU_INTERP_LSDUMP=<dir>`, ambos em `spu_interp.c`.
39. **Streaming do cellAtrac** (`cellAtrac.c`, NIDs registrados em `stubs.cpp`): `GetStreamDataInfo`, `AddStreamData`, `IsSecondBufferNeeded`, `GetSecondBufferInfo` e `SetSecondBuffer`. Com o jogo mais rápido, o MOPS3StreamLoader entrava em pânico ("Stream Memory Free State Error") e travava. Isso acabou: 0 pânicos e 0 erros de ATRAC.
40. **Tasks SPURS do EBOOT rodavam interpretadas** (queda para ~11 fps no fim do tutorial). Eram duas: LS 0xF2FC (ELF em 0x01832680) e 0x307C (ELF em 0x0177C200). A política de taskset (pmB) carrega cada ELF pelo campo `TaskInfo.elf` (LS 0x2794). As 11 tasks do EBOOT (base LS 0x3000) foram registradas com `spu_taskset_register_task_elf(elf_ea, image, 353941178)` em `stubs.cpp`. Resultado: **30 fps estáveis o tempo todo**, mínimo de 23, sem nenhum código SPU interpretado (run tk1).
41. **Destravar FPS (patch "Unlock FPS" do RPCS3, de Gibbed e Whatcookie).** O hash PPU é 83681f61…, e o patch.yml público vem de rpcs3.net. São duas instruções, transformadas em hooks com `scripts/apply_fps_unlock.py`, que é idempotente e **precisa ser rodado de novo após cada re-lift** (`p2/recompiled/ppu_recomp_000.cpp`):
    - 0x25ED8, em `func_00025DD8`: `stb r29,0x6ED(r31)` passa a gravar r0 (=1). Essa é a flag de frameskip/60 Hz.
    - 0x1B964, em `func_0001B8D0`: o timestep fixo `lfs f31,-0x7F78(r30)` passa a ser o dt real, limitado a 50 ms.
    - Os hooks `des_fps_frameskip_flag` e `des_fps_frame_dt` ficam em `stubs.cpp` e só valem com `DES_FPS_UNLOCK=1`. Os valores 2 (só a flag) e 3 (só o dt) existem para diagnóstico.
    - Resultado: **60 fps** nos menus e no começo do tutorial. Na parte pesada do tutorial cai para ~16–20 fps, abaixo dos 30 do modo normal, então o modo 60 **ainda não deve ser o padrão**.
42. **Diagnóstico da queda no modo 60** (ainda aberto):
    - `COND_PROF=1` (novo, `sys_cond.c`) mostrou a thread principal esperando no cond 46, que é sinalizado pela thread **HighGraphics** (tid 6), a thread de render do jogo.
    - O sampler da HighGraphics (`PS3_SAMPLE_MAIN=1 PS3_SAMPLE_THREAD=HighGraphics`, novo, com unwind até o primeiro frame do exe) indicou 80% do tempo dentro de `sys_timer_usleep`, em polling de usleep(30) no lr 0x00ACB0A0 (`func_00ACAFA8`). O laço grava um descritor de 0x90 bytes (cabeçalho 0x9023) num ring e espera uma palavra de conclusão ficar ≠ 0. Parece um job SPU ou consumo do RSX.
    - Com a flag ligada, cada frame tem cerca de 2× mais pacotes RSX (~1500 contra ~800). A vazão fica num teto de ~24k pacotes/s nos dois modos (16 fps × 1500 ≈ 30 fps × 800), então o teto é a vazão desse caminho, não a CPU, que fica quase toda ociosa.
    - Também testado sem ganho claro: `PS3_DRAIN_US=250`.
    - Corrigido de passagem: `sys_timer_usleep` ≥ 1 ms agora usa timer de alta resolução por thread. Antes arredondava para o tick de 15,6 ms.
    - Em andamento: `PS3_FC_PROF=1` (novo, `main.cpp`) mede o laço do `frame_clock` (espera do timer / FIFO / present / pump). Run `fp1`, ver as linhas `[fc-prof]`. Não amostrar o `frame_clock` com `PS3_SAMPLE_THREAD`, porque suspender a thread D3D derruba o processo.
44. **Trava ao sair do túnel: jump tables não reconhecidas.** O log mostrava `unresolved indirect call -> 0x01278270`, um case do `bctr` em 0x1277A2C. É o mesmo padrão do decodificador Havok: a base da tabela fica no stack (`lwz rX,D(r2)` → `std rX,S(r1)` … `ld r11,S(r1)`). O script `scratchpad/find_spilled_jt.py` varreu o EBOOT e achou 4 desses dispatchers: 1 já corrigido, o que causava a trava, e 2 latentes. O lift foi refeito com:
    `JT_EXTRA=0x127612C:0x1276130:108,0x1277A2C:0x1277A30:108,0x15EFE34:0x15EFE38:29,0x15F1604:0x15F1608:29`
    Só 3 dos 47 arquivos mudaram. Backup do lift anterior em `p2/recompiled_bak`. **Depois de qualquer re-lift, rode `scripts/apply_fps_unlock.py`.**
45. **Do port de God of War: Ascension** (github.com/arnaldolima1/gow-ascension-pc): é o mesmo runtime ps3recomp, com 44 patches. Portados para cá:
    - patch 0021: DDX/DDY no decompilador de FP, e sRGB de saída e gama de textura no live draw (o DeS não usa, então não faz diferença aqui);
    - patch 0039: amostragem de profundidade com comparação (shadow), unit 7 no tutorial (`LD_SHADOW=0` desliga);
    - patch 0025: vblank pelo QPC a 59,94 Hz (antes GetTickCount64 com período de 16 ms);
    - evento de *kick* do FIFO: o `frame_clock` agora acorda com ele, e um usleep curto de guest dispara o kick (`GCM_USLEEP_KICK=0` desliga). Antes o evento existia, mas ninguém esperava por ele.
    O usleep com timer de alta resolução e o TEX8/TEX9 deles já tinham sido feitos aqui de forma independente.
46. **Desempenho do live draw**:
    - `LD_FLUSH_PROF=1` (novo) mostra, a cada 5 s, os flushes por motivo e o tempo gasto em cada etapa do draw (fetch/remap/decode/pso/tex/desc).
    - O re-hash do conteúdo de toda textura usada, a cada frame, virou preguiçoso: uma textura estável é verificada a cada 16 frames (`LD_TEX_REHASH_LAZY=0` volta ao antigo). Tex caiu de 350 para 60 ms a cada 5 s.
    - O writeback de luminância criava um recurso e fazia um flush completo por superfície, por frame. Agora usa buffers persistentes e **um** flush por frame.
    - O modo `LD_SURF_WRITEBACK_ASYNC=1` (sem flush, com 1 frame de atraso) ficou fora do padrão: suspeito de duas travas.
    - Decode de vértices em lote (`rsx_vertex_fetch_batch`, `LD_VERTEX_BATCH=0` desliga): imagem correta, mas sem ganho mensurável. O custo é o volume (~140 mil vértices por frame em float4).
47. **O que limita o modo 60 na parte pesada** (`DES_WATCH` com timestamp na palavra que a HighGraphics espera, 0x01B50FD0):
    - A palavra é escrita por DMA de SPU (job em LS 0xC408). É **um job por frame**, que leva ~44,5 ms do envio à conclusão; depois a HighGraphics gasta mais ~18,8 ms até o próximo envio. Total ≈ 63 ms, ou seja, 16 fps.
    - Esse job monta o command buffer do RSX, então inclui o processamento do FIFO pelo live draw (~26 ms por frame: draws ~13 ms, sendo ~9 de fetch de vértices; o resto é outros métodos e o present).
    - Caminhos para melhorar: cache de vértices decodificados na GPU (pular decode e upload em geometria estática), present sem sincronizar a CPU com a GPU (rings particionados por frame) e acelerar o código PPU (o patch 0038 do gow, com stores inline, deu ~10% lá).
48. **Trava intermitente ao entrar no tutorial** (~2 em 9 runs): os frames param logo depois do 3º `cellAtrac SetData` (faixa de 121 frames). Nos runs bons, o 4º SetData acontece. A suspeita inicial era o writeback assíncrono, que agora está fora do padrão. Depois disso, h1 a h4 rodaram sem trava (4 de 4), todos a ~30 fps. Se voltar a acontecer, investigar o caminho de áudio (MultiStream / cellAtrac streaming).
49. **A trava intermitente era corrida no cellAtrac** (`cellAtrac.c`). Não havia lock nenhum: duas threads em `SetData` podiam pegar o mesmo slot livre (e uma faixa se perdia), e um `DeleteDecoder` podia liberar o buffer que um `Decode` estava lendo. Isso acontecia na entrada do mapa, quando várias faixas curtas começam juntas. Agora todas as 20 entradas passam por um `CRITICAL_SECTION` recursivo (wrappers gerados por `scratchpad/atrac_lock.py`, com as implementações renomeadas para `atrac_impl_*`). Antes: ~3 travas em 13 runs. Depois: 4 de 4 sem trava. Os runs com diagnóstico nunca travavam, porque o diagnóstico muda o timing, típico de corrida.
50. **Cache de geometria do live draw** (`LD_GEOM_CACHE=0` desliga).
    - A chave é um hash duplo de 64 bits sobre o fetch plan, os lotes do draw, os bytes de índice e a união dos intervalos de vértices referenciados (hash de 4 vias). O valor guarda o payload pronto e os índices finais (8192 slots, até 384 MB).
    - Ficam de fora draws inline e atributos instanciados.
    - ~60% dos draws acertam. O custo de CPU dos draws caiu de 1,27 s para 0,45 s a cada 5 s.
    - Mas o FPS na fase pesada do modo 60 continuou ~16, então o limite está em outro lugar (job SPU / frame_clock ~50% ocioso).
    - `GCM_SPU_PUT_KICK`: a escrita da SPU no PUT do controle GCM (0x20002000) agora acorda o walker na hora.
51. **O limite real do modo 60 era latência entre SPU e RSX.**
    - Vigiando o ponteiro de leitura do ring da HighGraphics (0x1B51000, escrito pela SPU): são ~290 pacotes por frame, um por draw, que a SPU consome a ~0,24 ms cada (~4.100/s). A SPU mal executa código guest; ela publica um segmento de comandos e espera o RSX consumir.
    - Cada ida e volta pagava o despertar da thread `frame_clock`. Com `FC_SPIN=1` (girar sem dormir), a fase pesada foi de ~16 para ~30 fps.
    - Agora é o padrão em forma adaptativa, **só com `DES_FPS_UNLOCK=1`**: enquanto o PUT do GCM muda, o `frame_clock` gira com yield por `FC_SPIN_US` (100 ms; 0 desliga) e só depois volta a dormir no timer. Custa um núcleo do host enquanto o jogo renderiza. O message pump roda no máximo 1×/ms.
    - No modo padrão (30) o giro fica desligado. Ligado, o jogo apresentava ~39 frames/s com passo fixo de 1/30 s, ou seja, jogo acelerado. Sem giro, o modo padrão fica em 30 (fase pesada ~27).
    - O walker do FIFO chamava `getenv()` (lock + varredura do ambiente) várias vezes a cada passada. Agora todos os 46 `getenv` do `cellGcmSys.c` passam por `gcm_getenv`, com cache por literal: 940 mil passadas custam 0,7 s.
    - Resultado no modo 60: menus a 60, entrada do mapa a 45–55, fase pesada a 20–40 conforme o lugar (antes ~16). O personagem passa do túnel e chega aos hollows.
52. **Código SPU com SSE4.1/FMA** (patches 0024/0027/0032/0033 do gow, parte do `spu_helpers.h`, e o modo MXCSR da SPU em `spu_channels.c`). Os fontes SPU agora compilam com `-msse4.1 -mfma /clang:-frounding-math`. A imagem continua correta, mas não houve ganho mensurável de FPS aqui (o limite era a latência acima). A SPU ocupada a 92% é o `_synth2` (áudio), em grupo próprio, fora da cadeia do frame.
53. **`mfc_for` era o gargalo das SPUs do SPURS** (`spu_channels.c`).
    - Com o sampler numa SPU do SPURS (`PS3_SAMPLE_THREAD=spu1000_0`, novo: as threads SPU se registram no sampler como `spu<grupo>_<índice>`), ~75% do tempo host estava em `mfc_for`. Ele percorre uma lista ligada de engines MFC, sob lock, a **cada** acesso a canal.
    - Agora há um cache de uma entrada por thread, invalidado por um contador de geração que sobe em toda remoção.
    - Resultado no modo 60: fase pesada de 20–30 para **40–55 fps**. O personagem chega ao pátio dos hollows, às escadas e à torre.
54. **Outras mudanças no live draw**:
    - Ring de upload de texturas: quando não cabe, `ld_upload_reserve` faz flush e recomeça. Antes, parava no meio da textura e deixava os mips seguintes zerados (como o patch 0040 do gow).
    - O writeback de luminância agora pega carona no flush do present (`ld_surfaces_writeback_finish`), sem sincronização própria. `LD_SURF_WRITEBACK_FLUSH=1` volta ao flush próprio.
    - `LD_ASYNC_PRESENT=1` (opcional): o present não espera a GPU. São dois command allocators alternados, e os rings não são rebobinados por frame (enchem e então fazem flush síncrono). A espera no present caiu de ~1,16 s para 0,05 s a cada 5 s, sem erros. O buffer de readback do writeback agora vai para a fila de retire.
55. **Limitador de flip para o modo 30** (`cellGcmSys.c`, `GCM_FLIP_MIN_US`).
    - Com o pipeline mais rápido, o modo padrão chegava a 31–39 fps nas cenas leves. Como ele avança 1/30 s por frame, isso significa jogo acelerado: o "30 fps" do jogo era só o ritmo natural, não um limite.
    - Agora o walker do FIFO não executa um flip antes de `GCM_FLIP_MIN_US` desde o anterior, como o flip com vsync e intervalo 2 do RSX. O walker para diante do flip; a SPU e a HighGraphics, que esperam o RSX, esperam junto. A cadência é mantida avançando pelo período.
    - A pausa ("flip-pace") é isenta das regras de "stuck-get" e de recycle.
    - O `run.ps1` define 33300 µs quando `DES_FPS_UNLOCK` não é 1.
    - Resultado: modo 30 cravado em 29,9–30,2 o tempo todo, inclusive na fase pesada (antes ~27).
56. **Áudio picotado resolvido** (`libs/audio/cellAudio.c`).
    - Sintoma: 46,5% dos blocos mudos no gameplay, ~434 dropouts a cada 5 s. A porta 1 (8 canais, 8 blocos) era preenchida alternadamente (`#.#.#.##`), ou seja, o produtor escrevia 100 blocos/s em vez de 187,5.
    - Cadeia (trace de syscalls por thread): `_cellsurMixerMain` (libmixer) recebe o evento de notify do cellAudio, mistura e posta um semáforo que acorda o `MOMixerThread` (MultiStream), que roda o synth na SPU.
    - Causa: a thread de mix do cellAudio era guiada pelo evento WASAPI (período de 10 ms, buffer de só 20 ms). Cada evento liberava espaço para 2 blocos, misturados em sequência, e os 2 notifies chegavam juntos. O surMixer lia o mesmo read index duas vezes, escrevia o mesmo bloco duas vezes e deixava o seguinte vazio.
    - Correção: os blocos são espaçados pelo relógio QPC (5,33 ms, timer de alta resolução). O espaço no device só limita, e o catch-up só acontece com menos de 1 bloco na fila. O buffer do device passou para 60 ms (`AUDIO_DEVICE_MS`). `AUDIO_CLOCK_PACE=0` volta ao comportamento antigo.
    - Resultado: 0% de blocos mudos, 0 dropouts e 0 clicks por janela no tutorial. Só sobra o gap na transição de carregamento.
    - Ferramentas de diagnóstico: `SYSCALL_TRACE_THREAD=<nome da thread guest>` (o tid muda entre runs), `SYSCALL_TRACE_SC=<n>|9999` + `SYSCALL_TRACE_A0=<hex>` (todas as chamadas da syscall, de qualquer thread), `RECV_TRACE_TID`, `AUDIO_SEQ_LOG`, `AUDIO_NOTIFY_LOG`.
    - O `run.ps1` agora define por padrão `RSX_LIVE_DRAW=1`, `DES_LIBSRE=1` e `DES_ONE_LOG=1` (sem eles o jogo roda num modo sem render).
57. **Névoa e partículas suaves** (`rsx_live_draw.c`, bind do zeta empacotado).
    - Os efeitos aditivos (névoa e feixes no tutorial: `src_alpha/one`, depth test ligado e depth write desligado) amostram o zeta `0xB50000` como A8R8G8B8 para o fade de profundidade.
    - O caminho empacotado do item 36 exigia `current_zslot != 1 + i`. Como esses draws testam contra o mesmo zeta que leem, caíam na VRAM do guest (profundidade 0), o fade zerava o alpha e nada aparecia: 0 pixels alterados.
    - Agora usam o snapshot quando não escrevem profundidade (`self_zeta_read`, a mesma regra do caminho D24). `LD_ZPACK_SELF=0` desliga.
    - Resultado: o draw de névoa (960 vértices) passou de 0 para ~105 mil pixels, e a névoa aparece no fim do corredor do tutorial.
    - O céu estourado das áreas externas não vinha daqui (ver item 58).
    - Ferramenta: `LD_DRAW_DUMP_AT_MS` agora conta do início do processo e loga o frame escolhido. Use junto com `RSX_DRAW_CSV_GATE` criado por um `Start-Job`, e com `draw_diff.py` (pixels alterados por draw).
58. **Occlusion query do sol e dos lens flares** (`rsx_live_draw.c` + `cellGcmSys.c`).
    - Sintoma: o sol e os flares coloridos apareciam com força total mesmo atrás de paredes. O resultado era um clarão branco com raios, até dentro de corredores.
    - Causa: o walker respondia todo `GET_REPORT` de ZPASS com 0xFFFF ("visível").
    - No gameplay o jogo faz ~1 par `CLEAR_REPORT_VALUE(1)` + `GET_REPORT(1)` a cada 5 frames. Ele nunca envia `SET_ZPASS_PIXEL_COUNT_ENABLE` depois do boot: liga uma vez antes do renderer existir.
    - Agora a contagem é uma occlusion query real do D3D12:
      - um segmento por trecho com contagem ligada, fechado também em cada flush de command list e em cada report;
      - o report é a soma dos segmentos desde o último CLEAR;
      - o resultado é resolvido num readback e entregue (`gcm_zpass_deliver`) quando a fence passa;
      - até lá o slot devolve o último valor conhecido (1 frame de atraso).
    - Chaves: `LD_ZPASS=0` volta ao 0xFFFF fixo; `LD_ZPASS_START=0` começa com a contagem desligada; `LD_ZPASS_LOG=1` conta os métodos de report a cada 3000 frames.
    - Resultado no mesmo trajeto: o clarão e os flares sumiram atrás das paredes. FPS igual (54,9 → 54,1, dentro do ruído).
59. **Testes reproduzíveis**: o jogo salva sozinho, então cada run continuava onde o anterior parou (outro personagem, outro lugar), e o A/B era impossível. O `run.ps1` agora restaura `des_port/save_base` sobre o savedata antes de cada run. `DES_SAVE_KEEP=1` mantém o save atual.
60. **Modo 60 cravado em ~59,5 fps** (`rsx_live_draw.c` + `run.ps1`).
    - Perfil (`PS3_FC_PROF`/`LD_FLUSH_PROF`): o `frame_clock` passava ~55% do tempo no present. Com `LD_ASYNC_PRESENT=1` a espera de fence caiu para 117 ms a cada 5 s, mas o present continuou igual: o bloqueio era o `Present(1)` com vsync e só 2 buffers. Um frame que passava um pouco de 16,7 ms esperava o vblank seguinte (50–55 fps no corredor).
    - `LD_VSYNC=0` + `LD_ASYNC_PRESENT=1`: 62–75 fps, com o `frame_clock` ocioso 1,7 s a cada 5 s.
    - O `GCM_FLIP_MIN_US` não limita no modo 60 (o flip segue outro caminho). Por isso há um limitador novo no lado do host, `LD_PRESENT_MIN_US`: timer de alta resolução até ~0,5 ms antes, depois spin, mantendo a cadência.
    - O `run.ps1`, com `DES_FPS_UNLOCK=1` (padrão), agora define `LD_VSYNC=0`, `LD_ASYNC_PRESENT=1` e `LD_PRESENT_MIN_US=16683`.
    - Resultado no trajeto do tutorial: 57,8–59,8 fps o tempo todo (antes 48–58), com áudio 0% mudo e imagem normal.
61. **Rota automática e cenas variadas.**
    - O `PAD_PATH` usa valores 0–255 com 128 no centro. Eu passava `0,-1,0,0`, o que deixava o stick esquerdo para trás/esquerda e a câmera girando: o personagem ficava preso na névoa do corredor (que é a névoa de passagem e agora aparece, ver item 57).
    - Rota padrão de teste, a partir do `save_base`:
      - `PAD_SCRIPT` = menus até 260, mais R1 (`0x0800`) em 304–314;
      - `PAD_PATH="270-302:128,0,128,128;302-316:128,128,128,128;316-322:255,128,128,128;322-345:128,0,128,128;345-350:0,128,128,128;350-380:128,0,128,128"`.
    - Percurso: corredor → pátio externo com névoa, hollows e mensagem no chão → portão de madeira com combate (faíscas dos golpes ok).
    - Visual ok em todas. FPS: corredor 59, pátio e combate 46–59 (média ~55).
    - Perfil das cenas pesadas:
      - o `frame_clock` fica ocioso (o render não é o gargalo);
      - a thread principal passa 67% em `sys_cond_wait`;
      - a HighGraphics passa ~55% em polling de `usleep` esperando jobs de SPU;
      - nas SPUs, ~30% é spin no lock global de lock-line e ~17% é `mfc_for`.
    - Testei um backoff para GETLLAR repetido sem PUTLLC (`SPU_LLAR_BACKOFF=1`): piorou (51 vs 55). Fica desligado.
    - Próximo: dividir o lock de lock-line por linha, ou reduzir a latência da cadeia SPU→HighGraphics.
62. **Cinemáticas não apareciam** (`run.ps1`/`env.ps1`). O player de vídeo no host (item 26, `SAIL_HOST_PLAY=1` + ffmpeg) existia, mas nenhum script o ligava: sem ele o cellSail declara cada vídeo terminado na hora, e o jogo pulava a abertura (`ds_advertise.pam`), a tela-título (`ds_title.pam`) e a introdução da história (`ds_opening.pam`, depois de Novo Jogo). Agora vem ligado por padrão; o ffmpeg vem do PATH ou de `third_party\ffmpeg\bin` (`SAIL_FFMPEG`). Conferido: a abertura toca com imagem e som até o ✕ pular, depois a tela-título.
63. **Tela escura depois dos vídeos** (`rsx_live_draw.c`). Com os vídeos tocando no host (item 62), duas threads usavam o renderizador ao mesmo tempo: a do vídeo, que copia o quadro e chama `Present`, e o walker do FIFO, que continuava gravando flips, blits, write-back de superfícies e consultas de oclusão (item 58) na mesma command list, no mesmo anel de upload e na mesma swap chain. O resultado era `Close` com E_FAIL ou `Present` com DEVICE_REMOVED no meio da abertura; depois disso o renderizador desligava, a tela ficava escura e o jogo parava. Os modos existentes (`RSX_MOVIE_TRACK`, `RSX_MOVIE_COMPOSITE_UI`) não protegiam todas as entradas. Agora toda entrada pública do live draw (method, present, present_rgba, set_movie_mode, blit, resolve_blit, note_resolve, flush, zpass_report) passa por uma única seção crítica recursiva. Conferido: Novo Jogo do zero passa por logos → abertura → "PRESS START" → título → brilho → opções → criação de personagem, sem erro de D3D. As consultas de oclusão também não mexem na lista durante um vídeo.
64. **Som dos vídeos** (`sail_host_movie.c`). (a) `ds_advertise.pam` (a abertura) é **5.1**: o cabeçalho LPCM traz o tipo de canal no nibble alto (`0x91`; os outros vídeos são `0x31`, estéreo), e 6 canais × 130 s batem com o vídeo. Tocado como estéreo, saía embaralhado e a 1/3 da velocidade. Agora o player lê o layout e mistura 5.1 → estéreo (frente + 0,707·centro + 0,707·surround + 0,5·grave, ganho 0,45). (b) Quando um pacote não cabia no buffer, o resto era descartado, o que dava um pequeno buraco a cada buffer; agora a sobra fica para o próximo. (c) `SAIL_HOST_PLAY=0` desliga de verdade (antes bastava existir). `SAIL_AUDIO_RAW=<arquivo>` grava o que vai para os alto-falantes. Medido: abertura 130 s + título 10 s, sem nenhum buraco; 0,0006% de amostras no limite.
65. **Telas pretas durante os vídeos** (`rsx_live_draw.c`). Com a trava do item 63, o jogo continuava apresentando o próprio quadro (preto) entre os quadros do vídeo: piscadas pretas. Agora, fora do modo composite, o present do jogo é ignorado enquanto um vídeo toca. Conferido com um Novo Jogo do zero: abertura (3896 quadros), título (300) e a introdução da história `ds_opening.pam` (5196 quadros, 173 s) tocam inteiros, sem erro de D3D. O som sai idêntico, amostra por amostra, à mixagem calculada fora do jogo. Para testes: `PAD_SCRIPT` aceita 256 eventos, e `PAD_SCRIPT_MOVIE_HOLD=1` não aperta nada durante um vídeo. A tela-título pede START, não ✕.
66. **Escudo invisível** (`rsx_vp_decompiler.c`). Na referência do RPCS3, o personagem tem um escudo redondo no braço esquerdo; no nosso render, nada. O draw do escudo (666 vértices, o mesmo programa de vértice das pernas) pintava 0 pixels. O modelo vem do arquivo com pesos de osso (`a1`, S1×4) e índice (`a7`) todos zero: é uma peça rígida. O shader soma os pesos e divide por essa soma, e com soma 0 todos os vértices colapsavam num ponto. O console desenha essas peças presas ao osso indicado. Agora, quando o programa lê `a1` e `a7`, um vértice com pesos todos zero usa peso 1 no primeiro índice (`LD_VP_ZERO_WEIGHTS=0` desliga). Conferido no corredor do tutorial: o escudo aparece igual ao do RPCS3. Ferramentas: `LD_TRACE_PSO=verts:<n>` (seletor estável entre runs), `LD_ATTR_LOG` mostra os valores imediatos dos atributos sem array, e `DES_FIND=<hex>:<s>[:<repetir>]` busca bytes na memória do jogo.
67. **"Vida zero sem morrer" no tutorial: comportamento original.** A vida em combate fica em `0x426A1D44` (o bloco do personagem: vida, máx, mana, máx, vigor, máx; a cópia em `0x401E802C` acompanha). Parado no portão, ela caiu 659 → 351 → 197 → **1** e travou em 1; a barra parece vazia, porque 1/659 é invisível. No tutorial de Demon's Souls não se morre antes do Vanguard (só ele, ou uma queda, mata), então não é bug do port. Ferramentas: `DES_FIND` aceita `??` como coringa, e `DES_WATCH` mostra o valor depois de cada escrita. `play.ps1 -Uncapped` tira o limite de 60 fps (experimental).
68. **Uma trava por linha nas operações atômicas dos SPUs** (`spu_coherency.c`, `spu_channels.c`, `ppu_loader.cpp`). `SPU_LOCK_PROF=1` (novo) mediu a trava global de lock-line no pátio: ~6 milhões de aquisições/s, quase todas GETLLAR/PUTLLC/PUTLLUC dos kernels SPURS (`spu_wrch`), mais as gravações da PPU em linhas reservadas. 65% esperavam: **2 núcleos inteiros só esperando**, e a trava ficava ocupada 41% do tempo. Agora há 64 travas indexadas pela linha (`spu_lockline_lock_ea`). Quem sabe o endereço (atômicas do SPU, CAS/gravações da PPU) pega só a sua; o resto (HLE SPURS/cellSync, remoção de contextos) pega todas. Também: o bitmap de linhas reservadas usa OR atômico, a lista de contextos ganhou inserção publicada com barreira (remoção só com todas as travas), e `event_status` passou a usar And/Or atômicos (a confirmação de eventos não precisa mais de trava). Resultado: espera de ~2 → ~0,8 núcleo, disputa de 65% → 41% (o resto é briga pela mesma linha). Na luta do pátio, média **53,4 → 57 fps** e mínima **44 → 51**. `SPU_LOCK_STRIPES=0` volta à trava única.
43. Ferramentas novas: `/MAP` agora fica permanente no `CMakeLists.txt`. O script `rva2sym.py` (no scratchpad) aceita como 3º argumento o índice do relatório (`-2` = penúltimo). Os relatórios do sampler já são por intervalo de 10 s, porque os contadores zeram a cada relatório.

**Próximo (antigo):** o carregamento do m08 para depois de `sound/ds_se_c2000.mo*` (≈207 arquivos do disco), sempre no mesmo ponto; a tela de loading continua animada.

## Próxima fase (1)
1. Rodar a ISO no RPCS3 e guardar o log como referência: módulos, threads SPU, syscalls usadas.
2. Clonar o ps3recomp e rodar `ppu_loader.py` + `ppu_lifter.py` no `EBOOT.elf`. Medir funções achadas, falhas e se o C++ gerado compila.
3. Cruzar as 389 FNIDs com a tabela HLE do ps3recomp para ver o que falta.
4. Go/no-go final.
