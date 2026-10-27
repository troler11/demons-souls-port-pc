# Testes automáticos

## Encerramento e savedata (25/10, etapa 17)

No terminal de desenvolvimento x64:

```powershell
clang-cl /nologo /TC /O2 /c tests/save_shutdown.c /Fo:work/save_shutdown.obj
link /OUT:work/save_shutdown.exe work/save_shutdown.obj
.\work\save_shutdown.exe work/save_shutdown-canary.bin
```

Usa a barreira real do runtime. Um writer mantém a operação em andamento
enquanto o host solicita fechamento, completa 32 KiB e fecha o arquivo;
nenhuma operação nova é admitida. Confere conteúdo/EOF, contador ativo e
handles; repete 50 disputas com quatro threads. Não substitui os callbacks
reais de save do jogo nem prova que o jogo enviou todas as alterações ao HLE.

Na integração, usar save/HDD isolados, abrir janela visível e fechar com
Alt+F4. Confirmar saída normal/código 0 e `[shutdown]` no log, além da ausência
do PID. `diagnose.ps1` ainda interrompe no prazo e não é teste de fechamento
normal. A janela do live draw permanece válida até a operação HLE já iniciada
terminar e os recursos gráficos serem drenados. Cache de shaders/pipelines
também é gravado no encerramento normal.

## Entrada nativa do nome (25/10)

No terminal de desenvolvimento x64, na raiz do projeto:

```powershell
clang-cl /nologo /TC /O2 /Ithird_party/ps3recomp/include /Ithird_party/ps3recomp/runtime /c tests/osk_input.c /Fo:work/osk_input.obj
link /OUT:work/osk_input.exe work/osk_input.obj user32.lib gdi32.lib
.\work\osk_input.exe
```

O teste inclui a implementação real de `cellOskDialog.c`, com memória guest
pequena e eventos/pad simulados. Cria janelas EDIT/diálogo reais do Windows e
confere digitação/Enter/Esc, Unicode, limites, endian e guardas de buffer,
resultado pendente sem FINISHED antecipado, cancelamento sem sobrescrever o
nome anterior, Abort/End, geração antiga e 12 ciclos de encerramento/handles.
Não mede FPS nem prova a renderização de todos os glifos da fonte do jogo.

Na integração, usar HDD/save isolados, sem `PS3_OSK_TEXT` ou
`PS3_NO_USER_INPUT=1`. Selecionar o campo de nome na criação, digitar um nome,
confirmar e conferir o texto no menu do jogo; repetir com Esc e conferir que
o nome anterior permanece. O evento FINISHED é publicado pelo pump sysutil,
não pela thread da janela. A janela bloqueia o pad virtual enquanto recebe
texto e aguarda brevemente a liberação das teclas ao fechar.

`diagnose.ps1` define `PS3_NO_USER_INPUT=1`: mantém resposta automática para
não travar os testes sem operador. `PS3_OSK_TEXT` fornece texto UTF-8 para
um teste automático específico, respeitando o limite pedido pelo jogo.
Depois de editar o SDK, exportar e conferir o patch como nas demais etapas.

## Diagnóstico com saves isolados

`scripts/windows/diagnose.ps1` copia os saves para `work/diagnostics/<Tag>` e
executa o jogo nessa pasta. Não remove nem restaura nada sobre os saves do
jogador. Use uma tag nova a cada execução; `summary.json` guarda FPS, áudio,
CPU e o SHA-256 do executável testado.

`-Save <pasta>` escolhe uma fonte diferente para a cópia isolada;
`-ExePath <arquivo.exe>` compara um executável anterior sem substituir a build.
O script também guarda resumos de flushes, saída, framegen e GPU quando os
respectivos diagnósticos estão ligados.

`-Visible` abre a janela normalmente para comparar com o padrão oculto.
Com `DES_FRAMELOG` definido pelo chamador, `frameProgress` registra progresso;
mais de 10 s sem crescimento observado enquanto o processo está vivo, ou
ausência de linhas de frame, reprova a execução apesar de FPS antigos.
`secondsSinceProgress` usa relógio monotônico e consultas a cada segundo;
pode incluir buffer do logger. O mtime permanece apenas informativo: pode
mudar ao fechar o arquivo no Windows. Isso não mede FPS de quadros únicos.
O resumo usa UTF-8, com a última linha convertida para texto simples,
inclusive no Windows PowerShell. O handle do processo é mantido até consultar
a saída; status indisponível não aprova a execução.

### Máscara de eventos SPU (etapa 22)

Após build, no terminal de desenvolvimento x64, na raiz do projeto:

```powershell
clang-cl /nologo /MD /O2 /c tests/spu_event_status.c /Fowork/spu-event.obj
clang-cl /nologo /MD /O2 /c tests/spu_event_test_link.c /Fowork/spu-event-link.obj
link /NOLOGO /OUT:work/spu-event.exe work/spu-event.obj work/spu-event-link.obj build/ps3recomp_sdk/ps3recomp_runtime.lib user32.lib gdi32.lib bcrypt.lib winmm.lib ws2_32.lib
.\work\spu-event.exe
```

Usa os canais reais da biblioteca do SDK. Verifica bits mascarados e
pendentes, ACK, prontidão, espera/wake, lanes e 10.000 disputas de produtores.
Os hooks externos não exercitados abortam se chamados. O teste não substitui
validação de DMA, todos os produtores de eventos ou gameplay. Evidências e
limites em [SPU_EVENT_FOLLOWUP.md](SPU_EVENT_FOLLOWUP.md).

```powershell
.\scripts\windows\diagnose.ps1 -Tag check60 -Seconds 120 -Fps 60
.\scripts\windows\diagnose.ps1 -Tag check30 -Seconds 120 -Fps 0 -Ui 1
.\scripts\windows\diagnose.ps1 -Tag check120 -Seconds 120 -Fps 120 -Ui 1
```

Para reproduzir de forma determinística a corrida de inicialização do áudio,
o teste pode fornecer uma chave zero no `SetNotifyEventQueue`, como no boot
que falhou. Compare a mesma condição com a recuperação desligada/ligada:

```powershell
.\scripts\windows\diagnose.ps1 -Tag raceoff -Seconds 40 -ForceAudioZero -AudioFix 0
.\scripts\windows\diagnose.ps1 -Tag raceon -Seconds 40 -ForceAudioZero -AudioFix 1
```

