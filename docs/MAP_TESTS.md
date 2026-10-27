# Mapas e novas lutas — 26/10/2026, etapa 19

Rodada posterior: [BOSS_MAP_FOLLOWUP.md](BOSS_MAP_FOLLOWUP.md) acrescenta três
combates observados e cinco pontos internos, com 14 execuções da etapa 20.

Foram concluídas **58 execuções**, incluindo tentativas dos **42 layouts MSB**
encontrados no disco, dez tentativas de bosses e controles/repetições no Nexus.
O inventário distingue 20 layouts de campanha/tutorial e 22 arquivos adicionais,
variantes ou cenários auxiliares. Essa classificação orienta a cobertura;
não torna os arquivos auxiliares etapas acessíveis numa partida normal.

**Não é uma aprovação integral dos 42 mapas.** Foram exercitados pontos de
entrada e trechos curtos, com deslocamento, câmera e ataques. Há entradas
compartilhadas com o mapa anterior, cinemáticas, loading e tentativas inválidas.
Não foram percorridos todos os corredores, transições, eventos ou fases.

## Preparação e controles

- Cópias privadas de saves; snapshot SHA-256 dos 14 arquivos originais.
  Personagem de teste com HP/vitalidade elevados; flags de progressão não
  substituem os de uma partida normal. Tentativas de mapa: 80 s; auxiliares:
  45 s; bosses: 150 s. Boot/carregamento fazem parte desses prazos.
- 720p, escala 1, alvo 60, FSR/FG/debug/sampler desligados. Present worker
  ligado, descriptor cache/FIFO idle/PSO warmup experimentais desligados.
  Timers, logs e capturas ativos: não é benchmark limpo nem A/B de FPS.
  Os diagnósticos iniciam a janela oculta; reprodução em janela visível
  fica pendente para atribuir as paradas à partida normal.
