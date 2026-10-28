# Personagem sumindo e voltando: causa e correção (27/10/2026)

## Sintoma

Em jogo, o personagem (e outros objetos) às vezes some por 1 a 3 quadros e volta.

## Causa

O log da sessão em 4K (`work/logs/play.err`, janela 3840x2160) registrou:

- `drop{... pso=1645}`: draws pulados porque o shader ainda compilava (`LD_ASYNC_SHADERS`).
- 170 misses de pixel shader no cache de disco, contra 64 hits, nos primeiros ~100 s.
- O cache do jogador cresceu de 1627 para 1933 arquivos.

Os shaders "novos" eram, quase todos, variantes dos que já estavam no cache. Diferiam em uma linha, a escala de coordenada do shadow map, que era embutida no HLSL:

```
-    float2 uv = coordinate.xy * float2(0.5, 0.5);
+    float2 uv = coordinate.xy * float2(0.25, 0.25);
```

A escala é a razão entre o tamanho da textura e o do snapshot de profundidade, e muda com a resolução interna (0,5 em 2x, 0,25 em 4x, 0,333 em 3x). Cada resolução diferente gerava outro conjunto de todos os programas que recebem sombra, inclusive os do personagem. Enquanto esses shaders compilavam, os draws que dependiam deles eram pulados.

Algumas variantes também diferiam por `saturate(...)` em operandos (`RSX_FP_SRC_SAT`). É uma mudança do decompilador, posterior aos arquivos antigos do cache, e custa uma única recompilação.

## Correção

- A escala vai para o constant buffer do pixel shader, em `fp_shadow[16]`, logo após `fp_alpha`. É o mesmo padrão já usado para `fp_alpha.y` (escala do WPOS).
- O HLSL fica igual em qualquer resolução, e a escala deixa de entrar na chave do PSO.
- Para voltar à forma antiga: `LD_SHADOW_SCALE_CB=0`.

## Resultado (Boletaria, rota fixa, 400 s, um cache compartilhado em sequência)

| Rodada | Escala | PS miss | Draws pulados (pso) |
|---|---:|---:|---:|
| Sessão do usuário (antes) | 4K | 170 | 1645 |
| s2 (1ª vez com o HLSL novo) | 2x | 82 | 5135 (compilação única) |
| s4 | 4x | 0 | 4 |
| s1 | 1x | 0 | 0 |
| b4 (`LD_SHADOW_SCALE_CB=0`) | 4x | 11 | 135 |

As capturas `s4_t375` e `b4_t375` (`work/qa_20261027/shadowcb/`) mostram iluminação e sombras equivalentes.

## Criação de pipeline em segundo plano (`LD_ASYNC_PSO`)

Um pipeline que não está nem aquecido nem na pipeline library é criado em uma thread auxiliar. `CreateGraphicsPipelineState` compila para a GPU dentro do driver: com o cache do driver frio, isso já segurou a thread do renderizador por 7,8 s (`frame_clock` dentro de `D3DSCache.dll`, varredura de mapas de 27/10).

- O renderizador espera até `LD_ASYNC_PSO_WAIT_MS` por pipeline (padrão 4) e `LD_ASYNC_PSO_FRAME_MS` por quadro (padrão 8). Assim, a criação rápida, que é o caso comum, ainda desenha no quadro certo.
- Passado esse prazo, o draw sai quando o pipeline fica pronto.
- Na rodada s2, 16 criações levaram de 50 a 104 ms; todas ficaram fora da thread do renderizador.
- Para voltar ao modo antigo: `LD_ASYNC_PSO=0`.

## Launcher

O launcher abria `ICON0.PNG` dos saves com `GdipLoadImageFromFile`, que mantém o arquivo aberto enquanto a imagem existe. Com o launcher aberto, o arquivo ficava bloqueado: a restauração de save dos testes falhou por isso, e o jogo também ficava impedido de substituí-lo. A imagem agora é copiada para a memória e o arquivo é fechado.