Com o fix desligado, é esperado silêncio e falhas de notificação; ligado,
`[audio-notify-fix]` deve registrar a fila do próprio libmixer, seguida de
notificações entregues e PCM não zero. O injetor nunca é ativado pelo launcher.
`-SpuReads 0` desliga só as novas leituras inline para A/B; não liga logs de
canais. `-ShortSleepPauses 0` compara o polling anterior.

Após regenerar o PPU, `prepare.ps1` reaplica os hooks de FPS, movimento, som
do menu e câmera. `apply_title_hooks.py` valida as instruções/âncoras e não
reescreve unidades já corretas. Seus testes:

```powershell
python -m unittest discover -s tests -p test_title_hooks.py -v
```

`tests/test_frame_timing.cpp` é um teste C++ independente das unidades geradas;
compila com clang-cl no ambiente do Visual Studio e valida períodos em mHz,
retorno ao padrão, limites de 30/60/120 e a exceção de cutscene.

Depois de editar o SDK, `python scripts/update_runtime_patch.py` atualiza e
verifica o patch público, em UTF-8, incluindo o player de vídeos do host.

## FSR, framegen e cenas pesadas

Use o mesmo save, resolução, escala interna e cache de shaders nas comparações.
Espera de compilação de shaders e carregamentos não são custo contínuo de FSR.
`LD_POST_PROF=1` mede separadamente os passes na GPU, sem inserir uma espera.
Com framegen, `[fps]` continua sendo a taxa de quadros produzidos pelo jogo;
`[framegen]` conta envios de quadros reais/gerados e seu espaçamento. O monitor
pode descartar apresentações: dois envios não garantem dois quadros visíveis.

```powershell
$env:DES_WINDOW = '1080p'
$env:LD_RES_SCALE = '1'
$env:LD_FSR = '1'
$env:LD_FRAMEGEN = '0'
$env:LD_FLUSH_PROF = '1'
$env:LD_POST_PROF = '1'
.\scripts\windows\diagnose.ps1 -Tag fsr_gpu -Seconds 100 -Ui 1
```

Para isolar CPU, desligue FSR/FG, compare `SPU_PROGRESS_ALWAYS=1` +
`LD_PSO_HASH=0` contra `0` + `1`, e mantenha o resto igual. Prefira câmera
parada: uma rota por segundos pode terminar olhando para cenários diferentes.
Confira capturas e quantidade de draws por quadro antes de atribuir diferenças
de FPS a uma otimização. O autoteste solta os botões ao abrir o script da fase
também via `cellFsSdataOpen`, evitando continuar apertando Start em gameplay.
O diagnóstico define `PAD_NO_HOST=1` para neutralizar dispositivos físicos.
O controle virtual também deve centralizar eixos em 128 quando desconectado;
zero faz a câmera girar, mesmo sem `PAD_PATH`.

O teste GPU usa o HLSL real do renderizador: compara vetores da busca exaustiva
e da busca com rejeição antecipada (cinco padrões, passes coarse/refine) e
verifica o fallback de sombra sem correspondência válida. Em um terminal do
Visual Studio, com `clang-cl`, `d3d11.lib` e `d3dcompiler.lib`:

```powershell
python tests/test_framegen_shader.py work/framegen.hlsl
clang-cl /EHsc /std:c++17 /O2 tests/framegen_gpu.cpp /Fo:work/framegen_gpu.obj /Fe:work/framegen_gpu.exe
.\work\framegen_gpu.exe work/framegen.hlsl
```

Regressão da entrada virtual, com o fonte real e uma memória guest pequena:

O teste também cobre o formato opcional `PAD_FILE`: `mask polls lx ly rx ry`
(por exemplo, `0 24 128 64 128 128`). Aceita eixos 0–255 e 1–600 polls; apenas
porta 0 consome o arquivo. O comando expira e a entrada volta ao estado do
host, sem analógico preso. O formato anterior de botões continua aceito.
Esse recurso precisa de `PAD_FILE` configurado e serve para dirigir testes
conforme a posição/captura observada; não muda a configuração normal do jogo.

```powershell
clang-cl /O2 /D_CRT_SECURE_NO_WARNINGS /Ithird_party/ps3recomp/include tests/pad_neutral.c /Fo:work/pad_neutral.obj /Fe:work/pad_neutral.exe /link user32.lib
.\work\pad_neutral.exe
```

Para medir uma mudança sem ninguém jogando, `scripts/windows/autotest.ps1` roda o jogo por um tempo fixo com botões e analógicos programados, guarda o log em `work/logs/<Tag>.err` e mostra o fps do último minuto (linhas `[fps]` a cada 5 s).

```powershell
.\scripts\windows\autotest.ps1 -Seconds 120 -Tag menus
.\scripts\windows\autotest.ps1 -Seconds 385 -Tag r1 -Route courtyard -Save work\save_base
```

## Sempre do mesmo ponto

O jogo salva sozinho. Sem cuidado, cada execução continua de onde a anterior parou: outro lugar e, às vezes, outro personagem. Para comparar antes/depois:

1. Jogue até o ponto de partida desejado, por exemplo o primeiro corredor do tutorial, e saia.
2. Copie `work\gamedata\dev_hdd0\home\00000001\savedata` para `work\save_base`.
3. Passe `-Save work\save_base` em todo teste. O save é restaurado antes de cada execução.

## Rotas

Os tempos são segundos de relógio desde o início do processo. Os menus levam ~260 s até o jogo carregar.

- `menus`: só confirma as telas e menus até entrar no jogo.
- `courtyard`: a partir de um save no primeiro corredor do tutorial, anda até o pátio externo, passa pelas mensagens no chão, chega ao portão de madeira e ataca (R1) os hollows. Corredor ~59 fps, pátio e luta 46-59 fps.

Para montar outra rota, defina `PAD_SCRIPT` e `PAD_PATH` (veja [ENVIRONMENT.md](ENVIRONMENT.md)) e use `LD_FRAME_DUMP=<pasta>` com `LD_FRAME_DUMP_EVERY=240` para ver por onde o personagem passou. Analógicos vão de 0 a 255, com 128 no centro (para frente: `128,0,128,128`).

## Outras áreas (saves de outras pessoas)

