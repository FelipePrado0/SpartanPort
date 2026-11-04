# Teclas alternativas por botão

Branch: `pr/alternative-keys`.

## Spec (SDD)

* **Objetivo:** como jogador de teclado e mouse, quero até duas teclas extras (ou botões do mouse)
  para cada botão do PS4, para manter o Quadrado no clique esquerdo no combate e também apertá-lo
  com o J ou com o Keypad 4 nos minigames, como a lira das Musas, sem trocar o mapeamento toda vez.
* **Contrato:**
  - Runtime (`src/runtime_pad.c`): um valor de `SPARTAN_KEY_MAP` (e de `SPARTAN_PAD_MAP`) aceita
    vários nomes ligados por `|`: `cross=Space|K,square=Mouse Left|J`. A entrada troca as fontes do
    botão por todas as listadas. Os limites são 4 fontes por botão e 128 entradas no mapa; o mapa
    tem lugar para os 31 botões com 4 fontes cada, e o valor é lido inteiro, qualquer que seja o
    tamanho. Um nome inválido da
    lista é registrado e pulado sozinho; sem nenhum nome válido a entrada é pulada inteira e o
    botão fica como estava; `None` sozinho continua tirando todas as fontes; nomes repetidos
    contam uma vez. Uma entrada de um só nome se comporta como antes.
  - Launcher: os ajustes `controls.key_alt` e `controls.key_alt2` (`{botão: entrada}`: a primeira e a
    segunda alternativa) em `settings.json`, cada um escrito só quando tem ao menos uma entrada.
    `controls_layout` continua 4 e as chaves antigas não mudam; um `settings.json` sem `key_alt2`
    (ou só com `key_alt`) carrega e grava exatamente como antes.
    Exemplo (principais: Cross Space, Circle Left Ctrl, Square Mouse Left, Triangle Mouse Right;
    alternativas K, L, J, I):
    `"controls": {"key": {"circle": "Left Ctrl"}, "key_alt": {"cross": "K", "circle": "L", "square": "J", "triangle": "I"}}`
    vira `SPARTAN_KEY_MAP=...cross=Space|K,circle=Left Ctrl|L,square=Mouse Left|J,triangle=Mouse Right|I...`.
    Com `"key_alt2": {"cross": "Keypad 2"}` a entrada vira `cross=Space|K|Keypad 2`; uma segunda
    alternativa sozinha (sem a primeira) é escrita logo depois da principal (`cross=Space|Keypad 2`),
    sem nome vazio no meio, e os nomes repetidos entram uma vez.
  - O launcher escreve `botão=<principal>|<alternativa>[|<segunda>]` sempre que há alternativa, mesmo com a
    principal no padrão do jogo (a entrada troca todas as fontes do botão, então o padrão precisa
    ser citado). Sem principal ("None") só a alternativa é escrita. Alternativas vazias, "None",
    iguais à principal (ou à primeira alternativa, no caso da segunda) ou com `,`, `=` ou `|` não são
    enviadas. Com Rage of Sparta em uma entrada, L3 e R3 continuam sem fontes e as alternativas
    deles não são enviadas.
  - Tela Controls: segunda e terceira caixa por linha da tabela de teclado e mouse (a primeira e a
    segunda alternativa), cada uma com o botão `×` para esvaziá-la; captura de tecla, botão do mouse
    e roda como na principal. A seta da linha devolve a principal ao padrão e remove as duas
    alternativas; "Reset keyboard & mouse to defaults" remove todas. As caixas têm 152 px para a
    tabela caber em 1280x800 e 1920x1080 (en, pt, ar).
  - Conflito: uma tecla usada por dois slots (principal, alternativa ou segunda alternativa, de
    botões diferentes ou do mesmo) troca com o outro, como as principais já faziam. A tecla que um slot
    cede vai para o slot que tinha a tecla escolhida; sem nada para ceder, uma principal fica "None"
    e uma alternativa some.
  - Ícones de teclado no jogo (`src/runner/glyphs.rs`): mostram a tecla principal. O ajuste novo
    *Keys on the icons* (`glyph_keys`: `main`, padrão, `alternative` ou `lyre`; variável
    `SPARTAN_GLYPH_KEYS`, enviada só quando não é `main`) mostra a alternativa nos botões que
    têm uma e a principal nos outros: com `alternative` em todos os ícones, com `lyre` ("Alternative
    key on the lyre only") só na lira das Musas (os quatro discos e a legenda do mecanismo dourado),
    e todos os outros ícones de teclado continuam com a principal. A linha só aparece com os estilos
    que desenham o teclado (*Keyboard* e *Match my controller*). `glyphs::shown_keys` reduz o mapa às
    teclas mostradas antes de `glyphs::build` carimbar as cópias, então elas só são redesenhadas
    quando uma tecla mostrada muda. Com `lyre`, `glyphs::lyre_keys` reduz o mapa às alternativas dos
    quatro botões da lira e `Choice::lyre_key_map` leva isso só ao desenho da lira; o carimbo ganha
    o elemento `["lyre", "<mapa>"]` depois do mapa de teclas, e só nesse caso, então o carimbo de
    `main` e de `alternative` não muda. `VERSION` dos glifos não muda.
* **Critérios de aceite:**
  - QUANDO `SPARTAN_KEY_MAP` tiver `cross=Space|K` O SISTEMA DEVE apertar Cross com Space e com K, e
    com nenhuma tecla padrão do botão.
  - QUANDO uma lista tiver um nome inválido O SISTEMA DEVE registrar esse nome e usar os outros.
  - QUANDO nenhum nome da lista for válido O SISTEMA DEVE deixar o botão com as fontes que tinha.
  - QUANDO o valor for `None` O SISTEMA DEVE deixar o botão sem fontes, como hoje.
  - QUANDO todos os botões tiverem quatro fontes O SISTEMA DEVE guardar todas, sem estourar o mapa.
  - QUANDO o valor de `SPARTAN_KEY_MAP` tiver mais de 2048 bytes O SISTEMA DEVE aplicar todas as
    entradas, a última inclusive.
  - QUANDO uma lista tiver um nome repetido ou vazio antes da alternativa (`cross=Space|Space|K`,
    `cross=Space||K`) O SISTEMA DEVE desenhar `K` como a alternativa nos ícones.
  - QUANDO `move_up`, `rage` ou `grab` tiverem várias fontes O SISTEMA DEVE responder a qualquer uma.
  - QUANDO um `settings.json` não tiver `key_alt` (ou tiver outra forma que um objeto) O SISTEMA DEVE
    carregá-lo sem alternativas e gravar o mesmo `controls` de antes.
  - QUANDO só a alternativa de um botão estiver definida O SISTEMA DEVE escrever a principal padrão
    antes dela (`cross=Space|K`).
  - QUANDO um botão tiver as duas alternativas O SISTEMA DEVE escrever a principal, a primeira e a
    segunda, nessa ordem (`cross=Space|9|Keypad 2`).
  - QUANDO só a segunda alternativa de um botão estiver definida O SISTEMA DEVE escrever a principal e
    a segunda sem nome vazio entre elas (`cross=Space|Keypad 2`).
  - QUANDO a segunda alternativa repetir a principal ou a primeira alternativa O SISTEMA NÃO DEVE
    enviá-la.
  - QUANDO o jogador capturar para uma caixa uma tecla que já está em qualquer uma das três caixas de
    qualquer botão O SISTEMA DEVE trocar as duas.
  - QUANDO o jogador usar a seta da linha ou o reset da tabela O SISTEMA DEVE remover as duas
    alternativas.
  - QUANDO um `settings.json` só tiver `key_alt` O SISTEMA DEVE carregá-lo e gravar o mesmo `controls`,
    sem `key_alt2`.
  - QUANDO um botão tiver as duas alternativas e `glyph_keys` for `alternative` ou `lyre` O SISTEMA
    DEVE desenhar a primeira alternativa e nunca a segunda.
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
  - QUANDO `glyph_keys` for `lyre` O SISTEMA DEVE desenhar na lira das Musas (discos e mecanismo
    dourado) a alternativa de cada botão que tem uma, a principal nos botões sem alternativa, e
    desenhar a tecla principal em todas as outras texturas de teclado.
  - QUANDO `glyph_keys` for `lyre` e a alternativa de um dos quatro botões da lira mudar O SISTEMA DEVE
    redesenhar as cópias dos ícones; QUANDO mudar a de outro botão O SISTEMA NÃO DEVE.
  - QUANDO `glyph_keys` for `main` ou `alternative` O SISTEMA DEVE carimbar as cópias exatamente como
    antes da escolha `lyre` existir.
  - QUANDO `settings.json` tiver `glyph_keys` `lyre` O SISTEMA DEVE enviar `SPARTAN_GLYPH_KEYS=lyre`
    e, depois de salvar e reabrir, mostrar a mesma escolha na linha de três valores.
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
  - `Slot::{Main, Alternative, SecondAlternative}` e `bind_key([principais, alternativas, segundas],
    botão, slot, nome)` aplicam a regra de conflito nos três conjuntos. `ControlsView` ganha `key_alt`,
    `key_alt2` e `capturing_slot`, e as mensagens `Capture`, `Captured` e `Clear` levam o slot.
  - `keyboard_map` escreve `principal|alternativa|segunda`, pulando o que está vazio ou repetido; o
    gamepad não tem alternativas. O runtime já lê até 4 nomes por botão.
  - Limite conhecido: o `SPARTAN_KEY_MAP` não distingue a primeira da segunda alternativa; com a
    primeira caixa vazia, a segunda é escrita no lugar dela (`cross=Space|Keypad 2`) e é essa que os
    ícones mostram com `alternative` ou `lyre`. Com as duas preenchidas os ícones mostram sempre a
    primeira.
  - `glyph_keys` é um ajuste de `settings.json` fora do arquivo novo (como `controls_device`), com a
    variável `SPARTAN_GLYPH_KEYS`; `glyph_mounts` entrega ao `build` o mapa já reduzido por
    `shown_keys` (e, com `lyre`, o mapa de `lyre_keys`), e `key_buttons` também fica com a principal
    quando recebe um mapa com `|`.
  - Decisão: o runtime copia o valor inteiro para o heap (`SDL_strdup`) antes de separá-lo, em vez
    de um buffer de 2048 bytes que cortava em silêncio um mapa longo e perdia as últimas entradas;
    sem memória, nada é aplicado (tudo ou nada). Há um teste com mais de 2048 bytes cuja última
    entrada precisa valer, e outro no launcher com o pior caso (todas as linhas com os nomes mais
    longos).
  - Os ícones leem os nomes como o runtime: cortam os espaços, pulam nomes vazios e repetidos (sem
    diferenciar maiúsculas) e o `None` ao lado de outro nome, então `cross=Space|Space|K` tem a
    alternativa `K` e um nome vazio não esconde o seguinte. Um nome que o SDL não conhece não é
    descartado (o launcher não tem a tabela dele).
* **Validação:** `pad-map-test` (Windows, MSYS2 CLANG64), `cargo fmt --check`,
  `cargo clippy --all-targets -- -D warnings`, `cargo test` e `cargo build --release` no
  `launcher-rust/`; teste no jogo (lira das Musas) pelo mantenedor.
