# Teclas alternativas por botão

Branch: `pr/alternative-keys`.

## Spec (SDD)

* **Objetivo:** como jogador de teclado e mouse, quero uma segunda tecla (ou botão do mouse) para
  cada botão do PS4, para manter o Quadrado no clique esquerdo no combate e também apertá-lo com o
  J nos minigames, como a lira das Musas, sem trocar o mapeamento toda vez.
* **Contrato:**
  - Runtime (`src/runtime_pad.c`): um valor de `SPARTAN_KEY_MAP` (e de `SPARTAN_PAD_MAP`) aceita
    vários nomes ligados por `|`: `cross=Space|K,square=Mouse Left|J`. A entrada troca as fontes do
    botão por todas as listadas. Os limites são 4 fontes por botão, 128 entradas no mapa e 2048
    bytes no valor; o mapa tem lugar para os 31 botões com 4 fontes cada. Um nome inválido da
    lista é registrado e pulado sozinho; sem nenhum nome válido a entrada é pulada inteira e o
    botão fica como estava; `None` sozinho continua tirando todas as fontes; nomes repetidos
    contam uma vez. Uma entrada de um só nome se comporta como antes.
  - Launcher: o ajuste `controls.key_alt` (`{botão: entrada}`) em `settings.json`, escrito só quando
    existe ao menos uma alternativa. `controls_layout` continua 4 e as chaves antigas não mudam.
    Exemplo (principais: Cross Space, Circle Left Ctrl, Square Mouse Left, Triangle Mouse Right;
    alternativas K, L, J, I):
    `"controls": {"key": {"circle": "Left Ctrl"}, "key_alt": {"cross": "K", "circle": "L", "square": "J", "triangle": "I"}}`
    vira `SPARTAN_KEY_MAP=...cross=Space|K,circle=Left Ctrl|L,square=Mouse Left|J,triangle=Mouse Right|I...`.
  - O launcher escreve `botão=<principal>|<alternativa>` sempre que há alternativa, mesmo com a
    principal no padrão do jogo (a entrada troca todas as fontes do botão, então o padrão precisa
    ser citado). Sem principal ("None") só a alternativa é escrita. Alternativas vazias, "None",
    iguais à principal ou com `,`, `=` ou `|` não são enviadas. Com Rage of Sparta em uma entrada,
    L3 e R3 continuam sem fontes e a alternativa deles não é enviada.
  - Tela Controls: segunda caixa por linha da tabela de teclado e mouse, com o botão `×` para
    esvaziá-la; captura de tecla, botão do mouse e roda como na principal. A seta da linha devolve
    a principal ao padrão e remove a alternativa; "Reset keyboard & mouse to defaults" remove todas.
  - Conflito: uma tecla usada por dois botões (principal ou alternativa) troca com o outro, como as
    principais já faziam. A tecla que um slot cede vai para o slot que tinha a tecla escolhida; sem
    nada para ceder, uma principal fica "None" e uma alternativa some.
  - Ícones de teclado no jogo (`src/runner/glyphs.rs`): mostram a tecla principal. O ajuste novo
    *Keys on the icons* (`glyph_keys`: `main`, padrão, ou `alternative`; variável
    `SPARTAN_GLYPH_KEYS`, enviada só quando é `alternative`) mostra a alternativa nos botões que
    têm uma e a principal nos outros. A linha só aparece com os estilos que desenham o teclado
    (*Keyboard* e *Match my controller*). `glyphs::shown_keys` reduz o mapa às teclas mostradas antes
    de `glyphs::build` carimbar as cópias, então elas só são redesenhadas quando uma tecla mostrada
    muda. `VERSION` dos glifos não muda.