Para testar além do tutorial é preciso um personagem que já chegou lá. Saves de PS3 baixados (de um console, do Apollo ou de sites de saves) vêm **encriptados** (`PARAM.PFD`), e às vezes de outra região (`BLES00932` europeu, `BLJS10001` japonês). Dois scripts resolvem:

1. `scripts\save_decrypt.py` decripta e troca a região para `BLUS30443`. As chaves não estão no repositório: coloque `syscon_manager_key` e o `secure_file_id` do Demon's Souls em `work\keys.conf` (veja o topo do script). Precisa de `pip install pycryptodome`.

   ```powershell
   python scripts\save_decrypt.py C:\Downloads\BLES00932DEMONSS005 work\saves\ng\BLUS30443DEMONSS005 --title BLUS30443
   ```

2. `scripts\save_warp.py` muda o lugar onde o personagem acorda (mundo, bloco, posição) para qualquer um dos 32 pontos da tabela do DeS-SaveEdit: os 5 mundos, cada um no início e nos chefes. O jogo carrega o `USER.DAT` (a cópia ativa), e o script ajusta ele e o slot do personagem.

   ```powershell
   python scripts\save_warp.py --list
   python scripts\save_warp.py work\saves\ng\BLUS30443DEMONSS005 "Prison of Hope"
   .\scripts\windows\autotest.ps1 -Seconds 400 -Tag latria -Save work\saves\ng
   ```

Conferido com um personagem NG++++ (Soul Level 501): Nexus, Boletaria, Latria, Tempestades e o Vale carregam e jogam. O início de Stonefang (`Smithing Grounds, Start`) voltou ao Nexus; os outros pontos de Stonefang ainda não foram testados.

## Regressões de CPU e apresentação

Para verificar transições de vídeos do host com savedata isolada, o diagnóstico
aceita `-Movies`: mantém logos e `SAIL_HOST_PLAY=1`. Sem esse parâmetro continua
pulando vídeos para comparar a mesma cena de gameplay. Exemplo:

```powershell
$env:LD_FRAMEGEN = '1'
$env:RSX_D3D_DEBUG = '1'
.\scripts\windows\diagnose.ps1 -Tag fg_movies -Seconds 100 -Movies -Save work\saves\boletaria
```

Regressões de preparação de vértices e apresentação, no terminal de
desenvolvimento do Visual Studio, com `work\perf2_20261023` já criado:

```powershell
clang-cl /O2 /arch:AVX2 /Ithird_party/ps3recomp/include tests/vertex_batch.c /Fo:work/perf2_20261023/vertex_batch.obj /Fe:work/perf2_20261023/vertex_batch.exe
.\work\perf2_20261023\vertex_batch.exe
clang-cl /O2 tests/framegen_present.c /Fo:work/perf2_20261023/framegen_present.obj /Fe:work/perf2_20261023/framegen_present.exe /link d3d12.lib dxgi.lib dxguid.lib user32.lib
.\work\perf2_20261023\framegen_present.exe
python tests/test_spu_taskset_gate.py
```

O teste de vértices compara o batch real contra o caminho escalar, incluindo
todos os 65.536 valores half, dados aleatórios, frequência e wrap.
O teste D3D12 apresenta 64 pares, verifica pixels do quadro real por readback,
modo de exibir só gerados e recriação do worker; consulta erros da camada de
depuração quando disponível. O teste SPU exercita as funções reais da tabela
de tarefas bloqueadas com 32 notificações por condition variable do Windows.

## Hash completo e registradores SPU

O build gera `spu_recomp_rc.c` ao lado de cada imagem SPU e mantém o original.
`PORT_SPU_REGCACHE=OFF` no CMake exclui as versões novas; `SPU_REGCACHE=0`
seleciona as originais no mesmo executável. `LD_GEOM_XXH3=0` compara o hash
anterior. Reiniciar o processo entre casos, usando cópias do mesmo save e
cache de shaders, câmera neutra e a mesma resolução.

No terminal de desenvolvimento do Visual Studio, com `work\perf3_20261023`
criado:

```powershell
python -m unittest discover -s tests -p test_spu_regcache.py
clang-cl /O2 /arch:AVX2 /D_CRT_SECURE_NO_WARNINGS tests/geom_hash.c /Fo:work/perf3_20261023/geom_hash.obj /Fe:work/perf3_20261023/geom_hash.exe
.\work\perf3_20261023\geom_hash.exe
clang-cl /O2 /D_CRT_SECURE_NO_WARNINGS tests/spu_regcache_select.c /Fo:work/perf3_20261023/spu_regcache_select.obj /Fe:work/perf3_20261023/spu_regcache_select.exe
.\work\perf3_20261023\spu_regcache_select.exe 0
.\work\perf3_20261023\spu_regcache_select.exe 1
python scripts/update_runtime_patch.py
python -m unittest discover -s tests -p test_runtime_patch.py
```

O teste diferencial executa as mesmas operações com helpers reais: compara
todos os GPRs, LS e observações/mutações de callbacks em 300 entradas,
incluindo laços, retornos condicionais, escritas parciais e tailcalls. O
teste do hash altera cada byte de entradas alinhadas/desalinhadas e verifica
as duas partes da chave. A seleção SPU é testada com inicialização concorrente.
O gerador rejeita fluxos estruturados que sua análise ainda não suporta;
os fontes SPU originais não são reescritos pelo passe.

## Ingresso do FIFO

O padrão agrupa pacotes 3D elegíveis na thread atual (`LD_FIFO_BATCH=1`,
`LD_FIFO_ASYNC=0`). Comparar `LD_FIFO_BATCH=0` com o padrão no mesmo binário.
`LD_FIFO_ASYNC=1` experimenta a execução num worker; mantém fila limitada e
drena antes de labels, referências, relatórios, transferências, flips e
publicação de progresso. Isso conserva a vida dos buffers convidados e também
limita a sobreposição possível; não equivale a snapshots independentes de
quadros. Usar `LD_FIFO_PROF=1` somente para investigar as esperas e desligá-lo
nas medições finais de FPS.

Teste nativo da fila real, num terminal de desenvolvimento do Visual Studio:

```powershell
New-Item -ItemType Directory -Force work/perf4_20261023
clang-cl /O2 /D_CRT_SECURE_NO_WARNINGS tests/fifo_worker.c /Fo:work/perf4_20261023/fifo_worker.obj /Fe:work/perf4_20261023/fifo_worker.exe /link user32.lib
.\work\perf4_20261023\fifo_worker.exe 1
.\work\perf4_20261023\fifo_worker.exe 0
.\work\perf4_20261023\fifo_worker.exe 1 fail
```

Compara a ordem/conteúdo em mais de 250 mil comandos, altera o scratch do
produtor depois do envio, exerce pressão na fila, observação concorrente,
entrada recursiva, drenagem, shutdown e fallback após falha de criação da
thread. Verifica limites dos pacotes e exclusão de relatórios/labels. A
integração gráfica precisa ainda conferir capturas e transições de vídeo;
esse teste usa um consumidor simulado sob a trava do renderizador.

## Cache de análise de shaders

`LD_SHADER_META=0` seleciona as análises originais; `1` é o padrão. Comparar
processos separados do mesmo executável e save, sem mudar FSR, resolução,
framegen ou instrumentação. `LD_FLUSH_PROF=1` registra também os acertos e
falhas acumulados do cache. A identidade das instruções é conferida antes
do reuso; o endereço sozinho não basta. As constantes continuam sendo
coletadas e saneadas em cada desenho.

Teste diferencial, com os decodificadores reais, no terminal de desenvolvimento:

```powershell
New-Item -ItemType Directory -Force work/perf5_20261023
clang-cl /O2 /arch:AVX2 /D_CRT_SECURE_NO_WARNINGS /Ithird_party/ps3recomp/include /Ithird_party/ps3recomp/libs/video /c third_party/ps3recomp/libs/video/rsx_fp_decompiler.c /Fo:work/perf5_20261023/fp.obj
clang-cl /O2 /arch:AVX2 /D_CRT_SECURE_NO_WARNINGS /Ithird_party/ps3recomp/include /Ithird_party/ps3recomp/libs/video /c third_party/ps3recomp/libs/video/rsx_vp_decompiler.c /Fo:work/perf5_20261023/vp.obj
clang-cl /O2 /arch:AVX2 /D_CRT_SECURE_NO_WARNINGS /Ithird_party/ps3recomp/include /Ithird_party/ps3recomp/libs/video /c tests/shader_meta.c /Fo:work/perf5_20261023/shader_meta.obj
link /OUT:work/perf5_20261023/shader_meta.exe work/perf5_20261023/shader_meta.obj work/perf5_20261023/fp.obj work/perf5_20261023/vp.obj
.\work\perf5_20261023\shader_meta.exe
python scripts/update_runtime_patch.py
python -m unittest discover -s tests -p test_runtime_patch.py
```

São 3.000 programas aleatórios e mais de 3,1 milhões de mutações de bytes
de instrução. Compara constantes e cauda zerada, contagem de valores não
finitos, máscaras de textura/gamma, análise de entradas VP e hashes PSO.
Exercita mudanças de constantes, reuso de endereço, comprimento mapeado
insuficiente, remapeamento, descarte de entradas e esgotamento real do
orçamento de snapshots. Layouts branch/CONST
ambíguos mantêm os walkers originais. Isso complementa, sem substituir,
capturas da gameplay e validação D3D12/áudio.

## Perfil SPURS e experimento de canais

Usar o `.map` do executável exato que gerou o log; outro build pode deslocar
todos os RVAs. `PS3_SAMPLE_FUNCTIONS=1` agrega por função antes do corte do
relatório. O sampler agora conta no máximo um RVA por amostra: o retorno
atribuído pelo unwind não é contado outra vez como RIP direto.

```powershell
$env:PS3_SAMPLE='2'
$env:PS3_SAMPLE_MAIN='1'
$env:PS3_SAMPLE_THREAD='spu1000_0'
$env:PS3_SAMPLE_FUNCTIONS='1'
.\scripts\windows\diagnose.ps1 -Tag spu_functions -Seconds 110 -Save work/perf_20261023/save_heavy
python scripts/profile_symbols.py work/diagnostics/spu_functions/run.err build/MyGameRecomp.map --from-ms 60000 --out work/spu_functions.json
Remove-Item Env:PS3_SAMPLE,Env:PS3_SAMPLE_MAIN,Env:PS3_SAMPLE_THREAD,Env:PS3_SAMPLE_FUNCTIONS
```

Percentuais são amostras do tempo de parede da thread selecionada. O sampler
suspende a thread, perturba locks/áudio e pode produzir travadas; suas janelas
não servem como benchmark de FPS. A comparação final usa processos sem sampler,
mesmo save e parâmetros. A lista guarda só as 25 funções mais frequentes de
cada relatório; o resumo não inventa as amostras que ficaram fora do corte.

`SPU_WRCH_DIRECT=1` testa escritas simples, mantendo confirmação de eventos
atômica e o caminho completo para comandos e canais com callbacks. O padrão
é desligado porque duas comparações de Boletaria ficaram ligeiramente piores.
Não habilitar automaticamente apenas porque o teste funcional passa.

Teste nativo no terminal de desenvolvimento, com `work/perf6_20261023` criado:

```powershell
clang-cl /O2 /arch:AVX2 /D_CRT_SECURE_NO_WARNINGS tests/spu_channel_direct.c /Fo:work/perf6_20261023/channel.obj /Fe:work/perf6_20261023/channel.exe
.\work\perf6_20261023\channel.exe
.\work\perf6_20261023\channel.exe 0
.\work\perf6_20261023\channel.exe 1
.\work\perf6_20261023\channel.exe 1 0
```

Verifica o padrão desligado, inicialização concorrente, diagnóstico forçando
fallback (inclusive `SPU_CHHIST=0`), canais complexos sem mutação antes do
fallback, slot preferido/TagID e 100 mil disputas entre confirmação de LR e
um produtor de outro evento. O evento não confirmado precisa sobreviver em
todas as disputas. Áudio, cenas e vídeos ainda precisam de integração real.

## Intervalos de índices da geometria

O padrão calcula min/max com AVX2, lendo todos os índices e preservando
primitive restart; `LD_INDEX_RANGE_SIMD=0` seleciona o código escalar.
`LD_INDEX_RANGE_CACHE=1` é um experimento separado, desligado por padrão:
usa snapshots completos, largura/quantidade/restart iguais e comparação de
todos os bytes antes de reutilizar o resultado. Não substituir validação por
amostragem ou pelo endereço do buffer.

