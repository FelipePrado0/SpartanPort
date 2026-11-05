# Teclas do teclado na lira das Musas

Continuação de `docs/specs/2026-10-22-rust-runner-design.md` (ícones de botões no jogo).
Branch: `pr/lyre-keyboard-glyphs`.

**Situação antes:** com o estilo de ícones *Keyboard* os tutoriais, a lista de golpes e os QTEs
mostram a tecla de cada botão, mas a lira das Musas (Câmara da Chama, `OLY150.WAD`) continua com os
símbolos do PlayStation: só os estilos de controle (Xbox, Nintendo, igual ao meu controle) a
repintam. Quem joga no teclado vê, no único quebra-cabeça que pede os quatro botões, um símbolo
que não aparece em nenhum outro lugar.

## Spec (SDD)

* **Objetivo:** como jogador de teclado e mouse, quero ver nos quatro discos da lira a tecla (ou
  botão do mouse) que aperta Cross, Circle, Square e Triangle, para resolver o quebra-cabeça sem
  traduzir símbolos do PlayStation de cabeça.
* **Contrato:**
  - O estilo `keyboard` passa a desenhar as sete texturas da lira (`Form::Lyre` e `Form::Gold`),
    como os estilos de controle já faziam; `glyphs::textures("keyboard")` é a lista de
    `TEXTURES` mais `KEYBOARD_TEXTURES`.
  - Cada disco mostra o rótulo de `key_label()` da tecla que `key_buttons(SPARTAN_KEY_MAP)` dá ao
    botão do disco (os mesmos rótulos curtos dos outros ícones de teclado: `SPACE`, `CTRL`, `LMB`,
    `RMB`, `WH UP`, `NUM 4`...). A tinta, o contorno, o relevo e o brilho são os dos estilos de
    controle: só o texto muda.
  - O rótulo é centrado no disco, com a altura de letra do disco; um rótulo mais largo que 1,5
    raio encolhe até caber (regra que já valia para os rótulos de controle).
  - A fonte da lira (Bodoni Moda) não tem setas: as teclas de seta aparecem por extenso (`UP`,
    `DOWN`, `LEFT`, `RIGHT`) só no estilo teclado.
  - `glyphs::VERSION` sobe de 12 para 13: as cópias de `out/glyphs` já feitas (que com o teclado
    não tinham `OLY150.WAD`) são refeitas na próxima partida.
  - Com os estilos Xbox, Nintendo e igual ao meu controle, a lira e todas as outras texturas
    ficam como estavam; o mapa de teclado não entra nessas cópias.
* **Critérios de aceite:**
  - QUANDO o estilo for `keyboard` O SISTEMA DEVE repintar os quatro discos de cada uma das sete
    texturas da lira com a tecla mapeada ao botão do disco, mantendo tamanho, formato e blocos
    fora do canto da lira iguais aos do jogo.
  - QUANDO uma tecla do mapa mudar (`circle=L`) O SISTEMA DEVE mudar o rótulo só do disco desse
    botão e refazer as cópias (o carimbo `stamp.json` já leva o mapa de teclado).
  - QUANDO o botão estiver no mouse (`Mouse Left`, `Mouse Right`) O SISTEMA DEVE desenhar `LMB` e
    `RMB`, como as outras texturas de teclado.
  - QUANDO o nome da tecla for longo (`Right Shift`, `Keypad 4`, `Left Bracket`) O SISTEMA DEVE
    desenhar o rótulo curto de `key_label()` inteiro dentro do disco.
  - QUANDO a tecla for uma seta O SISTEMA DEVE escrever `UP`, `DOWN`, `LEFT` ou `RIGHT` no disco,
    não o nome interno do ícone (`dpup`).
  - QUANDO o estilo for Xbox ou Nintendo O SISTEMA DEVE produzir os mesmos bytes de antes nas
    cópias de `R_UI.WAD`, `R_PERM.WAD` e `OLY150.WAD`, mesmo com um mapa de teclado definido.
  - QUANDO o estilo for `keyboard` O SISTEMA DEVE manter `R_UI.WAD` e `R_PERM.WAD` iguais ao que
    eram antes desta mudança.
* **Requisitos não funcionais:** a pasta do jogo continua só de leitura; o tempo de montagem das
  cópias sobe pelo `OLY150.WAD` (as mesmas sete texturas que os estilos de controle já refaziam);
  o tamanho em disco do `out/glyphs` com o teclado passa de cerca de 68 MB para cerca de 410 MB, o
  mesmo dos outros estilos.
* **Arquivos afetados:** `launcher-rust/src/runner/glyphs.rs`, `launcher-rust/src/runner/mod.rs`
  (testes do carimbo), `launcher-rust/tests/dump.rs`, os três README e o registro de desvios do Python
  (`docs/specs/2026-10-22-rust-runner-parity.md`, desvio 18).
* **Plano técnico:** `lyre_paint` escolhe o mapa de botões como `render` já escolhia
  (`host_buttons`: teclado para `keyboard`, controle para os outros) e passa o rótulo pelo mesmo
  `glyph()`; `textures("keyboard")` deixa de filtrar as formas da lira. Os discos de ouro (mecanismo
  dourado) usam o mesmo caminho: A, B, X e Y são desenhadas como traços, qualquer outro rótulo vai
  na fonte da lira, como já acontecia com letras fora dessas quatro.
* **Validação:** testes de unidade em `glyphs.rs`; `cargo fmt --check`, `cargo clippy
  --all-targets -- -D warnings`, `cargo test`, `cargo build --release`; montagem das cópias sobre
  o dump real nos mapas `cross=Space,circle=Left Ctrl,square=Mouse Left,triangle=Mouse Right` e
  `triangle=I,square=J,cross=K,circle=L`, com as texturas extraídas e olhadas; Xbox e Nintendo
  byte a byte iguais a antes. Não testado: dentro do jogo (a sala da lira) e no Linux.
* **Aprovação:** pendente do mantenedor.

## Desvio do Python

O Python (`scripts/glyphs.py`, congelado) deixa a lira com os símbolos do PlayStation no estilo
teclado e tem `VERSION = 12`. O launcher Rust passa a desenhar a lira e usa `VERSION = 13`; está
registrado como desvio 18 em `docs/specs/2026-10-22-rust-runner-parity.md`.