* **Critérios de aceite:**
  - QUANDO `SPARTAN_KEY_MAP` tiver `cross=Space|K` O SISTEMA DEVE apertar Cross com Space e com K, e
    com nenhuma tecla padrão do botão.
  - QUANDO uma lista tiver um nome inválido O SISTEMA DEVE registrar esse nome e usar os outros.
  - QUANDO nenhum nome da lista for válido O SISTEMA DEVE deixar o botão com as fontes que tinha.
  - QUANDO o valor for `None` O SISTEMA DEVE deixar o botão sem fontes, como hoje.
  - QUANDO todos os botões tiverem quatro fontes O SISTEMA DEVE guardar todas, sem estourar o mapa.
  - QUANDO `move_up`, `rage` ou `grab` tiverem várias fontes O SISTEMA DEVE responder a qualquer uma.
  - QUANDO um `settings.json` não tiver `key_alt` (ou tiver outra forma que um objeto) O SISTEMA DEVE
    carregá-lo sem alternativas e gravar o mesmo `controls` de antes.
  - QUANDO só a alternativa de um botão estiver definida O SISTEMA DEVE escrever a principal padrão
    antes dela (`cross=Space|K`).
  - QUANDO a principal de um botão for "None" e houver alternativa O SISTEMA DEVE escrever só a
    alternativa.
  - QUANDO o jogador capturar para uma alternativa uma tecla que já é principal ou alternativa de
    outro botão O SISTEMA DEVE trocar as duas.
  - QUANDO um evento de captura chegar para a outra caixa da linha O SISTEMA NÃO DEVE ligar nada.
  - QUANDO o jogador limpar a alternativa O SISTEMA DEVE manter a principal.
  - QUANDO houver só uma alternativa e nenhuma mudança nas principais O SISTEMA DEVE ativar o reset
    da tabela.
  - QUANDO o estilo dos ícones for Keyboard e um botão tiver alternativa O SISTEMA DEVE desenhar a
    tecla principal, salvo com `glyph_keys=alternative`, que desenha a alternativa onde houver e a
    principal nos outros botões.
  - QUANDO só uma alternativa mudar e `glyph_keys` for `main` O SISTEMA NÃO DEVE redesenhar as
    cópias dos ícones.
* **Requisitos não funcionais:** o formato novo só aparece quando o jogador define uma alternativa;
  sem alternativas, `SPARTAN_KEY_MAP` e `settings.json` ficam idênticos aos de antes. O launcher
  Python continua congelado: o desvio está nos documentos de paridade (`2026-10-21-rust-launcher-parity.md`,
  item 23, e `2026-10-22-rust-runner-parity.md`, item 17).
* **Arquivos afetados:**
  - `src/runtime_pad.c`, `tests/test_pad_map.c`
  - `launcher-rust/src/core/controls.rs` (`Slot`, `bind_key`, `alternative_values`, `keyboard_map`),
    `launcher-rust/src/ui/pages/controls.rs`, `launcher-rust/src/ui/persist.rs`
  - `launcher-rust/src/core/catalog.rs`, `launcher-rust/src/core/options.rs`,
    `launcher-rust/src/core/env.rs`, `launcher-rust/src/core/i18n.rs`
  - `launcher-rust/src/runner/glyphs.rs`, `launcher-rust/src/runner/mod.rs`
  - testes do launcher (`tests/ui_pages.rs`, `tests/ui_save.rs`, `tests/env_contract.rs`) e os
    snapshots `launcher-rust/snapshots/controls-*.png`
  - `README.md`, `README.pt-BR.md`, `README.es.md`
* **Plano técnico:**
  - `apply_button_map` separa o valor por `|`; `named_source` devolve -1 para um nome inválido
    (um botão do gamepad pode valer 0); `drop_sources` tira as fontes antigas só depois de ter ao
    menos uma nova válida. `MAX_SOURCES_PER_BUTTON` e `MAP_CAPACITY` dão os limites.
  - `Slot::{Main, Alternative}` e `bind_key(principais, alternativas, botão, slot, nome)` aplicam a
    regra de conflito nos dois conjuntos. `ControlsView` ganha `key_alt` e `capturing_slot`, e as
    mensagens `Capture`, `Captured` e `Clear` levam o slot.
  - `keyboard_map` escreve `principal|alternativa`; o gamepad não tem alternativas.
  - `glyph_keys` é um ajuste de `settings.json` fora do arquivo novo (como `controls_device`), com a
    variável `SPARTAN_GLYPH_KEYS`; `glyph_mounts` entrega ao `build` o mapa já reduzido por
    `shown_keys`, e `key_buttons` também fica com a principal quando recebe um mapa com `|`.
  - Decisão: o runtime lê o valor em um buffer de 2048 bytes; o pior caso do
    launcher (todas as linhas com os nomes mais longos) fica abaixo disso, e há um teste para isso.
* **Validação:** `pad-map-test` (Windows, MSYS2 CLANG64), `cargo fmt --check`,
  `cargo clippy --all-targets -- -D warnings`, `cargo test` e `cargo build --release` no
  `launcher-rust/`; teste no jogo (lira das Musas) pelo mantenedor.
