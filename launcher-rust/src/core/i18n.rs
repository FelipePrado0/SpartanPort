//! The 15 launcher languages: text lookup, language detection and small helpers.

use std::collections::HashMap;
use std::sync::OnceLock;

use serde::Deserialize;

use super::platform;

/// Launcher languages, in the order the interface lists them.
pub const LANGS: [&str; 15] = [
    "en", "ru", "ar", "es", "pt", "fr", "de", "it", "pl", "cs", "tr", "zh", "ja", "ko", "id",
];

/// Suffix marking a warning line; it is not translated.
const WARN: &str = "  ⚠";

/// A language code from [`LANGS`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Lang(pub &'static str);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Region {
    Americas,
    Europe,
    Asia,
}

#[derive(Deserialize, Default)]
struct Trophies {
    #[serde(default)]
    texts: HashMap<String, Vec<(String, String)>>,
    #[serde(default)]
    ui: HashMap<String, HashMap<String, String>>,
}

#[derive(Deserialize, Default)]
struct Table {
    #[serde(default)]
    keys: Vec<String>,
    #[serde(default)]
    translations: HashMap<String, Vec<String>>,
    #[serde(default)]
    extra: HashMap<String, HashMap<String, String>>,
    #[serde(default)]
    ru: HashMap<String, String>,
    #[serde(default)]
    windows_languages: HashMap<String, String>,
    #[serde(default)]
    trophies: Trophies,
    #[serde(skip)]
    index: HashMap<String, usize>,
}

fn table() -> &'static Table {
    static TABLE: OnceLock<Table> = OnceLock::new();
    TABLE.get_or_init(|| {
        let mut table: Table =
            serde_json::from_str(include_str!("../../assets/i18n.json")).unwrap_or_default();
        table.index = table
            .keys
            .iter()
            .enumerate()
            .map(|(i, key)| (key.clone(), i))
            .collect();
        table
    })
}

/// Same order as the Python table: the extra texts win over the positional ones.
fn lookup(lang: &str, en: &str) -> Option<String> {
    let table = table();
    if lang == "ru" {
        return table.ru.get(en).cloned();
    }
    let extra = table.extra.get(lang).and_then(|extra| extra.get(en));
    let positional = || {
        let position = *table.index.get(en)?;
        table.translations.get(lang)?.get(position)
    };
    extra
        .filter(|text| !text.is_empty())
        .or_else(|| positional().filter(|text| !text.is_empty()))
        .cloned()
}

