# Novos bosses e pontos internos — 26/10/2026, etapa 20

Concluídas **14 execuções**. Foram observados três combates adicionais:
**Velho Herói, Sanguessuga e Flamelurker**, além de cinco pontos internos em
mapas diferentes. Astraea permanece parcial: houve acesso e inimigos comuns,
mas não foi comprovado confronto com ela ou com Garl.

## Método e limites

Saves privados com HP/vitalidade elevados, sem alterar flags de eventos,
lógica da AI ou arquivos originais. Parte das tentativas começou nas entradas
de névoa; outras começaram dentro da arena, em posições conferidas no MSB e
nos scripts Lua locais. Foram usados movimento e ataques normais do jogo
para ativar as regiões existentes. Não equivale a validar a entrada normal,
progressão de quests, todas as fases, vitória ou morte/respawn de cada boss.

O Velho Herói passou pelas regiões 2897/2885 do mapa 03_02; o Flamelurker,
pela região 2898 e lógica de batalha do mapa 06_01. O Sanguessuga foi
exercitado na parte baixa da arena, dentro da região de batalha 2887 do
mapa 05_00. Nenhum callback de conclusão ou semáforo foi forçado.

720p, escala 1, alvo 60, FSR/FG/debug/sampler desligados, present worker
ligado e experimentos de FIFO idle/descriptor cache/PSO warmup desligados.
Timers, CPU, áudio e capturas ativos. `diagnose.ps1` inicia a janela oculta:
reprodução em janela visível é necessária antes de atribuir uma parada
observada aqui a uma partida normal. Não é benchmark limpo nem A/B de FPS.

## Combates confirmados nas capturas

| Boss | Evidência local | Observado |
|---|---|---|
| Velho Herói | `old_hero_trigger`, frame 007800 | Modelo, barra, golpes e movimentação da luta |
| Sanguessuga | `leechmonger_arena`, frame 009000 | Modelo próximo, barra e atividade/ataques na parte baixa da arena |
| Flamelurker | `flamelurker_trigger`, frame 008400 | Combate próximo, golpes, fogo e movimentação |

Os três processos chegaram ao prazo de 180 s sem encerramento espontâneo
ou a parada longa observada na tentativa externa. Esse prazo inclui boot e
carregamento; não são três minutos inteiros de combate. Não foi validada a
fase final nem a derrota dos bosses. Evidência visual conjunta em
`work/perf20_20261026/bosses-confirmed.jpg`, com capturas integrais por caso.

## Pontos internos dos mapas

| Caso | Resultado e alcance |
|---|---|
| `boletaria_primeval` | Boletaria, ponto interno da Ala Interior. Geometria e comandos observados; enquadramento final num canto, sem comprovar o Demônio Primordial. |
| `latria_lower` | Latria, nível inferior/ponte. Modelo e combate com fantasma vermelho observados. |
| `swamp_island` | Pântano da Amargura, área alagada interna. Deslocamento e inimigos próximos, incluindo mosquito; efeitos da área ativos. |
| `stonefang_deep` | Cidade dos Túneis, caverna/estrutura de madeira interna. Geometria e comandos observados; não é validação de um percurso completo. |
| `shrine_lower` | Caminho Ritual, câmara inferior. Geometria e comandos observados; trecho limitado junto a pilares, sem comprovar combate com NPC. |

Cada ponto teve execução de 120 s com câmera, deslocamento curto e/ou ataques.
Os cinco continuaram produzindo frames até o prazo. A cobertura é do trecho
observado; não aprova o mapa inteiro nem todos os seus eventos.

## Tentativas parciais e falha observada

- `old_hero_gate` e `old_hero_hold`: prompt de névoa, sem atravessar. Não
  contam como luta; houve teste de botão parado e variação de duração.
- `leechmonger_gate`: interação longa ativou a entrada/barra, sem comprovar
  combate. `leechmonger_hold` caiu da passarela e retornou ao começo; excluído
  da validação da luta. A validação do Sanguessuga vem de `leechmonger_arena`.
- `flamelurker_gate`: ficou antes da névoa e apresentou uma lacuna de cerca
  de 79 s sem avanço de frames até o prazo, com áudio ativo. O controlador
  havia sido parado durante essa execução. A causa não foi isolada; não
  demonstra falha durante combate normal. `flamelurker_trigger` completou
  o prazo com combate ativo sem a mesma parada. Não foi declarada correção.
- `astraea_gate`: barra e área do encontro, travessia e combate com inimigos
  comuns; modelo/ataques de Astraea e Garl não comprovados.

## Indicadores de desempenho

Persistem picos curtos. A tabela mostra apresentação após 55 s, não mede
isoladamente FPS de quadros únicos ou input lag. Os máximos incluem capturas;
PSO inclui boot/transições e preparo completo, não CPU exclusiva.

| Boss observado | Apresentação média no recorte | Maior intervalo de apresentação | Maior preparo PSO | Compilações nos eventos |
|---|---:|---:|---:|---:|
| Velho Herói | 59.33/s | 225.76 ms | 170.10 ms | 3 |
| Sanguessuga | 59.37/s | 97.58 ms | 61.81 ms | 1 |
| Flamelurker | 59.61/s | 105.75 ms | 108.70 ms | 1 |

A última amostra de CPU desses três casos usou aproximadamente 4,48–5,12
núcleos somados. A SPU0 permaneceu perto de um núcleo inteiro (98–99%);
o gargalo de CPU/SPURS não foi removido. As últimas amostras de áudio dos
três tinham atividade, silêncio 0% e perdas zero. Não há ganho sustentado
de FPS demonstrado nem garantia de ausência de engasgos.

## Arquivos e preservação

Logs brutos em `work/diagnostics/perf20_*`. Configuração, comandos, imagens,
progresso, classificação e métricas em `work/perf20_20261026` (`cases.json`,
`inputs.jsonl`, `coverage.json`, `analysis.json`, `validation-final.json`).
Todas as 14 execuções foram encerradas pelo prazo; zero encerramentos
espontâneos, sem padrões críticos de PSO/dispositivo nos logs verificados.

Os 14 arquivos de save originais e os 1.441 arquivos do cache do jogador
mantiveram seus SHA-256. O executável também permaneceu no SHA-256 da etapa 19:
`105C933F6FA61AE7E31BB029595375D109B0D3037BAD5F6F6CFB674C7D83C029`.
Não houve alteração de código nativo nesta etapa.

O cache diagnóstico permanece separado: 73 shaders individuais adicionais
frente ao cache do jogador, incluindo 45 da etapa anterior e 28 descobertos
nesta rodada. Não foram instalados no cache do jogador. A ausência de
processos de jogo/controlador restantes é conferida no manifesto final.
