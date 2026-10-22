# Demon's Souls Port PC

**Demon's Souls (PS3), recompilado para rodar nativamente no PC.**

![Status](https://img.shields.io/badge/status-experimental-orange) ![Plataforma](https://img.shields.io/badge/plataforma-Windows-blue) ![Licença](https://img.shields.io/badge/licen%C3%A7a-MIT-green)

Português (Brasil) · [English](README.en.md)

Este projeto traduz o código do jogo, do processador principal (PPU) e dos SPUs do PS3, para C++ e o compila para o seu PC, sobre o runtime do [ps3recomp](https://github.com/sp00nznet/ps3recomp). Não é um emulador: é o próprio jogo, recompilado.

> [!WARNING]
> É um experimento. O jogo inicia e o tutorial é jogável, mas ainda não dá para jogar do começo ao fim, e partes além do tutorial não foram testadas. Só funciona no Windows.

> [!IMPORTANT]
> **Nenhum dado do jogo está incluído**: nem o código, nem os arquivos do disco, nem o firmware do PS3. Você precisa da sua própria cópia, a versão americana **BLUS30443 (1.00)**, tirada do seu disco, e do firmware oficial do PS3 instalado no RPCS3. Ninguém aqui pode te dar o jogo, o firmware ou chaves. É um projeto de fãs, sem ligação com a Sony Interactive Entertainment, a FromSoftware ou a Bluepoint.

## Onde estamos

Outubro de 2026, no Windows:

- **Do boot ao tutorial**: avisos, menu principal, criação de personagem, cinemática de abertura e o tutorial (corredor, pátio, combate com os hollows, a torre). Os saves funcionam.
- **Renderizador próprio em Direct3D 12** (o "live draw" do ps3recomp, com as mudanças deste projeto): sombras, névoa, partículas suaves, adaptação de exposição, profundidade de campo e oclusão do sol e dos lens flares.
- **60 fps**: o patch "Unlock FPS" do RPCS3 (Gibbed e Whatcookie), aplicado como código. O corredor do tutorial fica em ~59 fps e o pátio com combate entre 46 e 59 fps. O modo original de 30 fps (`-Fps30`) fica cravado em 30.
- **Cinemáticas em vídeo** (`.pam`), tocadas pelo [ffmpeg](https://ffmpeg.org) com som: a abertura, a tela-título e a introdução da história.
- **Áudio** completo, sem cortes: músicas, vozes e efeitos (MultiStream + ATRAC, com o codec do seu firmware).
- **O kernel real do SPURS** (`libsre`, do seu firmware) roda recompilado, não emulado por aproximação.
- **Problemas conhecidos**: em brigas com muitos inimigos o fps cai para a casa dos 40, porque o limite são os SPUs recompilados, não a placa de vídeo. Algumas partículas podem faltar, e o céu às vezes estoura em áreas externas.

O diário técnico completo, com cada problema encontrado e como foi resolvido, está em [docs/REPORT.md](docs/REPORT.md).

## Do que você precisa

- **Windows 10 ou 11** e uma placa de vídeo com Direct3D 12. Foi testado numa Radeon RX 7600.
- **O jogo**: o seu disco **Demon's Souls (USA), BLUS30443**, extraído e decriptado. Uma pasta `PS3_GAME` tirada com o [ps3-disc-dumper](https://github.com/13xforever/ps3-disc-dumper) já serve. Se você tem uma ISO encriptada do Redump, decripte-a antes com o PS3Dec e a chave (`.dkey`) do seu disco, e extraia.
- **O [RPCS3](https://rpcs3.net) com o firmware oficial do PS3 instalado** (`PS3UPDAT.PUP` do site da Sony). Ele decripta o `EBOOT.BIN` e o `libsre.sprx`, e a pasta `dev_flash` dele serve o codec de áudio.
- **Para compilar**:
  - Visual Studio 2022 ou mais novo, com "Desenvolvimento para desktop com C++" e o componente **"C++ Clang tools for Windows"** (clang-cl);
  - Python 3.10+ e Git no PATH;
- **[ffmpeg](https://ffmpeg.org)** no PATH (`winget install Gyan.FFmpeg`) ou em `third_party\ffmpeg\bin`, para os vídeos. Sem ele o jogo pula as cinemáticas.
  - ~20 GB livres e **16 GB de RAM** ou mais. O código gerado tem ~2,4 GB e um build do zero leva de 30 a 60 minutos.

## Compilando

```
demons-souls-port-pc/
  port/          o programa (main, ponte com o jogo, CMake)
  lle/           a ponte para o kernel SPURS do firmware
  patches/       as mudanças deste projeto no ps3recomp
  scripts/       preparação, build, jogo e testes
  docs/          diário técnico, variáveis de ambiente, testes automáticos
  third_party/   o ps3recomp com o patch (criado pelo setup; fora do git)
  work/          o seu disco e tudo o que sai dele (fora do git, nunca suba isso)
```

No PowerShell, na pasta do repositório:

1. **Preparar o ambiente** (uma vez). Clona o ps3recomp no commit usado, aplica `patches/ps3recomp.patch` e cria o ambiente Python:

   ```powershell
   .\scripts\windows\setup.ps1
   ```

2. **Colocar o disco** em `work\disc`, de modo que exista `work\disc\PS3_GAME\USRDIR\EBOOT.BIN`.

3. **Gerar o código**. Decripta o `EBOOT.BIN` e o `libsre.sprx` com o seu RPCS3, traduz o código do jogo e do kernel SPURS e extrai os programas SPU (alguns minutos):

   ```powershell
   .\scripts\windows\prepare.ps1 -Rpcs3 "C:\caminho\do\rpcs3"
   ```

4. **Compilar**:

   ```powershell
   .\scripts\windows\build.ps1          # -Jobs 6 se faltar memória
   ```

## Jogando

```powershell
.\scripts\windows\play.ps1 -Rpcs3 "C:\caminho\do\rpcs3"   # da primeira vez (fica lembrado)
.\scripts\windows\play.ps1                                 # depois
.\scripts\windows\play.ps1 -Fps30                          # o jogo original, a 30 fps
.\scripts\windows\play.ps1 -Uncapped                       # sem limite de 60 fps (experimental)
.\scripts\windows\play.ps1 -Resolution 1080p               # janela maior: 1080p, 1440p, 4k ou LxA
```

**Controle**: um controle XInput (Xbox ou compatível) funciona direto, com os botões na posição do controle do PS3.

**Teclado** (usado quando não há controle):

| Tecla | Botão | Tecla | Botão |
|---|---|---|---|
| Setas | andar / direcional | Enter | Start |
| Z | ✕ | Tab | Select |
| X | ◯ | Q / W | L1 / R1 |
| A | □ | 1 / 2 | L2 / R2 |
| S | △ | | |

A câmera só gira pelo controle por enquanto.

Os saves ficam em `work\gamedata\dev_hdd0` e o log em `work\logs`.

## Testes automáticos

`scripts\windows\autotest.ps1` roda o jogo por um tempo fixo com entrada programada (botões e analógicos, sem ninguém jogando), mede o fps e guarda o log. Serve para testar uma mudança sempre no mesmo trajeto. Veja [docs/AUTOTEST.md](docs/AUTOTEST.md). Todas as variáveis de ambiente (diagnóstico, renderizador, áudio) estão em [docs/ENVIRONMENT.md](docs/ENVIRONMENT.md).

## Créditos

- [ps3recomp](https://github.com/sp00nznet/ps3recomp), de sp00nznet e colaboradores: o recompilador, o runtime e as bibliotecas de sistema. Este projeto é um conjunto de mudanças em cima dele (`patches/`).
- [RPCS3](https://rpcs3.net): referência de comportamento do PS3, a decriptação e o firmware que você instala nele. O patch "Unlock FPS" de BLUS30443 é de Gibbed e Whatcookie.
- [Ascension](https://github.com/arnaldolima1/gow-ascension-pc) (God of War: Ascension no PC, mesmo runtime): várias correções de SPU vieram de lá.
- Demon's Souls é marca registrada da Sony Interactive Entertainment. Desenvolvido pela FromSoftware.

## Licença

MIT: veja [LICENSE](LICENSE). Vale só para o código deste repositório, não para o jogo. O ps3recomp tem a sua própria licença (MIT).