/// Rust-only UI strings absent from the Python tables, as (English, [(language, text)]). Other
/// languages fall back to English: a translator gap to fill.
static UI_EXTRA: &[(&str, &[(&str, &str)])] = &[
    // Graphics > Effects: spatial anti-aliasing (FXAA / SMAA) and a pointer to the port's TAA.
    (
        "Anti-aliasing",
        &[
            ("zh", "抗锯齿"),
            ("pt", "Suavização de bordas"), ("es", "Suavizado de bordes"),
            ("cs", "Vyhlazování hran"),
        ],
    ),
    (
        "Smooths jagged edges. FXAA is cheap and a little soft; SMAA keeps the picture sharper and costs a bit more. Both also touch the HUD slightly. For temporal anti-aliasing at native resolution choose TAA under Upscaler (Native AA preset): the port's own, tuned for this game. Its default values are the optimized ones, and every setting can be adjusted to your taste in the in-game menu (Insert / R3+L2) > Image. Use TAA with this set to Off.",
        &[
            ("zh", "消除锯齿边缘。FXAA 开销小，但画面略软；SMAA 画面更锐利，开销略高。两者都会轻微影响 HUD。若要在原生分辨率下使用时间性抗锯齿，请在“超分辨率”下选择 TAA（原生 AA 预设）：这是本移植版自带、针对本游戏调校的方案。其默认值已经过优化，所有设置都可在游戏内菜单（Insert / R3+L2）> 画面 中按个人喜好调整。使用 TAA 时请将此项设为关闭。"),
            ("pt", "Suaviza as bordas serrilhadas. O FXAA é barato e um pouco suave; o SMAA mantém a imagem mais nítida e custa um pouco mais. Ambos também afetam levemente a HUD. Para anti-aliasing temporal na resolução nativa, escolha TAA em Upscaler (preset Native AA): é o do próprio port, ajustado para este jogo. Seus valores padrão são os otimizados, e cada ajuste pode ser mudado a seu gosto no menu do jogo (Insert / R3+L2) > Imagem. Use o TAA com esta opção em Desligado."),
            ("cs", "Vyhladí zubaté hrany. FXAA je levné a trochu měkké; SMAA drží obraz ostřejší a stojí o něco víc. Obojí se lehce dotkne i HUD. Pro časové vyhlazování hran v nativním rozlišení zvolte TAA v části Upscaler (předvolba Nativní AA): je to vlastní řešení portu, vyladěné pro tuto hru. Jeho výchozí hodnoty jsou optimalizované a každé nastavení můžete upravit podle sebe v herním menu (Insert / R3+L2) > Obraz. TAA používejte s tímto nastavením na Vypnuto."),
        ],
    ),
    // The switch attacks of the Controls page, in the Python's Portuguese and Spanish.
    (
        "Switch attacks on L1 + d-pad and L1 + Cross",
        &[
            ("zh", "使用 L1 + 方向键及 L1 + 叉键发动切换攻击"),
            ("pt", "Ataques de troca com L1 + direcional e L1 + X"),
            ("es", "Ataques de cambio con L1 + cruceta y L1 + X"),
            ("cs", "Útoky při výměně zbraně na L1 + D-pad a L1 + Křížek"),
        ],
    ),
    (
        "L1 + a d-pad direction switches to that weapon with its switch attack (the weapon in Kratos' hands does its attack again). L1 + Cross does the switch attack of the weapon in his hands instead of cycling to the next weapon. The d-pad alone switches as before. Applied when the game starts.",
        &[
            ("zh", "L1 + 方向键会切换至对应武器并使出切换攻击（若已持有该武器，则再次使出攻击）。L1 + 叉键会使出当前武器的切换攻击，不再轮换到下一把武器。单独按方向键仍按原方式切换。启动游戏时生效。"),
            ("pt", "L1 + uma direção do direcional troca para essa arma com o ataque de troca dela (a arma que já está na mão de Kratos repete o ataque). L1 + X faz o ataque de troca da arma na mão em vez de passar para a próxima arma. O direcional sozinho troca como antes. Aplicado ao iniciar o jogo."),
            ("es", "L1 + una dirección de la cruceta cambia a esa arma con su ataque de cambio (el arma que Kratos ya tiene en la mano repite su ataque). L1 + X hace el ataque de cambio del arma en la mano en lugar de pasar a la siguiente arma. La cruceta sola cambia como antes. Se aplica al iniciar el juego."),
            ("cs", "L1 + směr D-padu přepne na danou zbraň jejím útokem při výměně (zbraň v rukou Krata svůj útok zopakuje). L1 + Křížek provede útok při výměně zbraně v rukou místo přepnutí na další zbraň. Samotný D-pad přepíná jako dřív. Použije se při spuštění hry."),
        ],
    ),
    // Alternative keys of the Controls page.
    (
        "(alternative)",
        &[
            ("zh", "（备用）"),
            ("pt", "(alternativa)"),
            ("es", "(alternativa)"),
        ],
    ),
    (
        "(2nd alternative)",
        &[
            ("zh", "（备用 2）"),
            ("pt", "(2ª alternativa)"),
            ("es", "(2.ª alternativa)"),
        ],
    ),
    (
        "The second and third boxes of a row are alternative inputs for the same button: keep Square on the left mouse button for combat and add J and Keypad 4 for the minigames, for example. \"Clear input\" empties the main box, the × button next to a box empties that alternative, and the arrow puts the row back on its default with no alternatives.",
        &[
            ("zh", "每行的第二个和第三个输入框是同一按键的备用输入：例如，战斗时将方块保留在鼠标左键，并为小游戏添加 J 和小键盘 4。“清除绑定”会清空主输入框，输入框旁的 × 按钮会清空该备用输入，箭头会将该行恢复为默认绑定并移除所有备用输入。"),
            ("pt", "A segunda e a terceira caixa de uma linha são entradas alternativas para o mesmo botão: por exemplo, deixe o Quadrado no botão esquerdo do mouse para o combate e acrescente o J e o Keypad 4 para os minigames. \"Limpar entrada\" esvazia a caixa principal, o botão × ao lado de uma caixa esvazia essa alternativa e a seta devolve a linha ao padrão, sem alternativas."),
            ("es", "La segunda y la tercera casilla de una fila son entradas alternativas para el mismo botón: por ejemplo, deja el Cuadrado en el botón izquierdo del ratón para el combate y añade la J y el Keypad 4 para los minijuegos. \"Borrar entrada\" vacía la casilla principal, el botón × junto a una casilla vacía esa alternativa y la flecha devuelve la fila a su valor por defecto, sin alternativas."),
        ],
    ),
    (
        "Keys on the icons",
        &[
            ("zh", "图标显示的按键"),
            ("pt", "Teclas nos ícones"),
            ("es", "Teclas en los iconos"),
        ],
    ),
    (
        "Main key (default)",
        &[
            ("zh", "主按键（默认）"),
            ("pt", "Tecla principal (padrão)"),
            ("es", "Tecla principal (por defecto)"),
        ],
    ),
    (
        "Alternative key",
        &[
            ("zh", "备用按键"),
            ("pt", "Tecla alternativa"),
            ("es", "Tecla alternativa"),
        ],
    ),
    (
        "Which input the keyboard icons in the game show for a button that has an alternative: its main key, its alternative key on every icon, or its alternative key only on the Muses' lyre while the other icons keep the main key. A button without an alternative always shows its main input. Applied when the game starts.",
        &[
            ("zh", "当按键设置了备用输入时，游戏内键盘图标显示哪一个输入：主按键、所有图标上的备用按键，或仅在缪斯竖琴上显示备用按键（其他图标仍显示主按键）。没有备用输入的按键始终显示主输入。下次启动游戏生效。"),
            ("pt", "Qual entrada os ícones de teclado no jogo mostram para um botão que tem alternativa: a tecla principal, a alternativa em todos os ícones, ou a alternativa só na lira das Musas, com os outros ícones na principal. Um botão sem alternativa sempre mostra a entrada principal. Aplicado ao iniciar o jogo."),
            ("es", "Qué entrada muestran los iconos de teclado en el juego para un botón que tiene alternativa: la tecla principal, la alternativa en todos los iconos, o la alternativa solo en la lira de las Musas, con los demás iconos en la principal. Un botón sin alternativa siempre muestra su entrada principal. Se aplica al iniciar el juego."),
        ],
    ),
    (
        "Alternative key on the lyre only",
        &[
            ("zh", "仅在竖琴上显示备用按键"),
            ("pt", "Tecla alternativa só na lira"),
            ("es", "Tecla alternativa solo en la lira"),
        ],
    ),
    (
        "Key colors",
        &[
            ("zh", "按键颜色"),
            ("pt", "Cores das teclas"),
            ("es", "Colores de las teclas"),
        ],
    ),
    (
        "Gray (default)",
        &[
            ("zh", "灰色（默认）"),
            ("pt", "Cinza (padrão)"),
            ("es", "Gris (por defecto)"),
        ],
    ),
    (
        "PlayStation colors",
        &[
            ("zh", "PlayStation 配色"),
            ("pt", "Cores do PlayStation"),
            ("es", "Colores de PlayStation"),
        ],
    ),
    (
        "Custom",
        &[
            ("zh", "自定义"),
            ("pt", "Personalizadas"),
            ("es", "Personalizados"),
        ],
    ),
    (
        "Face button colors",
        &[
            ("zh", "面键颜色"),
            ("pt", "Cores dos botões"),
            ("es", "Colores de los botones"),
        ],
    ),
    (
        "Gray",
        &[
            ("zh", "灰色"),
            ("pt", "Cinza"),
            ("es", "Gris"),
        ],
    ),
    (
        "White",
        &[
            ("zh", "白色"),
            ("pt", "Branco"),
            ("es", "Blanco"),
        ],
    ),
    (
        "Blue",
        &[
            ("zh", "蓝色"),
            ("pt", "Azul"),
            ("es", "Azul"),
        ],
    ),
    (
        "Orange",
        &[
            ("zh", "橙色"),
            ("pt", "Laranja"),
            ("es", "Naranja"),
        ],
    ),
    (
        "Green",
        &[
            ("zh", "绿色"),
            ("pt", "Verde"),
            ("es", "Verde"),
        ],
    ),
    (
        "Red",
        &[
            ("zh", "红色"),
            ("pt", "Vermelho"),
            ("es", "Rojo"),
        ],
    ),
    (
        "Pink",
        &[
            ("zh", "粉色"),
            ("pt", "Rosa"),
            ("es", "Rosa"),
        ],
    ),
    (
        "Yellow",
        &[
            ("zh", "黄色"),
            ("pt", "Amarelo"),
            ("es", "Amarillo"),
        ],
    ),
    (
        "Purple",
        &[
            ("zh", "紫色"),
            ("pt", "Roxo"),
            ("es", "Morado"),
        ],
    ),
    (
        "Colour of the key labels of Cross, Circle, Square and Triangle on the keyboard icons, the Muses' lyre included; every other icon keeps its colour. PlayStation colors are the symbols' own colours; Custom picks one for each button. Applied when the game starts.",
        &[
            ("zh", "键盘图标上叉号、圆圈、方块和三角按键标签的颜色，缪斯竖琴上的也一样；其他图标保持原色。“PlayStation 配色”使用各符号自身的颜色；“自定义”可为每个按键单独选色。下次启动游戏生效。"),
            ("pt", "Cor das teclas de X, Círculo, Quadrado e Triângulo nos ícones de teclado, inclusive na lira das Musas; os outros ícones ficam como estão. \"Cores do PlayStation\" usa as cores dos próprios símbolos; \"Personalizadas\" escolhe uma cor para cada botão. Aplicado ao iniciar o jogo."),
            ("es", "Color de las teclas de X, Círculo, Cuadrado y Triángulo en los iconos de teclado, incluida la lira de las Musas; los demás iconos conservan su color. \"Colores de PlayStation\" usa los colores de los propios símbolos; \"Personalizados\" elige un color para cada botón. Se aplica al iniciar el juego."),
        ],
    ),
    (
        "Graphics card not checked: the game picks one.",
        &[
            ("zh", "尚未检测显卡：由游戏自动选择。"),("pt", "Placa de vídeo não verificada: o jogo escolhe uma."),
            ("cs", "Grafická karta není zkontrolována: hra si nějakou vybere."),
        ],
    ),
    (
        "The port folder is incomplete. Missing from {}:\n\n{}\n\nDownload the package again from the releases page and extract all of it; the launcher must stay inside that folder.",
        &[
            ("zh", "移植版文件夹不完整。{} 中缺少：\n\n{}\n\n请从发布页面重新下载安装包并完整解压；启动器必须保留在该文件夹内。"),("pt", "A pasta do port está incompleta. Faltando em {}:\n\n{}\n\nBaixe o pacote de novo na página de releases e extraia tudo; o launcher precisa ficar dentro dessa pasta."),
            ("cs", "Složka portu je neúplná. Chybí v {}:\n\n{}\n\nStáhněte balíček znovu ze stránky vydání a rozbalte ho celý; launcher musí zůstat uvnitř té složky."),
        ],
    ),
    (
        "From a source checkout, build.sh builds them.",
        &[
            ("zh", "使用源码时，可运行 build.sh 构建这些文件。"),("pt", "Num checkout do código-fonte, o build.sh gera esses arquivos."),
            ("cs", "Ze zdrojového checkoutu je vytvoří build.sh."),
        ],
    ),
    (
        "Missing or empty files:",
        &[
            ("zh", "缺失或为空的文件："),("pt", "Arquivos ausentes ou vazios:"),
            ("cs", "Chybějící nebo prázdné soubory:"),
        ],
    ),
    (
        "Initial setup",
        &[
            ("zh", "初始设置"),("pt", "Configuração inicial"),
            ("cs", "První nastavení"),
        ],
    ),
    // The setup's step line, whole: the table's "Step {} of {}" ("Etapa") is the old controller
    // wizard's. "Skip" and "Back" come from the table.
    (
        "Step {} of {} · {}",
        &[
            ("zh", "第 {} 步，共 {} 步 · {}"),("pt", "Passo {} de {} · {}"),
            ("cs", "Krok {} z {} · {}"),
        ],
    ),
    (
        "Video",
        &[
            ("zh", "视频"),("pt", "Vídeo"),
            ("cs", "Video"),
        ],
    ),
    (
        "Next",
        &[
            ("zh", "下一步"),("pt", "Avançar"),
            ("cs", "Další"),
        ],
    ),
    (
        "Finish",
        &[
            ("zh", "完成"),("pt", "Concluir"),
            ("cs", "Dokončit"),
        ],
    ),
    (
        "Choose the folder of your game dump. The update folder is optional.",
        &[
            ("zh", "选择游戏转储文件夹。更新文件夹可不选。"),("pt", "Escolha a pasta do dump do jogo. A pasta da atualização é opcional."),
            ("cs", "Vyberte složku s dumpem hry. Složka aktualizace je volitelná."),
        ],
    ),
    (
        "The game runs on this graphics card.",
        &[
            ("zh", "游戏使用此显卡运行。"),("pt", "O jogo roda nesta placa de vídeo."),
            ("cs", "Hra běží na této grafické kartě."),
        ],
    ),
    (
        "Resolution and frame rate. The in-game menu (Insert or R3+L2) changes the same values.",
        &[
            ("zh", "分辨率与帧率。游戏内菜单（Insert 或 R3+L2）也可修改这些设置。"),("pt", "Resolução e taxa de quadros. O menu do jogo (Insert ou R3+L2) muda os mesmos valores."),
            ("cs", "Rozlišení a snímková frekvence. Herní menu (Insert nebo R3+L2) mění stejné hodnoty."),
        ],
    ),
    (
        "You can change all of this later in the Graphics, Performance and Game tabs.",
        &[
            ("zh", "之后可在“画面”“性能”和“游戏”页面修改这些设置。"),("pt", "Tudo isso pode ser mudado depois nas abas Graphics, Performance e Game."),
            ("cs", "To vše můžete později změnit na kartách Grafika, Výkon a Hra."),
        ],
    ),
    (
        "{} lines kept",
        &[
            ("zh", "保留 {} 行"),
            ("pt", "{} linhas mantidas"),
            ("es", "{} líneas guardadas"),
            ("ru", "{} строк хранится"),
            ("cs", "Uchováno řádků: {}"),
        ],
    ),
    (
        "Open last_run.log",
        &[
            ("zh", "打开 last_run.log"),
            ("pt", "Abrir last_run.log"),
            ("es", "Abrir last_run.log"),
            ("ru", "Открыть last_run.log"),
            ("cs", "Otevřít last_run.log"),
        ],
    ),
    (
        "Americas",
        &[
            ("zh", "美洲"),("pt", "Américas"), ("es", "América"), ("ru", "Америка"),
            ("cs", "Amerika"),
        ],
    ),
    (
        "Europe",
        &[
            ("zh", "欧洲"),("pt", "Europa"), ("es", "Europa"), ("ru", "Европа"),
            ("cs", "Evropa"),
        ],
    ),
    (
        "Asia & Middle East",
        &[
            ("zh", "亚洲及中东"),
            ("pt", "Ásia e Oriente Médio"),
            ("es", "Asia y Oriente Medio"),
            ("ru", "Азия и Ближний Восток"),
            ("cs", "Asie a Blízký východ"),
        ],
    ),
    (
        "These files could not be read:\n\n{}\n\nThe launcher shows default values for what it could not read. The files stay as they are until you save (Save or PLAY), which writes what it shows.",
        &[
            ("zh", "无法读取以下文件：\n\n{}\n\n启动器会为无法读取的设置显示默认值。在点击“保存”或“开始游戏”前，文件保持原样；保存时会写入当前显示的设置。"),(
            "pt",
            "Não foi possível ler estes arquivos:\n\n{}\n\nO launcher mostra valores padrão no lugar do que não conseguiu ler. Os arquivos ficam como estão até você salvar (Salvar ou JOGAR), o que grava o que ele mostra.",
        ),
            ("cs", "Tyto soubory se nepodařilo přečíst:\n\n{}\n\nLauncher zobrazuje výchozí hodnoty pro to, co nepřečetl. Soubory zůstanou beze změny, dokud neuložíte (Uložit nebo HRÁT), čímž se zapíše to, co launcher ukazuje."),
        ],
    ),
    (
        "mods_presets.json could not be read, so no preset is listed and the file is left as it is. Fix or delete it to save presets again.",
        &[
            ("zh", "无法读取 mods_presets.json，因此不显示预设，文件保持原样。修复或删除该文件后，才可再次保存预设。"),(
            "pt",
            "Não foi possível ler mods_presets.json; nenhum preset é listado e o arquivo fica como está. Corrija ou apague o arquivo para voltar a salvar presets.",
        ),
            ("cs", "Soubor mods_presets.json se nepodařilo přečíst, proto se nezobrazuje žádná předvolba a soubor zůstává beze změny. Opravte ho nebo smažte, abyste mohli předvolby znovu ukládat."),
        ],
    ),
    // The drawer subtitles (ruling R56: the mockup's English and Portuguese). The Graphics and
    // Game ones are keys of the JSON table, and the Trophies and Mods ones are among its extra
    // texts in Portuguese, which `tr` reads first (the Trophies one in the 2.6.0 wording).
    (
        "Frame rate, shaders and GPU data paths. Applied when the game starts.",
        &[
            ("zh", "帧率、着色器与 GPU 数据传输方式。启动游戏时生效。"),(
            "pt",
            "Taxa de quadros, shaders e caminhos de dados da GPU. Aplicado ao iniciar o jogo.",
        ),
            ("cs", "Cesty ke snímkové frekvenci, shaderům a datům GPU. Použije se při spuštění hry."),
        ],
    ),
    (
        "Keyboard, mouse and gamepad mapping.",
        &[
            ("zh", "键盘、鼠标与手柄映射。"),("pt", "Mapeamento de teclado, mouse e controle."),
            ("cs", "Mapování klávesnice, myši a gamepadu."),
        ],
    ),
    (
        "Launcher options, diagnostics and system tools.",
        &[
            ("zh", "启动器选项、诊断与系统工具。"),(
            "pt",
            "Opções do launcher, diagnóstico e ferramentas do sistema.",
        ),
            ("cs", "Možnosti launcheru, diagnostika a systémové nástroje."),
        ],
    ),
    (
        "Output of the last start. last_run.log holds the same text.",
        &[
            ("zh", "上次启动的输出。相同内容也保存在 last_run.log。"),(
            "pt",
            "Saída da última execução. last_run.log tem o mesmo texto.",
        ),
            ("cs", "Výstup posledního spuštění. last_run.log obsahuje stejný text."),
        ],
    ),
    // The word of the native render resolution's label, when the tables have no whole label.
    ("Native", &[
            ("zh", "原生"),("pt", "Nativa")]),
    // GUI Scale's default choice (Advanced → Launcher).
    (
        "Auto (fits the screen)",
        &[
            ("zh", "自动（适应屏幕）"),
            ("pt", "Automático (cabe na tela)"),
            ("es", "Automático (se ajusta a la pantalla)"),
            ("cs", "Auto (přizpůsobí se obrazovce)"),
        ],
    ),
    // The Home saves chip (the Python's "• No saves yet: the game creates them in {}" and
    // "✓ Saves found in {}" without the folder) and the splash hint.
    ("• No saves yet", &[
            ("zh", "• 暂无存档"),("pt", "• Nenhum save ainda")]),
    (
        "✓ Saves found",
        &[
            ("zh", "✓ 已找到存档"),("pt", "✓ Saves encontrados"), ("es", "✓ Partidas guardadas"),
            ("cs", "✓ Uložené hry nalezeny"),
        ],
    ),
    // The Home hint line after a start that left patches out (ruling R85): the Python's
    // "{} not found in this eboot.bin at the last start (see Log)" with the word it followed.
    (
        "{} patches not found in this eboot.bin at the last start (see Log)",
        &[
            ("zh", "上次启动时，有 {} 项补丁未在此 eboot.bin 中找到匹配位置（见“日志”）"),(
            "pt",
            "{} patches não encontrados neste eboot.bin na última inicialização (veja o Registro)",
        ),
            ("cs", "Záplat nenalezeno při posledním startu v tomto eboot.bin: {} (viz Záznam)"),
        ],
    ),
    // The Home graphics card chip when the probe did not answer.
    (
        "• GPU: not checked",
        &[
            ("zh", "• GPU：尚未检测"),("pt", "• GPU: não verificada"), ("es", "• GPU: sin comprobar"),
            ("cs", "• GPU: nezkontrolováno"),
        ],
    ),
    ("any key skips", &[
            ("zh", "按任意键跳过"),("pt", "qualquer tecla pula")]),
    // The splash's two checks (ruling R97).
    (
        "Checking the settings…",
        &[
            ("zh", "正在检查设置…"),("pt", "Verificando as configurações…"),
            ("cs", "Kontroluji nastavení…"),
        ],
    ),
    (
        "Checking for updates…",
        &[
            ("zh", "正在检查更新…"),("pt", "Verificando atualizações…"),
            ("cs", "Kontroluji aktualizace…"),
        ],
    ),
    (
        "Effects",
        &[("pt", "Efeitos"), ("es", "Efectos"), ("ru", "Эффекты")],
    ),
    (
        "Embers and flames on the Home screen. Off saves GPU time.",
        &[
            ("zh", "在主页显示余烬和火焰效果。关闭可减少 GPU 开销。"),
            (
                "pt",
                "Brasas e chamas na tela inicial. Desligado economiza GPU.",
            ),
            (
                "es",
                "Brasas y llamas en la pantalla inicial. Apagado ahorra GPU.",
            ),
            (
                "ru",
                "Угли и пламя на главном экране. Выключено — экономия GPU.",
            ),
            ("cs", "Žhavé uhlíky a plameny na domovské obrazovce. Vypnuto šetří čas GPU."),
        ],
    ),
    (
        "Release",
        &[
            ("ar", "الإصدار"),
            ("es", "Lanzamiento"),
            ("pt", "Lançamento"),
            ("fr", "Version"),
            ("de", "Veröffentlichung"),
            ("it", "Versione"),
            ("pl", "Wydanie"),
            ("tr", "Sürüm"),
            ("zh", "正式版"),
            ("ja", "リリース"),
            ("ko", "릴리스"),
            ("id", "Rilis"),
            ("ru", "Релиз"),
            ("cs", "Vydání"),
        ],
    ),
    (
        "Pre-release",
        &[
            ("ar", "إصدار تجريبي"),
            ("es", "Prelanzamiento"),
            ("pt", "Pré-lançamento"),
            ("fr", "Préversion"),
            ("de", "Vorabversion"),
            ("it", "Versione preliminare"),
            ("pl", "Wersja przedpremierowa"),
            ("tr", "Ön sürüm"),
            ("zh", "预发布版"),
            ("ja", "プレリリース"),
            ("ko", "사전 릴리스"),
            ("id", "Pra-rilis"),
            ("ru", "Предрелиз"),
            ("cs", "Předběžné vydání"),
        ],
    ),
    (
        "Releases",
        &[
            ("ar", "الإصدارات"),
            ("es", "Lanzamientos"),
            ("pt", "Lançamentos"),
            ("fr", "Versions"),
            ("de", "Veröffentlichungen"),
            ("it", "Versioni"),
            ("pl", "Wydania"),
            ("tr", "Sürümler"),
            ("zh", "正式版"),
            ("ja", "リリース"),
            ("ko", "릴리스"),
            ("id", "Rilis"),
            ("ru", "Релизы"),
            ("cs", "Vydání"),
        ],
    ),
    (
        "Pre-releases",
        &[
            ("ar", "إصدارات تجريبية"),
            ("es", "Prelanzamientos"),
            ("pt", "Pré-lançamentos"),
            ("fr", "Préversions"),
            ("de", "Vorabversionen"),
            ("it", "Versioni preliminari"),
            ("pl", "Wersje przedpremierowe"),
            ("tr", "Ön sürümler"),
            ("zh", "预发布版"),
            ("ja", "プレリリース"),
            ("ko", "사전 릴리스"),
            ("id", "Pra-rilis"),
            ("ru", "Предрелизы"),
            ("cs", "Předběžná vydání"),
        ],
    ),
    (
        "new",
        &[
            ("ar", "جديد"),
            ("es", "nuevo"),
            ("pt", "novo"),
            ("fr", "nouveau"),
            ("de", "neu"),
            ("it", "nuovo"),
            ("pl", "nowe"),
            ("tr", "yeni"),
            ("zh", "新"),
            ("ja", "新着"),
            ("ko", "신규"),
            ("id", "baru"),
            ("ru", "новое"),
            ("cs", "nové"),
        ],
    ),
    (
        "{} available",
        &[
            ("ar", "{} متاح"),
            ("es", "{} disponible"),
            ("pt", "{} disponível"),
            ("fr", "{} disponible"),
            ("de", "{} verfügbar"),
            ("it", "{} disponibile"),
            ("pl", "{} dostępne"),
            ("tr", "{} mevcut"),
            ("zh", "{} 可用"),
            ("ja", "{} が利用可能"),
            ("ko", "{} 사용 가능"),
            ("id", "{} tersedia"),
            ("ru", "{} доступно"),
            ("cs", "Dostupné: {}"),
        ],
    ),
    (
        "Fetching…",
        &[
            ("ar", "جارٍ الجلب…"),
            ("es", "Obteniendo…"),
            ("pt", "Buscando…"),
            ("fr", "Récupération…"),
            ("de", "Wird abgerufen…"),
            ("it", "Recupero…"),
            ("pl", "Pobieranie…"),
            ("tr", "Alınıyor…"),
            ("zh", "正在获取…"),
            ("ja", "取得中…"),
            ("ko", "가져오는 중…"),
            ("id", "Mengambil…"),
            ("ru", "Загрузка…"),
            ("cs", "Stahuji…"),
        ],
    ),
    (
        "Could not fetch the versions",
        &[
            ("ar", "تعذّر جلب الإصدارات"),
            ("es", "No se pudieron obtener las versiones"),
            ("pt", "Não foi possível buscar as versões"),
            ("fr", "Impossible de récupérer les versions"),
            ("de", "Die Versionen konnten nicht abgerufen werden"),
            ("it", "Impossibile recuperare le versioni"),
            ("pl", "Nie udało się pobrać wersji"),
            ("tr", "Sürümler alınamadı"),
            ("zh", "无法获取版本"),
            ("ja", "バージョンを取得できませんでした"),
            ("ko", "버전을 가져올 수 없습니다"),
            ("id", "Gagal mengambil versi"),
            ("ru", "Не удалось получить версии"),
            ("cs", "Nepodařilo se načíst verze"),
        ],
    ),
    (
        "No test builds",
        &[
            ("ar", "لا توجد إصدارات اختبارية"),
            ("es", "No hay compilaciones de prueba"),
            ("pt", "Sem builds de teste"),
            ("fr", "Aucune build de test"),
            ("de", "Keine Test-Builds"),
            ("it", "Nessuna build di test"),
            ("pl", "Brak wersji testowych"),
            ("tr", "Test sürümü yok"),
            ("zh", "没有测试版本"),
            ("ja", "テストビルドはありません"),
            ("ko", "테스트 빌드 없음"),
            ("id", "Tidak ada build uji"),
            ("ru", "Нет тестовых сборок"),
            ("cs", "Žádná testovací sestavení"),
        ],
    ),
    (
        "No test builds for Linux",
        &[
            ("ar", "لا توجد إصدارات اختبارية لنظام Linux"),
            ("es", "No hay compilaciones de prueba para Linux"),
            ("pt", "Sem builds de teste para Linux"),
            ("fr", "Aucune build de test pour Linux"),
            ("de", "Keine Test-Builds für Linux"),
            ("it", "Nessuna build di test per Linux"),
            ("pl", "Brak wersji testowych dla Linuksa"),
            ("tr", "Linux için test sürümü yok"),
            ("zh", "没有适用于 Linux 的测试版本"),
            ("ja", "Linux 向けのテストビルドはありません"),
            ("ko", "Linux용 테스트 빌드 없음"),
            ("id", "Tidak ada build uji untuk Linux"),
            ("ru", "Нет тестовых сборок для Linux"),
            ("cs", "Žádná testovací sestavení pro Linux"),
        ],
    ),
    (
        "This version is older than the installed one.",
        &[
            ("ar", "هذا الإصدار أقدم من الإصدار المثبّت."),
            ("es", "Esta versión es anterior a la instalada."),
            ("pt", "Esta versão é mais antiga que a instalada."),
            ("fr", "Cette version est plus ancienne que celle installée."),
            ("de", "Diese Version ist älter als die installierte."),
            ("it", "Questa versione è più vecchia di quella installata."),
            ("pl", "Ta wersja jest starsza od zainstalowanej."),
            ("tr", "Bu sürüm, yüklü olandan eskidir."),
            ("zh", "此版本比已安装的版本旧。"),
            ("ja", "このバージョンはインストール済みのものより古いです。"),
            ("ko", "이 버전은 설치된 버전보다 오래되었습니다."),
            ("id", "Versi ini lebih lama dari yang terpasang."),
            ("ru", "Эта версия старше установленной."),
            ("cs", "Tato verze je starší než nainstalovaná."),
        ],
    ),
    (
        "FSR 3.1 (native HUD and text)",
        &[
            ("zh", "FSR 3.1（原生 HUD 与文字）"),
            ("pt", "FSR 3.1 (HUD e texto nativos)"),
            ("es", "FSR 3.1 (HUD y texto nativos)"),
            ("cs", "FSR 3.1 (nativní HUD a text)"),
        ],
    ),
    (
        "FSR 3.1 (native HUD and text): the game keeps its own 1920x1080 targets, so the HUD, text and loading screens are not stretched (FSR 3.1 draws everything at 1918x1078 with Native AA). Has the port's own sharpening in the in-game menu. Experimental.",
        &[
            ("zh", "FSR 3.1（原生 HUD 与文字）：游戏保留自身的 1920x1080 渲染目标，因此 HUD、文字和加载画面不会被拉伸（原生 AA 下的 FSR 3.1 会以 1918x1078 绘制全部内容）。游戏内菜单提供本移植版自带的锐化。实验性功能。"),
            ("pt", "FSR 3.1 (HUD e texto nativos): o jogo mantém seus próprios alvos de 1920x1080, então a HUD, o texto e as telas de carregamento não são esticados (o FSR 3.1 desenha tudo em 1918x1078 com Native AA). Tem a nitidez própria do port no menu do jogo. Experimental."),
            ("es", "FSR 3.1 (HUD y texto nativos): el juego mantiene sus propios objetivos de 1920x1080, así que el HUD, el texto y las pantallas de carga no se estiran (el FSR 3.1 dibuja todo a 1918x1078 con Native AA). Tiene el enfoque propio del port en el menú del juego. Experimental."),
            ("cs", "FSR 3.1 (nativní HUD a text): hra si ponechá vlastní cíle 1920×1080, takže HUD, text a loadingy se neroztahují (FSR 3.1 s Native AA kreslí všechno v 1918×1078). V herním menu má vlastní ostření portu. Experimentální."),
        ],
    ),
    (
        "Free camera (experimental)",
        &[
            ("pt", "Câmera livre (experimental)"),
            ("es", "Cámara libre (experimental)"),
            ("zh", "自由镜头（实验性）"),
            ("id", "Kamera bebas (eksperimental)"),
        ],
    ),
    (
        "Free camera on/off",
        &[
            ("pt", "Câmera livre: liga/desliga"),
            ("es", "Cámara libre: activar/desactivar"),
            ("zh", "自由镜头：开/关"),
            ("id", "Kamera bebas: nyala/mati"),
        ],
    ),
    (
        "Tap R3",
        &[("pt", "Toque no R3"), ("es", "Toque de R3"), ("zh", "轻按 R3"), ("id", "Ketuk R3")],
    ),
    (
        "Experimental. The right stick or the mouse turns the camera all the way around Kratos and Kratos moves the way the camera looks; L1 + right stick dodges. In play, the Free camera key (G by default) or a tap of R3 on the gamepad switches it off and on, for boss fights; quick-time events and the game's camera cuts go back to the game's camera. Also in the in-game menu.",
        &[
            ("pt", "Experimental. O analógico direito ou o mouse gira a câmera em volta do Kratos, por completo, e o Kratos anda para onde a câmera olha; L1 + analógico direito faz a esquiva. No jogo, a tecla da câmera livre (G por padrão) ou um toque no R3 do controle a desliga e liga, para as lutas de chefe; os quick-time events e os cortes de câmera do jogo voltam à câmera do jogo. Também no menu do jogo."),
            ("es", "Experimental. El stick derecho o el ratón gira la cámara alrededor de Kratos, por completo, y Kratos se mueve hacia donde mira la cámara; L1 + stick derecho esquiva. En el juego, la tecla de la cámara libre (G por defecto) o un toque de R3 en el mando la desactiva y activa, para las peleas contra jefes; los quick-time events y los cortes de cámara del juego vuelven a la cámara del juego. También en el menú del juego."),
            ("zh", "实验性功能。右摇杆或鼠标可让镜头围绕奎托斯完整旋转，奎托斯会朝镜头所看的方向移动；L1 + 右摇杆为闪避。游戏中，自由镜头按键（默认为 G）或轻按手柄上的 R3 可将其关闭和开启，适用于首领战；快速反应事件和游戏自带的镜头切换会回到游戏的镜头。游戏内菜单中也有此选项。"),
            ("id", "Eksperimental. Stik kanan atau mouse memutar kamera sepenuhnya di sekitar Kratos, dan Kratos bergerak ke arah pandangan kamera; L1 + stik kanan untuk menghindar. Dalam permainan, tombol Kamera bebas (bawaan G) atau ketukan R3 pada gamepad mematikan dan menyalakannya, untuk pertarungan bos; quick-time event dan potongan kamera game kembali ke kamera game. Juga tersedia di menu dalam game."),
        ],
    ),
    // Game page: the in-game menu's cheats and multipliers (texts of gpu/shim's tables), the
    // button that turns them all off and the trophy sound (#1135, #320, #1243).
    (
        "Automatic QTE",
        &[("zh", "自动完成 QTE"), ("pt", "QTE automático"), ("es", "QTE automático"), ("cs", "Automatické QTE"), ("ru", "Автоматические QTE"), ("fr", "QTE automatiques"), ("de", "Automatische QTEs"), ("it", "QTE automatici"), ("pl", "Automatyczne QTE"), ("tr", "Otomatik QTE"), ("id", "QTE otomatis")],
    ),
    (
        "Blue orbs (magic)",
        &[("zh", "蓝魂（魔法）"), ("pt", "Orbes azuis (magia)"), ("es", "Orbes azules (magia)"), ("cs", "Modré orby (magie)"), ("ru", "Синие сферы (магия)"), ("fr", "Orbes bleus (magie)"), ("de", "Blaue Orbs (Magie)"), ("it", "Sfere blu (magia)"), ("pl", "Niebieskie kule (magia)"), ("tr", "Mavi küreler (büyü)"), ("id", "Orb biru (sihir)")],
    ),
    (
        "Damage Kratos does to enemies.",
        &[("zh", "奎托斯对敌人造成的伤害。"), ("pt", "Dano que Kratos causa nos inimigos."), ("es", "Daño que Kratos hace a los enemigos."), ("cs", "Poškození, které Kratos působí nepřátelům."), ("ru", "Урон, который Кратос наносит врагам."), ("fr", "Dégâts que Kratos inflige aux ennemis."), ("de", "Schaden, den Kratos Gegnern zufügt."), ("it", "Danni che Kratos infligge ai nemici."), ("pl", "Obrażenia, które Kratos zadaje przeciwnikom."), ("tr", "Kratos'un düşmanlara verdiği hasar."), ("id", "Damage yang Kratos berikan ke musuh.")],
    ),
    (
        "Damage Kratos takes.",
        &[("zh", "奎托斯受到的伤害。"), ("pt", "Dano que Kratos recebe."), ("es", "Daño que recibe Kratos."), ("cs", "Poškození, které Kratos utrpí."), ("ru", "Урон, который получает Кратос."), ("fr", "Dégâts que subit Kratos."), ("de", "Schaden, den Kratos erleidet."), ("it", "Danni che Kratos subisce."), ("pl", "Obrażenia, które otrzymuje Kratos."), ("tr", "Kratos'un aldığı hasar."), ("id", "Damage yang Kratos terima.")],
    ),
    (
        "Damage dealt",
        &[("zh", "造成伤害"), ("pt", "Dano causado"), ("es", "Daño causado"), ("cs", "Způsobené poškození"), ("ru", "Наносимый урон"), ("fr", "Dégâts infligés"), ("de", "Verursachter Schaden"), ("it", "Danni inflitti"), ("pl", "Zadawane obrażenia"), ("tr", "Verilen hasar"), ("id", "Damage diberikan")],
    ),
    (
        "Damage taken",
        &[("zh", "受到伤害"), ("pt", "Dano recebido"), ("es", "Daño recibido"), ("cs", "Utržené poškození"), ("ru", "Получаемый урон"), ("fr", "Dégâts subis"), ("de", "Erlittener Schaden"), ("it", "Danni subiti"), ("pl", "Otrzymywane obrażenia"), ("tr", "Alınan hasar"), ("id", "Damage diterima")],
    ),
    (
        "Gold orbs (Rage of Sparta)",
        &[("zh", "金魂（斯巴达之怒）"), ("pt", "Orbes dourados (Fúria de Esparta)"), ("es", "Orbes dorados (Furia de Esparta)"), ("cs", "Zlaté orby (Sparťanův hněv)"), ("ru", "Золотые сферы (Ярость Спарты)"), ("fr", "Orbes dorés (Rage de Sparte)"), ("de", "Goldene Orbs (Zorn von Sparta)"), ("it", "Sfere d'oro (Furia di Sparta)"), ("pl", "Złote kule (Furia Sparty)"), ("tr", "Altın küreler (Sparta Öfkesi)"), ("id", "Orb emas (Rage of Sparta)")],
    ),
    (
        "Green orbs (health)",
        &[("zh", "绿魂（生命）"), ("pt", "Orbes verdes (vida)"), ("es", "Orbes verdes (vida)"), ("cs", "Zelené orby (zdraví)"), ("ru", "Зелёные сферы (здоровье)"), ("fr", "Orbes verts (santé)"), ("de", "Grüne Orbs (Leben)"), ("it", "Sfere verdi (salute)"), ("pl", "Zielone kule (zdrowie)"), ("tr", "Yeşil küreler (sağlık)"), ("id", "Orb hijau (nyawa)")],
    ),
    (
        "Health from each green orb. The bar stays within its maximum.",
        &[("zh", "每个绿魂恢复的生命，最高不超过生命上限。"), ("pt", "Vida de cada orbe verde. A barra fica dentro do máximo."), ("es", "Vida de cada orbe verde. La barra no pasa de su máximo."), ("cs", "Zdraví z každého zeleného orbu. Ukazatel zůstane v mezích maxima."), ("ru", "Здоровье от каждой зелёной сферы. Полоска не превышает максимум."), ("fr", "Santé de chaque orbe vert. La barre ne dépasse pas son maximum."), ("de", "Leben pro grünem Orb. Der Balken bleibt innerhalb seines Maximums."), ("it", "Salute da ogni sfera verde. La barra resta entro il suo massimo."), ("pl", "Zdrowie z każdej zielonej kuli. Pasek nie przekracza maksimum."), ("tr", "Her yeşil küreden gelen sağlık. Çubuk en yüksek değerini aşmaz."), ("id", "Nyawa dari setiap orb hijau. Bar tidak melebihi batas maksimum.")],
    ),
    (
        "Health stays full. Only Kratos is protected. Turning it off does not undo past refills.",
        &[("zh", "生命保持满值，仅保护奎托斯。关闭后不会撤销已恢复的生命。"), ("pt", "A vida fica cheia. Só Kratos é protegido. Desligar não desfaz o que já foi recuperado."), ("es", "La vida se mantiene llena. Solo protege a Kratos. Al desactivarlo no se deshace lo ya recuperado."), ("cs", "Zdraví zůstává plné. Chráněn je jen Kratos. Vypnutím se dřívější doplnění nevrátí."), ("ru", "Здоровье остаётся полным. Защищён только Кратос. Выключение не отменяет уже восполненное."), ("fr", "La santé reste pleine. Seul Kratos est protégé. Le désactiver n'annule pas ce qui a été rendu."), ("de", "Das Leben bleibt voll. Nur Kratos ist geschützt. Ausschalten macht bisherige Auffüllungen nicht rückgängig."), ("it", "La salute resta piena. Solo Kratos è protetto. Disattivarlo non annulla le ricariche già avvenute."), ("pl", "Zdrowie pozostaje pełne. Chroniony jest tylko Kratos. Wyłączenie nie cofa wcześniejszych uzupełnień."), ("tr", "Sağlık dolu kalır. Yalnızca Kratos korunur. Kapatmak önceki dolumları geri almaz."), ("id", "Nyawa tetap penuh. Hanya Kratos yang terlindungi. Mematikannya tidak membatalkan isi ulang sebelumnya.")],
    ),
    (
        "Infinite Rage of Sparta",
        &[("zh", "无限斯巴达之怒"), ("pt", "Fúria de Esparta infinita"), ("es", "Furia de Esparta infinita"), ("cs", "Nekonečný Sparťanův hněv"), ("ru", "Бесконечная Ярость Спарты"), ("fr", "Rage de Sparte infinie"), ("de", "Unendlicher Zorn von Sparta"), ("it", "Furia di Sparta infinita"), ("pl", "Nieskończona Furia Sparty"), ("tr", "Sonsuz Sparta Öfkesi"), ("id", "Rage of Sparta tanpa batas")],
    ),
    (
        "Infinite health",
        &[("zh", "无限生命"), ("pt", "Vida infinita"), ("es", "Vida infinita"), ("cs", "Nekonečné zdraví"), ("ru", "Бесконечное здоровье"), ("fr", "Santé infinie"), ("de", "Unendliches Leben"), ("it", "Salute infinita"), ("pl", "Nieskończone zdrowie"), ("tr", "Sonsuz sağlık"), ("id", "Nyawa tanpa batas")],
    ),
    (
        "Infinite item meter",
        &[("zh", "无限道具能量"), ("pt", "Barra de item infinita"), ("es", "Barra de objeto infinita"), ("cs", "Nekonečný ukazatel předmětů"), ("ru", "Бесконечная шкала предмета"), ("fr", "Jauge d'objet infinie"), ("de", "Unendliche Gegenstandsleiste"), ("it", "Barra degli oggetti infinita"), ("pl", "Nieskończony pasek przedmiotu"), ("tr", "Sonsuz eşya çubuğu"), ("id", "Meter item tanpa batas")],
    ),
    (
        "Infinite magic",
        &[("zh", "无限魔法"), ("pt", "Magia infinita"), ("es", "Magia infinita"), ("cs", "Nekonečná magie"), ("ru", "Бесконечная магия"), ("fr", "Magie infinie"), ("de", "Unendliche Magie"), ("it", "Magia infinita"), ("pl", "Nieskończona magia"), ("tr", "Sonsuz büyü"), ("id", "Sihir tanpa batas")],
    ),
    (
        "Magic from each blue orb. The bar stays within its maximum.",
        &[("zh", "每个蓝魂恢复的魔法，最高不超过魔法上限。"), ("pt", "Magia de cada orbe azul. A barra fica dentro do máximo."), ("es", "Magia de cada orbe azul. La barra no pasa de su máximo."), ("cs", "Magie z každého modrého orbu. Ukazatel zůstane v mezích maxima."), ("ru", "Магия от каждой синей сферы. Полоска не превышает максимум."), ("fr", "Magie de chaque orbe bleu. La barre ne dépasse pas son maximum."), ("de", "Magie pro blauem Orb. Der Balken bleibt innerhalb seines Maximums."), ("it", "Magia da ogni sfera blu. La barra resta entro il suo massimo."), ("pl", "Magia z każdej niebieskiej kuli. Pasek nie przekracza maksimum."), ("tr", "Her mavi küreden gelen büyü. Çubuk en yüksek değerini aşmaz."), ("id", "Sihir dari setiap orb biru. Bar tidak melebihi batas maksimum.")],
    ),
    (
        "Magic stays full.",
        &[("zh", "魔法保持满值。"), ("pt", "A magia fica cheia."), ("es", "La magia se mantiene llena."), ("cs", "Magie zůstává plná."), ("ru", "Магия остаётся полной."), ("fr", "La magie reste pleine."), ("de", "Die Magie bleibt voll."), ("it", "La magia resta piena."), ("pl", "Magia pozostaje pełna."), ("tr", "Büyü dolu kalır."), ("id", "Sihir tetap penuh.")],
    ),
    (
        "Max red orbs",
        &[("zh", "最大红魂"), ("pt", "Orbes vermelhos no máximo"), ("es", "Orbes rojos al máximo"), ("cs", "Maximum rudých orbů"), ("ru", "Максимум красных сфер"), ("fr", "Orbes rouges au maximum"), ("de", "Maximale rote Orbs"), ("it", "Sfere rosse al massimo"), ("pl", "Maksimum czerwonych kul"), ("tr", "En fazla kırmızı küre"), ("id", "Orb merah maksimum")],
    ),
    (
        "Multipliers",
        &[("zh", "倍率"), ("pt", "Multiplicadores"), ("es", "Multiplicadores"), ("cs", "Násobiče"), ("ru", "Множители"), ("fr", "Multiplicateurs"), ("de", "Multiplikatoren"), ("it", "Moltiplicatori"), ("pl", "Mnożniki"), ("tr", "Çarpanlar"), ("id", "Pengali")],
    ),
    (
        "Quick-time prompts answer themselves: single presses, repeated presses, holds and stick rotations.",
        &[("zh", "自动完成快速反应提示：单次按键、连按、长按及摇杆旋转。"), ("pt", "Os comandos de QTE se resolvem sozinhos: apertos simples, repetidos, segurados e giros do analógico."), ("es", "Las indicaciones de QTE se resuelven solas: pulsaciones simples, repetidas, mantenidas y giros de la palanca."), ("cs", "QTE výzvy se vyřeší samy: jednotlivé stisky, opakované stisky, podržení i otáčení páčkou."), ("ru", "QTE выполняются сами: одиночные, повторные и удерживаемые нажатия и вращения стика."), ("fr", "Les QTE se font tout seuls : appuis simples, répétés, maintenus et rotations du stick."), ("de", "QTE-Eingaben erledigen sich selbst: einzelnes, wiederholtes und gehaltenes Drücken sowie Stickdrehungen."), ("it", "I comandi QTE si risolvono da soli: pressioni singole, ripetute, prolungate e rotazioni della levetta."), ("pl", "Polecenia QTE wykonują się same: pojedyncze, powtarzane i przytrzymane wciśnięcia oraz obroty gałki."), ("tr", "QTE komutları kendiliğinden yapılır: tek basışlar, tekrarlı basışlar, basılı tutmalar ve çubuk çevirmeleri."), ("id", "Perintah QTE berjalan sendiri: tekan sekali, tekan berulang, tahan, dan putaran stik.")],
    ),
    (
        "Rage of Sparta from each gold orb. The bar stays within its maximum.",
        &[("zh", "每个金魂恢复的斯巴达之怒，最高不超过上限。"), ("pt", "Fúria de Esparta de cada orbe dourado. A barra fica dentro do máximo."), ("es", "Furia de Esparta de cada orbe dorado. La barra no pasa de su máximo."), ("cs", "Sparťanův hněv z každého zlatého orbu. Ukazatel zůstane v mezích maxima."), ("ru", "Ярость Спарты от каждой золотой сферы. Полоска не превышает максимум."), ("fr", "Rage de Sparte de chaque orbe doré. La barre ne dépasse pas son maximum."), ("de", "Zorn von Sparta pro goldenem Orb. Der Balken bleibt innerhalb seines Maximums."), ("it", "Furia di Sparta da ogni sfera d'oro. La barra resta entro il suo massimo."), ("pl", "Furia Sparty z każdej złotej kuli. Pasek nie przekracza maksimum."), ("tr", "Her altın küreden gelen Sparta Öfkesi. Çubuk en yüksek değerini aşmaz."), ("id", "Rage of Sparta dari setiap orb emas. Bar tidak melebihi batas maksimum.")],
    ),
    (
        "Rage of Sparta stays full.",
        &[("zh", "斯巴达之怒保持满值。"), ("pt", "A Fúria de Esparta fica cheia."), ("es", "La Furia de Esparta se mantiene llena."), ("cs", "Sparťanův hněv zůstává plný."), ("ru", "Ярость Спарты остаётся полной."), ("fr", "La Rage de Sparte reste pleine."), ("de", "Der Zorn von Sparta bleibt voll."), ("it", "La Furia di Sparta resta piena."), ("pl", "Furia Sparty pozostaje pełna."), ("tr", "Sparta Öfkesi dolu kalır."), ("id", "Rage of Sparta tetap penuh.")],
    ),
    (
        "Red orbs",
        &[("zh", "红魂"), ("pt", "Orbes vermelhos"), ("es", "Orbes rojos"), ("cs", "Rudé orby"), ("ru", "Красные сферы"), ("fr", "Orbes rouges"), ("de", "Rote Orbs"), ("it", "Sfere rosse"), ("pl", "Czerwone kule"), ("tr", "Kırmızı küreler"), ("id", "Orb merah")],
    ),
    (
        "Red orbs gained. Orbs you have and prices stay the same.",
        &[("zh", "获得的红魂数量倍率，不改变已有红魂和物品价格。"), ("pt", "Orbes vermelhos ganhos. Os que você tem e os preços não mudam."), ("es", "Orbes rojos obtenidos. Los que tienes y los precios no cambian."), ("cs", "Získané rudé orby. Orby, které máte, a ceny zůstávají stejné."), ("ru", "Получаемые красные сферы. Имеющиеся сферы и цены не меняются."), ("fr", "Orbes rouges gagnés. Ceux que vous avez et les prix ne changent pas."), ("de", "Erhaltene rote Orbs. Vorhandene Orbs und Preise bleiben gleich."), ("it", "Sfere rosse ottenute. Quelle che hai e i prezzi non cambiano."), ("pl", "Zdobywane czerwone kule. Posiadane kule i ceny się nie zmieniają."), ("tr", "Kazanılan kırmızı küreler. Sahip olduğunuz küreler ve fiyatlar değişmez."), ("id", "Orb merah yang didapat. Orb yang kamu punya dan harga tetap sama.")],
    ),
    (
        "Red orbs stay at the maximum.",
        &[("zh", "红魂保持最大值。"), ("pt", "Os orbes vermelhos ficam no máximo."), ("es", "Los orbes rojos se mantienen al máximo."), ("cs", "Rudé orby zůstávají na maximu."), ("ru", "Красных сфер всегда максимум."), ("fr", "Les orbes rouges restent au maximum."), ("de", "Rote Orbs bleiben auf dem Maximum."), ("it", "Le sfere rosse restano al massimo."), ("pl", "Czerwone kule pozostają na maksimum."), ("tr", "Kırmızı küreler en yüksek değerde kalır."), ("id", "Orb merah tetap di maksimum.")],
    ),
    (
        "The item meter (bow, Helios' head, boots) stays full.",
        &[("zh", "道具能量（弓、赫利俄斯之首、靴子）保持满值。"), ("pt", "A barra de item (arco, Cabeça de Hélios, botas) fica cheia."), ("es", "La barra de objeto (arco, Cabeza de Helios, botas) se mantiene llena."), ("cs", "Ukazatel předmětů (luk, Héliova hlava, boty) zůstává plný."), ("ru", "Шкала предмета (лук, голова Гелиоса, сандалии) остаётся полной."), ("fr", "La jauge d'objet (arc, tête d'Hélios, bottes) reste pleine."), ("de", "Die Gegenstandsleiste (Bogen, Helios' Kopf, Stiefel) bleibt voll."), ("it", "La barra degli oggetti (arco, Testa di Elio, sandali) resta piena."), ("pl", "Pasek przedmiotu (łuk, Głowa Heliosa, buty) pozostaje pełny."), ("tr", "Eşya çubuğu (yay, Helios'un kafası, çizmeler) dolu kalır."), ("id", "Meter item (busur, Head of Helios, sepatu) tetap penuh.")],
    ),
    (
        "Turn off all cheats",
        &[("zh", "关闭所有作弊"), ("pt", "Desligar todos os cheats"), ("es", "Desactivar todos los trucos"), ("cs", "Vypnout všechny cheaty"), ("ru", "Отключить все читы"), ("fr", "Désactiver toutes les astuces"), ("de", "Alle Cheats ausschalten"), ("it", "Disattiva tutti i trucchi"), ("pl", "Wyłącz wszystkie kody"), ("tr", "Tüm hileleri kapat"), ("id", "Matikan semua cheat")],
    ),
    (
        "Turns off every cheat and sets every multiplier back to 1x: the game as it was made. In game, hold F10 and press 1 to 8 to switch the cheats above, in their order.",
        &[("zh", "关闭所有作弊，并将所有倍率恢复为 1x：即游戏原本的样子。游戏中按住 F10 并按 1 到 8，可按顺序切换上方的作弊。"), ("pt", "Desliga todos os cheats e volta todos os multiplicadores para 1x: o jogo como foi feito. No jogo, segure F10 e aperte 1 a 8 para trocar os cheats acima, na ordem em que aparecem."), ("es", "Desactiva todos los trucos y devuelve todos los multiplicadores a 1x: el juego tal como se hizo. En el juego, mantén F10 y pulsa 1 a 8 para cambiar los trucos de arriba, en su orden."), ("cs", "Vypne všechny cheaty a vrátí všechny násobiče na 1x: hra tak, jak byla vytvořena. Ve hře podržte F10 a stiskněte 1 až 8 pro přepnutí cheatů výše, v jejich pořadí."), ("ru", "Отключает все читы и возвращает все множители к 1x: игра в исходном виде. В игре удерживайте F10 и нажмите 1–8, чтобы переключить читы выше по порядку."), ("fr", "Désactive toutes les astuces et remet tous les multiplicateurs à 1x : le jeu tel qu'il a été conçu. En jeu, maintenez F10 et appuyez sur 1 à 8 pour basculer les astuces ci-dessus, dans leur ordre."), ("de", "Schaltet alle Cheats aus und setzt alle Multiplikatoren auf 1x zurück: das Spiel, wie es gemacht wurde. Im Spiel F10 halten und 1 bis 8 drücken, um die Cheats oben in ihrer Reihenfolge umzuschalten."), ("it", "Disattiva tutti i trucchi e riporta tutti i moltiplicatori a 1x: il gioco com'è stato creato. In gioco, tieni premuto F10 e premi da 1 a 8 per attivare o disattivare i trucchi sopra, nel loro ordine."), ("pl", "Wyłącza wszystkie kody i przywraca wszystkie mnożniki do 1x: gra taka, jaka została stworzona. W grze przytrzymaj F10 i naciśnij 1–8, aby przełączyć kody powyżej, w ich kolejności."), ("tr", "Tüm hileleri kapatır ve tüm çarpanları 1x'e döndürür: oyunun orijinal hali. Oyunda F10'u basılı tutup 1-8 tuşlarına basarak yukarıdaki hileleri sırasıyla açıp kapatabilirsiniz."), ("id", "Mematikan semua cheat dan mengembalikan semua pengali ke 1x: game seperti aslinya. Di dalam game, tahan F10 dan tekan 1 sampai 8 untuk mengganti cheat di atas, sesuai urutannya.")],
    ),
    (
        "Trophy notifications",
        &[("zh", "奖杯通知"), ("pt", "Notificações de troféu"), ("es", "Notificaciones de trofeos"), ("cs", "Oznámení o trofejích"), ("ru", "Уведомления о трофеях"), ("fr", "Notifications de trophées"), ("de", "Trophäen-Benachrichtigungen"), ("it", "Notifiche dei trofei"), ("pl", "Powiadomienia o trofeach"), ("tr", "Kupa bildirimleri"), ("id", "Notifikasi trofi")],
    ),
    (
        "Mute the trophy sound",
        &[("zh", "静音奖杯提示音"), ("pt", "Silenciar o som do troféu"), ("es", "Silenciar el sonido de trofeo"), ("cs", "Ztlumit zvuk trofeje"), ("ru", "Отключить звук трофея"), ("fr", "Couper le son des trophées"), ("de", "Trophäenton stummschalten"), ("it", "Disattiva il suono dei trofei"), ("pl", "Wycisz dźwięk trofeum"), ("tr", "Kupa sesini kapat"), ("id", "Bisukan suara trofi")],
    ),
    (
        "Trophy sound volume",
        &[("zh", "奖杯提示音音量"), ("pt", "Volume do som do troféu"), ("es", "Volumen del sonido de trofeo"), ("cs", "Hlasitost zvuku trofeje"), ("ru", "Громкость звука трофея"), ("fr", "Volume du son des trophées"), ("de", "Lautstärke des Trophäentons"), ("it", "Volume del suono dei trofei"), ("pl", "Głośność dźwięku trofeum"), ("tr", "Kupa sesi seviyesi"), ("id", "Volume suara trofi")],
    ),
    (
        "The chime of an unlocked trophy. Also in the in-game menu (Insert): Trophies > Notifications.",
        &[("zh", "解锁奖杯时的提示音。也可在游戏内菜单（Insert）：奖杯 > 通知 中设置。"), ("pt", "O som de um troféu desbloqueado. Também no menu do jogo (Insert): Troféus > Notificações."), ("es", "El sonido de un trofeo desbloqueado. También en el menú del juego (Insert): Trofeos > Notificaciones."), ("cs", "Zvuk odemčené trofeje. Také v herním menu (Insert): Trofeje > Oznámení."), ("ru", "Звук открытого трофея. Также в игровом меню (Insert): Трофеи > Уведомления."), ("fr", "Le son d'un trophée débloqué. Aussi dans le menu en jeu (Insert) : Trophées > Notifications."), ("de", "Der Ton einer freigeschalteten Trophäe. Auch im Spielmenü (Insert): Trophäen > Benachrichtigungen."), ("it", "Il suono di un trofeo sbloccato. Anche nel menu di gioco (Insert): Trofei > Notifiche."), ("pl", "Dźwięk odblokowanego trofeum. Także w menu gry (Insert): Trofea > Powiadomienia."), ("tr", "Açılan bir kupanın sesi. Oyun içi menüde de var (Insert): Kupalar > Bildirimler."), ("id", "Suara trofi yang terbuka. Juga ada di menu dalam game (Insert): Trofi > Notifikasi.")],
    ),
];