Para investigar o custo desse trecho, ligar `LD_INDEX_RANGE_PROF=1` junto
com `LD_FLUSH_PROF=1`. Comparar tempo por consulta e fetch por draw, porque
mais FPS também produz mais trabalho em cada janela de 5 s. Desligar os
temporizadores por consulta e o sampler nos benchmarks finais. Rodar a mesma
cópia de save por 120 s e usar somente a parte estável da gameplay.

Teste nativo, com `work/perf7_20261024` criado e terminal de desenvolvimento:

```powershell
clang-cl /O2 /arch:AVX2 /D_CRT_SECURE_NO_WARNINGS tests/index_range.c /Fo:work/perf7_20261024/index_range.obj /Fe:work/perf7_20261024/index_range.exe
.\work\perf7_20261024\index_range.exe
clang-cl /O2 /D_CRT_SECURE_NO_WARNINGS tests/index_range.c /Fo:work/perf7_20261024/index_range_scalar.obj /Fe:work/perf7_20261024/index_range_scalar.exe
.\work\perf7_20261024\index_range_scalar.exe
python scripts/update_runtime_patch.py
python -m unittest discover -s tests -p test_runtime_patch.py
```

Compara escalar, AVX2 e cache com um oráculo independente que decodifica e
ordena os índices: 2.048 conjuntos, 783.234 alterações individuais de bytes,
16/32 bits BE, buffers desalinhados, restart misturado, valores unsigned altos,
vetores vazios e caudas, troca de quantidade/largura/restart no mesmo endereço,
evicção e orçamento real de memória. No Windows, comprimentos 0..257 terminam
exatamente numa página protegida para detectar leitura além do buffer. O
segundo build valida também o fallback sem AVX2. Conferir capturas e áudio
na integração gráfica; esse teste valida o cálculo e a vida do cache em CPU.

## Apresentação normal em worker e medição de intervalos

`LD_PRESENT_WORKER=0/1` compara apresentação serial e worker no mesmo
executável, mantendo `LD_ASYNC_PRESENT=1`, que controla os allocators/rings
entre frames. São mecanismos diferentes. O worker admite um único trabalho
pendente e drena antes da seleção de outro backbuffer, vídeos e encerramento.
O fallback continua serial com overlay ou falha na criação do worker.

Para localizar custos, usar `LD_PRESENT_PROF=1` e `LD_POST_PROF=1` numa coleta
separada: o primeiro mede parede/CPU da thread chamadora, incluindo lock e
espera pelo worker; o segundo usa timestamps GPU sem bloqueio. Não somar
CPU do chamador e parede como se fossem trabalho removível. O worker pode
transferir CPU para outra thread sem reduzir utilização total do processo.
Não suspender `frame_clock` para amostragem enquanto chama D3D/DXGI.

No A/B de FPS, desligar esses dois profilers e o sampler, manter as outras
opções iguais e alternar quatro processos de 120 s com save/câmera isolados.
`LD_PRESENT_STATS=1` em ambos registra intervalos de submissões bem-sucedidas
usando o QPC existente: p50/p95/p99/máximo por janela de cerca de 5 s. Usar
apenas a gameplay estável. A média de p95 de várias janelas não é o p95
calculado sobre todos os frames agrupados. Também não mede scanout nem
latência de entrada; com FG somente o primeiro Present do par entra no ring.
`scripts/windows/diagnose.ps1` inclui os três novos relatórios no JSON.

Testes, com a pasta de saída criada e terminal de desenvolvimento:

```powershell
clang-cl /O2 tests/framegen_present.c /Fo:work/perf8_20261024/framegen_present.obj /Fe:work/perf8_20261024/framegen_present.exe /link d3d12.lib dxgi.lib dxguid.lib user32.lib
.\work\perf8_20261024\framegen_present.exe
clang-cl /O2 tests/present_stats.c /Fo:work/perf8_20261024/present_stats.obj /Fe:work/perf8_20261024/present_stats.exe
python tests/test_present_stats.py work/perf8_20261024/present_stats.exe
python scripts/update_runtime_patch.py
python -m unittest discover -s tests -p test_runtime_patch.py
```

O teste de apresentação verifica publicação sem esperar pelo limitador,
barreira antes do próximo uso do swapchain, inicialização sem relógio/histórico
FG e independência de `LD_FRAMEGEN_ASYNC=0`, além dos 64 pares FG, pixels reais,
modo 2, reinício e debug D3D12. O coletor de intervalos recebe relógio
controlado e compara seis janelas com ordenação independente, incluindo
travadas, wrap do ring e reinício. Validar também gameplay, limite de 30 FPS,
transições de vídeo, áudio e saves na integração; o teste nativo não substitui
essa validação.

## Consultas vazias do FIFO

O padrão é desligado: `GCM_FIFO_IDLE=0/1` compara o caminho completo com o certificado de fila
vazia no mesmo executável. O atalho mantém a trava FIFO, relê PUT/GET,
confere contexto/call e mapeamento do endereço drenado, e continua publicando
uma referência por chamada. Só arma após uma passagem completa terminar com
GET=PUT e o backend consumido. Não usar PUT inalterado como prova de vazio:
self-JUMP e semáforos podem ser alterados sem publicar outro PUT. Não há
novos sleeps, alterações de kick/vblank ou remoção da trava.

A otimização exige backend live, reciclagem pelo título desativada no host
(`PS3_GCM_NO_TITLE_RECYCLE`), worker FIFO experimental desligado e contexto
normal. Modo syscall ou diagnósticos `GCM_RATE`, `GCM_CTXDBG`, `GCM_DRAINDBG`,
`GCM_SCANREF`, `GCM_FIFO_SNAP`, `GCM_RECDBG` e `GCM_GET_EQ_PUT` conservam o
caminho completo. A presença desses diagnósticos é suficiente para desativar,
inclusive se seu texto for `0`.

`GCM_FIFO_IDLE_PROF=1` conta chamadas, entradas vazias/ativas, progresso,
paradas, falta de contexto, atalhos e referências publicadas pelo walker.
`GCM_FIFO_IDLE_TIMING=1` junto com o profiler mede parede por chamada e espera
pela trava. Os tempos incluem instrumentação, não são CPU total. Desligar os
dois no A/B de FPS; usar a mesma cena/save por 120 s e as últimas seis janelas
estáveis. Mais de 99% de chamadas vazias não significa 99% do tempo vazio.
`scripts/windows/diagnose.ps1` inclui `[fifo-idle]` no JSON.

