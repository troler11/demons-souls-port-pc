# FPS, FSR e geração de quadros — investigação de 23/10/2026

## Atualização: leitura de eventos SPU (26/10, etapa 22)

Corrigido SPU_RdEventStat para devolver somente eventos habilitados pela
máscara, preservando os demais até ACK explícito. Teste com canais reais
falha no objeto anterior e passa no corrigido, incluindo espera e 10.000
disputas de produtores. Seis execuções com gameplay e uma tentativa de
startup excluída. O Nexus continua parando em ambas as versões; não há
ganho sustentado de FPS/CPU comprovado. Diagnóstico passou a observar
crescimento do log durante a execução, evitando depender do mtime ao fechar.
Saves/cache preservados. [Método e resultados](SPU_EVENT_FOLLOWUP.md).

## Atualização: travadas e cache (26/10, etapa 21)

Oito execuções adicionais. Nexus parou também com janela visível e
apresentação síncrona; um controle sem regcache avançou, insuficiente para
atribuir a causa. Limite 30 FPS não resolveu a névoa na cópia do Velho Herói.
Essas falhas seguem abertas. Instalados 74 shaders individuais validados,
preservando os 1.441 anteriores. Na comparação do Velho Herói, o cache
ampliado reutilizou os três shaders que a primeira execução compilou;
maior preparo PSO observado 150,086→50,415 ms. Ainda houve outro shader novo
e outros custos de preparo. Não demonstra ganho sustentado de FPS/CPU.
Diagnóstico corrigido para reprovar frames parados apesar de FPS antigos.
Método e evidências em [STUTTER_FOLLOWUP.md](STUTTER_FOLLOWUP.md).

## Atualização: três novos combates e cinco pontos internos (26/10, etapa 20)

14 execuções adicionais. Combate observado com Velho Herói, Sanguessuga e
Flamelurker em cópias posicionadas dentro das arenas, usando as regiões de
ativação existentes e controles normais, sem editar flags ou AI. Os três
chegaram ao prazo com atividade e áudio, sem a parada longa da tentativa
externa. Cinco pontos internos observados em Boletaria, Latria, Pântano,
Stonefang e Caminho Ritual; alcance de cada recorte documentado. Astraea
continua parcial. Uma tentativa antes da entrada do Flamelurker apresentou
parada de frames por aproximadamente 79 s até o prazo; causa/correção abertas.
Picos curtos e pressão de CPU/SPURS persistem. Executável, saves e cache do
jogador preservados; sem alteração nativa nesta etapa. Evidências e limites
em [BOSS_MAP_FOLLOWUP.md](BOSS_MAP_FOLLOWUP.md).

## Atualização: inventário de mapas e novas lutas (26/10, etapa 19)

58 execuções, com tentativas dos 42 layouts MSB locais e controles no Nexus.
Novos combates confirmados: Velho Monge offline e Aranha Blindada. Os pontos
de entrada da campanha foram exercitados, mas entradas compartilhadas,
cinemáticas, layouts auxiliares e tentativas que não chegaram ao destino
continuam parciais/inconclusivos. Não equivale a uma passagem integral por
todos os mapas. Três paradas longas no Nexus em testes com câmera; controles
neutros continuaram e um rastreio alterou a reprodução. Causa/correção abertas.
Uma tentativa artificial de Allant caiu, retornou ao Penetrador e encerrou
com código 1; não comprova crash na luta de Allant. Cobertura, evidências e
limites em [MAP_TESTS.md](MAP_TESTS.md). Sem ganho sustentado de FPS declarado.

## Atualização: outros bosses (25–26/10, etapa 18)

28 diagnósticos (16 destinos e 12 repetições), sem crash espontâneo nos
trechos observados. Combate conferido visualmente em cinco encontros:
Cavaleiro da Torre, Penetrador, Adjudicador, Maneater e Colosso Imundo.
Os demais ficaram parciais/inconclusivos; algumas posições artificiais
caíram e reapareceram em outra área. Não foram aprovados como lutas completas.
Persistem pausas curtas e pressão de CPU/SPURS. Adicionados 284 shaders ao
cache local, preservando todos os 1.157 arquivos anteriores e os 14 saves;
sem mudança de executável ou flags padrão. Metodologia, tempos, limitações
e classificação por boss em [BOSS_TESTS.md](BOSS_TESTS.md).

## Atualização: Vanguard, compilação e processo sem janela (25/10, etapa 17)

Relato: travadas no boss do tutorial, sem FSR/geração de quadros. Encontrado
um problema adicional verificável: a sessão antiga PID 14972 não tinha mais
janela e continuava usando 3,72 núcleos em uma amostra de 3 s, ~31,1% dos
12 processadores lógicos deste PC. Seu log de apresentação havia parado às
21:58, mas as threads continuavam vivas. Encerrada somente essa sessão após
autorização do usuário. Isso pode disputar CPU com uma sessão seguinte;
não prova a causa das travadas ocorridas antes de fechar a primeira janela.

### Encerramento corrigido

`frame_clock` tratava o fechamento apenas com `rsx_ok=0`, deixando o laço e
as threads PPU/SPU ativos indefinidamente. Agora o backend da janela solicita
fechamento mantendo o HWND válido; o host impede novas operações de savedata,
continua atendendo FIFO/flips enquanto termina uma operação já iniciada,
drena o renderizador/caches e encerra o processo. Não força um novo autosave.
A barreira cobre a operação HLE até o fechamento dos seus arquivos; não é
uma garantia sobre alterações do jogo ainda não enviadas ao HLE.

O teste nativo `tests/save_shutdown.c` passou: fechamento durante escrita,
arquivo completo de 32 KiB após `fclose`, novas operações recusadas e 50
rodadas com quatro threads concorrendo com o fechamento, sem operações
restantes ou handles vazados. Dois testes reais por Alt+F4, com saves
isolados (Boletaria e Vanguard), encerraram normalmente com código 0 e log
`window closed, savedata idle, renderer drained`; nenhum processo remanescente.
O caso em que há escrita pendente foi exercitado pelo teste nativo, não
comprovado nos dois fechamentos do jogo. O teste de Boletaria tinha debug
D3D12; o do Vanguard não tinha debug.

### Reprodução do boss e caches preparados

