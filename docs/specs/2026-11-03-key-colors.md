# Cores das teclas dos botões nos ícones de teclado

Branch: `pr/lyre-only-alternative-keys` (depois de `docs/specs/2026-11-02-alternative-keys.md`).

## Spec (SDD)

* **Objetivo:** como jogador de teclado e mouse, quero que as teclas dos quatro botões de ação (X,
  Círculo, Quadrado e Triângulo) apareçam coloridas nos ícones de teclado do jogo, na lira das Musas
  e nos outros ícones, para achar o botão certo de relance, sem mudar nenhum outro ícone.
* **Contrato:**
  - Launcher (tela Controls, junto de *Keys on the icons*, só com os estilos que desenham o teclado,
    *Keyboard* e *Match my controller*): a escolha *Key colors* (`glyph_key_colors`: `gray`, padrão,
    `playstation` ou `custom`) e, só com `custom`, quatro caixas (*Face button colors*) com uma cor
    da paleta curta por botão (`glyph_color_cross`, `glyph_color_circle`, `glyph_color_square`,
    `glyph_color_triangle`: `gray`, `white`, `blue`, `orange`, `green`, `red`, `pink`, `yellow`,
    `purple`). São ajustes de `settings.json` fora do arquivo novo (como `glyph_keys`): um
    `settings.json` antigo carrega como `gray` e nada é gravado até o jogador mudar.
  - Variável `SPARTAN_GLYPH_KEY_COLORS` (`playstation` ou `custom`; `gray` não é enviado, como
    `main` em `SPARTAN_GLYPH_KEYS`) e, só com `custom`, `SPARTAN_GLYPH_KEY_COLOR_MAP` com as entradas
    `botão=cor` dos botões que não estão em cinza (`cross=blue,circle=orange`). Com `custom` sem
    nenhuma cor escolhida, ou com um nome fora da paleta, o mapa não é enviado e nada é colorido.
  - `glyphs::face_colors` reduz esses valores a um texto canônico, `botão=rrggbb` na ordem X, Círculo,
    Quadrado, Triângulo (nomes da paleta ou seis dígitos hex, com ou sem `#`; o que não é um desses
    botões, nem uma cor, nem difere da tinta cinza, sai). `Choice::face_colors` leva esse texto ao
    desenho: `render` (ícones inline, QTE) e `lyre_paint` (discos e legenda do mecanismo dourado)
    trocam só a cor de preenchimento da tecla desses quatro botões; a tinta cinza (240, 240, 240) é a
    cor de todo o resto, e contorno, relevo e brilho continuam como estão. Na lira dourada, a
    legenda é gravada em tons da cor escolhida em vez do tom do ouro.
  - PlayStation colors usa as cores que o próprio launcher usa nos símbolos: X `#6b9ae8`, Círculo
    `#e5555e`, Quadrado `#d683d6`, Triângulo `#45c4a2`. A paleta: gray `#f0f0f0` (a tinta), white
    `#ffffff`, blue `#428ee8`, orange `#f28c28`, green `#6cc44a`, red `#e84842`, pink `#d683d6`,
    yellow `#f4c430`, purple `#a06ee6`.
  - Cópias dos ícones: o carimbo ganha `["colors", "<texto canônico>"]` depois do mapa de teclas (e
    do `["lyre", ...]`), só quando há alguma cor, então o carimbo sem cor é o mesmo de antes, a
    `VERSION` não muda e a saída com `gray` é a mesma, byte a byte. Mudar uma cor redesenha as
    cópias. Só o estilo de teclado tem cores: os outros estilos não carimbam nada.
* **Critérios de aceite:**
  - QUANDO `glyph_key_colors` for `gray` (ou o ajuste não existir) O SISTEMA DEVE desenhar e carimbar
    os ícones exatamente como antes.
  - QUANDO `glyph_key_colors` for `playstation` ou `custom` e o estilo for Keyboard O SISTEMA DEVE
    desenhar as teclas de X, Círculo, Quadrado e Triângulo na cor escolhida, na lira (discos e
    mecanismo dourado) e nas outras texturas de teclado, e as teclas dos outros botões na tinta.
  - QUANDO só alguns botões tiverem cor em `custom` O SISTEMA DEVE mudar só os discos e ícones
    deles.
  - QUANDO o estilo for Xbox ou Nintendo O SISTEMA NÃO DEVE aplicar as cores.
  - QUANDO uma cor mudar O SISTEMA DEVE redesenhar as cópias; QUANDO o valor for outro texto da mesma
    cor (nome ou hex) O SISTEMA NÃO DEVE.
  - QUANDO `glyph_key_colors` for `custom` O SISTEMA DEVE mostrar as quatro caixas, e só nesse caso.
  - QUANDO os ajustes forem salvos e o launcher reaberto O SISTEMA DEVE mostrar as mesmas escolhas.
  - Exemplo: *Custom* com Quadrado e X azuis e Triângulo e Círculo laranjas, alternativas (primeira)
    5, 6, 9 e 0, segundas no Keypad 4, 2, 8 e 6 e *Keys on the icons* em *Alternative key on the
    lyre only* desenham na lira 5, 6, 9 e 0 coloridos e, nos outros ícones, Mouse Left, Space,
    Mouse Right e Left Ctrl (LMB, SPACE, RMB e CTRL) com as mesmas cores.
* **Requisitos não funcionais:** sem cor escolhida nada muda: os testes de fixtures e de PSNR dos
  glifos continuam sem alteração. O launcher Python continua congelado.
* **Arquivos afetados:**
  - `launcher-rust/src/runner/glyphs.rs`, `launcher-rust/src/runner/mod.rs`
  - `launcher-rust/src/core/catalog.rs`, `launcher-rust/src/core/options.rs`,
    `launcher-rust/src/core/env.rs`, `launcher-rust/src/core/i18n.rs`
  - `launcher-rust/src/ui/pages/controls.rs`
  - testes do launcher (`tests/env_contract.rs`, `tests/ui_pages.rs`, `tests/ui_save.rs`)
  - `README.md`, `README.pt-BR.md`, `README.es.md`
* **Validação:** `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings`, `cargo test` e
  `cargo build --release` no `launcher-rust/`; pré-visualizações PNG da lira com a dump do jogo;
  teste no jogo pelo mantenedor.