Teste nativo, com pasta de saída criada e terminal de desenvolvimento:

```powershell
clang-cl /O2 tests/fifo_idle.c /Fo:work/perf9_20261024/fifo_idle.obj /Fe:work/perf9_20261024/fifo_idle.exe
.\work\perf9_20261024\fifo_idle.exe
foreach ($flag in @('default','no_live','engine','recycle','GCM_FIFO_IDLE','LD_FIFO_ASYNC','GCM_RATE','GCM_CTXDBG','GCM_DRAINDBG','GCM_SCANREF','GCM_FIFO_SNAP','GCM_RECDBG','GCM_GET_EQ_PUT')) {
    .\work\perf9_20261024\fifo_idle.exe $flag
}
python scripts/update_runtime_patch.py
python -m unittest discover -s tests -p test_runtime_patch.py
```

O teste reusa o certificado real contra um consumidor que sempre percorre um
fluxo modelado: 250 mil passos, referências pendentes ao esvaziar, ordem de
publicação, GET externo corrigido, alteração de contexto/call/mapeamento,
invalidade/reinício, self-JUMP e semáforo liberados sem alterar PUT. Testa
separadamente o padrão desligado e doze seleções de fallback. Não substitui validação do decoder
RSX ou concorrência/GPU no jogo: conferir capturas, áudio, referências e
transições com saves isolados e debug D3D12.

## Diagnóstico de escritas PPU

`PPU_WRITE_WATCH_FAST=0/1` compara, no mesmo executável, o helper completo
com o gate para watches inativos. Por padrão o gate está ligado. A seleção
de ambiente ocorre antes de iniciar threads guest; `g_barrier_sync_watch`
continua lido a cada escrita, permitindo ativar/desativar o watch dinâmico.
Nenhuma operação de memória, reserva ou coerência foi retirada.

No terminal de desenvolvimento Windows com clang-cl:

```powershell
python -m unittest discover -s tests -p test_ppu_write_watch.py -v
python scripts/update_runtime_patch.py
python -m unittest discover -s tests -p test_runtime_patch.py -v
```

O teste extrai e compila o código real de watch do loader. Compara os logs
e efeitos entre completo/gate: seleção sem watch, arma/desarma/rearma dinâmico,
limites/janela e metades de 64 bits, page guard, cap de logs, valor/faixa e
arquivo de gate. Cada variável diagnóstica presente, inclusive `0`, mantém
o caminho completo. Os callbacks de stack/guard são stubs: o teste verifica
sua invocação, não a proteção de páginas ou coerência concorrente do jogo.
Completar com saves isolados, áudio e debug D3D12 na integração.

O A/B de FPS usa quatro processos separados de 120 s na ordem 0/1/1/0,
sem sampler, contadores detalhados por escrita ou outra compilação durante
a coleta. Mesma cena/câmera/save/configuração; seis últimas janelas de FPS
e três de CPU. Não comparar números absolutos com coletas de outro horário
ou usar aceleração isolada de helper como ganho de FPS do jogo.

### Limitações do sampler

Ao investigar HighGraphics, não aceitar nomes guest do sampler interno sem
verificar os endereços: `samp_lookup` delimita entradas PPU/SPU por intervalo,
incluindo lacunas de runtime, e o linker pode fundir stubs vazios. No perfil
de 24/10 isso produziu uma falsa função vazia dominante. O coletor de trabalho
`work/perf10_20261024/host_sampler.c` registra RIP bruto da HighGraphics do
processo de diagnóstico, sem suspender `frame_clock`; `analyze-raw.py` resolve
o mapa completo do mesmo binário e usa `.pdata` quando possível. Não reutilizar
mapa de outra build. Retomar a thread antes de qualquer IO do coletor.

A cadência nominal `Sleep(2)` foi ~15,6 ms; repetir com cadência variada para
reduzir viés de fase. Frações de amostras diretas não são tempos inclusivos
nem percentuais de CPU total. Sampler/POLLTOP alteram a execução: desligar
no A/B de FPS, preservar save/câmera/configuração e comparar repetições.
`PS3_POLLTOP` soma atrasos solicitados, não espera medida. Resultados e limites
em `docs/PERFORMANCE.md`; nenhum ganho novo foi afirmado nessa coleta.

## CPU menor

`-Threads N` roda o jogo só nos primeiros N processadores lógicos, simulando uma CPU menor. `PS3_THREAD_CPU=10` mostra a cada 10 s quanto de um núcleo cada thread gastou (SPUs, `HighGraphics`, `frame_clock`...).

```powershell
$env:PS3_THREAD_CPU = "10"
.\scripts\windows\autotest.ps1 -Seconds 400 -Tag cpu6 -Threads 6 -Save work\saves\boletaria
```

## Regressão: SAT de operandos FP e prévias de personagem (24/10)

No terminal de desenvolvimento x64, a partir da raiz:

```powershell
New-Item -ItemType Directory -Force work/fp_source_sat
clang-cl /O2 /D_CRT_SECURE_NO_WARNINGS /Ithird_party/ps3recomp/include /Ithird_party/ps3recomp/libs/video /c third_party/ps3recomp/libs/video/rsx_fp_decompiler.c /Fo:work/fp_source_sat/fp.obj
clang-cl /O2 /EHsc /D_CRT_SECURE_NO_WARNINGS /Ithird_party/ps3recomp/include /Ithird_party/ps3recomp/libs/video /c tests/fp_source_sat_gpu.cpp /Fo:work/fp_source_sat/test.obj
link /OUT:work/fp_source_sat/test.exe work/fp_source_sat/test.obj work/fp_source_sat/fp.obj d3d11.lib d3dcompiler.lib
.\work\fp_source_sat\test.exe
.\work\fp_source_sat\test.exe legacy
```

Esperado: 120 casos WARP em cada processo. Usa o decompilador real e executa
os shaders traduzidos em um alvo float, com leitura dos quatro componentes.
Cobre SAT ligado/desligado no bytecode, três posições de operando,
TEMP/INPUT/CONST, literal/buffered, valores abaixo de zero/acima de um,
negação e abs em SRC0/SRC1. Rejeita resultados não finitos. Não é validação
de todos os modos de precisão ou de abs em SRC2.