fn ui_extra(lang: &str, en: &str) -> Option<&'static str> {
    UI_EXTRA
        .iter()
        .find(|(key, _)| *key == en)?
        .1
        .iter()
        .find(|(code, _)| *code == lang)
        .map(|(_, text)| *text)
}

/// The text in `lang`; the English text when there is no translation.
pub fn tr(lang: &str, en: &str) -> String {
    if let Some(base) = en.strip_suffix(WARN) {
        return tr(lang, base) + WARN;
    }
    lookup(lang, en)
        .filter(|text| !text.is_empty())
        .or_else(|| ui_extra(lang, en).map(str::to_string))
        .unwrap_or_else(|| en.to_string())
}

/// Translates `en`, then replaces each `{}` in order with `args`; extra `{}` stay.
pub fn tr_fmt(lang: &str, en: &str, args: &[&str]) -> String {
    let text = tr(lang, en);
    let mut out = String::with_capacity(text.len());
    let mut rest = text.as_str();
    let mut args = args.iter();
    while let Some(at) = rest.find("{}") {
        let Some(arg) = args.next() else { break };
        out.push_str(&rest[..at]);
        out.push_str(arg);
        rest = &rest[at + 2..];
    }
    out.push_str(rest);
    out
}

/// Name and description of trophy `id` (0-based); English when the language has no list.
pub fn trophy_text(lang: &str, id: usize) -> (String, String) {
    let texts = &table().trophies.texts;
    let list = texts
        .get(lang)
        .filter(|list| !list.is_empty())
        .or_else(|| texts.get("en"));
    list.and_then(|list| list.get(id))
        .cloned()
        .unwrap_or_default()
}

