# Travadas: apresentação, SPU e cache (26/10/2026, etapa 21)

Atualização posterior: [eventos SPU e detector de progresso, etapa 22](SPU_EVENT_FOLLOWUP.md).

Instalados **74 shaders individuais validados**, preservando os 1.441 arquivos
anteriores. O diagnóstico agora reprova frames parados apesar de amostras
antigas de FPS. Congelamento do Nexus e névoa do Velho Herói ainda sem
correção comprovada. Nenhuma alteração de código nativo nesta etapa.

## Nexus

Cinco processos separados, cópia privada do mesmo save, 720p, limite 60,
FSR/FG desligados, sem captura de frames ou sampler durante a reprodução.
Câmera via PAD_PATH: `45-49:128,128,160,128;49-53:128,128,96,128`.
O relógio do PAD_PATH começa na primeira consulta ao pad. A lacuna abaixo
compara o mtime do frame log com a conclusão do diagnóstico; inclui buffer.

| Caso em work/perf21_20261026 | Duração | Janela | Present worker | SPU regcache | Lacuna final aproximada |
|---|---:|---|---|---|---:|
| nexus_visible_worker | 100 s | visível | ligado | ligado | 1,27 s |
| nexus_hidden_worker | 100 s | oculta | ligado | ligado | **14,20 s** |
| nexus_visible_sync | 100 s | visível | desligado | ligado | **53,20 s** |
| nexus_sync_capture | 180 s | visível | desligado | ligado | **136,72 s** |
| nexus_original_spu | 120 s | visível | desligado | desligado | 1,09 s |

A parada ocorre também com janela visível e apresentação síncrona.
Desligar o worker não a elimina. Nenhum desses cinco casos registrou
compilação de shader nos eventos PSO medidos. Áudio e atendimento do FIFO
continuaram nas paradas examinadas.

Em nexus_sync_capture, um coletor externo foi acionado **depois** da parada.
HighGraphics foi amostrada em sys_event_queue_receive_impl; SPU0 executava
o kernel f5cc5c, incluindo LS 0x5120 e helpers de canais/atomics. Outras SPUs
estavam em esperas ou execução. Isso não identifica o produtor responsável.
O coletor retoma a thread antes de imprimir e exclui frame_clock. Cadeias
por varredura de stack podem incluir valores antigos; não são um unwind
completo. O mapa corresponde ao mesmo executável.

O único controle SPU_REGCACHE=0 continuou até o prazo. A reprodução é
intermitente e um controle com regcache ligado também continuou: hipótese
inconclusiva. Padrões do jogador mantidos; nenhum semáforo/flag foi forçado.

## Comparação do cache no Velho Herói

Duas execuções visíveis de 150 s, mesmo save privado/controlador, 720p,
limite 60, sem FSR/FG/captura/sampler. Cópias independentes do cache original;
a segunda recebeu os 73 shaders das etapas 19/20. Pack/biblioteca de PSOs
começaram idênticos. Spawn interno e gatilhos normais descritos em
[BOSS_MAP_FOLLOWUP.md](BOSS_MAP_FOLLOWUP.md); HP aumentado apenas na cópia.

| Indicador registrado | Cache anterior | Cache com 73 shaders adicionais |
|---|---:|---:|
| Novos shaders compilados (VS / PS) | 1 / 2 | 0 / 1 |
| Eventos PSO com compilação | 2 | 1 |
| Maior preparo PSO observado | 150,086 ms | 50,415 ms |
| Idade final do frame log | 1,32 s | 1,45 s |

Os três shaders compilados na primeira estavam presentes, com SHA-256
idênticos, no cache inicial da segunda. Esta encontrou outro PS numa
variação da luta (36,805 ms de compilação), também validado e instalado:
**74** arquivos novos, **1.515** no cache local. A operação de 50,415 ms
tinha compiled=0/0; reutilizar DXBC não elimina todo custo de preparo.

Demonstrado reaproveitamento de shaders e redução dos picos PSO observados
nesse recorte. AI, trajetória e trabalho variaram; não demonstra ganho
sustentado de FPS/CPU, p99 ou latência. Novos shaders e engasgos continuam
possíveis. O cache não corrige o congelamento sem compilação do Nexus.

Instalação validou versão/estágio, tamanhos, fonte/FNV, DXBC e SHA-256;
publicou somente arquivos novos por rename atômico. Todos os 1.441 hashes
anteriores preservados, inclusive pack/biblioteca do driver. Manifestos em
work/perf21_20261026/cache-installed.json e cache-comparison.json.

## Névoa e diagnóstico

old_hero_gate_30: 110 s, mesma cópia de entrada da etapa 20, limite 30 FPS,
worker e regcache ligados. Cross de 120 consultas repetido com intervalos;
posição permaneceu (-5, 5,379, -389), frames ativos até o prazo. Reduzir
somente o limite não ativou a entrada nessa preparação. Não prova falha
na progressão normal e não substitui um controle DES_FPS_UNLOCK=0.

diagnose.ps1 ganhou -Visible. Com DES_FRAMELOG solicitado, frameProgress
registra a última linha e idade da escrita. Mais de 10 s sem escrita ao
atingir o prazo reprova a execução; arquivo/linha ausente também reprova.
Variáveis antes ausentes são removidas na restauração, evitando NAME=
presente em filhos do Windows PowerShell. Resumos explicitamente UTF-8.

Fixture executável pelo script real: atualizações contínuas aprovadas;
FPS 60 no log com frame log estagnado reprovado; AUDIO_MUTE antes ausente
continuou ausente em ambos. Fixture em work/perf21_20261026, separada da
gameplay. Oito execuções de jogo, todas encerradas pelo prazo, sem saída
espontânea. Os 14 saves originais conservaram seus hashes. Executável:
105C933F6FA61AE7E31BB029595375D109B0D3037BAD5F6F6CFB674C7D83C029.
Configurações, entradas, análise e validação em work/perf21_20261026;
logs brutos em work/diagnostics/perf21_*.