Conferência no jogo: com cópias isoladas do savedata, crie um personagem e
compare rosto/armadura na mesma classe, brilho e configuração. O padrão deve
preservar a cor da pele; `RSX_FP_SRC_SAT=0` reproduz o legado escuro/vermelho.
Não aumentar o brilho global para compensar. O cache de bytecode usa o HLSL
completo e separa automaticamente as traduções corrigida e antiga.

O teste `tests/shader_meta.c` também passou a conferir máscaras absolutas dos
sete opcodes de textura para todas as 16 unidades, além de bytes inline,
branches e mutação. O procedimento de compilação é o da seção de metadados.

`LD_USED_TEXTURES=1` permanece experimental/desligado por padrão: no percurso
medido baixou o custo local sem melhorar FPS/CPU/p99 consistentemente.
`LD_HITCH_PROF=1` é investigação, não benchmark: identifica frame, compilação,
criação de PSO e espera em flushes. Desligar para a comparação final de FPS.
Após editar o SDK, exportar e conferir com `scripts/update_runtime_patch.py`
e `python -m unittest discover -s tests -p test_runtime_patch.py`.

## Caches de shaders/pipelines: regressão nativa (25/10)

No terminal de desenvolvimento x64:

```powershell
clang-cl /O2 /EHsc /D_CRT_SECURE_NO_WARNINGS /Ithird_party/ps3recomp/libs/video /c tests/renderer_caches.cpp /Fo:work/renderer_caches.obj
link /OUT:work/renderer_caches.exe work/renderer_caches.obj d3d12.lib d3dcompiler.lib dxgi.lib
$testRoot = Join-Path (Resolve-Path work).Path ('cache-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot
.\work\renderer_caches.exe preload (Join-Path $testRoot 'preload')
.\work\renderer_caches.exe pipeline-write (Join-Path $testRoot 'pipeline')
.\work\renderer_caches.exe pipeline-read (Join-Path $testRoot 'pipeline')
```

Usa os headers reais do runtime. O teste de preload cria DXBC real e confere
bytes completos, HLSL diferente com o mesmo hash fornecido, stage, round-trip
do pack, checksum/truncamento/versão, teto de memória, quatro leitores durante
a publicação e 20 ciclos de cancelamento/encerramento. Também testa padrão
sem variável e desativação explícita.

O teste de pipelines exige suporte `D3D12_SHADER_CACHE_SUPPORT_LIBRARY` na GPU
padrão. Cria PSOs reais, confere hits em RAM e em outro processo, mudança de
estado, padding irrelevante, writer periódico, cache truncado, limite de
armazenamento e encerramento. Esses testes não medem FPS nem substituem a
integração de gameplay/áudio/D3D12.

Para A/B no jogo, copiar o mesmo snapshot inicial de cache e save para cada
processo; controlar `LD_SHADER_PRELOAD` e `LD_PSO_LIBRARY` separadamente.
Reiniciar ao trocar. Para benchmark, desligar `LD_HITCH_PROF`,
`LD_FLUSH_PROF`, debug D3D12, captura e sampler. `LD_PRESENT_STATS=1` usa o QPC
já registrado e agora inclui `elapsed`, contagens de intervalos acima de
33 1/3 ms e 50 ms. São intervalos de submissão, não scanout ou input lag.
O oráculo `test_present_stats.py` também confere essas contagens.

Para investigação separada, `LD_FLUSH_PROF=1` mostra hits/faltas dos dois
caches e `LD_HITCH_PROF=1` acrescenta `library=1/0/-1` aos eventos PSO
(hit/miss/bypass). Não misturar números dessa execução instrumentada com o
A/B final. Conferir também início com cache vazio e o fallback do driver.

## Preparação antecipada de PSOs: regressão nativa (25/10, etapa 16)

No terminal de desenvolvimento x64, com suporte de biblioteca de pipelines
na GPU padrão:

```powershell
clang-cl /O2 /EHsc /Ithird_party/ps3recomp/libs/video /c tests/pso_warmup.cpp /Fo:work/pso_warmup.obj
link /OUT:work/pso_warmup.exe work/pso_warmup.obj d3d12.lib d3dcompiler.lib dxgi.lib
$warmRoot = Join-Path (Resolve-Path work).Path ('pso-warm-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $warmRoot
.\work\pso_warmup.exe write $warmRoot
.\work\pso_warmup.exe read $warmRoot
```

O primeiro processo somente registra 12 PSOs reais. O segundo recria os
objetos em background a partir do pack e confere identidade completa,
padding, mudança de estado, colisão forçada, referências independentes,
truncamento/checksum/versão/root, limite de entradas, corrida com fallback
do renderizador, falha injetada do driver, cancelamento de leitura enquanto
há receita nova pendente, orçamento suave de tempo e ciclos de shutdown
com contagem de handles. O codec usa todos os campos de estado, cinco
estágios e layout de entrada; compara a identidade canônica independente.
O teste usa os headers reais e cria PSOs no driver, mas não executa desenhos.

`LD_PSO_WARMUP=1` habilita preparação/gravação; o padrão é desligado.
Para produzir somente as receitas, usar `LD_PSO_WARMUP=0` e
`LD_PSO_WARMUP_RECORD=1`. Na primeira sessão sem pack, verificar o fallback e
a gravação periódica; no reinício, `[pso-warmup]` deve mostrar objetos criados.
`LD_FLUSH_PROF=1` imprime `[pso-warmup-stats]` e `LD_HITCH_PROF=1` acrescenta
`warm=1` aos hits prontos (o campo `library=-1` indica que a biblioteca foi
contornada). Esse log deve ficar fora do benchmark.

A/B: copiar o mesmo snapshot inicial de shaders/biblioteca/receitas e o mesmo
save para diretórios isolados por processo; off/on/on/off com percurso e
opções iguais. Remover variáveis de presença com `Remove-Item Env:PS3_SAMPLE`
(e demais flags de sampler/trace), em vez de atribuir string vazia via .NET.
Desligar debug, captura, timers detalhados e FG; manter `LD_PRESENT_STATS=1`.
Não compilar, amostrar threads, capturar nem fazer leituras grandes em paralelo.
Conferir no log que debug/sampler não foram ativados. Quantis por janela
não são percentis globais; os intervalos são de submissão e não input lag.

