# God of War III Remastered for PC 

<div align="center">
  <a href="https://ko-fi.com/S7O428PBCC" target="_blank"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="ko-fi"></a>
</div>

[English](README.md) | **Português (Brasil)** | [Bahasa Indonesia](README.id.md)

Port para PC de **God of War III Remastered** (PlayStation 4) para Windows 10 e 11, com versões
experimentais para Linux e para Macs com Apple Silicon ([macOS](#macos-apple-silicon-experimental)).

A parte da CPU é nativa: o executável original do jogo roda direto no processador do PC (o PS4
usa a mesma arquitetura x86-64), e um runtime escrito para este jogo substitui as bibliotecas de
sistema do PS4, sem emulador. Os gráficos são traduzidos: rodam no núcleo de vídeo do
[shadPS4](https://github.com/shadps4-emu/shadPS4), que converte os comandos e shaders da GPU do
PS4 para Vulkan, com as correções de renderização de God of War III da
[branch `eltutz` de cuesta4](https://github.com/cuesta4/shadPS4/tree/eltutz). Rodar o código do
jogo direto na CPU também é como o shadPS4 funciona; o que este projeto muda é a camada de
sistema e o trabalho específico do jogo (patches, cache de shaders em segundo plano, launcher e
menu no jogo). Num Mac, o Rosetta 2 da Apple traduz esse código x86-64 para o Apple Silicon, e os
gráficos chegam ao Metal pelo driver Vulkan KosmicKrisp, do Mesa.
É um fork do [port para Windows de Supermedo](https://github.com/Supermedo/bloodborne_pc) do
[bloodborne_pc de deadinside28](https://github.com/deadinside28/bloodborne_pc) (veja os
[Créditos](#créditos)).

> **Nenhum arquivo do jogo está incluído.** É preciso um dump do seu próprio PS4 (testados:
> CUSA01623 e CUSA01715, versões 01.00 e 01.02). Este projeto não tem relação com a Sony Interactive Entertainment nem com o
> Santa Monica Studio. God of War é marca registrada da Sony Interactive Entertainment.

Na máquina de teste (AMD Radeon RX 6700 XT, Ryzen 5 5600X, 32 GB), o jogo roda a 144 FPS de
média (55 a 240) em 1080p com o motor a 240 FPS, e a 108 FPS de média em 4K com FSR 1.

**Testado e jogável do início ao fim** em 1080p nativo, com o motor a 240 FPS. Em outras
resoluções de renderização algumas texturas aparecem quebradas; a correção está em andamento.

O andamento está em
[docs/gow3/ROADMAP.md](docs/gow3/ROADMAP.md).

## O que funciona

- Abertura, menus, cutscenes em vídeo (H.264) e gameplay.
- Áudio: música, vozes e efeitos (ATRAC9 e MP3).
- Controle (DualSense, Xbox e outros, via SDL3) e teclado ao mesmo tempo, os dois com botões
  configuráveis.
- Salvar e carregar: a lista de saves aparece na tela, como no PS4. O autosave também funciona.
  O launcher faz backup dos saves antes de cada partida.
- Patches: resolução de renderização de 480p, 720p, 1440p, 1800p ou 4K, correção de texturas
  corrompidas e pular vídeos com o botão X (comunidade); FPS do motor de 120 (kvicken) ou 240
  (adaptado por Felipe Prado).
- **Abertura imediata:** o cache de shaders salvo carrega em segundo plano enquanto você joga, e
  os shaders novos também são compilados em segundo plano, sem parar o jogo.
- **Launcher** com as cores do jogo, em inglês com traduções parciais: ajustes rápidos na tela inicial, tela,
  desempenho, pasta do jogo e backups de save, patches, mods (ordem, presets e conflitos),
  controles com ícones de botões PlayStation ou Xbox, atualizações e diagnóstico.
- **Menu dentro do jogo** (Insert no teclado, ` num Mac, R3+L2 no controle), com quatro abas:
  - **Game:** trapaças (vida, magia, item e Fúria de Esparta infinitos, orbes vermelhos no
    máximo); multiplicadores de 0,1x a 100x para orbes vermelhos, verdes (vida), azuis (magia)
    e dourados (Fúria de Esparta) e para dano causado e recebido; barra de vida do inimigo.
  - **Image:** modo de tela (janela, sem borda, tela cheia), VSync, ampliação FSR 1 (ou o
    MetalFX da Apple num Mac) e nitidez RCAS.
  - **Performance:** limite de FPS (de 30 a ilimitado), quadros na fila, compilação de shaders
    em segundo plano; resolução, FPS do motor e as opções de leitura da GPU valem com um único
    botão "Apply and restart".
  - **Overlay:** contador de desempenho com FPS, tempo de quadro, upscaler, GPU (nome e uso),
    CPU, RAM e VRAM, cada um opcional, com tamanho da fonte, canto, transparência e formato. Os
    números são o uso do próprio jogo, não do sistema inteiro (num Mac o uso da GPU é o da GPU
    inteira: o macOS não tem contador por processo). O item do upscaler diz o que amplia a
    imagem, como "MetalFX 720p->1080p".

## Baixar e jogar

**Você precisa de**
- Windows 10 (1903 ou mais novo) ou Windows 11, 64 bits.
- Uma placa de vídeo com Vulkan 1.3 e driver atualizado.
- Sua própria cópia de God of War III Remastered, numa pasta que contenha `eboot.bin`,
  `sce_module` e `sce_sys`. Testados: **CUSA01623 (EUA) e CUSA01715 (Europa), versões 01.00 e
  01.02** (atualização completa). Outras regiões e versões podem funcionar, mas não foram
  testadas: se usar uma delas, abra uma issue com o seu `user/last_run.log`. Nenhum arquivo do
  jogo vem neste download.

**Passos**
1. Baixe o `gow3-windows.zip` na
   [página de releases](https://github.com/FelipePrado0/gow3_pc/releases/latest).
2. Extraia numa pasta de sua escolha, por exemplo `C:\Jogos\gow3-windows`. Não abra direto de
   dentro do zip.
3. Abra o **`God of War III.exe`**. Se o Windows mostrar "O Windows protegeu o computador",
   clique em **Mais informações** e depois em **Executar assim mesmo**: o programa não tem
   assinatura digital.
4. Clique em **Choose the game folder**. Isso leva à tela **Game**. Em **Game folder**, clique
   em **Browse…** e escolha a pasta que tem o `eboot.bin`.
5. De volta à **Home**, aparecem a capa e "CUSA01623 · version 01.02". Ajuste os **Quick
   settings** se quiser: modo de tela, limite de FPS, resolução e FPS do motor.
6. Clique em **PLAY**. A primeira partida demora um pouco mais, enquanto a imagem do jogo é
   preparada. As próximas abrem rápido.

Patches e correções já vêm incluídos e são aplicados sozinhos: correção de texturas, pular vídeos
com X e 120 FPS vêm ligados. Mude resolução e FPS na **Home** ou em **Performance**.

**Durante o jogo**
- **Insert** no teclado, ou **R3 + L2** no controle, abre o menu do port: trapaças,
  multiplicadores, imagem, desempenho e o contador de desempenho.
- Os shaders salvos carregam em segundo plano. Um aviso pequeno "Loading shader cache" no canto
  mostra o progresso.

**Das próximas vezes**
- O **`Play God of War III.exe`** abre o jogo direto, com as configurações salvas. Dá para criar
  um atalho na área de trabalho ou adicionar à Steam em **Adicionar um jogo que não é da Steam**.
- Saves, cache de shaders e log ficam na pasta `user`, ao lado do `God of War III.exe`. Guarde
  essa pasta ao atualizar.
- **Advanced → Check for updates** baixa e instala a versão nova. Saves e configurações são
  mantidos.

**Se algo der errado**
- Tela preta ao abrir: **Advanced → Clear shader cache** e abra de novo.
- O jogo não abre: veja a tela **Log**, ou envie o `user\last_run.log` junto com o relato.

### Num Mac (Apple Silicon, experimental)

**Você precisa de**
- Um Mac com Apple Silicon (M1 ou mais novo) e o macOS 26 (Tahoe) ou mais novo. Macs com Intel
  não são suportados.
- Rosetta 2: o código do próprio jogo é x86-64. Se ele faltar, a tela **Log** avisa; instale no
  Terminal com `softwareupdate --install-rosetta --agree-to-license`.
- Sua própria cópia do jogo, como acima. Nada mais: o Python, as bibliotecas e o driver Vulkan
  vêm dentro do aplicativo.

**Passos**
1. Baixe o `gow3-macos.dmg` na
   [página de releases do macOS](https://github.com/nizwar/gow3_macos/releases/latest).
2. Abra e arraste **God of War III** para **Aplicativos**.
3. O aplicativo não tem assinatura de Developer ID da Apple, então o macOS o bloqueia. Libere uma
   vez no Terminal:
   ```
   xattr -dr com.apple.quarantine "/Applications/God of War III.app"
   ```
4. Abra o **God of War III** e clique em **Choose the game folder**. Em **Game folder**, clique em
   **Browse…** e escolha a pasta que tem o `eboot.bin`. Um update dumpado separado do jogo vai em
   **Update folder**.
5. Clique em **PLAY**. A primeira partida demora mais: o Rosetta traduz o código do jogo e os
   shaders são compilados.

Para mais FPS numa tela 1080p: **Performance → Render resolution → 720p** (ou 480p) e
**Display → Upscale to the window** com **Upscaler → MetalFX (Apple)**. Toda resolução de
renderização menor que a janela é ampliada, e o contador de desempenho mostra
"MetalFX 720p->1080p". Num M4 Pro isso dá cerca de 52 FPS na primeira luta, contra 32 em 1080p
nativo. Em 480p as áreas claras estouram (um bug conhecido do jogo nessa resolução).

**Durante o jogo**
- **`** (a tecla à esquerda do 1) no teclado, ou **R3 + L2** no controle, abre o menu do port: o
  teclado do Mac não tem a tecla Insert.

**Das próximas vezes**
- Abra o **God of War III** pelos Aplicativos, pelo Launchpad ou pelo Dock. **Advanced → Add to
  Steam** inclui o jogo na Steam (feche a Steam antes); a Steam então abre o jogo sem o launcher.
- **Advanced → Check for updates** procura um `gow3-macos.dmg` mais novo e o baixa: arraste o novo
  aplicativo por cima do antigo. Saves e configurações são mantidos.
- Saves, configurações, cache de shaders e o log ficam em `~/Library/Application Support/gow3`
  (**Advanced → Port folder** abre a pasta); as opções do próprio launcher, em
  `~/.config/gow3-launcher`.

**Se algo der errado**
- "God of War III está danificado" ou "não pode ser aberto": rode de novo a linha `xattr` do passo 3.
- Tela preta ao abrir: **Advanced → Clear shader cache** e abra de novo.
- O jogo não abre: veja a tela **Log**, ou envie o
  `~/Library/Application Support/gow3/user/last_run.log` junto com o relato.

## Linux (AppImage, experimental)

A versão para Linux usa o mesmo runtime nativo e o mesmo launcher. Foi testada no Arch Linux com uma
Radeon RX 7800 XT (RADV) numa sessão Wayland.

**Você precisa de:** um driver Vulkan 1.3 e do seu próprio dump do jogo (CUSA01623, versão 01.02).

**Compilar e rodar**
```
bash build.sh
bash packaging/linux/build.sh   # precisa de PyInstaller, Tk e Pillow para Python, mais patchelf e curl
./dist/gow3-x86_64.AppImage
```
Escolha a pasta do jogo no launcher. `--play` inicia o jogo com as opções salvas e sem janela. A
partir do código: `GOW3_GAME_DIR=/caminho/para/CUSA01623 python3 run.py` depois de `bash build.sh`.

- **Arquivos:** saves, configurações, cache de shaders e o registro ficam em `~/.local/share/gow3`; as
  configurações do próprio launcher, em `~/.config/gow3-launcher`.
- **Pasta do update:** se o update foi dumpado separado do jogo, escolha-a em **Jogo → Pasta do
  update**. Ela é colocada por cima da pasta do jogo sem alterar nenhuma das duas.
- **Avançado → Linux:** sistema de janelas (automático, Wayland ou X11), GameMode e MangoHud. Só as
  ferramentas instaladas no sistema podem ser ligadas.
- **Steam:** **Avançado → Adicionar à Steam** inclui "God of War III Remastered" como jogo de fora da
  Steam, com o ícone, as capas vertical e horizontal, o fundo da biblioteca e o logotipo feitos a
  partir das imagens do dump. Feche a Steam antes (ela reescreve os atalhos quando fecha). O botão
  também está nos launchers do Windows e do Mac (ainda sem teste com a Steam neles).
- **Entrada no menu:** o AppImage grava `~/.local/share/applications/gow3.desktop` ao iniciar, para o
  launcher e a janela do jogo mostrarem o ícone no dock; **Avançado → Linux** desliga e remove a entrada.
- **Ícone:** o AppImage, o launcher e a janela do jogo usam `packaging/linux/gow3.png` quando esse
  arquivo existe na hora da compilação, e `launcher/gow3.png` caso contrário.
- **Patches:** o mesmo arquivo de patches serve a atualização 01.02 completa (`eboot.bin` com sha256
  `6e72330e…`) e o título europeu (CUSA01715): os patches e as trapaças acham seus pontos por assinatura.
- **Gráficos (experimental):** os upscalers temporais (FSR 3.1, FSR 4, TAA, DLSS) ficam desligados por
  padrão; o interruptor está no topo dessa página.

## macOS (Apple Silicon, experimental)

A versão para macOS roda o mesmo runtime nativo sob o Rosetta 2, com os gráficos no driver Vulkan
KosmicKrisp, do Mesa (Vulkan sobre Metal). Ela precisa de um Mac com Apple Silicon e do macOS 26
(Tahoe) ou mais novo. Foi testada num M4 Pro (macOS 27) na abertura e nas primeiras lutas, ainda
não do começo ao fim. Para jogar, use a imagem de disco ([Num Mac](#num-mac-apple-silicon-experimental));
para compilar e rodar a partir do repositório (detalhes e o que muda em relação ao Linux:
[packaging/macos/README.md](packaging/macos/README.md)):

**Você precisa de:** as ferramentas de linha de comando do Xcode (`xcode-select --install`),
`brew install cmake ninja pkg-config` do Homebrew, o Rosetta 2 e Python 3 com Tk e Pillow para o
launcher (`brew install python python-tk`, depois `python3 -m pip install pillow`).

```
git clone --recursive https://github.com/nizwar/gow3_macos && cd gow3_macos
git checkout macos-port            # o trabalho do macOS
./play-mac.sh                      # compila, acha o dump em ~/Downloads, ~/Desktop ou ~/Documents e joga
./play-mac.sh /caminho/CUSA01623   # essa pasta (eboot.bin, sce_module, sce_sys)
./play-mac.sh --launcher           # o launcher: configurações, saves, mods
bash packaging/macos/package.sh    # dist/gow3-macos.dmg e dist/God of War III.app: uma release
```

- **Arquivos:** a partir do repositório, saves, `gow3.ini`, cache de shaders e `user/last_run.log`
  ficam na pasta do repositório, como no Windows; o aplicativo os guarda em
  `~/Library/Application Support/gow3`. As configurações do próprio launcher: `~/.config/gow3-launcher`.
- **Sem a janela do launcher:** `./play-mac.sh` (acima), ou
  `"/Applications/God of War III.app/Contents/MacOS/God of War III" --play` com as configurações salvas.
- **Ampliação:** **Display → Upscale to the window** com **Upscaler → MetalFX (Apple)** ou FSR 1. A
  página **Graphics (experimental)** não aparece no Mac: os upscalers temporais ainda não mudam nada
  neste jogo, e o FSR 3.1 não inicia no KosmicKrisp.
- **Display → Fewer GPU waits (experimental):** menos carga na CPU onde as leituras da GPU param os
  desenhos; no M4 Pro não aumenta o FPS, que o lado da GPU limita.
- **Advanced → macOS → Metal performance HUD:** o overlay da Apple com FPS, tempos de quadro e
  memória na janela do jogo, o equivalente do MangoHud no Linux.
- **Contador de desempenho:** FPS, tempo de quadro, upscaler, uso da GPU (da GPU inteira), CPU, RAM
  (o número do Monitor de Atividade) e VRAM, como no Windows.
- **Performance → GPU data** não tem escolha de placa de vídeo nem de memória dos buffers: o Mac tem
  uma GPU que divide a memória do sistema.
- **Steam:** **Advanced → Add to Steam**, como no Linux (`~/Library/Application Support/Steam`),
  com a Steam fechada.
- **Atualizações:** **Advanced → Check for updates** consulta as releases deste repositório (tags
  `macos-v<versão>` com `gow3-macos.dmg`) e baixa a imagem de disco mais nova.
- **Atalho na área de trabalho:** a partir do repositório, **Advanced → Desktop shortcut** põe na
  área de trabalho um aplicativo que abre o launcher; o aplicativo empacotado é o seu próprio atalho
  (Dock, Launchpad).

## Requisitos

- Windows 10 (1903 ou mais novo) ou Windows 11, 64 bits; ou um Linux com driver Vulkan 1.3; ou um
  Mac com Apple Silicon (M1 ou mais novo), macOS 26 (Tahoe) ou mais novo e o Rosetta 2.
- Processador de 64 bits com os conjuntos de instruções do PS4 (AVX, F16C, BMI1, MOVBE, LZCNT):
  Intel Haswell (2013) ou mais novo, AMD Ryzen ou mais novo. O código do jogo roda na CPU como
  está, então um processador mais antigo para na abertura com uma linha `STOP: this CPU lacks`
  no log. Num Mac, o Rosetta 2 fornece essas instruções.
- Placa de vídeo com Vulkan 1.3 e driver atualizado. Num Mac o driver Vulkan (o KosmicKrisp do
  Mesa, sobre o Metal) vem com o port.
- Sua cópia do jogo (a pasta com `eboot.bin`, `sce_module` e `sce_sys`). Testados: CUSA01623 e
  CUSA01715, versões 01.00 e 01.02.
- Os patches e os ganchos do menu (trapaças, multiplicadores, barra de vida) acham seus pontos
  por assinatura: os bytes originais em volta de cada um, onde quer que a versão do jogo tenha
  colocado esse código (`tools/make_signatures.py`). Um ponto que não é achado deixa só aquele
  patch ou gancho desligado; o jogo abre sem ele, e o log e o menu dizem o motivo.
- Para compilar a partir do código: [MSYS2](https://www.msys2.org) e [Python 3](https://www.python.org) com Pillow
  (`pip install pillow`, usado no ícone e na capa do jogo). Num Mac: as ferramentas de linha de
  comando do Xcode, `cmake`, `ninja` e `pkg-config` do Homebrew, e Python 3 com Tk e Pillow
  ([macOS](#macos-apple-silicon-experimental)).

## Compilar a partir do código

O desenvolvimento acontece na branch `gow3`; a `main` recebe as versões já testadas.

1. Instale o MSYS2 (`winget install MSYS2.MSYS2`) e, no terminal **MSYS2 CLANG64**, os pacotes
   listados em [packaging/windows/README.md](packaging/windows/README.md).
2. Clone o repositório com os submódulos e compile:

   ```bash
   git clone --recursive https://github.com/FelipePrado0/gow3_pc
   cd gow3_pc
   git checkout gow3   # o trabalho mais recente; pule para compilar a main
   bash build.sh
   ```

3. Abra o launcher (no PowerShell, na pasta do projeto):

   ```powershell
   python launcher/gow3_launcher_win.py
   ```

4. Em **Game**, escolha a pasta do jogo. Ajuste **Display**, **Performance** e **Patches** se
   quiser e clique em **PLAY**.

Para jogar com as configurações salvas sem abrir a janela do launcher (para um atalho ou a
Steam):

```powershell
python launcher/gow3_launcher_win.py --play
```

Ou sem o launcher:

```powershell
$env:GOW3_GAME_DIR = 'D:\caminho\CUSA01623'
python run.py
```

As configurações ficam em `gow3.ini` (o jogo e o menu do jogo) e em
`%APPDATA%\gow3-launcher\settings.json` (o launcher). Saves, cache de shaders e `last_run.log`
ficam na pasta `user` do projeto.

Num Mac a compilação é `bash build.sh` no Terminal (na primeira vez ele baixa e compila as
bibliotecas x86-64, cerca de 5 minutos); depois `./play-mac.sh --launcher` ou
`python3 launcher/gow3_launcher_win.py` (`--play` sem a janela), ou
`GOW3_GAME_DIR=/caminho/CUSA01623 python3 run.py` sem o launcher; veja
[macOS](#macos-apple-silicon-experimental). As configurações do launcher ficam em
`~/.config/gow3-launcher/settings.json`; saves e o log na pasta `user` do projeto, como aqui.

Em **Performance → GPU data → Stream buffer memory** (Windows e Linux), escolha **System RAM (cached, default)**,
**System RAM (uncached)** ou **GPU memory (previous behavior)**. A RAM do sistema pode melhorar o
FPS em notebooks com placa de vídeo NVIDIA. A escolha é salva e vale na próxima partida,
inclusive pelo `Play God of War III.exe` ou pelo `--play`. Escolha **GPU memory** para voltar à
alocação anterior. Sem o launcher, `GOW3_STREAM_MEMORY` aceita `cached`, `host` ou `device`; com
o launcher, a escolha salva nele tem prioridade.

### Importar saves do shadPS4

Apenas **saves de God of War III Remastered no shadPS4** são compatíveis. Saves do
RPCS3 pertencem ao jogo de PS3 e não funcionam. Use o mesmo identificador do jogo
(este exemplo usa `CUSA01623`).

1. Feche o jogo e o shadPS4. Faça backup das duas pastas de saves.
2. No diretório de dados do shadPS4, procure `user\savedata\1\CUSA01623\` nas versões
   antigas, ou `home\<user-id>\savedata\CUSA01623\` nas versões mais recentes. Num Mac, o
   diretório de dados do shadPS4 costuma ser `~/Library/Application Support/shadPS4`.
3. O destino é **`user\savedata\1\CUSA01623\` ao lado de `God of War III.exe`**.
   Exemplo: `C:\Games\gow3-windows\user\savedata\1\CUSA01623\`. Com uma **Saves folder**
   personalizada no launcher, use `<pasta escolhida>\savedata\1\CUSA01623\`. Num Mac (o
   aplicativo): `~/Library/Application Support/gow3/user/savedata/1/CUSA01623/`; num build a
   partir do código, `user/savedata/1/CUSA01623/` na pasta do projeto.
4. Copie cada pasta de slot para lá, mantendo o nome e os arquivos do jogo. Não crie
   outra pasta `CUSA01623` dentro dela. Exclua o `sce_sys` interno dos dados copiados.
5. Ao lado de cada slot, crie `<nome do slot>.sce_sys` se não existir. Você pode copiar
   o `sce_sys\icon0.png` do shadPS4 para lá. **Não renomeie `param.sfo` para `param.bin`**:
   os formatos são diferentes. Preserve qualquer `param.bin` existente do port. Sem ele,
   o slot aparece pelo nome da pasta; salvar novamente no port grava os metadados e
   atualiza as informações exibidas.
6. Abra o port e selecione **Carregar jogo**. Saves com o jogo concluído também podem ser importados.

O menu de saves do port exibe no máximo **32 entradas**. Se o slot importado não aparecer
numa lista longa, teste com uma **Saves folder** separada contendo apenas esse slot em
`savedata\1\CUSA01623\`, junto da pasta irmã de metadados. Preserve os saves originais
e restaure a configuração anterior de Saves folder depois do teste.

### Overlays externos (Windows)

O processo do jogo é **`bin\gow3-probe.exe`**, não `God of War III.exe` (builds do código
usam `out\gow3-probe.exe`). Adicione esse executável ao RTSS e ative a exibição na tela
do Afterburner. No instalador do ReShade, selecione esse executável e a API **Vulkan**.

Se o overlay continuar ausente, informe as versões do port e do overlay, a última versão
do port em que funcionou e envie `user\last_run.log`. Essas verificações não confirmam
uma correção para todas as configurações de overlay.

### Overlays num Mac

RTSS, Afterburner e ReShade não existem para macOS. O Metal performance HUD da Apple ocupa esse
lugar: **Advanced → macOS → Metal performance HUD** no launcher, ou `MTL_HUD_ENABLED=1` para abrir
sem ele. O processo do jogo é o `gow3-probe` (no aplicativo:
`God of War III.app/Contents/Resources/bin/gow3-probe`), que é também o nome que o Monitor de
Atividade mostra. O contador de desempenho do próprio port funciona como no Windows.

### Teclado

Padrão; mude na tela **Controls** do launcher.

| Tecla | Botão |
|---|---|
| WASD | analógico esquerdo |
| Setas | analógico direito (câmera) |
| Espaço | Xis (Cross) |
| Shift esquerdo | Círculo |
| E | Quadrado |
| Q | Triângulo |
| 1 e 3 | L1 e R1 |
| R e F | L2 e R2 |
| Z e C | L3 e R3 |
| I, K, J, L | direcional |
| Enter | Options |
| Tab | clique esquerdo do touchpad (modo foto) |
| Backspace | clique direito do touchpad (menu de pausa) |
| Insert (` num Mac) | menu do port (R3+L2 no controle) |

Em um controle sem touchpad (Xbox e outros), View abre o menu de pausa (o clique direito do
touchpad) e L1 + View abre o modo foto (o clique esquerdo). No DualSense o touchpad funciona
como no PS4, e Share/Create funciona como View.

No menu: L1/R1 ou Q/E trocam de aba. Na lista de saves: setas ou direcional escolhem, Enter ou
Xis confirmam, Esc ou Círculo cancelam. O teclado do Mac não tem a tecla Insert: nele, **`** (a
tecla à esquerda do 1) abre o menu, e o Insert continua valendo num teclado que tenha.

## Testes

```bash
python -m unittest discover -s tests
bash build.sh --test
ninja -C out/gpu stat-multipliers-test perf-overlay-test warmup-inbox-test graphics-settings-test
out/gpu/stat-multipliers-test.exe out/eboot.elf
out/gpu/perf-overlay-test.exe
out/gpu/warmup-inbox-test.exe
out/gpu/graphics-settings-test.exe
```

Os testes em C++ são alvos do [gpu/CMakeLists.txt](gpu/CMakeLists.txt); os que conferem o
código do jogo recebem o executável preparado (`out/eboot.elf`) como argumento. No Linux e no
macOS os programas não têm `.exe`; num Mac rode `source packaging/macos/env.sh` antes do `ninja`
para ele achar o compilador e as bibliotecas x86-64 (o `bash build.sh --test` faz isso sozinho).

Para conferir se todas as funções de sistema que o jogo usa estão implementadas, sem abrir o
jogo:

```bash
out/gow3-probe.exe out/boot-linked.bin --app0 <pasta do jogo> --check-imports
```

## Como funciona

| Parte | Onde | O que faz |
|---|---|---|
| Preparação | `scripts/` | Lê o `eboot.bin`, aplica as realocações e junta a `libc` e a `libSceFios2` do próprio jogo numa imagem |
| Loader | `src/probe.c` | Carrega a imagem, instala os ganchos conferidos (trapaças, multiplicadores, barra de vida) e inicia o código original do jogo |
| Runtime | `src/runtime_*.c` | Reimplementa as funções de sistema do PS4: memória, threads, arquivos, áudio, vídeo, controle, saves e serviços |
| GPU | `gpu/shadps4/` | O núcleo de vídeo do shadPS4, que traduz os comandos e shaders da GPU do PS4 para Vulkan, com as correções de God of War III de cuesta4 (branch `eltutz`) e as otimizações deste port (cache de shaders em segundo plano, leituras adiadas). Num Mac, o Vulkan roda sobre o Metal pelo KosmicKrisp do Mesa, e o MetalFX pode ampliar a imagem (`host_passes/metalfx_pass.cpp`, `vk_metalfx.mm`) |
| Menu do jogo | `gpu/shim/gow3_overlay.cpp` | Menu e contador de desempenho em Dear ImGui; configurações em `gpu/shim/gow3_settings.cpp` |
| Patches | `patches/God_of_War_III_Remastered.xml`, `scripts/patches.py` | Aplicados na memória ao iniciar, sem alterar os arquivos do jogo |
| Launcher | `launcher/gow3_launcher_win.py` | Configurações, patches, mods, controles e o botão de jogar |
| Pacotes | `packaging/windows/`, `packaging/linux/`, `packaging/macos/` | O zip do Windows, o AppImage e o aplicativo do Mac com a imagem de disco (`package.sh`) |

Mais documentação: [mods](docs/MODS.md), [mapa do quadro e upscaler](docs/upscaler.md),
[trabalho paralelo da GPU](docs/parallel_gpu.md) e o [roadmap](docs/gow3/ROADMAP.md).

## Créditos

Este projeto é um fork do port para Windows de Supermedo, que parte do original de deadinside28:

- **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc), o port nativo
  original.
- **Supermedo** (Mohammed Albarghouthi): [bloodborne_pc para Windows](https://github.com/Supermedo/bloodborne_pc),
  o fork de onde este projeto vem.
- **cuesta4**: [fork do shadPS4 focado em God of War III Remastered](https://github.com/cuesta4/shadPS4/tree/eltutz)
  (branch `eltutz`). A renderização do jogo neste port depende das correções dele: a regravação
  de blends da GNM, o estado de profundidade e stencil e as correções de imagens que
  compartilham memória. cuesta4 também é coautor dos patches de resolução, correção de texturas,
  120 FPS e pular vídeos (abaixo). Para jogar God of War III no shadPS4, este é o build indicado.
- **Equipe do shadPS4**: o núcleo de vídeo do [shadPS4](https://github.com/shadps4-emu/shadPS4),
  que faz toda a tradução de GPU e shaders para Vulkan neste port.

Patches: kvicken e cuesta4 (resolução, correção de texturas, 120 FPS e pular vídeos, a partir
dos patches de PS3 de illusion0001), publicados para o shadPS4 em
[shadps4-emu/ps4_cheats#145](https://github.com/shadps4-emu/ps4_cheats/pull/145) e adaptados a
este executável. O patch de 240 FPS foi adaptado por Felipe Prado a partir do de 120 FPS.
Trapaças: adaptadas dos [cheats GoldHEN](https://github.com/GoldHEN/GoldHEN_Cheat_Repository) de
Celogamez.

Também usados: [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9),
[FFmpeg](https://ffmpeg.org), [SDL3](https://github.com/libsdl-org/SDL),
[Dear ImGui](https://github.com/ocornut/imgui), [sirit](https://github.com/shadps4-emu/sirit),
[magic_enum](https://github.com/Neargye/magic_enum), [miniz](https://github.com/richgel999/miniz),
[xbyak](https://github.com/herumi/xbyak),
[Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator),
[FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan) e o AMD FidelityFX SDK,
[MSYS2](https://www.msys2.org), [LLVM](https://llvm.org) e [Pillow](https://python-pillow.org).
No macOS: o driver Vulkan KosmicKrisp do [Mesa](https://mesa3d.org) (da release do shadPS4 para
macOS), o MetalFX da Apple, o [mimalloc](https://github.com/microsoft/mimalloc) e o
[PyInstaller](https://pyinstaller.org) para o aplicativo.

## Licença

GNU GPL v2 ou posterior ([LICENSE](LICENSE)). Componentes de terceiros mantêm suas licenças.
