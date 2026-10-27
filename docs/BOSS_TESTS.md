# Diagnóstico de bosses — 25–26/10/2026, etapa 18

Atualização posterior: a etapa 20 confirmou trechos de combate com Velho
Herói, Sanguessuga e Flamelurker, que estavam parciais/inconclusivos aqui.
Método, capturas e limites em [BOSS_MAP_FOLLOWUP.md](BOSS_MAP_FOLLOWUP.md).
Os resultados históricos abaixo não foram reclassificados retroativamente.

Pedido: verificar outras lutas após o relato de travadas no Vanguard.
Este documento distingue combate observado, arena carregada e tentativas
que não chegaram ao boss. Uma arena carregada não valida uma luta.

## Método

- Executável da etapa 17, SHA-256
  `1F9C94711691FD3A78D7E1DA6746310E3DC0177E5C7361612C02DF168C032070`.
- 720p, escala interna 1, alvo 60, FSR/FG/FXAA/sharpen desligados;
  sem debug D3D12, GPU validation, sampler ou PSO warmup experimental.
  Present worker ligado, FIFO idle e descriptor cache experimentais desligados.
- Diagnósticos sequenciais, com `scripts/windows/diagnose.ps1`, comandos de
  câmera/deslocamento/ataque e capturas a cada 900 frames. Cada execução tem
  diretório de trabalho e savedata próprios. O prazo encerra somente seu PID;
  código -1 por esse encerramento não é evidência de crash espontâneo.
- Saves artificiais de teste: personagem novo após a primeira entrada em
  Boletaria, posição/mapa alterados apenas nas cópias, HP 30000 e vitalidade
  99 para tolerar os ataques. Não são uma progressão normal completa.
  Equipamento/dano e gatilhos de história continuam limitando o teste.
- Cache inicialmente copiado do jogador e acumulado entre casos, nunca
  apagado entre bosses. Portanto os resultados não são comparações entre
  bosses em condições idênticas nem um A/B de FPS. Instrumentação, capturas,
  AI e trajetórias diferentes também influenciam os tempos.
- 16 pontos principais do arquivo de warps; não inclui o verdadeiro Allant
  final, todas as fases dos encontros nem a variante PvP do Velho Monge.
  As repetições aproximam o spawn das posições dos NPCs no MSB local.

## Evidências

Diretório local `work/perf18_20261025`: sementes isoladas, configurações,
capturas, progresso, análise de eventos e hashes dos saves originais.
Logs brutos em `work/diagnostics/perf18d_*` e repetições `perf18r_*`.
`analysis.json` agrega métricas; a classificação visual é necessária para
interpretá-las. Não usar FPS de um corredor como FPS de combate.

As primeiras tentativas (`perf18_*`, `perf18b_*`, `perf18c_*`) foram usadas
para acertar a preparação. Algumas acionaram a introdução de Boletaria,
usaram personagens com progressão diferente ou ficaram antes da névoa.
Estão excluídas das conclusões sobre combate.