Integração separada deve conferir imagem/caminhada/áudio e erros D3D12 com
FSR/FG/debug ligados, além do padrão sem warmup. O diagnóstico isola os saves
e interrompe o PID lançado no prazo; o nativo cobre o shutdown normal.
Não apagar caches globais do driver para forçar resultado. Resultados e
limitações da etapa 16 em `docs/PERFORMANCE.md`.

Ao preparar um benchmark neste host PowerShell, usar `Remove-Item` para
variáveis de diagnóstico que dependem de presença:

```powershell
foreach ($name in @('RSX_D3D_DEBUG','PS3_SAMPLE','PS3_SAMPLE_MAIN','LD_FRAME_DUMP','DES_FRAMELOG')) {
    Remove-Item "Env:$name" -ErrorAction SilentlyContinue
}
```

Atribuir `null` via `[Environment]::SetEnvironmentVariable` deixou strings
vazias nos processos observados em 25/10; isso manteve a camada D3D12 e o
sampler ligados, pois essas builds consultavam presença (sampler vazio usa
5 ms). A build final da etapa 14 corrige vazio/`0` para debug D3D12/GBV;
para o sampler, remover a variável continua necessário. Conferir que o log não contém
`[d3d-debug] validation layer enabled`. Não inferir a configuração apenas
pelo script, nem deixar `[samp] sampling every ...` no benchmark. As rodadas
iniciais das etapas 13/14 foram reclassificadas como medições sob debug/sampler;
as rodadas `perf14_clean_*` removem as variáveis de fato.

## Índices de tabelas de descritores (25/10)

No terminal de desenvolvimento x64, executar:

```powershell
python tests/test_descriptor_tables.py --out work/descriptor-test
```

O runner extrai do `rsx_live_draw.c` as funções reais de criação/consulta das
tabelas e seus helpers. Compila contra heaps simulados que registram os bytes
copiados: um oráculo independente compara o conteúdo de cada um dos 16 slots
com o descritor CPU atual. Quatro processos: off, on, variável ausente e
reutilização SRV desabilitada. Cada processo executa 50 mil desenhos, alterações
de origem, reciclagem com fence igual, anéis cheios e proteção da entrada final.
Avançar o fence sem reciclar o anel mantém tabelas válidas, inclusive quando
o anel de samplers já está cheio; isso não pode provocar cópia/flush adicional.
Também verifica a leitura das flags de debug/GBV com strings vazias herdadas
pelo processo e valores explícitos `0`/`1`/`yes`/ausente.
Confere também que uma tabela já copiada não muda após modificar o descritor
CPU. O índice puro força 40 chaves para o mesmo bucket inicial, teto de 32,
mudanças de geração/fence e wrap da época. Não testa execução real da GPU.

Na integração, `LD_DESCRIPTOR_CACHE=1` seleciona os índices experimentais de
SRV/samplers, desligados por padrão por falta de ganho consistente sem diagnóstico;
`=0` compara com a reutilização anterior. Reiniciar ao trocar. `LD_SRV_REUSE=0`
desliga também a nova reutilização SRV, mantendo a seleção dos samplers.
`LD_FLUSH_PROF=1` acrescenta `[descriptor-cache]` com contagens cumulativas:
hits/misses/cópias; o hit SRV não inclui o atalho consecutivo já existente.
Conferir D3D12, gameplay, áudio e imagens; depois comparar FPS/p99 em processos
separados, mesmos saves/caches/câmera, sem profiler detalhado ou captura.

## Perfil de CPU e eventos correlacionados (etapa 15)

`work/perf15_20261025/run-profiles.ps1` compila o coletor externo e executa
dois processos isolados, um para HighGraphics e outro para `spu1000_0`.
Ele aceita somente esses dois nomes e um binário chamado `profile.exe`;
nunca suspender `frame_clock` para amostrar. O mapa deve corresponder ao hash
exato da cópia do executável. Janela 80–140 s, cadência variada 1–5 ms, retoma
a thread antes de escrever. Isso é diagnóstico: não usar seu FPS como A/B.
Percentuais dos RIPs são amostras de parede, incluindo chamadas de sistema,
e não percentuais de CPU total. Resolver aliases/limites com mapa + `.pdata`.

Teste do logger, no terminal de desenvolvimento x64:

```powershell
clang-cl /O2 /Ithird_party/ps3recomp/libs/video tests/perf_trace.c /Fo:work/perf_trace.obj /Fe:work/perf_trace.exe
python tests/test_perf_trace.py work/perf_trace.exe
```

Verifica modo desligado, início futuro, mínimo de duração, inversão/zero,
teto de registros e conversão QPC em uptime longo. O trace é opt-in:
`PS3_PERF_TRACE=1`, `PS3_PERF_TRACE_MIN_US=250`, início absoluto em
`PS3_PERF_TRACE_START_US`. Calcular QPC µs antes de iniciar o processo:

```powershell
$traceStart = [long](([decimal][Diagnostics.Stopwatch]::GetTimestamp()*1000000)/[Diagnostics.Stopwatch]::Frequency) + 60000000
$env:PS3_PERF_TRACE_START_US = $traceStart.ToString()
```

Eventos usam `qpc_us` de fim e `duration_us`; início = fim − duração.
`fs-prefault`/`fs-lock`/`fs-read`/`fs-aio`: a=fd, b=bytes. `inflate-data`:
a=comprimidos, b=saída; `inflate-signal`: a=stream, b=flag. `texture`:
a=localização/offset em 32+32 bits, b=largura/altura. `pso`: a=frame,
b=0 bypass/1 miss/2 hit da biblioteca; `flush`: a=frame, b=motivo;
`present-gap`: a=ID do Present, b=frame. Limiar próprio dos gaps: ≥33⅓ ms.
Teto de 20 mil eventos por unidade de compilação. Reiniciar para mudar opções.

Correlacionar intervalos pela união temporal, sem somar spans aninhados como
CPU exclusiva. Coincidência com uma operação em outra thread não prova que
ela estava no caminho crítico. O próprio log pode bloquear e criar gaps;
os timestamps da operação devem terminar antes de imprimir. Não misturar
FPS do trace com benchmarks sem instrumentação. Ausência de evento de I/O
só exclui o que foi medido acima do limiar naquele recorte, não outras áreas.