/// Text of the Trophies page in `lang`; English when there is none.
pub fn trophy_ui(lang: &str, en: &str) -> String {
    table()
        .trophies
        .ui
        .get(lang)
        .and_then(|ui| ui.get(en))
        .filter(|text| !text.is_empty())
        .cloned()
        .unwrap_or_else(|| en.to_string())
}

/// Launcher language for a Windows primary language id; `en` when unknown.
pub fn windows_language_code(primary: u32) -> &'static str {
    table()
        .windows_languages
        .get(&primary.to_string())
        .and_then(|code| LANGS.iter().copied().find(|lang| lang == code))
        .unwrap_or("en")
}

/// Language from `LC_ALL`, `LC_MESSAGES` or `LANG` (first two lowercase letters); `en` by default.
pub fn environment_language(env: &dyn Fn(&str) -> Option<String>) -> String {
    for name in ["LC_ALL", "LC_MESSAGES", "LANG"] {
        if let Some(value) = env(name) {
            let mut chars = value.chars();
            if let (Some(a), Some(b)) = (chars.next(), chars.next()) {
                if a.is_ascii_lowercase() && b.is_ascii_lowercase() {
                    return format!("{a}{b}");
                }
            }
        }
    }
    "en".to_string()
}

/// The system language: the Windows UI language, or the environment elsewhere.
pub fn system_language() -> String {
    if cfg!(windows) {
        return match platform::user_default_ui_language() {
            Some(id) => windows_language_code(id & 0x3ff).to_string(),
            None => "en".to_string(),
        };
    }
    environment_language(&|name| std::env::var(name).ok())
}