- MSB lido conforme a estrutura do
  [SoulsFormats](https://github.com/JKAnderson/SoulsFormats/blob/master/SoulsFormats/Formats/MSB/MSBD/MSBD.cs).
  Pontos/volumes e scripts Lua locais ajudaram a localizar entradas. Uma posição
  artificial pode cair, carregar vizinhos ou deixar um evento sem ativar.
- Acrescentados analógicos ao diagnóstico `PAD_FILE`: `mask polls lx ly rx ry`.
  O comando expira e volta à entrada normal; porta 0; formato antigo preservado.
  Só funciona com essa variável de teste configurada. Não é uma otimização de
  CPU/renderização e não altera os menus. Teste nativo de neutralidade, expiração,
  isolamento de porta, botões antigos, eixos inválidos e bloqueio do host passou;
  build e reprodução do patch integral do SDK também passaram.

## Novos combates e tentativas

| Destino | Resultado |
|---|---|
| Velho Monge | Combate offline confirmado em `perf19_boss_old_monk`: modelo/barra e ataque mágico visíveis. Não valida PvP, vitória ou todas as fases. |
| Aranha Blindada | Combate confirmado no ponto de entrada de `perf19_map_06_01_00_00`, que é compartilhado com a arena anterior. A tentativa `boss_armor_spider` ficou na névoa e não é a evidência de combate. |
| Velho Herói, duas tentativas | Altura corrigida; permaneceu antes da névoa. Luta não validada. |
| Phalanx | Tentativa terminou entre inimigos comuns, sem comprovação do boss. |
| Falso Rei Allant | Queda/mudança de área, seguida de retorno ao Penetrador e encerramento com código 1. Não foi comprovado crash durante a luta de Allant. |
| Sanguessuga | Névoa/barra; boss/ataques não comprovados. |
| Astraea | Acesso/névoa; luta não validada. |
| Flamelurker | Névoa/barra na nova tentativa; não amplia a validação parcial da etapa 18. |
| Rei Allant final | Barra/efeitos com posição fora da geometria; tentativa inválida para combate normal. |

Os outros cinco encontros observados anteriormente continuam descritos em
[BOSS_TESTS.md](BOSS_TESTS.md). Nenhuma vitória ou progressão completa foi
declarada apenas pela presença de uma barra de boss.

## Falhas que precisam de investigação

### Nexus: parada de frames associada ao teste de câmera

Três execuções sem rastreio apresentaram uma parada longa: `map_01_00_00_00`,
`nexus_original` e `nexus_fixed_camera`. O áudio continuou; os logs de flips e
apresentação pararam. As duas primeiras perderam progresso por mais de 30 s;
a terceira por mais de 20 s segundo os horários dos arquivos. Os próprios
tempos da última linha não devem ser confundidos com o tempo de vida do processo.

`nexus_no_input` (100 s), `nexus_pad_blank` e `nexus_pad_neutral` (90 s cada)
continuaram produzindo frames. O controle de câmera antigo `PAD_PATH`, sem
`PAD_FILE`, também reproduziu a parada. Isso afasta uma explicação limitada
ao novo formato analógico, mas não identifica a causa no renderizador/SPURS.

Um novo teste de câmera com `SYSCALL_TRACE_THREAD=HighGraphics` e `PS3_WAITBT`
durou 100 s sem a mesma parada longa. Instrumentação altera o agendamento;
essa execução não comprova correção. Não foram forçados semáforos ou flags
de conclusão. **A causa e a correção permanecem abertas.**

### Tentativa de Allant: encerramento após uma queda

`perf19_boss_allant` encerrou por conta própria com código 1, antes do prazo.
Houve chamadas a ponteiro de função nulo, `abort() is called` e
`sys_process_exit(code=1)` nos logs. A posição já estava próxima do Penetrador
(Y aproximadamente 90, não Y 210 da sala de Allant), com barra do Penetrador
nas capturas. É uma falha da execução artificial após queda/recarregamento;
a reprodução durante progressão normal ainda falta. Não foi silenciado o abort
nem alterada a lógica do jogo para classificar a tentativa como aprovada.

Os demais processos foram encerrados pelo prazo do diagnóstico. As pausas
curtas continuam e os tempos incluem o custo de capturas. FPS de loading,
cinemáticas ou salas vazias não foi interpretado como desempenho de combate.

## Cobertura por layout

| Layout | Identificação | Observado |
|---|---|---|
| `m01_00_00_00` | Nexus | Parada de frames em três execuções com câmera; três controles neutros/sem comandos continuaram; rastreio não reproduziu a mesma parada longa. |
| `m02_00_00_00` | Portões de Boletaria | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m02_01_00_00` | Caminho do Lorde | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m02_02_00_00` | Ala Interior | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m02_03_00_00` | Torre do Rei | Entrada compartilhada com a área do Penetrador; não valida a subida nem o Falso Rei. |
| `m03_00_00_00` | Layout adicional/auxiliar | Cena sem geometria utilizável na captura; não validado como mapa jogável. |
| `m03_01_00_00` | Limite da Ilha | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m03_01_00_99` | Layout adicional/auxiliar | Shrine of Storms renderizado; variante de sufixo 99 não confirmada como cenário separado. |
| `m03_02_00_00` | Caminho Ritual | Entrada/escadaria do Caminho Ritual; não valida o percurso inteiro até o Velho Herói. |
| `m03_03_00_00` | Altar das Tempestades | Acesso na saída do Velho Herói; exterior e luta do Rei da Tempestade não validados. |
| `m04_00_00_00` | Prisão da Esperança | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m04_01_00_00` | Latria Superior | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m04_02_00_00` | Torre de Marfim | Entrada da torre; combate do Velho Monge validado separadamente no mesmo mapa. |
| `m05_00_00_00` | Abismo Depravado | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m05_01_00_00` | Pântano da Amargura | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m05_02_00_00` | Refúgio Apodrecido | Entrada compartilhada com Colosso Imundo; luta de Astraea não validada. |
| `m06_00_00_00` | Minas de Stonefang | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m06_01_00_00` | Cidade dos Túneis | Entrada compartilhada: combate real com Aranha Blindada, do mapa anterior; restante dos túneis não percorrido. |
| `m06_02_00_00` | Templo Subterrâneo | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m07_00_00_00` | Layout adicional/auxiliar | Tentativa permaneceu em carregamento; cenário não validado. FPS do loading não mede combate. |
| `m07_01_00_00` | Layout adicional/auxiliar | Tentativa permaneceu em carregamento; cenário não validado. FPS do loading não mede combate. |
| `m07_02_00_00` | Layout adicional/auxiliar | Tentativa permaneceu em carregamento; cenário não validado. FPS do loading não mede combate. |
| `m08_00_00_00` | Tutorial: chegada | Cinemática de chegada observada; não é medição de gameplay nem queda para 30 FPS. |
| `m08_01_00_00` | Tutorial: área principal | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m08_02_00_00` | Tutorial: área final | Entrada renderizada, com comandos curtos de câmera/caminhada/ataque; sem parada longa registrada neste recorte. |
| `m99_60_00_00` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_60_01_00` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_10_00` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_11_00` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_12_00` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_97_00` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_98_00` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_98_01` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_98_02` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_98_04` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_98_05` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_98_06` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_98_07` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_98_08` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_98_09` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_98_10` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |
| `m99_99_99_99` | Layout adicional/auxiliar | Loading ou sequência introdutória nas capturas; carregamento do layout específico não comprovado em 45 s. |

## Evidências e estado final

`work/perf19_20261026` contém `inventory.json`, `coverage.json`, `analysis.json`,
`cases.json`, progresso, comandos do controlador, capturas e manifests.
Logs brutos: `work/diagnostics/perf19_*`. A identidade de uma tentativa é seu
destino pretendido; o boss/mapa real depende das imagens, posições e arquivos
carregados. O arquivo `coverage.json` guarda essa distinção para cada layout.

O cache diagnóstico ficou separado do cache do jogador. Foram descobertos
45 arquivos individuais adicionais de shader; não foram instalados nesta
etapa. Não há ganho sustentado de FPS demonstrado nem correção de SPURS/Nexus.

Executável com a extensão de entrada de diagnóstico, SHA-256:
`105C933F6FA61AE7E31BB029595375D109B0D3037BAD5F6F6CFB674C7D83C029`.
Hashes finais dos saves/cache e ausência de processos de teste são conferidos
em `validation-final.json`.
