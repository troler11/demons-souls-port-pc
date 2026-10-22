# Testes automáticos

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