Os offsets do personagem foram conferidos no
[editor de saves de Wulf2k](https://github.com/Wulf2k/DeS-SaveEdit/blob/master/DeS-SaveEdit/DeS.vb);
a leitura das posições MSBD segue a estrutura descrita no
[SoulsFormats](https://github.com/JKAnderson/SoulsFormats/blob/master/SoulsFormats/Formats/MSB/MSBD/PartsParam.cs).
Nenhum desses programas foi executado sobre os saves originais.

## Limites de interpretação

Ausência de crash em um trecho curto não garante ausência de travadas no
restante da luta. Boss/barra/ataques precisam aparecer nas capturas para
confirmar combate; uma barra sozinha não comprova que a AI foi exercitada.
Vitórias, transições, morte/respawn e progressão entre bosses exigem testes
adicionais com saves de progressão normal.

Os eventos `stage=pso` medem preparo completo, incluindo cache, compilação e
criação do objeto; sua soma não é CPU exclusiva. Contagens de compilação são
as dos eventos registrados. Carregamento/boot estão incluídos nesse total.
Os máximos de apresentação também incluem o custo das capturas; não são
medidas limpas das pausas percebidas por um jogador.

## Validade das tentativas

O nome de um diretório identifica o **destino pretendido**. Não basta para
identificar a luta realmente executada. A conferência visual encontrou:

- `perf18r_old_hero` reapareceu no Adjudicador, com barra, modelo e ataques
  desse boss. Não valida o Velho Herói.
- `perf18r_old_monk` terminou na área do Maneater, com esse modelo/barra e
  efeitos. Não valida o Velho Monge nem a sua variante online.
- `perf18r_adjudicator` apresentou queda/geometria vista por fora. Esse
  recorte não serve como teste normal de combate.
- `perf18r_leechmonger` caiu/carregou outra posição; não valida a luta.
- Allant apareceu próximo e selecionável, mas a ativação normal do encontro
  e seus ataques não ficaram comprovados. Phalanx exibiu barra e sala,
  sem modelo/ataques suficientes para validar o encontro.

Essas limitações decorrem do procedimento artificial de reposicionamento;
não demonstram, isoladamente, um defeito de colisão ou progressão durante
uma partida normal. Os casos restantes precisam de saves de progressão normal.

| Destino pretendido | Resultado visual observado |
|---|---|
| Phalanx | Sala/barra; modelo e ataques não comprovados |
| Cavaleiro da Torre | Boss próximo, barra e ataques; combate observado |
| Penetrador | Boss próximo, barra e atividade; combate observado |
| Falso Rei Allant | Modelo selecionável; ativação e ataques não comprovados |
| Adjudicador | Combate observado no recorte que reapareceu aqui, `perf18r_old_hero` |
| Velho Herói | Tentativa mudou para o Adjudicador; inconclusiva |
| Rei da Tempestade | Área/barra; céu muito saturado e boss fora do enquadramento |
| Ídolo dos Tolos | Arena/barra, boss distante e combate com inimigos da sala; parcial |
| Maneater | Modelo próximo/efeitos/atividade em `perf18r_old_monk`; segundo boss/fases não validados |
| Velho Monge | Tentativa terminou no Maneater; inconclusiva |
| Sanguessuga | Névoa/barra; repetição caiu e carregou outra posição |
| Colosso Imundo | Boss selecionado, modelo/barra e atividade na arena; combate observado |
| Astraea | Personagem ficou na escada de acesso; luta não validada |
| Aranha Blindada | Névoa/barra; modelo/ataques não comprovados nas capturas |
| Flamelurker | Arena, modelo e partículas visíveis na repetição; ataques/fases não completos |
| Deus Dragão | Entrada/barra/efeitos; percurso e duas balistas não validados |

Assim, a cobertura visual mais forte é de cinco encontros em trechos curtos.
Os demais são parciais ou inconclusivos. Nenhum desses resultados equivale
a completar a luta, testar todas as fases ou garantir ausência de travadas.

## Resultado técnico e cache instalado

Concluídas 28 execuções: 16 destinos e 12 repetições, todas encerradas no
prazo pelo diagnóstico. Sem encerramento espontâneo, falha de PSO, erro de
dispositivo ou exceção crítica nos padrões de log verificados. O jogo
continuou produzindo frames nos recortes observados. Isso não exclui defeitos
sem log ou problemas nas fases que não foram exercitadas.

Persistem pausas curtas. Nos testes com combate observado, a apresentação
após 75 s teve máximos de 63,26 a 365,74 ms, incluindo capturas e, em um
caso, mudança de área. Não atribuir esses valores inteiros ao jogo sem
instrumentação. A SPU0 continuou perto de um núcleo inteiro; o gargalo de
CPU/SPURS não foi corrigido nesta etapa. As últimas amostras de áudio dos
cinco encontros tinham atividade, silêncio de 0–0,2% e perdas zero.

Comparação do preparo de PSO nos dois destinos repetidos com evidência de
combate em ambas as execuções (totais incluem boot/transições):

| Destino | Compilações nos eventos, primeira → repetição | Maior evento PSO, primeira → repetição |
|---|---:|---:|
| Cavaleiro da Torre | 1 → 0 | 74,250 → 6,822 ms |
| Penetrador | 11 → 1 | 168,372 → 115,461 ms |

A repetição do Penetrador ainda descobriu uma variante. Duração, AI,
trajetória, conjunto de shaders e biblioteca de pipelines diferem; esses
números não demonstram ganho sustentado de FPS nem eliminação de engasgos.

Adicionados **284 arquivos individuais de shader** ao cache local
`work/scratch/rsx_shader_cache_v2`. Todos os **1.157 arquivos anteriores**,
incluindo pack e biblioteca de pipelines, foram preservados byte a byte.
Cada arquivo novo passou por conferência de formato/versão/stage/tamanhos,
hash da fonte, assinatura DXBC e SHA-256, com publicação por rename atômico.
O preload existente procura arquivos individuais além do pack; os arquivos
adicionados serão incorporados por ele ao iniciar o jogo.

Foi instalado somente o acréscimo de shaders; a biblioteca de pipelines
acumulada no diagnóstico permaneceu na cópia de teste. Assim, o cache do
jogador não é idêntico ao ambiente das repetições e não se promete reproduzir
os mesmos tempos. O acréscimo evita recompilar as fontes correspondentes,
mas a criação de PSOs e novas variantes ainda podem causar pausas.

Os 14 arquivos de save originais mantiveram seus SHA-256. Executável e
configuração padrão não foram alterados nesta etapa. Não restou processo do
jogo nem controlador de teste em execução. Manifestos e conferências em
`cache-installed.json`, `validation-final.json` e `analysis.json`, dentro do
diretório de evidências.
