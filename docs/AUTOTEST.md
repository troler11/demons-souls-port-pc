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

## CPU menor

`-Threads N` roda o jogo só nos primeiros N processadores lógicos, simulando uma CPU menor. `PS3_THREAD_CPU=10` mostra a cada 10 s quanto de um núcleo cada thread gastou (SPUs, `HighGraphics`, `frame_clock`...).

```powershell
$env:PS3_THREAD_CPU = "10"
.\scripts\windows\autotest.ps1 -Seconds 400 -Tag cpu6 -Threads 6 -Save work\saves\boletaria
```