/// The launcher's language: the `ui_language` setting, or the system language when it is empty
/// (the "System" choice). `system` is only asked then.
pub fn launcher_language(ui_language: &str, system: impl FnOnce() -> String) -> String {
    if ui_language.is_empty() {
        system()
    } else {
        ui_language.to_string()
    }
}

pub fn is_rtl(lang: &str) -> bool {
    lang == "ar"
}

pub fn region_of(lang: &str) -> Region {
    match lang {
        "fr" | "de" | "it" | "pl" | "cs" | "ru" | "tr" => Region::Europe,
        "ar" | "zh" | "ja" | "ko" | "id" => Region::Asia,
        _ => Region::Americas,
    }
}

/// The language's own name; `English` for an unknown code.
pub fn native_name(lang: &str) -> &'static str {
    match lang {
        "ru" => "Русский",
        "ar" => "العربية",
        "es" => "Español",
        "pt" => "Português (Brasil)",
        "fr" => "Français",
        "de" => "Deutsch",
        "it" => "Italiano",
        "pl" => "Polski",
        "cs" => "Čeština",
        "tr" => "Türkçe",
        "zh" => "简体中文",
        "ja" => "日本語",
        "ko" => "한국어",
        "id" => "Bahasa Indonesia",
        _ => "English",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn simplified_chinese_covers_rust_only_text_and_preserves_arguments() {
        for (en, _) in UI_EXTRA {
            let zh = tr("zh", en);
            assert!(!zh.is_empty() && zh != *en, "no zh for {en:?}");
            assert_eq!(en.matches("{}").count(), zh.matches("{}").count(), "{en}");
        }
        for key in [
            "Configure any control, in any order.",
            "Assigned input",
            "Cancel capture",
            "Menu confirmation",
            "{} lines kept",
        ] {
            assert_ne!(tr("zh", key), key);
        }
    }

    #[test]
    fn rust_only_strings_have_the_spec_translations() {
        assert_eq!(tr("pt", "Americas"), "Américas");
        assert_eq!(tr("pt", "Asia & Middle East"), "Ásia e Oriente Médio");
        assert_eq!(tr("ru", "Europe"), "Европа");
        assert_eq!(tr("de", "Americas"), "Americas");
        // A key of the JSON table still comes from it.
        assert_eq!(tr("pt", "Graphics"), "Gráficos");
    }

    #[test]
    fn every_rust_only_string_has_portuguese() {
        // Ruling R33: each string of the Rust launcher alone comes in English and Portuguese.
        for (en, _) in UI_EXTRA {
            let pt = tr("pt", en);
            assert!(!pt.is_empty() && pt != *en, "no pt for {en:?}");
        }
    }

    #[test]
    fn no_rust_only_string_is_a_key_of_the_table() {
        // `tr` reads the JSON table first: such an entry would never show.
        let table = table();
        let shadowed: Vec<&str> = UI_EXTRA
            .iter()
            .map(|(en, _)| *en)
            .filter(|en| table.keys.iter().any(|key| key == en))
            .collect();
        assert!(shadowed.is_empty(), "in assets/i18n.json: {shadowed:#?}");
    }

    #[test]
    fn no_rust_only_translation_is_shadowed_by_the_table() {
        // `lookup` reads the JSON's extra texts and positional table (and the Russian pairs)
        // before `UI_EXTRA`: a translation it already finds would never show.
        let shadowed: Vec<(&str, &str)> = UI_EXTRA
            .iter()
            .flat_map(|(en, texts)| texts.iter().map(move |(lang, _)| (*lang, *en)))
            .filter(|(lang, en)| lookup(lang, en).is_some_and(|text| !text.is_empty()))
            .collect();
        assert!(shadowed.is_empty(), "in assets/i18n.json: {shadowed:#?}");
    }

    #[test]
    fn embedded_table_parses() {
        let table = table();
        assert_eq!(table.keys.len(), 154);
        assert_eq!(table.translations.len(), 13);
    }

    #[test]
    fn portuguese_translates_graphics() {
        assert_eq!(tr("pt", "Graphics"), "Gráficos");
    }

    #[test]
    fn russian_uses_the_inline_pairs() {
        assert_eq!(tr("ru", "Graphics"), "Графика");
    }

    #[test]
    fn unknown_text_falls_back_to_english() {
        assert_eq!(tr("pt", "No such text"), "No such text");
        assert_eq!(tr("ru", "No such text"), "No such text");
        assert_eq!(tr("xx", "Graphics"), "Graphics");
    }

    #[test]
    fn warn_suffix_survives_translation() {
        let text = tr("pt", "Graphics  ⚠");
        assert!(text.starts_with("Gráficos"));
        assert!(text.ends_with("  ⚠"));
    }

    #[test]
    fn every_language_table_is_complete() {
        let table = table();
        for (lang, texts) in &table.translations {
            assert_eq!(texts.len(), table.keys.len(), "{lang}");
        }
    }

    #[test]
    fn tr_fmt_fills_placeholders_in_order() {
        let both = tr_fmt("en", "{} · game version {}", &["A", "1.0"]);
        assert_eq!(both, "A · game version 1.0");
        let one = tr_fmt("en", "{} · game version {}", &["A"]);
        assert_eq!(one, "A · game version {}");
    }

    #[test]
    fn trophies_fall_back_to_english() {
        let (name, _) = trophy_text("en", 0);
        assert_eq!(name, "King of the Hill");
        assert_eq!(trophy_text("id", 0).0, name);
        assert_eq!(trophy_text("en", 35), (String::new(), String::new()));
        assert_eq!(trophy_ui("pt", "Trophies"), "Troféus");
        assert_eq!(trophy_ui("en", "Trophies"), "Trophies");
    }

    #[test]
    fn windows_language_0x16_is_pt() {
        assert_eq!(windows_language_code(0x16), "pt");
        assert_eq!(windows_language_code(0x19), "ru");
        assert_eq!(windows_language_code(0x3f), "en");
    }

    #[test]
    fn environment_language_reads_the_first_set_variable() {
        let env = |name: &str| (name == "LANG").then(|| "pt_BR.UTF-8".to_string());
        assert_eq!(environment_language(&env), "pt");
        assert_eq!(environment_language(&|_| None), "en");
        assert_eq!(environment_language(&|_| Some("C".to_string())), "en");
    }

    #[test]
    fn regions_cover_all_15() {
        let mut seen = Vec::new();
        for region in [Region::Americas, Region::Europe, Region::Asia] {
            for lang in LANGS.iter().filter(|l| region_of(l) == region) {
                seen.push(*lang);
            }
        }
        seen.sort_unstable();
        let mut all = LANGS.to_vec();
        all.sort_unstable();
        assert_eq!(seen, all);
        assert_eq!(region_of("en"), Region::Americas);
        assert_eq!(region_of("tr"), Region::Europe);
        assert_eq!(region_of("id"), Region::Asia);
        assert!(is_rtl("ar") && !is_rtl("en"));
        assert_eq!(native_name("pt"), "Português (Brasil)");
    }

    #[test]
    fn every_version_menu_string_has_every_language() {
        const KEYS: [&str; 11] = [
            "Release",
            "Pre-release",
            "Releases",
            "Pre-releases",
            "new",
            "{} available",
            "Fetching…",
            "Could not fetch the versions",
            "No test builds",
            "No test builds for Linux",
            "This version is older than the installed one.",
        ];
        for en in KEYS {
            for lang in [
                "ar", "es", "pt", "fr", "de", "it", "pl", "cs", "tr", "zh", "ja", "ko", "id", "ru",
            ] {
                let text = tr(lang, en);
                assert!(!text.is_empty() && text != en, "no {lang} for {en:?}");
            }
        }
        assert_eq!(tr_fmt("pt", "{} available", &["2.7.0"]), "2.7.0 disponível");
        assert_eq!(tr("pt", "Pre-release"), "Pré-lançamento");
    }
}