Criada uma cópia dos saves e alterada apenas essa cópia para mapa 8/bloco 1,
posição (0,3; 19,3; –94), próxima do Vanguard. A posição do boss c1030 foi
lida do MSB local, usando a estrutura documentada no
[leitor MSBD do SoulsFormats](https://github.com/JKAnderson/SoulsFormats/blob/master/SoulsFormats/Formats/MSB/MSBD/PartsParam.cs).
Boss/barra/ataques conferidos em imagem. Comandos de câmera/deslocamento
programados; em parte do recorte a câmera colide com o modelo do boss.
Os flags são os da cópia do personagem, não uma progressão nova completa
do tutorial; morte/continuação normal da luta não foram validadas.

Três diagnósticos a 720p/escala 1/alvo 60, FSR/FG desligados, sem debug D3D12
ou sampler, com timers/logs/capturas. Primeiro cache vazio, depois cache
produzido, depois cópia do cache do jogador acrescida das variantes descobertas:

| Diagnóstico | Compilações VS/PS nos eventos | Eventos PSO ≥1 ms | Soma desses eventos | Maior evento |
|---|---:|---:|---:|---:|
| Vazio, 120 s | 146 | 131 | 13.858,79 ms | 534,97 ms |
| Repetição, 120 s | 3 | 15 | 298,41 ms | 142,94 ms |
| Cópia do cache do jogador preparada, até fechar normalmente | 7 | 18 | 844,95 ms | 153,08 ms |

Os números incluem boot/carregamento/transições. PSO mede o preparo inteiro,
incluindo compilação, e não CPU exclusiva. O terceiro processo durou mais;
trajetória/AI/renderização não são um replay determinístico, e a instrumentação
também perturba os tempos. **Não é um A/B de FPS nem demonstra ganho sustentado
de FPS/CPU na luta.** Mostra o custo alto de preparar shaders novos e que
repetições ainda descobrem variantes novas. A SPU0 continuou perto de um
núcleo inteiro, CPU total perto de 4,2 núcleos/35% em amostras tardias;
o limite SPURS/HighGraphics não foi eliminado.

Preparados e instalados neste PC 39 shaders adicionais em
`work/scratch/rsx_shader_cache_v2`. Os 1.118 arquivos originais foram mantidos,
exceto os dois índices/blobs atualizados a partir da cópia validada:
`ld_shader_pack_v1.bin` e `ld_pipelines_v1_932B0A5932C63003_51F60342B68CEAD9.bin`.
A biblioteca anterior foi carregada com sucesso, recebeu 55 stores sem
falhas e foi serializada no fechamento normal; não foi substituída por uma
biblioteca criada vazia. Os originais desses dois arquivos estão em
`work/perf17_20261025/cache_before`. Demais arquivos existentes permaneceram
byte a byte iguais; arquivos copiados conferidos por SHA-256 e publicados
por substituição atômica. Não há shaders do jogo adicionados ao repositório.
É preparação local do cache, não uma solução geral para todo material ainda
desconhecido. Flags experimentais de PSO/descritores continuam desligadas.

Quatorze arquivos dos saves originais iguais ao snapshot anterior aos testes;
nenhum restore sobre o savedata do jogador. Build, nativo e reprodução
integral do patch passaram. Executável final `build/MyGameRecomp.exe`:
`1F9C94711691FD3A78D7E1DA6746310E3DC0177E5C7361612C02DF168C032070`.
Evidências: `work/perf17_20261025` (incluindo `boss-analysis.json`,
`cache-manifest.json`, `cache-installed.json`), `work/diagnostics/perf17_boss*`.

## Atualização: preparação antecipada de objetos PSO (25/10, etapa 16)

Implementado e testado um cache de receitas completas que cria os objetos
D3D12 em uma thread de baixa prioridade durante a inicialização. A consulta
do renderizador usa somente objetos prontos e nunca espera essa thread.
Faltas, receitas novas e falhas seguem a criação normal. **Experimental,
desligado por padrão**: `LD_PSO_WARMUP=1` liga preparação e gravação de receitas;
`LD_PSO_WARMUP=0` com `LD_PSO_WARMUP_RECORD=1` somente grava para o próximo
processo. Ausente/vazia mantém a preparação desligada. Reiniciar ao trocar.
Na primeira sessão sem receitas, os objetos desconhecidos ainda precisam
ser preparados pelo caminho normal; o benefício depende das sessões seguintes.

### Comparação sem instrumentação detalhada

Quatro processos de 130 s, ordem off/on/on/off, mesmo candidato
`BBF4DC7AB9ADF51CB7602844928F0FCF3733535C4C443E0B9A64ECA7FA885598`,
mesmo snapshot de save/cache copiado por execução, Boletaria, câmera e
caminhada programadas, alvo 120, saída 1080p/720p interno, FSR/RCAS, FG
desligado. Debug D3D12, sampler interno, trace, captura e timers detalhados
realmente ausentes/desligados; sem compilação ou leitura grande concorrente.
Ambos os caches de shader/biblioteca anteriores permaneceram ligados.
O controle desliga também a gravação das novas receitas; a seleção on inclui
o custo da gravação periódica e da consulta, sem remover a biblioteca antiga.

Onze janelas completas de aproximadamente 5 s, contidas entre 60–120 s,
totalizando ~55,1 s por processo:

| Seleção | FPS de submissão | p95 médio/janela (ms) | p99 médio/janela (ms) | Maior intervalo (ms) | Intervalos >50 ms | >33⅓ ms |
|---|---:|---:|---:|---:|---:|---:|
| off | 87,57 | 22,31 | 27,24 | 57,97 | 4 | 42 |
| on | 87,54 | 21,90 | 26,91 | 48,73 | 0 | 31 |
| on repetido | 88,79 | 22,36 | 26,74 | 53,85 | 2 | 45 |
| off repetido | 85,09 | 23,18 | 29,02 | 206,40 | 4 | 43 |

O FPS do primeiro par ficou praticamente igual (–0,04%); a repetição deu
+4,35%, com variação também entre controles. CPU total nas últimas três
amostras de 10 s: ~49,00/49,28/48,50/48,89% dos 12 processadores lógicos,
sem redução consistente; essa janela de CPU difere da janela de cadência.
P95/p99 são médias de quantis por janela, **não percentis globais**, scanout
ou atraso do controle. Os intervalos >50 ms diminuíram nos dois pares, mas
são poucos eventos e os >33⅓ ms não melhoraram na repetição. Por isso o
recurso permanece opt-in, sem alegar ganho sustentado de FPS ou fim das travadas.

As duas execuções on prepararam os mesmos 155 objetos, sem falhas, em
322,77 e 51,49 ms; 2.240.015 bytes de receitas/entradas retidos e pack de
1.956.523 bytes. Memória interna do driver é adicional e não foi quantificada.
Os caches globais do driver já estavam aquecidos e não foram apagados;
este A/B não reproduz necessariamente a primeira pausa de 183,94 ms da etapa
15. Caminhada programada não equivale a replay determinístico nem cobre
transições de área. Dados: `work/perf16_20261025/measurements.json`,
`run-matrix.ps1`, `analyze.py` e `work/diagnostics/perf16_{off,on,on_repeat,off_repeat}`.

### Implementação, limites e integração

O pack `pso_warm_v1_<root-hash>.bin` guarda estado definido campo a campo,
os cinco estágios DXBC e layout de vértices/nomes semânticos; nenhum ponteiro
ou padding é persistido. Root signature conferida byte a byte, versão,
checksum e limites de tamanho. Hash direciona a busca, mas a igualdade da
receita completa decide o hit. Objetos e receitas ficam imutáveis após
publicação; referências independentes preservam os PSOs usados pelo renderer.
O worker chama `CreateGraphicsPipelineState` diretamente, sem acessar a
biblioteca de pipelines da thread de renderização. Concorrência com a criação
normal adota o objeto já pronto e libera o duplicado. Shutdown sinaliza e
junta o worker antes de liberar device/root/receitas.

Tetos: `LD_PSO_WARMUP_MB=64` (1–256 MiB de entradas/receitas, alocações do
driver adicionais), `LD_PSO_WARMUP_MAX=512` (1–2048 objetos) e
`LD_PSO_WARMUP_TIME_MS=5000` (100–30000 ms de orçamento suave, verificado
entre objetos; não cancela uma chamada do driver em andamento). Um único
writer a cada 10 s usa snapshot e substituição atômica, sem I/O sob a trava
do índice. Leitura interrompida por limite/cancelamento preserva o pack
completo anterior. Exige biblioteca/root compatíveis; sem suporte usa o
caminho normal. O custo da serialização/consulta ocorre nas faltas do cache
de PSOs já existente em RAM, não em cada desenho.

O teste nativo `tests/pso_warmup.cpp` passou com PSOs/DXBC reais na RX 7600:
persistência entre processos, codec completo comparado à identidade
canônica independente, padding, colisão forçada, estado distinto, referências
após encerramento, checksum/truncamento/versão/root, limite de entradas,
corrida entre worker e renderizador, falhas do driver, cancelamento de leitura
com receita nova pendente, orçamento de tempo e 12 ciclos com contagem de
handles. Ele cria objetos reais, mas não desenha na GPU.

Integração de 140 s separada do benchmark, FSR/RCAS, FG e debug D3D12 ligados:
155 objetos preparados em 471,89 ms, 220 hits e duas receitas novas adotadas,
zero falhas/descartes/duplicados; ~1,54 milhão de grupos executados sem
descarte. Imagem real de Boletaria conferida, caminhada registrada, áudio
ativo com zero perdas; últimas três amostras de silêncio 0/0,1/0,1%.
Sem erros/corrupção D3D12 ou falhas de PSO; avisos anteriores 679/820/695 e
miss de biblioteca 971 continuam possíveis. Interrupção no prazo pelo
diagnóstico; shutdown normal coberto pelo nativo. Não valida todo artefato
de framegen ou input lag, nem constitui benchmark de FG.

Conferência final de 100 s com o mesmo executável, variáveis de warmup e
gravação realmente removidas: nenhuma inicialização/estatística de warmup,
sem debug/sampler, caminhada em 17 intervalos registrados (máximo 3,80 m/s),
FSR/RCAS e imagem preservados. Áudio ativo, zero perdas e silêncio
0,1/0/0% nas três últimas amostras; sem falhas de PSO/dispositivo ou grupos
descartados. Na integração FG, caminhada em 18 intervalos, máximo 3,74 m/s.
Os 14 arquivos dos saves originais mantiveram os hashes anteriores à etapa,
sem restaurar sobre o savedata do jogador. Auditoria em
`work/perf16_20261025/audit-final.json`; ambos os diagnósticos encerrados
no prazo pelo coletor, não por saída normal do jogo.

Executável final `build/MyGameRecomp.exe`:
`ABF166FD9B3442BE9C36AF68664BA454DCF876AE44D272459AC5CD5B264A296E`.
Após o candidato A/B, acrescentados proteção do pack no cancelamento e
orçamento suave de preparação; ambos testados no nativo. O caminho de
consulta medido permanece igual, e os dois preparos medidos ficaram abaixo
do orçamento. Build e reprodução integral do patch sobre o SDK base passaram.
Cabeçalho novo incluído no exportador/teste do patch.

Referências de API: [objetos PSO D3D12](https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12)
e [amostra oficial com criação em background](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12PipelineStateCache/src/PSOLibrary.cpp).
Implementação própria, sem importar o fallback de shader genérico da amostra.

## Atualização: perfil SPURS/HighGraphics e correlação das travadas (25/10)

Diagnóstico, sem nova otimização de FPS ativada. Binário final com os
diagnósticos desligados por padrão:
`F87F145C335F2F70A7680FE86C9DF21BECAAFADAF87A67E4F08BE997463EDF32`.

### Amostragem nativa, separada dos benchmarks

Dois processos de 150 s no binário da etapa 14 (`60B28895…04546`), cópias
isoladas do save/cache de Boletaria, FSR/RCAS, FG desligado e alvo 120.
Debug D3D12 e sampler interno realmente ausentes. O coletor externo selecionou
somente HighGraphics ou `spu1000_0`, com cadência variada de 1–5 ms e timer de
alta resolução. Janela entre ~80–140 s de cada processo, zero falhas de coleta.
Não amostra `frame_clock`; sempre retoma a thread antes de escrever o CSV.
Mapa e `.pdata` correspondem ao executável exato; aliases são preservados.

| Thread | Amostras | CPU da thread em 60 s | Destaques das amostras de parede |
| --- | ---: | ---: | --- |
| HighGraphics | 17.230 | 42,92 s (~71,5% de um núcleo) | `NtWaitForAlertByThreadId` 26,53%; `NtDelayExecution` 8,51%; `NtWaitForSingleObject` 7,11%; `sys_timer_usleep` 7,42%; `vm_write64` 4,00%; `vm_write32` 2,73% |
| spu1000_0 | 17.220 | 59,30 s (~98,8% de um núcleo) | `spu_wrch_full` 17,28%; SPU PC 0x5120 9,66%; PC 0x4694 7,72%; `spu_ch_wait` 7,31%; `spu_drain_call` 7,06%; `spu_rdch` 5,96% |

São percentuais de amostras de parede da thread, não CPU total nem ganhos
potenciais de FPS. Um RIP no stub de espera também pode representar uma
chamada de sistema; não transforma sua porcentagem em CPU ociosa removível.
HighGraphics teve 48,63% das amostras em ntdll; exports Nt/Zw são aliases do
mesmo stub, não funções somadas. Helpers VMX aparecem em grupos de aliases
do linker: madd ~3,31%, add ~2,25%. Escritas coerentes/reservas ainda têm custo;
o gate de watch diagnóstico anterior não remove a coerência necessária.

No fonte gerado da imagem SPU `f5cc5c43…`, 0x4694 pode voltar a 0x5120;
este lê eventos, faz ACK, escreve registradores MFC, emite comando e lê o
estado atômico. O perfil aponta para protocolo de eventos/MFC/despacho,
sem demonstrar que todo esse trabalho seja espera dispensável. Não alterar
reservas/ACK/wakeups nem adicionar sleeps apenas pela ocupação de um núcleo.
O atalho `SPU_WRCH_DIRECT` anterior continua experimental e desligado.

### Eventos de leitura, descompressão e renderização

Foi adicionado `PS3_PERF_TRACE=1`, opt-in, com timestamps QPC em microssegundos
compartilhados entre threads. Separa prefault/trava/leitura do VFS, inflate
dos dados/sinalização, decode/upload de textura, preparo de PSO, flush e gaps
entre Presents. O recorte começou ~60 s após lançar o processo e registrou
operações de pelo menos 250 µs, gaps de pelo menos 33⅓ ms. Sem essa opção não
consulta QPC para esses eventos nem escreve o trace; não é ferramenta de FPS.

Dois processos de 180 s, alvo 120, mesmo roteiro de entradas, cache aquecido
inicial da etapa 14 e depois cópia do cache produzido pela primeira passagem.
Sem debug/sampler interno; com os profilers de renderização e log de posições.
Caches globais do sistema/driver não foram limpos. Não é replay determinístico
nem comparação de FPS. Movimento ficou restrito: repetição com sete janelas
de 0,5 s >1 m/s, máximo 3,68 m/s; não testou uma transição longa de área.

| Medida no recorte instrumentado | Primeira passagem | Repetição aquecida |
| --- | ---: | ---: |
| Eventos PSO ≥0,25 ms | 26 | 21 |
| Soma desses eventos PSO | 532,76 ms | 8,39 ms |
| Maior evento PSO | 63,58 ms | 0,71 ms |
| Maior gap de submissão | 183,94 ms | 64,42 ms |
| Gaps ≥50 ms | 9 | 4 |
| Eventos de textura ≥0,25 ms | 12 / máximo 6,03 ms | 12 / máximo 1,10 ms |

No gap de 183,94 ms, a união dos intervalos de PSO sobrepostos cobriu
158,99 ms; outro de 76,31 ms coincidiu com 63,58 ms de PSO. VS/PS já estavam
em cache (`compiled=0/0`), e vários desses casos também eram hits da biblioteca
de pipelines. O custo está no preparo/recuperação do objeto de pipeline no
processo, não demonstradamente em recompilar HLSL. A primeira sessão terminou
com 223 hits/6 PSOs novos na biblioteca; a repetição com 230 hits/zero novos.
O cache ajuda na revisita, mas a primeira aparição ainda merece pré-aquecimento
dos objetos PSO durante carregamento. Esses números não isolam cache do driver,
cache da biblioteca ou variação do percurso e não são um ganho de FPS medido.

Não houve evento VFS/prefault/trava/inflate ≥250 µs no recorte de gameplay.
Isso não exclui operações menores, outras áreas nem streaming em transições.
Vários gaps restantes não foram explicados pelos eventos capturados.

O log também pode criar atraso: na repetição, um evento PSO de ~0,40 ms foi
seguido por um relatório antigo de ~29,95 ms, que incluía o tempo de imprimir
o próprio evento. As análises acima usam os timestamps capturados **antes**
da impressão. A build final corrigiu o cronômetro de `LD_HITCH_PROF` para
também terminar antes dessa escrita. Ainda assim, os gaps podem conter custo
da instrumentação; não atribuir os gaps restantes automaticamente à GPU/CPU.
Próximo experimento de travadas: pré-criar PSOs na fase de carregamento;
para CPU, detalhar os caminhos MFC/canais/despacho preservando sua semântica.

### Validação e artefatos

Build e reprodução do patch sobre o SDK base passaram. Teste nativo confere
desligado, janela inicial, limiar, span invertido/zero, teto de 20 mil eventos
por unidade de compilação e conversão QPC sem overflow para uptime longo.
Conferência normal de 100 s no binário `38AF9979…EA6B5`: zero eventos com a
opção ausente, gameplay/FSR, áudio ativo/silêncio 0%/perdas zero nas três últimas
amostras, sem falhas de PSO/dispositivo ou grupos descartados, captura
`default-gameplay.jpg` preservada. A build final difere dessa conferência
somente pelo ajuste do cronômetro diagnóstico de PSO, desabilitado no padrão.
Os cinco processos concluíram no prazo; 14 hashes dos saves originais iguais
ao snapshot anterior à rodada, sem restauração sobre savedata do jogador.

Evidências em `work/perf15_20261025`: CSVs/JSONs/mapa do sampler, `events-analysis.json`
(primeiro resumo), `trace-warm-events-analysis.json`, `audit.json` e scripts.
Logs completos em `work/diagnostics/perf15_*`. O arquivo bruto de posições
da primeira passagem foi reutilizado pela repetição; seu resumo já estava
salvo. `trace-warm.frames` contém as posições da repetição. Os logs de eventos
dos dois processos estão separados e intactos; não comparar suas trajetórias
como um replay exato.

## Atualização: índices de tabelas SRV/samplers (25/10)

Executável entregue: `60B28895C40D1B75876BF9050AB425774AD9841AA63F9AF91DB4936107804546`.

O novo `rsx_descriptor_index.h` permite reencontrar tabelas SRV não consecutivas
e substitui a busca linear de tabelas de samplers por um índice limitado.
Mantém a comparação integral dos 16 slots; o hash apenas seleciona candidatos.
O atalho consecutivo SRV existente continua e os samplers ganham um equivalente.
Não altera os shaders, o número de desenhos ou a qualidade de imagem.

As entradas apontam para cópias imutáveis já existentes nos heaps visíveis à
GPU. Reescrever uma view CPU invalida o índice SRV; reciclar os cursores dos
anéis invalida ambos. Um avanço de fence sem reciclagem preserva as tabelas:
invalidar nesse caso criaria duplicatas e flushes desnecessários. Reservas
auxiliares de depth/UAV no mesmo anel são respeitadas pelo endereço real de
cada entrada. Heaps GPU mantêm os limites anteriores, incluindo 2.048 samplers.
Os arrays fixos acrescentam 698.880 bytes (682,5 KiB), além dos pequenos
estados dos índices; não há alocação por desenho.

Permanece **experimental e desligado por padrão**: não houve ganho consistente
de FPS sem instrumentação. `LD_DESCRIPTOR_CACHE=1` habilita o experimento;
ausente/vazia/`0` seleciona a reutilização anterior. `LD_SRV_REUSE=0` também impede o
novo índice SRV, mantendo a seleção dos samplers. Reiniciar ao trocar.
Os caches de shaders/pipelines da etapa 13 continuam ligados. FG permanece
desligado por padrão.

### A/B sem debug/sampler, alvo 60 — referência de uso normal

Quatro novos processos de 130 s, off/on/on/off, mesmos save/cache/câmera,
resolução e FSR/RCAS. `Remove-Item Env:<nome>` retirou as flags de fato;
os quatro logs confirmam ausência da camada D3D12 e do sampler. Sem captura,
profiler detalhado ou compilação concorrente. Nove janelas completas entre
70–120 s (~45 s), conforme o critério abaixo, no binário `ABA42D41…FC132`.
O binário entregue acrescenta o tratamento de flags debug/GBV vazias/`0` e
seleciona o padrão desligado do novo índice. O A/B usou seleções explícitas;
com flags D3D realmente ausentes, os caminhos testados são os mesmos.

| Ordem | Índice | FPS | p95 médio | p99 médio | Maior intervalo | >50 ms | CPU total |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | off | 58,91 | 20,13 ms | 25,55 ms | 61,61 ms | 2 | 39,17% |
| 2 | on | 59,12 | 20,36 ms | 23,90 ms | 46,79 ms | 0 | 39,06% |
| 3 | on | 59,21 | 19,69 ms | 23,44 ms | 55,12 ms | 1 | 39,14% |
| 4 | off | 59,02 | 20,39 ms | 24,27 ms | 60,57 ms | 1 | 39,81% |

FPS +0,36%/+0,31%, p99 médio –6,46%/–3,41%; p95 não melhorou nos dois pares.
Com o limitador de 60, o ganho médio é pequeno e a CPU total permanece
próxima de 39–40%. Poucas pausas >50 ms (2→0 e 1→1); não demonstra eliminar
travadas. Esses são os números apropriados para o uso normal neste percurso,
sem extrapolar os ganhos do experimento instrumentado abaixo.
Logs `work/diagnostics/perf14_clean_*`; consolidação `measurements-clean.json`.

### Teste de capacidade com alvo 120 — sem debug/sampler

Um par adicional de 130 s no mesmo binário
`77504349048E033AD5096031635D7B64E146301A231FEBA9B85BA266D9E9E276`,
mesmo save/cache/percurso/FSR, somente alvo alterado para 120. Nove janelas
completas entre 70–120 s por processo; sem profiler detalhado/captura/compilação
concorrente. Flags debug/GBV explicitamente `0`, verificando também a nova
leitura correta; logs sem camada D3D12 ou sampler. Não é um par repetido.

| Índice | FPS | p95 médio | p99 médio | Maior intervalo | >50 ms | CPU total |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| off | 80,73 | 25,11 ms | 32,26 ms | 70,16 ms | 4 | 46,42% |
| on | 79,26 | 25,73 ms | 31,64 ms | 62,62 ms | 4 | 47,08% |

FPS –1,81%, p95 pior, p99 médio ~1,94% menor, mesma contagem >50 ms e CPU
ligeiramente maior. Uma comparação não estabelece regressão universal, mas,
junto ao ganho mínimo a 60, não justifica ligar o experimento por padrão.
`LD_DESCRIPTOR_CACHE=1` fica disponível para testes em outras áreas. Não há
ganho grande de FPS ou redução de CPU demonstrados no uso normal desta etapa.
Resultados em `measurements-capacity.json`, `run-capacity.ps1` e
`work/diagnostics/perf14_capacity_*`.

### A/B inicial com caminhada e giro — debug e sampler ativados

Quatro processos de 130 s, ordem off/on/on/off, no mesmo candidato
`63610727B1CABBA7E7DFB46C038F96DCE1FF0828754ED5E1AB3D6977F92B7DFC`,
com seleções explícitas. Cópias independentes do mesmo cache aquecido e save
de Boletaria; RX 7600, alvo 60, janela 1080p/720p interno, FSR/RCAS ligado,
FG desligado, apresentação em worker e FIFO idle desligado. Câmera alternada,
caminhada para frente/trás aos 75–79 s e 110–114 s. Sem compilação concorrente,
dumps ou profiler detalhado no A/B. A auditoria dos logs confirmou
debug D3D12 e sampler de 5 ms/boot ligados nos quatro processos: neste PowerShell, remover a variável
com a API .NET deixou uma string vazia, que o renderizador trata por presença.
Esse conjunto mede o ganho sob validação D3D12 e sampler; pode ampliar o custo das
cópias/buscas e não deve ser apresentado como ganho normal sem debug.
O roteiro seguinte usa `Remove-Item Env:RSX_D3D_DEBUG` e verifica o log.

Selecionadas nove janelas completas contidas entre 70–120 s, ~45 s por processo,
para excluir o carregamento. FPS = frames/tempo dessas janelas. P95/p99 são
médias dos quantis das janelas, não percentis globais. Contagens >50 ms são
exatas nas janelas; durações variam até ~0,3%. Mede submissões, sem medir
scanout/input lag. CPU corresponde aos três últimos relatórios, normalizados
por 12 processadores lógicos, em outro intervalo. O parser aceita mensagens
com prefixos de outras threads na mesma linha, sem perder janelas válidas.

| Ordem | Índice | FPS | p95 médio | p99 médio | Maior intervalo | >50 ms | CPU total |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | off | 46,02 | 36,90 ms | 47,08 ms | 227,35 ms | 38 | 52,50% |
| 2 | on | 54,11 | 27,27 ms | 32,10 ms | 69,51 ms | 5 | 52,69% |
| 3 | on | 53,90 | 27,85 ms | 32,06 ms | 64,64 ms | 6 | 51,33% |
| 4 | off | 50,69 | 30,80 ms | 37,92 ms | 76,67 ms | 22 | 50,03% |

Os dois pares favorecem o índice. A comparação repetida, mais conservadora,
teve +6,33% FPS, –15,46% no p99 médio e 22 → 6 pausas >50 ms (–72,7%).
O primeiro par teve +17,57% FPS/–31,81% no p99 médio, mas o controle variou
de 46,02 para 50,69 FPS; não tratar 17,6% como ganho universal. Caches globais
do sistema/driver não foram apagados. Não houve redução de CPU total:
o benefício demonstrado foi executar mais quadros e reduzir a cauda neste
percurso instrumentado. O binário entregue mantém o experimento desligado e
corrige a leitura de flags D3D12: ausente/vazia/`0` desliga debug/GBV;
`1` continua opt-in. Não extrapolar a tabela para uso sem instrumentação.
Áudio ativo e perdas zero nas três últimas amostras dos quatro A/B; silêncio
0–0,4%, sem falhas de PSO/dispositivo registradas nem grupos descartados por
anel/PSO nas contagens finais.

Resultados e scripts: `work/perf14_20261025/measurements.json`, `analyze.py`,
`run-matrix2.ps1` e `work/diagnostics/perf14_v2_*`. Protótipos e controle
interrompido anteriores não participam da tabela.

### Verificação de conteúdo e vida das tabelas

Teste nativo extrai as funções reais de `rsx_live_draw.c` e as compila contra
heaps simulados, com oráculo independente para os 16 valores copiados.
Quatro seleções: off/on/padrão sem variável/SRV reuse desligado, 50 mil desenhos
por processo. Confere mutação de origem sem alterar a cópia GPU anterior,
reutilização não consecutiva, reset com o mesmo fence, fence sem reset com
anel de samplers cheio, reservas auxiliares no anel, capacidade/guardas,
40 chaves no mesmo bucket inicial, orçamento de 32 entradas e wrap da época.
Esse teste não executa comandos GPU. Build e reprodução integral do patch
no SDK base passaram; o novo header faz parte do exportador e do teste.

Integração `perf14_integration`: 130 s com o protótipo ligado (variáveis
vazias), FSR/RCAS + FG, debug D3D12 e câmera/caminhada. Terminou com
791.534 hits/8.021 cópias no novo índice SRV e 1.422.225 hits/207 cópias no
índice sampler; estes contadores são de consultas, não número de desenhos.
SRV não inclui o atalho consecutivo anterior; sampler pode incluir preflight.
Áudio ativo/perdas zero/silêncio 0/0,2/0% nas três últimas amostras, sem
erros/corrupção D3D12 ou PSOs/grupos descartados. Aviso 820 permaneceu.
Caminhada confirmada: 16 janelas >1 m/s, máximo 3,71 m/s. A captura real
`integration-gameplay.jpg` preserva personagem, materiais, cenário e HUD;
não é validação de todos os frames interpolados nem de input lag. Filmes
interrompidos pelo autoavanço após 12/27 frames, sem validar filmes completos.
Quatorze hashes dos saves originais iguais ao snapshot anterior aos testes.

Conferência do binário entregue `perf14_default`, 95 s: `LD_DESCRIPTOR_CACHE`
vazia confirmou `on=0`; debug/GBV explicitamente `0` confirmaram ausência da
camada, sem sampler. Gameplay com FSR/RCAS, giro e caminhada (oito janelas
>1 m/s, máximo 3,83 m/s), captura `default-gameplay.jpg` preservada. Áudio
ativo/perdas zero/silêncio 0,2/0/0,1% nas três últimas amostras; sem falhas de
PSO/dispositivo registradas ou grupos descartados. Essa conferência usa
profiler/captura e não é benchmark. Build, teste nativo final e reprodução
do patch passaram; hashes dos 14 saves originais continuam iguais.
Auditoria: `work/perf14_20261025/audit.json`.

## Atualização: preload de shaders e biblioteca persistente de PSOs (25/10)

Executável entregue: `C48DCBC72BB6FD86DF159324D703F8A1067DB14FCAA06A3D05202F1E3FB0089E`.

Foram implementados os dois próximos alvos da análise do inFAMOUSreborn:
pré-carregar bytecode em RAM por uma thread de baixa prioridade, com pack
consolidado, e persistir os pipelines do driver com `ID3D12PipelineLibrary`.
Ambos ficam ligados por padrão. `LD_SHADER_PRELOAD=0` e `LD_PSO_LIBRARY=0`
permitem comparação independente no mesmo executável; ausência ou string
vazia usa o padrão ligado. Não alteram shaders, qualidade, resolução ou FG.

### Implementação e recuperação

`rsx_shader_preload.h` mantém entradas imutáveis, com estágio, hash e comparação
integral do HLSL antes de devolver o DXBC. Lê primeiro `ld_shader_pack_v1.bin`
e acrescenta arquivos individuais ainda ausentes. Formato versionado, checksum,
limite padrão de 64 MiB para registros retidos, máximo de 16.384 entradas e
substituição atômica do pack. Índice e blobs quentes são memória adicional.
Cache limitado/cancelado não sobrescreve um pack completo. Faltas usam o
carregador anterior; shaders compilados durante a sessão entram no próximo pack.
Na amostra aquecida: 330 entradas, 4.469.188 bytes retidos, leitura em ~22 ms.

`rsx_pipeline_library.h` identifica a descrição completa do PSO, incluindo os
bytes dos cinco estágios e da root signature, com duas chaves de 64 bits,
sem padding ou endereços. O arquivo guarda também a root signature completa.
O D3D12 confere compatibilidade do blob do driver; falha, arquivo inválido ou
suporte ausente retorna à criação normal. O buffer emprestado pela API vive
até liberar a biblioteca. Um writer grava a cada 10 s, preserva gerações
pendentes em falhas e é sinalizado/juntado antes da gravação final e liberação.
O teto padrão de 128 MiB limita o blob serializado, não toda a RAM do driver.
`RSX_NO_SHADER_DISK_CACHE` impede a inicialização dos dois recursos.

### Medição A/B de cadência — debug D3D12 e sampler ativados

Seis processos de 130 s no mesmo candidato
`F2D98EF26D9B47D436B160A5EB437EC32EEF4D87689DDBDF5A6A88A751517972`,
com seleções explícitas 0/1 e cópias independentes do mesmo cache aquecido e
save de Boletaria. RX 7600, alvo 60, janela 1080p/720p interno, FSR/FG desligados,
apresentação em worker, FIFO idle desligado. Câmera alternada a cada 15 s,
personagem parado. Sem dumps ou timers detalhados no A/B.
Auditoria posterior em 25/10 confirmou debug D3D12 e sampler de 5 ms/boot ligados nos seis processos:
a API .NET usada para limpar a variável a deixou vazia, e o renderizador
ativa a camada e o sampler por presença. São comparações com a mesma instrumentação nos dois
lados, mas seus ganhos não foram demonstrados sem debug nesta etapa.
Caches globais do sistema/driver não foram limpos; controles no início/fim.

Foram selecionadas 14 janelas completas de cerca de 5 s contidas entre
45–120 s, aproximadamente 70 s por processo. P95/p99 são médias dos quantis
de cada janela, não percentis globais. Contagens acima de 50 ms são exatas
nessas janelas; durações diferem menos de 0,1%. Mede intervalos de submissão,
não scanout/atraso do controle. CPU é a média dos três últimos relatórios,
normalizada por 12 processadores lógicos, em outro intervalo.

| Ordem | Preload | Biblioteca | FPS | p95 médio | p99 médio | Maior intervalo | >50 ms | CPU total |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | off | off | 56,49 | 23,49 ms | 38,70 ms | 449,40 ms | 23 | 48,78% |
| 2 | on | off | 56,24 | 23,31 ms | 31,00 ms | 149,29 ms | 19 | 49,31% |
| 3 | off | on | 56,93 | 22,12 ms | 36,45 ms | 274,71 ms | 14 | 52,81% |
| 4 | on | on | 56,94 | 22,26 ms | 30,36 ms | 207,80 ms | 16 | 49,25% |
| 5 | on | on | 57,05 | 21,77 ms | 24,76 ms | 73,18 ms | 11 | 51,22% |
| 6 | off | off | 55,82 | 24,38 ms | 39,13 ms | 258,73 ms | 22 | 51,28% |

O par repetido (5/6) apresentou +2,19% FPS, –36,7% no p99 médio e metade das
pausas >50 ms. O primeiro par (1/4) teve +0,8% FPS e –21,6% no p99 médio;
pausas >33 ms não melhoraram nesse par. Não houve queda consistente da CPU.
Na primeira execução off houve leitura do hash do executável (~444 MB) perto
de 90–95 s; nessa janela não houve intervalo >33 ms. O maior pico aconteceu
~50 s, antes dessa leitura. O par repetido não teve essa interferência e é a
comparação principal. Há variação entre execuções; não promete esse ganho
em todas as áreas nem elimina as travadas ao revelar material pela primeira vez.

O executável entregue difere do candidato pela seleção padrão ligada, tratamento
de variáveis vazias e log de configuração; algoritmo e shaders são os mesmos.
Medições brutas/consolidador: `work/perf13_20261024/measurements.json`,
`analyze.py` e `work/diagnostics/perf13_{off,preload,library,both,both_repeat,off_repeat}`.

### Verificação

Build e reprodução integral do patch sobre o SDK base passaram. Teste nativo
real de D3DCompile/D3D12 cobre comparação byte a byte, estágios/colisão,
pack corrompido/truncado/versionado, orçamento, publicação com quatro leitores,
20 ciclos de cancelamento/encerramento e contagem de handles. PSOs reais na
RX 7600: criação/acerto, reload em outro processo, estado/padding, writer,
arquivo truncado e teto com fallback. `tests/test_present_stats.py` confere
quantis e contagens >33/>50 ms contra oráculo independente em seis janelas.
Encerramento normal dos caches é coberto no teste nativo; o diagnóstico do
jogo é interrompido no prazo e depende das gravações periódicas.

A integração aquecida `perf13_verify` (130 s, FSR/RCAS, debug D3D12,
caminhada/giro e capturas) confirmou os padrões mesmo com variáveis vazias:
137 acertos/3 faltas de preload, 210 acertos/19 pipelines novos, zero falhas
na biblioteca. Novos materiais continuaram no fallback. Áudio ativo e perdas
zero nas três últimas amostras; sem erro/corrupção D3D12 ou `pso-fail`.
Avisos anteriores 679/820 permanecem; a biblioteca acrescenta o aviso 971
quando o nome ainda não existe, um miss esperado seguido de criação normal.
Captura `verify-gameplay.jpg`
preserva cenário, materiais e HUD. Essa execução instrumentada não mede ganho
de FPS. Filmes tiveram apenas transições curtas por autoavanço.

A integração `perf13_cold` executou o binário final por 130 s com diretório
de cache inicialmente vazio, FSR/RCAS e debug D3D12. Terminou com 145 PSOs
armazenados/74 acertos internos, zero falhas e gravação periódica de 962.348
bytes. Preload vazio recuou ao compilador/carregador, como esperado. Sem erros
D3D12/PSO, áudio ativo/silêncio 0%/perdas zero nas três últimas amostras.
Caminhada confirmada no framelog: seis janelas >1 m/s, máximo 3,65 m/s.
Na integração aquecida foram 12 janelas, máximo 3,69 m/s.

Reinício final `perf13_fg` (110 s, FSR/RCAS + FG, debug D3D12) usou uma cópia
do cache criado na execução vazia: reload do blob de 962.348 bytes com S_OK,
136 entradas individuais consolidadas em pack, 136 hits/zero misses de preload,
221 hits/3 novos PSOs e zero falhas. Voltou à gameplay, com nove janelas de
caminhada >1 m/s, máximo 3,72 m/s. Áudio ativo, perdas zero nas três últimas
amostras, silêncio 0/0,1/0%. Sem erros/corrupção D3D12/PSO; avisos
679/820/695 e misses 971. `fg-gameplay.jpg` confere a imagem real renderizada;
não valida todos os artefatos dos frames interpolados nem a latência de entrada.
Filmes interrompidos pelo autoavanço após 20/25 frames. FG continua desligado
por padrão e este caso não é benchmark de ganho FG.

Os 14 hashes dos saves originais continuam iguais ao snapshot feito em 25/10
antes dos testes finais; nenhuma restauração sobre o savedata do jogador.
Auditoria consolidada em `work/perf13_20261024/integration-audit.json`.

Próximo alvo: custo restante de descritores/preparo dos desenhos e streaming
ao caminhar; usar perfil separado antes de alterar sincronização ou SPURS.

## Atualização: iluminação das prévias e investigação de travadas ao girar a câmera (24/10)

Executável desta rodada: `37F9D4D36C5E3E4784BF2584E3F7028DE8769E19E055FFA57F66F20A62062C85`.

### Correção visual confirmada na criação e no retrato do save

O tradutor FP ignorava os modificadores de precisão dos operandos. O shader
real da prévia usa SAT (SRC1.src0_prec_mod=4) ao ler `h6`: seu W contém o
fator de névoa vindo de TC2.w. Na captura, a avaliação do VS com os vértices e
constantes reais deu aproximadamente –0,977. Sem SAT, o shader extrapolava a
mistura com branco para valores negativos: o rosto ficava vermelho escuro e
partes da armadura quase pretas.

`rsx_fp_decompiler.c` agora aplica o SAT codificado na instrução a cada um dos
três operandos, antes de abs/negação. Não muda o brilho global. Modos de
precisão diferentes de SAT mantêm o comportamento anterior. O caminho antigo
pode ser comparado iniciando outro processo com `RSX_FP_SRC_SAT=0`.

No rastreamento do shader real, offset 1536, palavras
`02401080 1C9DC918 0021C802 0001C800`, a tradução passou a conter
`saturate((h[6]).xyzw)`. A imagem corrigida mostra pele com cor normal e a
armadura visível. Trocar constantes buffered por literal não corrigia a imagem;
o problema também existia antes das otimizações desta rodada. As capturas
`creation1.jpg`, `literal.jpg` e `corrected.jpg` estão em
`work/perf12_20261024`; `portrait-comparison.jpg` reúne antes/depois.
Na lista de perfis, o mesmo binário com SAT desligado reproduziu o rosto
preto/vermelho de Gabranth; com o padrão corrigido, rosto e equipamento
ficaram visíveis. Comparação da mesma cópia de save em `load-legacy.jpg`
e `load-sat2.jpg`. A correção não exige editar o savedata nem seus ícones.

Teste numérico em D3D11 WARP executa HLSL gerado pelo tradutor real: 120 casos
no modo corrigido e 120 no legado. Inclui operandos 0/1/2, origens temporário,
entrada e constante inline, traduções literal/buffered, limites negativos e
acima de 1, negação e abs em SRC0/SRC1. Verifica também valores finitos; não
valida a precisão half/fixed, alias de registradores ou abs de SRC2.
Codificação e ordem dos modificadores conferidas em
[RSXFragmentProgram.h](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/RSX/Program/RSXFragmentProgram.h)
e [GetSRC](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/RSX/Program/FragmentProgramDecompiler.cpp).

### Texturas não usadas: experimento permanece desligado

`LD_USED_TEXTURES=1` usa a máscara do programa FP para evitar preparação de
texturas/samplers que nenhuma instrução pode ler, incluindo todos os ramos.
Não interfere nas texturas de vértice. Diagnósticos que injetam amostras
forçam o caminho completo. A máscara continua validada contra as instruções;
sem cache de metadados e com o experimento desligado, não faz nova varredura.

Quatro processos de 135 s, ordem desligado/ligado/ligado/desligado, mesmo
binário experimental `DA6688D7…D4EC`, save copiado de Boletaria, alvo 60,
1080p/720p interno, FSR/RCAS, sem FG; rotação alternada da câmera por PAD_PATH,
com personagem parado. Sem sampler, compilação concorrente ou LD_HITCH_PROF.
LD_FLUSH_PROF e estatísticas de submissão iguais nos quatro processos.
Últimas seis janelas de FPS/quantis/flush; três de CPU, normalizadas por 12
processadores lógicos. Tempos de preparação são do profiler, não tempo de GPU.

| Ordem | FPS | CPU total | p99 médio por janela | Preparar texturas por draw |
| --- | ---: | ---: | ---: | ---: |
| Desligado | 59,90 | 37,03% | 17,00 ms | 1,823 µs |
| Ligado | 59,83 | 37,78% | 17,76 ms | 1,499 µs |
| Ligado, repetição | 59,80 | 38,36% | 19,01 ms | 1,405 µs |
| Desligado, repetição | 59,77 | 36,92% | 19,14 ms | 1,774 µs |

Custo local por desenho caiu aproximadamente 18–21%; câmera por tempo real
não garante sequência idêntica de materiais/desenhos. **Não demonstrou ganho
consistente de FPS, CPU total ou p99.** Esse percurso ficou perto do limite de
60; não representa o pior caso de uma área pesada. Não habilitar por padrão
nem apresentar a redução local como ganho de FPS. As imagens preservam HUD,
geometria, materiais e sombras na inspeção das capturas. Áudio ativo/perdas
zero nas amostras finais, sem falhas D3D registradas nessas quatro execuções.

### Travadas: custo de PSOs ainda precisa ser atacado

Novo diagnóstico opt-in `LD_HITCH_PROF=1` registra o frame e separa o tempo de
VS, PS e criação do pipeline; registra também flushes >=1 ms e a espera da
GPU. Desligado por padrão, sem novos temporizadores nos flushes no modo normal.
Não usar os logs detalhados como benchmark da versão final.

Processo diagnóstico separado de 145 s, mesma cópia de save e giro da câmera,
FSR/FG desligados: ao revelar materiais houve preparação de PSOs de 34–75 ms
mesmo com `compiled=0/0` (bytecode encontrado no cache). Exemplo frame 2346:
75,385 ms totais, 47,480 no segmento PS/cache e 27,882 na criação D3D12;
frame 2728: 65,491 ms, 39,260 PS/cache e 26,209 criação. O carregamento inicial
também teve pausas maiores e deve ser separado da gameplay. Depois de os
materiais estarem residentes, as últimas janelas ficaram perto de 60 FPS.

Isso fundamenta investigar cache persistente/pré-aquecimento de PSOs e leitura
antecipada de bytecode. **Essas mudanças ainda não foram implementadas e as
travadas ao movimentar a câmera não estão declaradas resolvidas.** O filtro
de texturas não resolveu esse custo. Também há esperas de alguns ms nos
anéis de constantes/descritores; não aumentar o heap de samplers além do
limite D3D12 para tentar escondê-las.

Build, testes nativos e reprodução integral do patch no SDK base passaram.
Integração final de 150 s com a correção no padrão, sem FG/FSR, vídeos e
debug D3D12, mais 100 s com FSR/RCAS, caminhada e giro de câmera: sem erros
ou corrupção D3D12 e sem falhas de PSO. Avisos anteriores 679/820, e 695 na
execução com FSR, permanecem. Nas três últimas amostras, áudio 187,4/187,2/
187,6 blocos/s com silêncio zero na primeira, e 187,0/187,6/187,4 com silêncio
0,1/0/0,1% na segunda; perdas zero nas duas. Caminhada confirmada pelo log de
posição, com 16 janelas acima de 1 m/s (máximo 3,78 m/s). Não são benchmarks
de ganho de FPS. Vídeos encerrados pelo autoavanço após 0/27 frames: transição
de SAIL testada, não filmes completos. Hashes dos 14 arquivos originais iguais
antes/depois dessas integrações; todos seus mtimes antecedem o início da
etapa 12. O snapshot de hashes herdado da etapa 11 antecede as alterações
de saves de 20:21–20:26 e não foi usado como referência dessa conferência.
O teste de metadados inclui agora máscaras absolutas para sete opcodes de
textura, 16 unidades, dados inline que parecem instruções, branches e mutação
do programa; mantém os 3.000 programas diferenciais e 3.127.008 mutações.
Evidências: `work/perf12_20261024` e `work/diagnostics/perf12_*`.


## Atualização: gate de diagnóstico PPU validado e teste de flags VMX (24/10)

Executável entregue: `3B315CCE1687FE92659EADF094B68720A0A869D009360725DB8CB4652CEC2EF2`.
`PPU_WRITE_WATCH_FAST=1` por padrão; `=0` permite comparar o caminho completo.
Nenhum acesso de memória, reserva/coerência SPU, atomic, guarda de endereço
ou comando gráfico foi eliminado. FG e FIFO idle continuam desligados por padrão.

`barrier_watch_hit` ganhou um gate inline pequeno. Sem diagnóstico de escrita
selecionado no início do processo e sem watch dinâmico armado, evita chamar
o corpo pesado, que continua intacto em `barrier_watch_hit_slow`. O watch
`g_barrier_sync_watch` é consultado em cada escrita; pode armar/desarmar depois.
Qualquer `PPU_WWATCH`, `PPU_WVAL` ou modificador presente mantém o caminho
completo, inclusive com texto `0`. Configuração antes das threads guest,
como os switches inline já existentes; reiniciar para trocar variáveis.

Quatro processos de 120 s, mesmo executável e save/câmera neutra de Boletaria,
1080p de saída/720p interno, FSR/RCAS, alvo 60, sem FG, apresentação em worker,
FIFO idle desligado. Ordem 0/1/1/0, sem sampler ou temporizadores por escrita,
sem compilação/teste nativo durante as execuções. `LD_FLUSH_PROF`, contador de
intervalos e frame clock iguais nos quatro. Seis últimas janelas para FPS e
intervalos; três últimas para CPU, normalizada pelos 12 processadores lógicos.

| Caso, na ordem executada | FPS base | CPU total | Média do p95 por janela | Média do p99 por janela |
| --- | ---: | ---: | ---: | ---: |
| Completo (`off`) | 42,77 | 51,47% | 44,17 ms | 53,32 ms |
| Gate (`on`) | 47,30 | 50,86% | 40,75 ms | 52,08 ms |
| Gate (`on_repeat`) | 48,60 | 49,22% | 39,58 ms | 49,91 ms |
| Completo (`off_repeat`) | 43,97 | 51,42% | 44,05 ms | 52,44 ms |

FPS +10,6% no primeiro par e +10,5% na repetição; médias dos p95 –7,7% e
–10,2%. CPU total caiu 0,61 e 2,19 pontos percentuais nesses pares; não há
uma redução grande/universal de CPU demonstrada. O jogo entrega mais frames
com uso total semelhante. Quantis são médias de janelas de submissão, não
percentis globais ou medições de input lag. Números absolutos de outra rodada
não são uma referência equivalente; a comparação acima é no mesmo binário.

Conferência adicional contra a versão realmente entregue antes desta rodada:
dois processos de 120 s, mesmas condições de benchmark e seis últimas janelas,
sem sampler/debug/compilação simultânea. Não misturar com a integração abaixo.

| Executável | FPS base | CPU total | Média do p95 por janela | Média do p99 por janela |
| --- | ---: | ---: | ---: | ---: |
| Anterior `E2A237CF…60611` | 45,25 | 49,81% | 42,92 ms | 51,03 ms |
| Novo `3B315CCE…C2EF2`, gate ligado | 48,10 | 50,36% | 40,27 ms | 51,59 ms |

Nesse par, FPS **+6,3%**, p95 médio –6,2%, p99 sem melhora e CPU total
sem redução. É uma conferência de um par, não uma nova série contrabalanceada.
O ganho de ~10,5% da tabela anterior é contra o modo completo dentro da
build nova; não apresentá-lo como ganho medido contra o executável anterior.
Disposição do código e variação entre execuções podem alterar a referência.

Capturas dos quatro casos preservam câmera, geometria, materiais, sombras e
HUD (`work/perf11_20261024/comparison.jpg`). Áudio ativo e perdas zero nas seis
últimas amostras dos quatro A/B, sem erros D3D registrados. Seis testes nativos
extraem/compilam o código real do watch e comparam logs e efeitos contra o
caminho completo: limites/janela, metades, valor/faixa/arquivo, cap, invocação
de guard e armar/desarmar/rearmar o watch dinâmico. Callbacks de stack/guard
são stubs nesse teste. Build e reprodução integral do patch no SDK base passaram.

Integração no mesmo executável: 130 s sem FG/alvo 60 e 110 s com FG/alvo 30,
ambos com vídeos, debug D3D12 e profilers detalhados, retornaram à gameplay.
Nas três últimas amostras de áudio: 187,0/187,4/187,6 blocos/s e silêncio
0,7/0/0% sem FG; 187,4/187,6/187,4 e silêncio zero com FG; perdas zero nos dois.
Sem erros/corrupção D3D12; avisos anteriores 679/820 permanecem. Vídeos
`ds_advertise`/`ds_title` encerrados pelo autoavanço após 5/30 frames sem FG
e 4/32 com FG: transições curtas, sem validar filmes completos. Os FPS dessas
execuções instrumentadas não fazem parte do A/B acima nem validam ganho de FG.

Na integração sem FG, um coletor externo restrito à HighGraphics registrou
11.553 RIPs em 40 s (80–120 s de processo), sem falhas, com timer de alta
resolução e intervalo variado de 1–5 ms; cadência efetiva ~3,46 ms. Resolução
pelo mapa exato/`.pdata` do binário. Nenhuma amostra direta no helper de watch.
Esse perfil tem configuração/cadência diferentes do perfil antigo; não usar
a diferença entre porcentagens para calcular CPU economizada. O A/B sem
sampler demonstra o ganho de FPS. Memória, VMX e polling continuam como alvos.

Segundo experimento, **isolado e sem mudar as flags do jogo**: helpers VMX
reais compilados no modo PPU atual versus AVX2/BMI2/FMA,
`-fno-math-errno` e `-ffp-contract=off`, em objetos separados. 2,4 milhões de
resultados vetoriais bit a bit iguais para add/madd/rsqrt, VSCR NJ ligado e
desligado, destino sobreposto a cada entrada, NaN/infinito/denormais/zeros
e operandos aleatórios. Não é prova de equivalência de todo o PPU.

Microbenchmark de três repetições, cinco milhões de chamadas por operação,
entradas finitas normalizadas, ordem alternada: madd ~19,7–20,0 → 9,6–9,8 ns
(~2,04x); rsqrt ~15,1–15,3 → 10,7–10,9 ns (~1,4x); add ~13,0–13,1 →
13,3–13,4 ns (pequena piora). O ganho é local, não FPS do jogo. Esse pacote
de flags continua candidato para uma build PPU completa com A/B e integração,
sem usar fast-math. Stores inline no PPU principal, registradores PPU locais
e replay de jobs SPU não foram implementados nesta rodada.

Evidências: `work/perf11_20261024` (binários/mapas, testes, capturas, scripts,
`measurements.json`, perfil bruto e experimento VMX) e
`work/diagnostics/perf11_*`. Opção documentada em `ENVIRONMENT.md` e teste
reproduzível em `AUTOTEST.md`. Hashes dos 14 arquivos de save originais
preservados; não houve restauração sobre o savedata do jogador.

## Atualização: perfil de HighGraphics por endereço nativo (24/10)

Investigação sem alterar fontes do runtime, padrões ou executável entregue.
Foi usada uma cópia do executável `E2A237CF…60611` e seu mapa completo,
mesmo save pesado de Boletaria/câmera neutra, saída 1080p/interno 720p,
FSR/RCAS ligados, FG desligado, apresentação em worker e FIFO idle desligado.
As duas execuções de diagnóstico duraram 150 s. Não constituem um A/B de FPS.

O sampler interno atribuiu mais da metade das amostras a `func_004C7BE8`,
uma função vazia. Essa atribuição foi descartada: `samp_lookup` usa intervalos
entre entradas PPU/SPU, mas as funções nativas do runtime/CRT continuam fora
dessa tabela. Lacunas e stubs fundidos pelo linker tornam o nome guest
enganoso. Incluir SPU na tabela não resolve as lacunas do runtime.

Um coletor externo, restrito à HighGraphics do processo de diagnóstico,
registrou RIP bruto sem interpretar nomes guest. A resolução offline usa
todos os símbolos CODE do mapa exato, preserva aliases e prefere limites
de função da `.pdata` quando disponíveis. Entre 80,087 e 140,075 s houve
3.851 amostras, zero falhas; 3.000 no executável, das quais 2.878 resolvidas
por início exato da função na `.pdata`. As demais usam intervalos do mapa
completo, menos precisos. DLLs permanecem separadas, sem inventar o chamador.

| Endereço amostrado em HighGraphics | Amostras | Fração de todas as amostras |
| --- | ---: | ---: |
| `barrier_watch_hit` | 614 | 15,94% |
| `vm_write64` (fora do helper acima) | 172 | 4,47% |
| `vmx_vmaddfp` | 161 | 4,18% |
| `vm_write32` (fora do helper acima) | 144 | 3,74% |
| `vmx_vaddfp` | 119 | 3,09% |
| `lk_acquire` | 99 | 2,57% |
| `spu_coh_is_reserved` | 94 | 2,44% |
| `ps3_indirect_call_impl` | 83 | 2,16% |
| `sys_timer_usleep` | 75 | 1,95% |
| `ntdll.dll`, sem atribuição de chamador | 817 | 21,22% |

Essas são amostras diretas e disjuntas do ponto de execução, não tempos
inclusivos de chamadas, percentuais de CPU total ou ganhos possíveis de FPS.
HighGraphics consumiu 58,094 s de CPU em 60,004 s de parede (~96,8% de um
núcleo). A cadência efetiva do coletor foi ~15,6 ms, apesar do `Sleep(2)`;
uma única janela pode ter viés de fase. Repetir com cadência variada antes
de tirar conclusões quantitativas sobre uma alteração. O sampler e os logs
perturbam a execução; não comparar seus FPS aos benchmarks sem sampler.

O alvo prioritário encontrado é `barrier_watch_hit`, em `ppu_loader.cpp`:
verifica diagnósticos `PPU_WVAL`, `PPU_WWATCH` e o watch dinâmico de barreira
em cada store; `vm_write64` chama duas vezes. A função também arma a janela
inline e pode ativar page guard quando solicitado. Próximo candidato:
evitar o caminho diagnóstico completo quando inativo, mantendo sua ativação,
o watch dinâmico e todas as operações de memória/coerência/reservas intactas.
Isso ainda não foi implementado ou comparado em A/B. Helpers vetoriais
aparecem como alvo secundário; preservar semântica VMX/VSCR antes de trocar
operações por SIMD do host. Não remover locks ou coerência por esses números.

`PS3_POLLTOP=10` corroborou chamadas curtas de espera da HighGraphics, mas
é global e seus milissegundos somam atrasos solicitados, não espera real.
Não permite classificar as amostras de `ntdll.dll` como tempo removível.
Na última execução o áudio teve 0–0,4% de blocos silenciosos e zero perdas
nas seis últimas amostras. Evidências e coletor em `work/perf10_20261024`,
logs em `work/diagnostics/perf10_highgraphics*`. Saves originais e binário
entregue preservados, conforme `work/perf10_20261024/verification.json`.



## Atualização: perfil de FIFO vazio, sem novo ganho confirmado (24/10)

Executável final: `E2A237CF91716CFEE755CEF45B43182B36C155D2B7E3C5B7210503A1BCE60611`.
O novo atalho **fica desligado por padrão**. `GCM_FIFO_IDLE=1` é opt-in
experimental; não foi confirmado ganho consistente de FPS ou CPU total.
Apresentação em worker e as otimizações anteriores continuam no padrão.

O perfil separou entrada vazia, trabalho ativo, mudança de GET, parada,
falta de contexto, referências e atalhos. Na parte estável, ~99,87–99,88%
das consultas entraram com GET=PUT. Isso não significa que quase todo o tempo
fosse vazio: as ~2 milhões de consultas vazias por janela de 5 s somaram
cerca de 180–215 ms na investigação, contra ~2,2–2,3 s de trabalho ativo.
A espera pela trava foi ~60–70 ms. O maior custo continua sendo a execução
dos comandos/preparo de desenhos, com HighGraphics e um SPU perto de 100%
de um núcleo cada.

O candidato mantém a trava FIFO e só usa um certificado criado ao concluir
uma passagem completa com GET=PUT, backend consumido e watchdogs atualizados.
Antes de reutilizá-lo confere PUT, GET guest, contexto, ponteiro de contexto,
CALL pendente e endereço drenado/mapeamento IO atual. Sem contexto, desarma;
modo syscall não usa o atalho. PUT inalterado não basta: self-JUMP, semáforos
e comandos alterados no mesmo endereço continuam no caminho completo quando
GET difere de PUT. A publicação de uma referência por chamada permanece
fora do atalho, sob a trava original, inclusive quando a fila já está vazia.
Sem novos sleeps, mudanças de kick, vblank ou corte de comandos.

A seleção exige backend live, `PS3_GCM_NO_TITLE_RECYCLE` presente e worker
FIFO experimental desligado. Engine alternativo, reciclagem do título no
host, modo syscall e diagnósticos RATE/CTXDBG/DRAINDBG/SCANREF/FIFO_SNAP/
RECDBG/GET_EQ_PUT mantêm o caminho completo. O certificado não tenta
substituir os mecanismos de ring recycle de outros títulos.

Na investigação instrumentada, a parede por consulta vazia foi ~0,093 →
0,044 µs. São coletas em builds de investigação diferentes (`75A10A24…`
e `B0D442A9…`) e incluem temporizadores por chamada. Indicam redução do custo
local, sem demonstrar redução da CPU total nem ganho de FPS. A comparação
final abaixo usa o mesmo executável `B0D442A910CB7B5A9A9B17ECFC30B75D5AF03F7FFD753C9F605041FF19B62DCA`,
alternando explicitamente `GCM_FIFO_IDLE=0/1`; ambos os profilers FIFO ficam
desligados. A build final acima muda o padrão para desligado.

Quatro processos de 120 s, ordem desligado/ligado/ligado/desligado, mesmo
save/câmera neutra de Boletaria, RX 7600/12 lógicos, 1080p de saída/720p
interno, FSR/RCAS, alvo 60, FG desligado e apresentação em worker ligada.
Caches anteriores e AVX2 ativos, worker FIFO/SPU direto/cache de intervalos
continuam desligados. `LD_FLUSH_PROF=1`, `LD_PRESENT_STATS=1` e profiler do
frame clock iguais nos dois caminhos, sem sampler ou temporizadores FIFO.
Sem compilação/teste nativo durante essas quatro execuções. Usadas as seis
últimas janelas de FPS/intervalos e três de CPU, após o carregamento.

| Caso, na ordem executada | FPS base | CPU total | Média do p95 por janela | Média do p99 por janela |
| --- | ---: | ---: | ---: | ---: |
| Desligado (`off`) | 54.17 | 40.78% | 32.38 ms | 43.19 ms |
| Ligado (`on`) | 55.27 | 41.31% | 32.71 ms | 42.92 ms |
| Ligado (`on_repeat`) | 51.50 | 41.67% | 35.10 ms | 47.28 ms |
| Desligado (`off_repeat`) | 53.93 | 41.42% | 34.04 ms | 45.70 ms |

Primeiro par: +2,0% de FPS; repetição: **–4,5%**. CPU total ~41% em ambos,
sem queda demonstrada. As médias de p95 não melhoraram consistentemente.
A variação do cenário é maior que o benefício local medido, e não há base
para recomendar o atalho ao jogar. Quantis são médias de seis janelas de
submissões, não percentis globais ou medições de input lag/scanout.

O diagnóstico permanece disponível: `GCM_FIFO_IDLE_PROF=1` conta consultas
sem ativar temporizadores por chamada; `GCM_FIFO_IDLE_TIMING=1` junto com ele
mede parede/espera pela trava e perturba o polling. Ambos desligados por
padrão. O JSON do diagnóstico passou a incluir `[fifo-idle]`.

Teste nativo diferencial: certificado real contra um consumidor que sempre
percorre um fluxo modelado, 250 mil passos, 33.104 referências publicadas e
564 atalhos. Compara estado após cada consulta: referências pendentes ao
esvaziar e sua ordem, GET guest alterado, mapeamento/endereço drenado,
contexto/CALL, ausência de contexto, modo syscall/reset, self-JUMP e semáforo
liberados sem publicar outro PUT. Treze seleções separadas cobrem padrão
desligado e fallbacks. O teste não valida o decoder RSX ou concorrência GPU;
as capturas e integração cobrem o uso no jogo. Build e reprodução integral
do patch no SDK base passaram. Capturas dos quatro A/B preservam câmera,
geometria, materiais, sombras e HUD. Nos quatro A/B, áudio ativo e perdas
zero nas últimas amostras, sem erros D3D registrados; hashes dos 14 saves
originais preservados.

Integração final: dois processos de 120 s com o executável final e debug
D3D12, um no padrão desligado/sem FG e outro com o atalho opt-in/FG. Ambos
retornaram à gameplay. Nas três últimas amostras, áudio do padrão em
187,6/186,8/187,6 blocos/s, silêncio 0/0,1/0% e perdas zero; no caso opt-in/FG,
187,4/187,6/187,4, silêncio/perdas zero. Sem erros/corrupção D3D12; avisos
820/679 anteriores permanecem. `ds_advertise`/`ds_title` foram interrompidos
pelo autoavanço após 14/31 frames no padrão e 12/28 com FG: somente transições
curtas, sem validar os filmes completos. Essas duas execuções com vídeos,
debug e configurações diferentes não integram o A/B de desempenho acima.

Evidências: `work/diagnostics/perf9_*`, `work/perf9_20261024/measurements.json`,
scripts/binários/mapas e `comparison.png`. O próximo alvo fundamentado é o
trabalho ativo em HighGraphics/preparo dos desenhos, que ocupa muito mais
tempo que as consultas vazias.

## Atualização: apresentação normal em worker (24/10)

Executável atual: `C9601CA308176F55418C4E5ADBD57B450A82A3FA9E91185FEB571C93A14393A0`.
O padrão passa a ser `LD_PRESENT_WORKER=1`: Present e seu limitador executam
no worker também com framegen desligado. `LD_PRESENT_WORKER=0` conserva o
caminho serial para comparação no mesmo executável. `LD_ASYNC_PRESENT=1`
continua controlando allocators e rings entre frames; é um mecanismo separado.
FSR/RCAS, resolução, sombras, desenhos e conteúdo da geometria ficam iguais.
FG continua experimental e desligado por padrão; seu worker usa o controle
já existente `LD_FRAMEGEN_ASYNC`.

A coleta inicial separou parede da apresentação guest em lock, preparo,
espera pelo worker, cópia/comandos, flush, writeback, overlay, limitador,
DXGI e cauda. Na parte estável, a trava praticamente não teve contenção.
O custo dominante fora dos flushes foi a chamada DXGI (~750–1.000 ms por
janela de 5 s), seguida do limitador (~480–660 ms). Os timestamps GPU do
FSR/RCAS indicaram ~0,34 ms por imagem. Isso não autoriza tratar todo o
tempo de parede como CPU removível: inclui esperas e trabalho no driver.
As duas coletas exploratórias são instrumentadas e não demonstram por si
o ganho final de FPS.

O worker admite um único trabalho pendente. O produtor drena o anterior
antes de escolher/reutilizar outro backbuffer; depois de gravar a cópia e
submeter seus comandos, publica o novo trabalho e pode preparar a próxima
cena offscreen enquanto o worker limita/apresenta. O worker não grava na
command list/rings do renderizador. Vídeos e encerramento drenam antes da
transição; overlay e falha ao criar o worker conservam o caminho serial.
O relógio do worker é próprio: apresentação normal não inicializa os recursos
de interpolação nem depende do relógio FG. O handoff mantém eventos e bombeia
mensagens síncronas do HWND enquanto o produtor espera, evitando deadlock DXGI.
Não foi medida latência de entrada nem tempo de scanout.

A/B final: quatro processos de 120 s em ordem serial/worker/worker/serial,
mesmo executável atual, save/câmera neutra de Boletaria, RX 7600 e 12 lógicos,
saída 1080p/720p interno, FSR/RCAS, alvo 60, FG desligado, FIFO síncrono em
lotes. AVX2 e caches anteriores ativos; cache de intervalos/SPU direto/sampler
continuam desligados. Sem compilação ou teste nativo durante esses quatro
processos. `LD_FLUSH_PROF=1` e `LD_PRESENT_STATS=1` iguais nos dois caminhos;
profiler detalhado de apresentação e timestamps GPU desligados. Últimas seis
janelas de FPS/intervalos e três de CPU, depois do carregamento.

| Caso, na ordem executada | FPS base | CPU total | Média do p95 por janela | Média do p99 por janela |
| --- | ---: | ---: | ---: | ---: |
| Serial (`off`) | 49.93 | 40.56% | 37.52 ms | 44.93 ms |
| Worker (`on`) | 53.97 | 41.64% | 33.65 ms | 42.19 ms |
| Worker (`on_repeat`) | 54.70 | 40.53% | 32.86 ms | 41.15 ms |
| Serial (`off_repeat`) | 47.42 | 40.28% | 38.75 ms | 48.21 ms |

Os pares resultaram em **+8,1% e +15,4% de FPS** e cerca de **10,3% e 15,2%
menos p95 médio por janela**. A referência serial variou de 47,42 a 49,93 FPS;
o worker ficou em 53,97–54,70. Essa variação impede atribuir um ganho universal
de 15% ao recurso. A CPU total permanece em aproximadamente 40–42%; a mudança
sobrepõe trabalho entre threads, sem demonstrar redução da utilização total.
HighGraphics e um SPU continuam próximos de ocupar um núcleo inteiro. A
estimativa de CPU agregada por frame foi 97,46 → 92,59 ms no primeiro par e
101,93 → 88,91 ms no segundo; não é o tempo de uma thread ou um frame na GPU.
O resultado é de câmera parada nessa cena, sem garantia de 60 FPS em combate,
outros mapas ou CPUs.

`LD_PRESENT_STATS=1` usa o ring QPC existente e publica p50/p95/p99/máximo
por janela de cerca de 5 s, sem alocação ou consulta adicional de relógio por
frame. As quantidades da tabela são **médias de quantis de seis janelas**,
não percentis calculados sobre todos os frames agrupados. A medida usa
submissões bem-sucedidas a Present, sem provar o instante visto no monitor;
com FG registra somente o primeiro Present do par. O coletor é desligado por
padrão. `LD_PRESENT_PROF=1` mede parede e CPU da thread chamadora, incluindo
a espera pelo worker, mas não inclui sua CPU. Também desligado por padrão.

Teste nativo com D3D12/debug: publicação sem esperar pelo limitador, drenagem
antes do próximo uso do swapchain, apresentação sem relógio/histórico FG,
independência de `LD_FRAMEGEN_ASYNC=0`, mais 64 pares FG, pixels reais,
modo 2 e reinício. Sem erros/corrupção D3D12. O coletor de intervalos passou
em seis janelas contra ordenação independente: cadência constante, travadas,
wrap de 16.384 entradas e reinício. Build e reprodução integral do patch no
SDK base passaram. Capturas dos quatro A/B preservam câmera, geometria,
materiais, sombras e HUD. Áudio ativo, perdas zero nas últimas amostras dos
quatro A/B; saves originais preservados pelos hashes dos 14 arquivos.

Integração no mesmo executável/default: dois processos de 110 s com vídeos,
debug D3D12 e profilers detalhados, um a 30 FPS sem FG e outro a 60 com FG.
Ambos retornaram à gameplay. O caso 30 terminou com três janelas em 30,0 FPS
(arredondado; intervalos QPC ~29,97), confirmando o limitador no worker. Áudio
nas três últimas amostras: 186,8/187,4/187,6 blocos/s, silêncio 0,1/0/0% e
perdas zero. Com FG: 187,4/187,4/187,6, silêncio/perdas zero. Sem erros ou
corrupção D3D12; avisos anteriores 820/679 e, com FG, 695 permanecem. Os vídeos
`ds_advertise`/`ds_title` foram interrompidos pelo autoavanço após 0/34 frames
no caso 30 e 9/26 com FG: valida transições curtas, sem validar os filmes
completos. O caso FG teve média base de 31,48 FPS nas seis últimas janelas;
não integra o A/B, não demonstra ganho do framegen e mantém seu custo conhecido.

Evidências: `work/diagnostics/perf8_*`, binários/mapas/scripts/medições e
capturas em `work/perf8_20261024`, incluindo `measurements.json` e `comparison.png`.

## Atualização: varredura min/max dos índices com AVX2 (24/10)

Executável atual: `736FB5A7640F2B8CE047E0DFE714BC9F4063128066E80C87AD1032B639590DD4`.
Padrão: `LD_INDEX_RANGE_SIMD=1`, cache de intervalos experimental desligado
(`LD_INDEX_RANGE_CACHE=0`). As otimizações anteriores continuam ativas, com
worker FIFO e escritas SPU diretas desligados.

O preparo da chave de geometria percorria os índices BE16/BE32 para achar
min/max em cada desenho, mesmo quando a malha já estava no cache. A varredura
escalar gerada usava comparações/movimentos condicionais por índice. O novo
caminho processa 16 índices de 16 bits ou 8 de 32 bits por bloco AVX2, com
troca de bytes e mínimo/máximo unsigned. Índices de primitive restart não
entram no resultado. Blocos inteiramente de restart mantêm o sentinela vazio;
0xFFFF continua sendo um índice válido de 16 bits quando não é restart. A
cauda usa o código escalar sem ler além do buffer. Builds sem AVX2 conservam
a varredura escalar; `LD_INDEX_RANGE_SIMD=0` permite A/B no mesmo executável.
Todos os índices são lidos. Hashes completos, base index, validações de faixa,
payloads de vértices e identidade da geometria continuam iguais.

Foi testado primeiro um cache exato de min/max, com endereço/quantidade/largura/
restart e comparação de todos os bytes contra um snapshot. Na investigação
instrumentada, acertou 74,05% das consultas e reteve cerca de 1,3 MiB, mas o
custo médio por consulta foi 0,223 → 0,257 µs, incluindo temporizadores. A
busca, alocação/cópia nos misses e descarte superaram a economia de decodificação.
Esse experimento fica desligado por padrão. Tem orçamento de 8 MiB de snapshots,
além das tabelas fixas, e fallback para varredura se não couber/alocar. As duas
coletas instrumentadas usam o binário de investigação `58E4B87D…`; suas médias
de FPS não demonstram ganho do cache nem participam do A/B final de AVX2.

`LD_INDEX_RANGE_PROF=1` junto com `LD_FLUSH_PROF=1` mede min/max por janela;
desligar os temporizadores por consulta na comparação final de FPS. O contador
`fetch` inclui a preparação completa da geometria, e não só a varredura.

A/B final: quatro processos de 120 s, mesmo executável atual e cópia do save
pesado de Boletaria, câmera neutra, RX 7600/12 processadores lógicos, saída
1080p/720p interno, FSR/RCAS, alvo 60, sem FG, sampler ou temporizadores por
consulta. `LD_FLUSH_PROF=1` igual em todos, cache de intervalos desligado.
Últimas seis janelas de FPS/fetch e três de CPU. As duas primeiras passagens
exploratórias foram excluídas: a primeira coincidiu com um teste nativo.
Os quatro processos posteriores abaixo não tiveram compilação/testes paralelos.

| Caso, na ordem executada | FPS base | CPU total | Fetch/preparo por draw |
| --- | ---: | ---: | ---: |
| AVX2 (`simd_repeat`) | 50,58 | 40,08% | 1,967 µs |
| Escalar (`scalar_repeat`) | 49,47 | 40,53% | 3,043 µs |
| Escalar (`scalar_confirm`) | 49,48 | 40,31% | 3,026 µs |
| AVX2 (`simd_confirm`) | 50,58 | 41,00% | 1,921 µs |

Os pares mostram **+2,26% e +2,22% de FPS**, com aproximadamente **36% menos
custo de fetch/preparo por desenho**. Isso não equivale a 36% menos CPU total:
a utilização continua em torno de 40–41%, e SPURS/HighGraphics/apresentação
continuam limitando a cena. São duas comparações numa câmera parada; não
garantem o mesmo ganho em combate, outros mapas, CPUs ou nos piores frametimes.

Teste diferencial nativo: 2.048 conjuntos e 783.234 mutações de bytes, comparando
escalar, AVX2 e cache com um oráculo independente que decodifica e ordena os
índices. Inclui desalinhamento, extremos unsigned, largura/quantidade/restart
alterados no mesmo endereço, descarte e orçamento de memória. No Windows,
comprimentos 0..257 terminam numa página protegida, verificando que vetores e
caudas não leem além do span. Também há build do teste sem AVX2. O patch
exportado aplica sobre o SDK base e reproduz todos os fontes alterados.

A rodada final do teste passou em três seleções: build AVX2 no padrão,
o mesmo teste com seleção escalar e build sem AVX2. Capturas dos quatro casos
finais preservam cena, geometria, materiais, sombras e HUD. Nos quatro A/B,
áudio ativo e zero perdas nas últimas amostras, sem erros D3D registrados.
Os hashes dos 14 arquivos de save originais permaneceram iguais.

Integração de 100 s com AVX2, FG/worker e debug D3D12: voltou à gameplay,
áudio ativo, silêncio 0/0/0,1% e perdas zero nas três últimas amostras. Sem
erros/corrupção D3D12; avisos 820/679 permanecem. Os vídeos foram encerrados
pelo autoavanço após 8/28 frames: valida as transições curtas, sem validar os
filmes completos. Esse teste não participa do benchmark de FPS da fase.

A coleta adicional com temporizadores no build atual registrou aproximadamente
0,041 µs por consulta de min/max (~43 ms por janela de 5 s). A investigação
escalar inicial registrara 0,223 µs em outro build; essa comparação inclui a
separação do caminho de cache e o custo dos temporizadores. A atribuição do
ganho de FPS e do fetch acima usa somente o A/B no mesmo executável, sem
temporizadores por consulta.

Evidências em `work/diagnostics/perf7_*` e `work/perf7_20261024`, com fontes de
referência, binários/mapas, scripts, capturas e `measurements.json`.

## Etapa anterior: perfil SPURS e experimento de escritas de canais (24/10)

Executável desta etapa: `C0107F59CF83A55A15927451206712DF02B8D52920C56B4D614F2C27DC3FF122`.
O experimento `SPU_WRCH_DIRECT` fica **desligado por padrão**. Os ganhos das
etapas anteriores (cache de análise de shaders, FIFO em lotes, XXH3 e regcache
SPU) continuam ativos; o worker FIFO continua desligado.

Corrigida a duplicação do histograma RVA do sampler: em modo de thread única,
o unwind podia registrar o helper nativo e a mesma amostra registrava o RIP
novamente. `PS3_SAMPLE_FUNCTIONS=1` agrega pelo início da função antes do corte
de 25 entradas. Isso permite enxergar o custo de helpers grandes sem dispersá-lo
em vários buckets de instruções. `scripts/profile_symbols.py` resolve o mapa do
executável exato e mantém o denominador de amostras, inclusive as não exibidas.

No perfil por função de `spu1000_0`, quatro relatórios posteriores a 60 s somam
762 amostras: escritas completas de canal 11,3%, trampolim `spu_drain_call`
7,7%, rotina LS 0x4694 da imagem `f5cc5c43…` 7,1%, transferência MFC 4,9%,
aquisição de locks 4,3% e espera de canal 3,3%. A imagem está embutida no EBOOT.
São **amostras de tempo de parede dessa thread**, com corte dos 25 símbolos
mais frequentes por relatório; não são percentuais da CPU total nem promessa
de FPS. O sampler suspende a thread e alterou cadência/áudio, portanto esses
processos foram excluídos das medições de desempenho e integração de áudio.
O perfil não sustenta tratar todo o núcleo ocupado como uma única espera
ociosa que se possa remover.

Foi implementado um caminho pequeno para WrEventAck/Mask/SRR0 e parâmetros MFC.
Evita o frame grande da função que também trata comandos, buffers de diagnóstico
e atômicos DMA. A confirmação de eventos continua usando AND atômico, sem perder
eventos de produtores concorrentes. Comandos, tag update, mailboxes, decrementer
e canais desconhecidos continuam no runtime completo. Qualquer presença de
`SPU_CHHIST` força o caminho completo para preservar a instrumentação.

Quatro processos de 100 s, mesmo binário experimental
`FFC6E23B0A09C783796045430F9E9EAA92CC1516F17D772ECD4BEF1CDDFBB7CB`,
save/câmera neutra de Boletaria, 1080p/720p interno, FSR/RCAS, alvo 60, sem FG,
FIFO síncrono em lotes e sem sampler. Últimas seis janelas de FPS e três de CPU:

| Caso, na ordem executada | FPS base | CPU total, 12 lógicos | CPU agregada por quadro |
| --- | ---: | ---: | ---: |
| Escritas diretas desligadas | 50,88 | 39,94% | 94,20 ms |
| Escritas diretas ligadas | 50,10 | 39,67% | 95,01 ms |
| Ligadas, repetição | 48,87 | 40,42% | 99,25 ms |
| Desligadas, repetição | 49,67 | 40,83% | 98,66 ms |

Os dois pares ficaram cerca de **1,5–1,6% piores** com o experimento. A pequena
redução do uso total de CPU não se converteu em melhor eficiência por quadro.
Não foi demonstrado ganho consistente; por isso o build final exige
`SPU_WRCH_DIRECT=1` para experimentá-lo. O binário final difere pelo padrão
desligado; os quatro casos acima usam a seleção explícita no mesmo binário.

Build, reprodução do patch sobre o SDK base e teste nativo passaram. O teste
exercita inicialização concorrente, padrão desligado, bloqueio pelo diagnóstico,
fallback sem efeitos e 100 mil disputas entre ACK de LR e outro evento produtor,
em cada uma de quatro seleções. Capturas dos quatro casos preservam geometria,
materiais e HUD. Áudio ativo, zero perdas nas últimas amostras e nenhum erro
D3D registrado nos quatro A/B. Saves originais preservados por hashes.

O build final com o experimento ausente do ambiente terminou em 49,27 FPS
nas últimas seis janelas. Houve também uma execução do binário anterior:
suas seis últimas janelas ainda incluíram carregamento, por isso sua média
48,50 não serve para atribuir ganho ao build novo. A/B válido do recurso é
o dos quatro processos controlados acima.

Integração adicional de 100 s no build final, com escritas diretas opt-in,
FG/worker e debug D3D12: voltou à gameplay, áudio ativo, 0% de blocos silenciosos
e zero perdas nas três últimas amostras. Sem erros/corrupção D3D12; avisos
820/679/695 permanecem. Os dois vídeos iniciaram e o autoavanço os encerrou
após 8/26 frames: transições curtas, sem validação de filmes completos.
Essa execução não é benchmark da fase e não resolve o custo do framegen.

Evidências: `work/diagnostics/perf6_*` e `work/perf6_20261023`, incluindo
`functions.json`, mapas, capturas, scripts e `measurements.json`. Próximo alvo
de CPU: medir a varredura completa de índices e as conversões dentro de
`geom_key`/fetch. Mesmo um acerto do cache de geometria ainda percorre os índices
para obter min/max; qualquer novo atalho precisa validar todos os bytes atuais.

## Etapa anterior: cache da análise de shaders e preparo dos pipelines

Executável desta etapa: `95C4452D48F7E5245326B4A837D363355BE97B08859AEFF2D81BBDD45843A60A`.
O cache é ligado por padrão; `LD_SHADER_META=0` restaura a análise anterior.
FIFO permanece em lotes na mesma thread (`LD_FIFO_BATCH=1`, `LD_FIFO_ASYNC=0`).

Quatro processos de 100 s, mesmo binário e cópia de save de Boletaria, câmera
neutra, RX 7600/12 processadores lógicos, saída 1080p/720p interno, FSR/RCAS,
alvo 60 e FG desligado. Cache de shaders compartilhado, sem sampler nem
`LD_FIFO_PROF`; `LD_FLUSH_PROF=1` igual em todos. Valores das últimas seis
janelas de FPS/preparo e três de CPU. O custo de PSO abaixo é o tempo de
preparação por desenho, incluindo a consulta ao cache, com pipelines aquecidos.

| Caso, na ordem executada | FPS base | CPU total | CPU agregada por quadro | Preparo PSO/draw |
| --- | ---: | ---: | ---: | ---: |
| Análise anterior | 46,18 | 41,28% | 107,25 ms | 3,873 µs |
| Cache ligado | 49,17 | 40,33% | 98,44 ms | 1,128 µs |
| Cache ligado, repetição | 48,73 | 41,08% | 101,16 ms | 1,180 µs |
| Análise anterior, repetição | 46,38 | 40,36% | 104,42 ms | 3,885 µs |

Os pares mostram **+6,5% e +5,1% de FPS** nesta cena e redução de aproximadamente
**70% no custo de preparação de PSO por desenho**. Isso não representa 70%
menos CPU total: a utilização agregada permanece perto de 41%, com mais quadros
por segundo. `spu1000_0` continua ocupando aproximadamente um núcleo, enquanto
HighGraphics/frame_clock continuam importantes. Não mede 1% low nem estabelece
o mesmo ganho em combate, mapas diferentes ou cache frio.

O caminho antigo analisava as entradas VP, percorria VP/FP, coletava máscaras
de textura/gamma e recalculava hashes antes de consultar o cache PSO, mesmo
quando o pipeline já existia. O novo cache guarda essas análises e os offsets
das constantes. As instruções VP são comparadas integralmente; no FP, cada
byte de instrução é conferido, pulando somente os payloads CONST identificados.
O endereço e o hash servem para encontrar candidatos, sem dispensar a comparação.
O mapeamento do FP é validado pelo comprimento completo antes da leitura.

Constantes são relidas/saneadas em cada draw; ao diminuir sua contagem, a cauda
anterior é zerada. Isso evita limpar todos os 32 KiB do bloco a cada desenho.
Os hashes originais dos pipelines são preservados, incluindo a identidade por
payload no modo literal. Estado de renderização, escalas de sombras e dimensões
de recursos continuam atuais. A decodificação das 16 texturas ocorre uma vez por
preparo e atende às análises de cubemap, UNNORM, gamma e sombras desse desenho.

Snapshots/offsets têm orçamento compartilhado de 8 MiB, além das tabelas fixas;
a cena testada reteve 118.236 bytes (~115,5 KiB), com 34 misses VP e 67 FP na
primeira execução ligada. Falhas de alocação/orçamento e layouts branch/CONST
ambíguos mantêm o caminho original. O cache é liberado no shutdown.

Build e teste diferencial nativo passaram: 3.000 programas, 3.127.008 mutações
de bytes de instrução, constantes/cauda, contadores de valores não finitos,
texturas/gamma/TXL/entradas VP, hashes, limites de mapeamento e orçamento real.
O patch exportado aplica sobre o SDK base e reproduz os fontes. Capturas dos
quatro casos mantêm a cena e a câmera; áudio ativo e zero perdas nas últimas
amostras, sem erros D3D registrados. Saves originais preservados por hashes.

Integração adicional de 100 s com FG/worker de apresentação e debug D3D12:
voltou à gameplay, com áudio ativo, 0% de blocos silenciosos e zero perdas
nas três últimas amostras. Nenhum erro/corrupção D3D12; avisos 820/679/695
continuam presentes. O avanço automático cancelou `ds_advertise` antes do
primeiro frame e `ds_title` após 23 frames: valida a transição curta, sem
validar os vídeos completos. Esse teste tem debug, carregamento e FG, portanto
não integra a comparação de FPS acima. FG continua experimental e desligado
por padrão; esta etapa não resolve seu custo de apresentação.

Evidências: `work/diagnostics/perf5_*`, capturas/scripts e `measurements.json`
em `work/perf5_20261023`. O binário medido é idêntico a `build/MyGameRecomp.exe`.

## Etapa anterior: ingresso do FIFO em lotes

Executável desta etapa: `1F16E030B6AB2A4E9E1FC0D035414364F26B5AE141AA96D57F3B582518C5C90A`.
Padrão: `LD_FIFO_BATCH=1`, worker experimental **desligado** (`LD_FIFO_ASYNC=0`).

Mesma cena pesada de Boletaria, cópia de save e câmera neutra, saída 1080p/
720p interno, FSR/RCAS, alvo 60, sem FG, sem sampler e sem `LD_FIFO_PROF`.
Cinco processos de 100 s no mesmo binário; últimas seis janelas de FPS e três
de CPU. XXH3 e cache SPU permanecem ligados em todos os casos.

| Caso, na ordem executada | FPS base | CPU total, 12 lógicos | CPU agregada por quadro |
| --- | ---: | ---: | ---: |
| Ingresso anterior (`LD_FIFO_BATCH=0`) | 43,25 | 40,11% | 111,29 ms |
| Pacotes em lotes, mesma thread | 47,55 | 40,06% | 101,09 ms |
| Lotes + worker experimental | 28,93 | 36,08% | 149,65 ms |
| Lotes na mesma thread, repetição | 46,87 | 40,31% | 103,20 ms |
| Ingresso anterior, repetição | 42,40 | 40,08% | 113,44 ms |

Os pares mostram **+9,9% e +10,5% de FPS** nesta cena. A utilização total da
CPU fica perto de 40%; mais quadros são produzidos com custo agregado por
quadro menor. Duas rodadas não garantem esse ganho em todos os mapas, CPUs
ou piores tempos de quadro.

O caminho anterior resolvia/lia o mesmo argumento várias vezes, verificava
opções de diagnóstico por palavra e tomava a trava do renderizador em cada
escrita de registrador. Agora, pacotes NV4097 elegíveis leem os argumentos
uma vez, preservam incremento/NI e entram no renderizador sob uma trava por
pacote. A sequência entregue ao dispatcher é a mesma. Relatórios, labels,
referências, subcanais/transferências e comandos de controle continuam no
caminho ordenado. Diagnósticos que precisam observar cada palavra mantêm o
ingresso anterior. `LD_FIFO_BATCH=0` serve para A/B.

Também foi implementada e testada uma **fila limitada de comandos copiados**
com três blocos de 2.048 métodos, backpressure, drenagem e fallback síncrono.
Ela move o preparo e gravação D3D12 para `rsx_render`, mas ainda depende da
memória de vértices/texturas do jogo. Por isso drena antes de publicar get/ref,
labels, relatórios, transferências e flips. Os buffers não podem ser liberados
cedo somente para melhorar o benchmark.

**Essa separação por pacotes piorou o FPS e não foi habilitada por padrão.**
A primeira execução instrumentada ficou em 28,60 FPS; depois de remover
instrumentação/drenagens vazias do caminho quente, 28,93. A medição inicial
registrou aproximadamente 95–99 mil blocos por janela de 5 s, muitos parciais:
os handoffs e as esperas frequentes superaram a sobreposição. Os tempos de
execução da instrumentação são tempos de parede, incluem esperas e não são
CPU isolada.

Isso não equivale ao `RF_ASYNC` por quadros do inFAMOUS. Para atravessar essas
fronteiras mantendo a semântica, o próximo passo estrutural exige snapshots
próprios de vértices/índices, constantes, programas e recursos por quadro,
com vida governada por fences e replay separado. A fila atual preserva as
barreiras existentes e não demonstra ganho dessa arquitetura completa.

Build e teste nativo passaram: 265.833 comandos, conteúdo/ordem, alteração do
scratch após envio, pressão na fila, observação concorrente, entrada recursiva,
drenagem, shutdown e falha de criação da thread. O teste usa a fila real com
consumidor simulado; a integração gráfica é uma verificação adicional. O patch
aplicou sobre o SDK base e reproduziu todos os fontes, incluindo ambos os novos
headers. Capturas conferidas preservam câmera, geometria e HUD, com diferenças
naturais de animação. Nos cinco benchmarks não houve erros D3D registrados;
áudio ativo e zero perdas nas últimas amostras.

Logs `work/diagnostics/perf4_*`, scripts/capturas/medidas em
`work/perf4_20261023`. Os testes usam cópias de save; o monitor de hashes dos
originais permaneceu igual. Controles e teste nativo em
[ENVIRONMENT.md](ENVIRONMENT.md) e [AUTOTEST.md](AUTOTEST.md).

Integração adicional: dois processos de 100 s com vídeos, FG e debug D3D12,
um com lotes síncronos e outro com worker. Ambos iniciaram `ds_advertise.pam`
e `ds_title.pam`, retornaram à fase e mantiveram áudio ativo/zero perdas nas
últimas três amostras. O avanço automático interrompeu os vídeos após 13–28
frames; não valida assistir às cenas completas. Sem erros/corrupção D3D12;
avisos 820 (clear otimizado) e 679 (saídas sem RTV) permaneceram. As médias
desses testes incluem logos/carregamento e não entram na comparação de FPS.

## Etapa anterior: XXH3 e registradores SPU

Executável dessa etapa: `02B23A5AE8FBDBD52C7FC3B8B8388BBE8D6A63D5E77731E831F6AF0867F2D685`.
Adaptações da referência [inFAMOUSreborn](INFAMOUS_PERFORMANCE_REVIEW.md), com
os helpers SPU já existentes no DeS. As duas otimizações vêm ligadas.

Mesma cópia de save de Boletaria, câmera neutra, RX 7600, 12 processadores
lógicos, saída 1080p/720p interno, FSR/RCAS, alvo 60, sem framegen e sem
sampler. Seis processos de 100 s, **o mesmo executável**; seis últimas janelas
de FPS e três de CPU por caso. O cache de shaders é compartilhado. Não houve
redução de qualidade ou supressão de desenhos.

| Caso, na ordem executada | FPS base | CPU total, 12 lógicos | CPU agregada por quadro |
| --- | ---: | ---: | ---: |
| Originais, primeira execução | 27,93 | 40,06% | 172,08 ms |
| Só XXH3 | 44,43 | 40,50% | 109,38 ms |
| Só cache SPU | 36,97 | 40,00% | 129,85 ms |
| XXH3 + cache SPU | 44,48 | 41,39% | 111,65 ms |
| XXH3 + cache SPU, repetição | 42,45 | 39,64% | 112,05 ms |
| Originais, repetição | 35,17 | 39,78% | 135,73 ms |

A comparação entre as repetições dá **35,17 → 42,45 FPS (+20,7%)** nesta
cena. A primeira referência teve oscilações maiores (21,6–33,6 FPS nas últimas
janelas); fica registrada, mas não é usada para anunciar o percentual maior.
Há apenas duas rodadas dos extremos, com atividade normal do Windows:
não é garantia de ganho em todos os mapas ou de tempos mínimos de quadro.

O hash apresentou o efeito mais claro: só XXH3 e os dois recursos juntos
ficaram praticamente iguais na primeira rodada. **Não há evidência de ganho
isolado consistente do cache SPU nessa cena.** O job SPURS principal ainda
ocupa aproximadamente um núcleo. CPU total permanece perto de 40%; na
comparação das repetições, a estimativa de CPU somada entre threads por quadro
cai de 135,73 para 112,05 ms (~17,4%). São amostras agregadas, não uma medição
de latência de entrada. A categoria `fetch` cai de 4,451 para 3,937 µs/draw
entre as repetições; ela inclui mais trabalho que somente o hash.

- `rsx_geom_hash.h`: XXH3-128 nas regiões de pelo menos 64 bytes; metadados
  curtos mantêm o misturador anterior. Lê todos os bytes, preserva as duas
  partes da chave e todas as entradas de `geom_key`. `LD_GEOM_XXH3=0` compara
  o caminho antigo; não há reutilização de vértices baseada em amostragem.
- `scripts/spu_regcache.py`: versões paralelas com GPRs locais e análise de
  registradores alterados/vivos. Publica o estado antes de chamadas/canais/
  retornos e recarrega os valores necessários depois. Cobre escritas parciais,
  escapes de ponteiros e desvios; rejeita fluxos que ainda não suporta.
  Mantém os helpers e flags de compilação existentes. O registro seleciona e
  memoriza a versão por função, mantendo as regras de imagem/overlay.
  `SPU_REGCACHE=0` escolhe originais; seleção fixa no início do processo.
  `PORT_SPU_REGCACHE=OFF` omite os twins do build. Diagnósticos de observação
  de LS/contexto selecionam originais.

Passaram os testes nativos de cobertura do hash, seleção SPU com oito threads
e equivalência de GPRs/LS/callbacks em 300 entradas, incluindo laços, retornos
condicionais e tailcalls. O teste do gerador verifica idempotência e fonte
original intacto. O patch aplica ao SDK base e reproduz todos os fontes,
incluindo os novos headers e a licença do xxHash. Créditos MIT do gerador em
`scripts/INFAMOUS_REGCACHE_LICENSE.txt`.

As capturas conferidas preservam o enquadramento, geometria e HUD; há diferenças
naturais de animação. Nos seis casos, as últimas amostras mantiveram áudio
ativo, zero frames de áudio perdidos e nenhum erro D3D registrado. Os testes
usam cópias do save. Logs em `work/diagnostics/perf3_*`; executável, scripts,
capturas e consolidação em `work/perf3_20261023`. Procedimentos em
[AUTOTEST.md](AUTOTEST.md).

Com ambos ligados e framegen assíncrono, outro teste de 100 s registrou
**37,83 FPS base**, CPU total 39,36%, áudio ativo e zero perdas nas últimas
amostras. A geração continua tendo custo frente aos 42–44 FPS sem ela;
permanece desligada por padrão. Quadros gerados submetidos não equivalem
automaticamente a quadros adicionais exibidos no monitor.

Integração `-Movies`, também 100 s, com FG e depuração D3D12: iniciou
`ds_advertise.pam` e `ds_title.pam` (o avanço automático interrompeu após
8 e 26 frames), voltou ao carregamento/fase e manteve áudio ativo com zero
perdas nas últimas amostras. Não valida assistir às cenas completas. Não houve
mensagem de erro/corrupção D3D12; apareceram avisos 820 (clear diferente do
valor otimizado), 679 (saídas de pixel shader sem RTV correspondente) e 695
(scissor vazio). A média desse teste inclui logos/vídeos/carregamento e não
entra na comparação de FPS da fase.

## Etapa anterior: apresentação separada e preparação de draws

Executável dessa etapa: `3829FF91437D2C86DEC8631E76357C150A071ACFD4063D8F85FE6A292F6125C9`.
Os resultados abaixo substituem a recomendação baseada no caminho de apresentação
serial; as medições anteriores permanecem como histórico nas próximas seções.

Mesma cena pesada de Boletaria, câmera neutra, sem o menu personalizado, saída
1080p, cena interna 720p, FSR/RCAS e alvo 60. Quatro processos separados de
100 s, seis últimas janelas de 5 s por caso; sem sampler durante a medição final.
Capturas confirmaram o mesmo enquadramento, com animações naturais diferentes.

| Caso | FPS base | CPU total, 12 lógicos |
| --- | ---: | ---: |
| Referência, sem FG | 35,43 | 39,08% |
| Atual, sem FG | 36,13 | 39,50% |
| Atual, FG serial (`LD_FRAMEGEN_ASYNC=0`) | 26,85 | 34,58% |
| Atual, FG com worker (padrão ao ligar FG) | 34,37 | 37,19% |

O A/B de FG usa **o mesmo executável**, mudando somente a apresentação:
aproximadamente +28% de FPS base nesta rodada. Interpolação GPU: 0,570 ms
serial e 0,583 ms com worker; FSR/RCAS: 0,354 e 0,339 ms. O ganho não vem de
contar os quadros gerados como quadros do jogo. O consumo de CPU total sobe
porque mais quadros são processados; a estimativa agregada por quadro base
caiu de 154,56 para 129,87 ms de CPU, somando as threads.

Sem FG, +0,70 FPS é uma diferença pequena em uma rodada e **não demonstra
ganho consistente nem redução importante da utilização total de CPU**. Na
thread de desenhos, a categoria de busca/validação de texturas caiu de 1,918
para 1,222 µs/draw (~36%); preparo dos intervalos de vértices de 0,095 para
0,067 µs/draw (~29%). A conversão completa não mostrou redução consistente
no perfil dessa cena: 2,581 contra 2,812 µs/draw. O teste confirma equivalência
dos valores, não uma aceleração universal da conversão.

Mudanças aplicadas:

- **SPURS/CPU:** não varrer os 64 slots de tarefas HLE em cada commit de linha
  enquanto nenhuma tarefa HLE tiver sido registrada. A ativação é permanente,
  ocorre antes da publicação do primeiro slot, e preserva a busca/notificação
  original depois disso. `SPU_TASKSET_SCAN=1` força a varredura para comparar.
  A sondagem de perda de reserva usa a comparação vetorial de 128 bytes que
  já atendia PUTLLC; não elimina verificações de reserva ou despertares.
- **Vértices:** intervalo comum calculado uma vez por draw para atributos sem
  instanciamento; divisores/modulo mantêm os próprios intervalos. Conversão
  SIMD de float4/half4 preserva byte order, NaNs e o tratamento de denormais.
- **Texturas:** dica de índice por hash com conferência do descritor inteiro.
  Colisões, índices antigos e evicções retornam à busca linear. Atualizações
  de conteúdo continuam usando o mesmo hash/cadência e decodificador.
- **Apresentação:** `framegen_present` tem lista, allocator, fence e timer
  próprios. Recebe no máximo um par pendente. O FIFO grava a próxima cena
  enquanto o worker apresenta e atende o prazo do real. Antes de reutilizar
  backbuffers/histórico, vídeo ou encerramento, o produtor drena o par.
  Ainda pode esperar se a fila DXGI/worker estiver atrasada; não há fila
  ilimitada nem garantia de zero espera. Há fallback serial para overlay
  ativo ou falha de criação do worker. `LD_FRAMEGEN_ASYNC=0` compara o legado.

O caminho atende mensagens síncronas enviadas pelo DXGI à thread dona da
janela durante a gravação e a drenagem. Isso segue a necessidade de manter
o message pump disponível durante Present descrita pela
[Microsoft](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi).
As listas independentes compartilham a fila D3D12, com submissão e fences
próprias, conforme o modelo de
[submissão de trabalho](https://learn.microsoft.com/en-us/windows/win32/direct3d12/command-queues-and-command-lists).

Validação: build, patch reproduzido no SDK base, comparação escalar/batch de
todos os 65.536 valores half e dados aleatórios, índices com wrap e instancing;
32 despertares reais da tabela SPU por CV; teste D3D12 de 64 pares com readback
dos pixels do real, modo 2 e reinicialização do worker. A camada D3D12 do teste
nativo não registrou erros/corrupção. Integração de 100 s com vídeos ativos:
troca do anúncio e título (37 frames do título apresentados antes do skip
automático), retorno à fase e áudio ativo. Zero erros/corrupção D3D12 nesse
run; houve avisos de clear otimizado incompatível (id 820), não classificados
como erro. Os quatro benchmarks terminaram com áudio ~187,5 blocos/s, sem
perdas na saída. Savedata sempre em cópias isoladas.

Logs e hashes: `work/diagnostics/perf2_*`. Consolidação reproduzível:
`work/perf2_20261023/{summarize.py,measurements.json,RESULTS.md}`.
Framegen permanece experimental, com atraso e artefatos de correspondência;
esta mudança não acrescenta profundidade ou vetores de movimento do jogo.

## Histórico anterior à separação da apresentação

Hardware consultado: Radeon RX 7600, 12 processadores lógicos; monitor informado
pelo sistema em 1920×1080 a 60 Hz. Os testes usam cópias isoladas dos saves,
sem controle/teclado do jogador, e encerram somente o processo de diagnóstico.

## O que os testes mostraram

**O A/B final reproduziu a piora com framegen na mesma cena pesada.** Com alvo
60 FPS, câmera neutra, overlay inicializado, janela 1080p e cena 720p:

| Configuração final | FPS base, média das últimas seis janelas | Saída na GPU | Interpolação na GPU |
| --- | ---: | ---: | ---: |
| FSR + RCAS, framegen desligado | 36,52 | 0,343 ms | desligada |
| FSR + RCAS, framegen ligado | 26,60 | 0,352 ms | 0,583 ms |

O perfil do frame clock passou de aproximadamente 1,1 s em apresentação por
janela de 5 s para aproximadamente 2,0 s. A categoria inclui cópias, submissões,
sincronização e Present. O custo GPU dos efeitos sozinho não explica a perda:
o caminho extra de apresentação compartilha a thread que consome o FIFO RSX.
Este resultado não mede a latência do controle em milissegundos nem prova que
o monitor é a única causa. Confirma que, neste PC e nesta cena, é melhor
manter a geração desligada para preservar FPS base e resposta.

As quedas de Boletaria continuam ligadas à cadeia de trabalho da CPU: o job
SPURS e a HighGraphics ocupam quase um núcleo cada, além do frame clock e das
outras threads do jogo. FSR reduz pixels e acrescenta passes na GPU; não reduz
o trabalho de emulação/recompilação dos jobs SPU. O tempo abaixo é somente o
dos passes de saída/interpolação, não o tempo de toda a cena na GPU.

Medições iniciais, em cenas/ângulos diferentes do A/B final:

| Configuração | Saída na GPU, média das últimas seis janelas | Interpolação na GPU | FPS base, últimas seis janelas |
| --- | ---: | ---: | ---: |
| 1080p, cena 720p, FSR + RCAS | 0,350 ms | desligada | 55,4–57,4 |
| 1080p, cena 720p, framegen após ajuste de cadência | 0,075 ms | 0,908 ms | 49,6–53,8 |
| 4K, cena 1440p, FSR + RCAS + framegen | 1,248 ms | 2,931 ms | 47,2–51,8 |

No teste inicial, o executável anterior teve médias de 55,27 FPS sem efeitos,
55,70 com FSR e 53,50 com framegen nas seis últimas amostras. A queda forte com
FSR sozinho não foi reproduzida em 1080p. Em 4K, a saída e a interpolação
somam aproximadamente 4,18 ms por quadro, além da cena e do custo de Present.
Essas configurações diferem em resolução e efeitos: não representam ganho de
uma otimização de código.

Estas são medições iniciais de pós-processamento, antes da correção do controle
virtual descrita abaixo; não são um A/B com câmera parada. As primeiras
execuções tinham cache de shaders frio; ficaram fora das médias
acima as primeiras janelas de carregamento. O teste com câmera em movimento
atingiu 33,7–47,6 FPS na referência e 39,6–58,2 no caminho otimizado, mas as
capturas e os draws por quadro mostram ângulos diferentes. **Essa diferença
não pode ser atribuída às otimizações como um percentual de ganho.**

Depois de neutralizar o controle virtual, o A/B na mesma cena pesada, com
overlay inicializado, alvo 120 e efeitos desligados teve **34,43 contra 35,02
FPS** nas seis últimas janelas. Os draws por quadro foram 879,14 e 877,42;
as capturas mostram a mesma câmera. CPU nas três últimas amostras:
501/497/484% contra 491/472/487% de um núcleo (12 threads lógicas). É uma
única rodada, com diferença de cerca de 0,6 FPS: não demonstra um ganho grande
ou estatisticamente consistente. O custo do trecho de PSO caiu de 3,911 para
3,832 µs por draw. O gargalo da cena permanece.

## Correções implementadas

- **Cadência do framegen:** o prazo do quadro real começa depois do Present
  do gerado. O limitador atende esse prazo enquanto espera, antes de reutilizar
  o histórico/backbuffer. Antes, o prazo começava antes da GPU/limitador, e um
  novo quadro podia antecipar a apresentação do real. Em 1080p, os envios
  antecipados caíram de 134–160 para 28–41 por janela de aproximadamente 5 s.
  Ainda pode haver antecipação sob carga: o código prioriza não acumular
  latência indefinidamente.
- **Rastros:** correspondências de movimento incertas usam o quadro atual;
  pixels estáticos/HUD também preservam a cor atual. Evita a média de imagens
  incompatíveis no caso testado de sombra/iluminação. Não elimina todos os
  artefatos de desoclusão: esta implementação não recebe vetores de movimento,
  profundidade nem identificação do HUD do jogo.
- **Busca de movimento:** mantém a busca exaustiva e a ordem de desempate, mas
  rejeita um candidato quando seu erro parcial já não pode vencer o melhor.
  Os vetores coincidiram exatamente nos testes GPU. Em 1080p, o custo observado
  foi 0,877 contra 0,889 ms com a busca completa; a diferença é pequena demais
  para anunciar ganho de FPS.
- **CPU/SPU:** a sequência global de progresso deixa de sofrer uma operação
  atômica em cada escrita DMA/atômica no modo padrão, onde ninguém a consome.
  `progress` e `spinwake` conservam a sequência; não se usa o número de waiters
  como condição, pois isso perderia notificações. No perfil de referência,
  `ps3_progress_bump` apareceu com 5,6% das amostras da thread SPU em uma janela;
  isso não equivale a 5,6% da CPU total nem a um ganho garantido de FPS.
- **Cache de pipelines:** índice por hash substitui a varredura de materiais a
  cada draw, mantendo a comparação da chave inteira e o cache de falhas.
- **Launcher:** `-Scale 1` agora substitui uma escala maior herdada. As caixas
  desmarcadas também limpam FSR, FXAA e framegen herdados. Antes, uma escolha
  visualmente desligada podia manter o efeito ativo no processo filho.
- **Diagnóstico:** timestamps GPU não bloqueantes, contagem de Presents reais
  em vez da estimativa "2× FPS vistos", seleção de save/executável e liberação
  do controle automático quando o script da fase abre por `cellFsSdataOpen`.
- **Controle virtual:** sem dispositivo e com teclado desligado, o estado
  inicial deixava analógicos em zero, causando movimento de câmera sem comando.
  O estado inicial e a desconexão XInput agora centralizam os quatro eixos em
  128 e soltam botões. `PAD_NO_HOST=1` isola os testes dos dispositivos físicos,
  mantendo a injeção de botões/analógicos dos scripts. Por isso os testes
  `perf_fixed_*` anteriores a essa correção não validam uma câmera parada.

## Uso prático e limites

Para priorizar resposta do controle neste monitor de 60 Hz, comece em 1080p,
escala interna 1× e framegen desligado. FSR é opcional; a nitidez pode ser
reduzida se realçar serrilhado. Diminuir a resolução não resolverá sozinho uma
cena limitada pelos jobs SPU/CPU. Também foram observados borrões em capturas
da cena antes de FSR/FG; não foi isolada uma causa única para todos os rastros
relatados pelo jogador.

O recurso de geração é uma implementação experimental por comparação de
imagens, não a integração de FSR 3. Dois Presents enviados não garantem dois
quadros vistos: a fila DXGI e a taxa do monitor influenciam a exibição. O log
agora usa "submitted" para distinguir esses números.

O [FSR 1 é espacial](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-spatial/),
sem histórico/vetores, e pressupõe uma entrada com anti-aliasing. A documentação
de [Present da Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present)
descreve o descarte e o enfileiramento de apresentações no modelo flip.

## Validação e evidências

Build concluído. Teste GPU com o HLSL real: busca coarse/refine em cinco
padrões e fallback de correspondência incerta passaram. O teste nativo do pad
verificou eixos centrados, injeção de botão de menu e retenção pelo overlay.
O patch UTF-8 foi
aplicado sobre o SDK base e reproduziu todos os fontes alterados. Sintaxe dos
scripts PowerShell verificada. Nos testes concluídos, nenhum erro D3D foi
registrado; as seis últimas janelas de cada teste tiveram zero perdas de áudio.

Logs completos, `summary.json` e hash dos executáveis:
`work/diagnostics/perf_*`. Medições consolidadas e capturas:
`work/perf_20261023`. Procedimentos: [AUTOTEST.md](AUTOTEST.md).

Executável validado nessa etapa: `build/MyGameRecomp.exe`, SHA-256
`2678991BFE840377585EF87DC23BFE9DD2C5D3311F4B14E03B5519278E3AE146`.
Os 14 arquivos originais de save mantiveram os hashes. Os encerramentos
programados aparecem como `stoppedAtDeadline=true`/`exitCode=-1` nos resumos.
