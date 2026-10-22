# SPDX-License-Identifier: GPL-2.0-or-later
"""Translations of the Windows launcher. KEYS holds the English texts; every language lists its
texts in the same order. Russian lives next to the English text in spartanport_launcher_win.py.
Missing or empty entries fall back to English."""

KEYS = [
    'Play', 'Graphics', 'Display & FPS', 'Game & effects', 'Mods & patches', 'Advanced', 'Log',
    'PLAY', 'Stop', 'PC port · Windows', "In the game: Insert or R3+L2\nopens the port's menu.",
    'Ready check', 'Quick settings', 'Frame rate', 'Upscaler', 'Fullscreen', '{} · game version {}',
    'Game folder not set', 'Choose your game folder (Game & effects)',
    '✗ No eboot.bin in the game folder (Game & effects)', '✓ Saves found in {}',
    '• No saves yet: the game creates them in {}', '✓ Graphics card: {}',
    '• Graphics card: not checked', '• Checking the graphics card…',
    '✗ No Vulkan 1.3 graphics card found (update the driver)', 'Choose the game folder first.',
    'Preparing the game; it opens in its own window…', 'The game is running.',
    'Restarting with the new settings…', '\nThe game exited (code {}).\n', 'Could not start: {}',
    'Choose the game folder with eboot.bin (CUSA01623).',
    'Stored in spartanport.ini; the in-game menu (Insert or R3+L2) changes the same values.',
    'Upscaling',
    "Temporal upscaling with the game's own motion vectors. FSR 4 needs its assets (below) and a GPU with INT8 dot products; otherwise the game falls back to FSR 3.1 by itself.",
    'Quality preset', 'Render scale per axis: Quality renders at 1/1.5 of the output size.',
    'Output resolution', 'What the upscaler produces; the HUD is drawn at this size too.',
    'Live resolution changes',
    'Off: outputs other than 1080p are set by a patch at start (fastest; changing them in the game restarts it). On: change output and preset in the game without a restart, at a cost.',
    'Sharpening (RCAS)', 'Sharpness', 'Object motion vectors',
    'Less ghosting on characters, cloth and weapons; costs about 10% FPS.', 'FSR 4 assets',
    'Download FSR 4 assets',
    "From FireBurn/Q2RTX on GitHub (built from AMD's MIT-licensed FidelityFX source), about 30 MB, into the fsr4_shaders folder of the port.",
    'Installed: {} files in {}.', '{} of {} files missing in {}.', 'Download failed: {}',
    'Show the FPS counter', 'FSR 4 (best quality)', 'FSR 4.1.1 (needs fsr4_411 assets)',
    'FSR 3.1 (every GPU)', 'TAA (native resolution anti-aliasing)', 'Off',
    'Auto (by graphics card)', 'Off (faster)', 'On (change without restarting)',
    'Applied when the game starts.', 'Frames ahead of the GPU',
    '1 keeps frame pacing even; more can raise FPS when the graphics card is the limit.', 'Window',
    'Presentation', 'Allow HDR output', 'When HDR is on in Windows and the display supports it.',
    'Auto: display refresh, max 120 (recommended)', 'No limit', 'Mailbox (low latency, no tearing)',
    'Immediate (tearing)', '1 (default)', 'Unbounded',
    'Your game dump, saves and the game patches.', 'Game', 'Game folder',
    'Choose the folder with eboot.bin', 'Saves folder', 'Choose the saves folder',
    'Empty: {} (shader caches are kept there too).', 'Game language',
    'English', 'Russian', 'Japanese',
    'French', 'Spanish', 'German', 'Italian',
    'The game files are never changed: mods are layered over them at start.', 'Mods', 'Load mods',
    'Mods folder', 'Choose the mods folder', 'Empty: {}', 'Third-party patches', 'Patches folder',
    'Choose the patches folder', 'Refresh', 'No mods in {} yet.',
    'Lower in the list loads later and wins conflicts.',
    'Launcher options, performance switches and diagnostics.', 'Launcher', 'Launcher language',
    'Applies when the launcher opens again.', 'Close the launcher when the game starts',
    'Desktop shortcut', 'Port folder', 'Shortcut created on the desktop.', 'Performance',
    'Two-stage GPU pipeline', '20–30% faster; switch it off if the game is unstable.',
    'Auto (8+ threads)', 'On', 'Off (more stable)', 'GPU readbacks',
    'How exactly data the GPU writes is copied back for the game.', 'Relaxed (default)',
    'Diagnostics', 'Frame statistics in the log (every 5 s)', 'GPU time per pass in the log',
    'Vulkan validation layers (needs the Vulkan SDK; much slower)', 'Extra variables',
    'NAME=value pairs separated by spaces (README lists them).', 'System', 'Copy', 'Clear',
    'Browse…', 'Open', 'Clear shader cache',
    'If the game only shows a black screen, this usually helps.',
    'Shader cache cleared. The next start stutters for a few minutes while it is rebuilt.',
    'Version {} is available.', 'Update', "What's new",
    'Install version {} now? The launcher closes, installs it and opens again. Saves and settings are kept.',
    'Downloading version {}… {}%', 'Update failed: {}', 'Close the game before updating.',
    'Check for updates when the launcher opens', 'Check for updates',
    'You have the latest version ({}).', 'Could not check for updates: {}',
    'Could not install the update. Download it from the releases page.', 'Game patches',
    'Community patches of the selected game, applied at start.', '{} · version {} · {}',
    'Native (1920 × 1080)', 'Resolution',
    'The game renders at this size; a resolution patch replaces the texture fix and reserves the memory it needs.',
    'Patches', '✓ Game version {}: the game patches apply',
    '⚠ Game version {}: the game patches need {}; none applied', '{} · {} more patches',
]

TRANSLATIONS = {
    'ar': [
        'العب', 'الرسومات', 'الشاشة ومعدل الإطارات', 'اللعبة والمؤثرات', 'التعديلات والرقع',
        'متقدم', 'السجل', 'العب', 'إيقاف', 'منفذ أصلي · ويندوز',
        'داخل اللعبة: Insert أو R3+L2\nيفتح قائمة المنفذ.', 'فحص الجاهزية', 'إعدادات سريعة',
        'معدل الإطارات', 'أداة التكبير', 'ملء الشاشة', '{} · إصدار اللعبة {}',
        'لم يتم اختيار مجلد اللعبة', 'اختر مجلد اللعبة (اللعبة والمؤثرات)',
        '✗ لا يوجد eboot.bin في مجلد اللعبة (اللعبة والمؤثرات)', '✓ تم العثور على حفظ في {}',
        '• لا يوجد حفظ بعد: ستنشئه اللعبة في {}', '✓ بطاقة الرسومات: {}',
        '• بطاقة الرسومات: لم يتم الفحص', '• جارٍ فحص بطاقة الرسومات…',
        '✗ لم يتم العثور على بطاقة رسومات تدعم Vulkan 1.3 (حدّث التعريف)',
        'اختر مجلد اللعبة أولًا.', 'جارٍ تجهيز اللعبة؛ ستفتح في نافذتها الخاصة…', 'اللعبة تعمل.',
        'إعادة التشغيل بالإعدادات الجديدة…', '\nأُغلقت اللعبة (الرمز {}).\n', 'تعذّر التشغيل: {}',
        'اختر مجلد اللعبة الذي يحتوي eboot.bin (CUSA01623).',
        'محفوظة في spartanport.ini؛ قائمة اللعبة (Insert أو R3+L2) تغيّر القيم نفسها.', 'التكبير',
        'تكبير زمني باستخدام متجهات الحركة الخاصة باللعبة. يحتاج FSR 4 إلى ملفاته (بالأسفل) وبطاقة تدعم INT8؛ وإلا تعود اللعبة تلقائيًا إلى FSR 3.1.',
        'إعداد الجودة', 'مقياس الرسم لكل محور: Quality يرسم بـ 1/1.5 من دقة الإخراج.',
        'دقة الإخراج', 'ما ينتجه المكبّر؛ وتُرسم الواجهة بهذه الدقة أيضًا.',
        'تغيير الدقة أثناء اللعب',
        'إيقاف: الدقات غير 1080p تُضبط برقعة عند التشغيل (الأسرع؛ تغييرها داخل اللعبة يعيد تشغيلها). تشغيل: غيّر الدقة والإعداد داخل اللعبة دون إعادة تشغيل، مع كلفة في الأداء.',
        'الحدة (RCAS)', 'قوة الحدة', 'متجهات حركة الأجسام',
        'تقليل الظلال المتبقية على الشخصيات والملابس والأسلحة؛ تكلّف نحو 10% من الإطارات.',
        'ملفات FSR 4', 'تنزيل ملفات FSR 4',
        'من FireBurn/Q2RTX على GitHub (مبنية من شيفرة AMD FidelityFX المرخصة MIT)، نحو 30 ميغابايت، إلى مجلد fsr4_shaders في المنفذ.',
        'مثبتة: {} ملفًا في {}.', 'ينقص {} من {} ملفًا في {}.', 'فشل التنزيل: {}',
        'إظهار عدّاد الإطارات', 'FSR 4 (أفضل جودة)', 'FSR 4.1.1 (يحتاج ملفات fsr4_411)',
        'FSR 3.1 (أي بطاقة)', 'TAA (تنعيم بالدقة الأصلية)', 'إيقاف', 'تلقائي (حسب البطاقة)',
        'إيقاف (أسرع)', 'تشغيل (تغيير دون إعادة تشغيل)', 'تُطبّق عند تشغيل اللعبة.',
        'إطارات مسبقة قبل بطاقة الرسومات',
        'القيمة 1 تحافظ على انتظام الإطارات؛ قيمة أكبر قد ترفع الأداء عندما تكون البطاقة هي الحد.',
        'النافذة', 'طريقة العرض', 'السماح بإخراج HDR', 'عند تفعيل HDR في ويندوز ودعم الشاشة له.',
        'تلقائي: تردد الشاشة، بحد أقصى 120 (موصى به)', 'بدون حد', 'Mailbox (تأخير منخفض، بلا تمزق)',
        'Immediate (مع تمزق)', '1 (افتراضي)', 'بلا حد', 'نسخة اللعبة والحفظ ورقع اللعبة.', 'اللعبة',
        'مجلد اللعبة', 'اختر المجلد الذي يحتوي eboot.bin', 'مجلد الحفظ', 'اختر مجلد الحفظ',
        'فارغ: {} (يُحفظ فيه أيضًا كاش الشيدر).', 'لغة اللعبة',
        'الإنجليزية', 'الروسية', 'اليابانية',
        'الفرنسية', 'الإسبانية', 'الألمانية', 'الإيطالية',
        'ملفات اللعبة لا تتغير أبدًا: التعديلات تُطبّق فوقها عند التشغيل.', 'التعديلات',
        'تحميل التعديلات', 'مجلد التعديلات', 'اختر مجلد التعديلات', 'فارغ: {}', 'رقع خارجية',
        'مجلد الرقع', 'اختر مجلد الرقع', 'تحديث', 'لا توجد تعديلات في {} بعد.',
        'الأسفل في القائمة يُحمّل لاحقًا ويتغلب عند التعارض.',
        'إعدادات المشغّل وخيارات الأداء والتشخيص.', 'المشغّل', 'لغة المشغّل',
        'تُطبّق عند فتح المشغّل مرة أخرى.', 'إغلاق المشغّل عند بدء اللعبة', 'اختصار على سطح المكتب',
        'مجلد المنفذ', 'تم إنشاء الاختصار على سطح المكتب.', 'الأداء', 'خط معالجة GPU بمرحلتين',
        'أسرع بنسبة 20–30%؛ أوقفه إذا كانت اللعبة غير مستقرة.', 'تلقائي (8 أنوية فأكثر)', 'تشغيل',
        'إيقاف (أكثر استقرارًا)', 'قراءة بيانات GPU',
        'مدى دقة إعادة البيانات التي تكتبها البطاقة إلى اللعبة.', 'Relaxed (افتراضي)', 'التشخيص',
        'إحصاءات الإطارات في السجل (كل 5 ثوانٍ)', 'زمن GPU لكل مرحلة في السجل',
        'طبقات التحقق Vulkan (تحتاج Vulkan SDK؛ أبطأ بكثير)', 'متغيرات إضافية',
        'أزواج NAME=value مفصولة بمسافات (القائمة في README).', 'لغة النظام', 'نسخ', 'مسح',
        'استعراض…', 'فتح', 'مسح كاش الشيدر', 'إذا ظهرت شاشة سوداء فقط، فهذا يساعد عادةً.',
        'تم مسح كاش الشيدر. سيكون التشغيل التالي متقطعًا لبضع دقائق أثناء إعادة بنائه.',
        'الإصدار {} متاح.', 'تحديث', 'ما الجديد',
        'تثبيت الإصدار {} الآن؟ سيُغلق المشغّل ويثبّته ثم يفتح من جديد. يبقى الحفظ والإعدادات كما هي.',
        'جارٍ تنزيل الإصدار {}… {}%', 'فشل التحديث: {}', 'أغلق اللعبة قبل التحديث.',
        'التحقق من التحديثات عند فتح المشغّل', 'التحقق من التحديثات', 'لديك أحدث إصدار ({}).',
        'تعذّر التحقق من التحديثات: {}', 'تعذّر تثبيت التحديث. نزّله من صفحة الإصدارات.', '', '',
        '', '', '', '', '', '', '', '',
    ],
    'es': [
        'Jugar', 'Gráficos', 'Pantalla y FPS', 'Juego y efectos', 'Mods y parches', 'Avanzado',
        'Registro', 'JUGAR', 'Detener', 'port para PC · Windows',
        'En el juego: Insert o R3+L2\nabre el menú del port.', 'Comprobación', 'Ajustes rápidos',
        'Velocidad de fotogramas', 'Escalador', 'Pantalla completa', '{} · versión del juego {}',
        'Carpeta del juego sin elegir', 'Elige la carpeta del juego (Juego y efectos)',
        '✗ No hay eboot.bin en la carpeta del juego (Juego y efectos)',
        '✓ Partidas guardadas en {}', '• Aún no hay partidas: el juego las creará en {}',
        '✓ Tarjeta gráfica: {}', '• Tarjeta gráfica: sin comprobar',
        '• Comprobando la tarjeta gráfica…',
        '✗ No se encontró una tarjeta con Vulkan 1.3 (actualiza el controlador)',
        'Primero elige la carpeta del juego.',
        'Preparando el juego; se abrirá en su propia ventana…', 'El juego está en marcha.',
        'Reiniciando con los nuevos ajustes…', '\nEl juego se cerró (código {}).\n',
        'No se pudo iniciar: {}', 'Elige la carpeta del juego con eboot.bin (CUSA01623).',
        'Se guarda en spartanport.ini; el menú del juego (Insert o R3+L2) cambia los mismos valores.',
        'Escalado',
        'Escalado temporal con los vectores de movimiento del propio juego. FSR 4 necesita sus archivos (abajo) y una GPU con INT8; si no, el juego vuelve solo a FSR 3.1.',
        'Preajuste de calidad',
        'Escala de render por eje: Quality renderiza a 1/1,5 del tamaño de salida.',
        'Resolución de salida',
        'Lo que produce el escalador; la interfaz también se dibuja a este tamaño.',
        'Cambio de resolución en vivo',
        'Desactivado: las salidas distintas de 1080p se fijan con un parche al iniciar (más rápido; cambiarlas en el juego lo reinicia). Activado: cambia salida y preajuste sin reiniciar, con un coste.',
        'Nitidez (RCAS)', 'Intensidad de nitidez', 'Vectores de movimiento de objetos',
        'Menos estelas en personajes, ropa y armas; cuesta alrededor de un 10% de FPS.',
        'Archivos de FSR 4', 'Descargar archivos de FSR 4',
        'Desde FireBurn/Q2RTX en GitHub (compilados del código FidelityFX de AMD con licencia MIT), unos 30 MB, en la carpeta fsr4_shaders del port.',
        'Instalados: {} archivos en {}.', 'Faltan {} de {} archivos en {}.',
        'La descarga falló: {}', 'Mostrar el contador de FPS', 'FSR 4 (mejor calidad)',
        'FSR 4.1.1 (necesita archivos fsr4_411)', 'FSR 3.1 (cualquier GPU)',
        'TAA (antialiasing a resolución nativa)', 'Desactivado', 'Automático (según la tarjeta)',
        'Desactivado (más rápido)', 'Activado (cambiar sin reiniciar)',
        'Se aplica al iniciar el juego.', 'Fotogramas por delante de la GPU',
        '1 mantiene un ritmo de fotogramas uniforme; más puede subir los FPS si la tarjeta es el límite.',
        'Ventana', 'Presentación', 'Permitir salida HDR',
        'Cuando HDR está activado en Windows y la pantalla lo admite.',
        'Automático: frecuencia de la pantalla, máx. 120 (recomendado)', 'Sin límite',
        'Mailbox (baja latencia, sin cortes)', 'Immediate (con cortes)', '1 (predeterminado)',
        'Sin límite', 'Tu volcado del juego, partidas y parches.', 'Juego', 'Carpeta del juego',
        'Elige la carpeta con eboot.bin', 'Carpeta de partidas', 'Elige la carpeta de partidas',
        'Vacío: {} (también guarda la caché de shaders).', 'Idioma del juego',
        'Inglés', 'Ruso',
        'Japonés', 'Francés', 'Español', 'Alemán', 'Italiano',
        'Los archivos del juego nunca cambian: los mods se superponen al iniciar.', 'Mods',
        'Cargar mods', 'Carpeta de mods', 'Elige la carpeta de mods', 'Vacío: {}',
        'Parches de terceros', 'Carpeta de parches', 'Elige la carpeta de parches', 'Actualizar',
        'Aún no hay mods en {}.', 'Más abajo en la lista se carga después y gana en conflictos.',
        'Opciones del lanzador, rendimiento y diagnóstico.', 'Lanzador', 'Idioma del lanzador',
        'Se aplica al volver a abrir el lanzador.', 'Cerrar el lanzador al iniciar el juego',
        'Acceso directo en el escritorio', 'Carpeta del port',
        'Acceso directo creado en el escritorio.', 'Rendimiento', 'Canal de GPU en dos etapas',
        'Un 20–30% más rápido; desactívalo si el juego es inestable.', 'Automático (8+ hilos)',
        'Activado', 'Desactivado (más estable)', 'Lecturas de la GPU',
        'Con qué precisión se devuelven al juego los datos que escribe la GPU.',
        'Relaxed (predeterminado)', 'Diagnóstico',
        'Estadísticas de fotogramas en el registro (cada 5 s)',
        'Tiempo de GPU por pasada en el registro',
        'Capas de validación de Vulkan (necesita el Vulkan SDK; mucho más lento)',
        'Variables extra', 'Pares NOMBRE=valor separados por espacios (lista en el README).',
        'Sistema', 'Copiar', 'Borrar', 'Examinar…', 'Abrir', 'Borrar la caché de shaders',
        'Si el juego solo muestra una pantalla negra, esto suele ayudar.',
        'Caché de shaders borrada. El próximo inicio tendrá tirones unos minutos mientras se reconstruye.',
        'La versión {} está disponible.', 'Actualizar', 'Novedades',
        '¿Instalar la versión {} ahora? El lanzador se cerrará, la instalará y volverá a abrirse. Se conservan partidas y ajustes.',
        'Descargando la versión {}… {}%', 'La actualización falló: {}',
        'Cierra el juego antes de actualizar.', 'Buscar actualizaciones al abrir el lanzador',
        'Buscar actualizaciones', 'Tienes la última versión ({}).',
        'No se pudieron buscar actualizaciones: {}',
        'No se pudo instalar la actualización. Descárgala desde la página de versiones.',
        'Parches del juego', 'Parches de la comunidad para el juego elegido, aplicados al iniciar.',
        '{} · versión {} · {}', 'Nativa (1920 × 1080)', 'Resolución',
        'El juego renderiza a este tamaño; un parche de resolución reemplaza la corrección de texturas y reserva la memoria que necesita.',
        'Parches', '✓ Versión del juego {}: se aplican los parches del juego',
        '⚠ Versión del juego {}: los parches del juego necesitan {}; no se aplicó ninguno',
        '{} · {} parches más',
    ],
    'pt': [
        'Jogar', 'Gráficos', 'Tela e FPS', 'Jogo e efeitos', 'Mods e patches', 'Avançado',
        'Registro', 'JOGAR', 'Parar', 'port para PC · Windows',
        'No jogo: Insert ou R3+L2\nabre o menu do port.', 'Verificação', 'Ajustes rápidos',
        'Taxa de quadros', 'Upscaler', 'Tela cheia', '{} · versão do jogo {}',
        'Pasta do jogo não definida', 'Escolha a pasta do jogo (Jogo e efeitos)',
        '✗ Não há eboot.bin na pasta do jogo (Jogo e efeitos)', '✓ Saves encontrados em {}',
        '• Nenhum save ainda: o jogo vai criá-los em {}', '✓ Placa de vídeo: {}',
        '• Placa de vídeo: não verificada', '• Verificando a placa de vídeo…',
        '✗ Nenhuma placa com Vulkan 1.3 encontrada (atualize o driver)',
        'Escolha a pasta do jogo primeiro.', 'Preparando o jogo; ele abrirá em sua própria janela…',
        'O jogo está rodando.', 'Reiniciando com as novas configurações…',
        '\nO jogo foi fechado (código {}).\n', 'Não foi possível iniciar: {}',
        'Escolha a pasta do jogo com eboot.bin (CUSA01623).',
        'Salvo em spartanport.ini; o menu do jogo (Insert ou R3+L2) altera os mesmos valores.',
        'Upscaling',
        'Upscaling temporal com os vetores de movimento do próprio jogo. O FSR 4 precisa de seus arquivos (abaixo) e de uma GPU com INT8; senão o jogo volta sozinho para o FSR 3.1.',
        'Predefinição de qualidade',
        'Escala de renderização por eixo: Quality renderiza a 1/1,5 da resolução de saída.',
        'Resolução de saída',
        'O que o upscaler produz; a interface também é desenhada nesse tamanho.',
        'Troca de resolução ao vivo',
        'Desligado: saídas diferentes de 1080p são definidas por um patch ao iniciar (mais rápido; mudá-las no jogo o reinicia). Ligado: mude saída e predefinição sem reiniciar, com um custo.',
        'Nitidez (RCAS)', 'Intensidade da nitidez', 'Vetores de movimento de objetos',
        'Menos rastros em personagens, roupas e armas; custa cerca de 10% de FPS.',
        'Arquivos do FSR 4', 'Baixar arquivos do FSR 4',
        'Do FireBurn/Q2RTX no GitHub (compilados do código FidelityFX da AMD, licença MIT), cerca de 30 MB, na pasta fsr4_shaders do port.',
        'Instalados: {} arquivos em {}.', 'Faltam {} de {} arquivos em {}.',
        'Falha no download: {}', 'Mostrar o contador de FPS', 'FSR 4 (melhor qualidade)',
        'FSR 4.1.1 (precisa dos arquivos fsr4_411)', 'FSR 3.1 (qualquer GPU)',
        'TAA (anti-aliasing na resolução nativa)', 'Desligado', 'Automático (pela placa de vídeo)',
        'Desligado (mais rápido)', 'Ligado (mudar sem reiniciar)', 'Aplicado ao iniciar o jogo.',
        'Quadros à frente da GPU',
        '1 mantém o ritmo de quadros uniforme; mais pode aumentar o FPS quando a placa é o limite.',
        'Janela', 'Apresentação', 'Permitir saída HDR',
        'Quando o HDR está ligado no Windows e a tela suporta.',
        'Automático: taxa da tela, máx. 120 (recomendado)', 'Sem limite',
        'Mailbox (baixa latência, sem tearing)', 'Immediate (com tearing)', '1 (padrão)',
        'Ilimitado', 'Seu dump do jogo, saves e patches.', 'Jogo', 'Pasta do jogo',
        'Escolha a pasta com eboot.bin', 'Pasta de saves', 'Escolha a pasta de saves',
        'Vazio: {} (o cache de shaders também fica lá).', 'Idioma do jogo',
        'Inglês', 'Russo', 'Japonês',
        'Francês', 'Espanhol', 'Alemão', 'Italiano',
        'Os arquivos do jogo nunca mudam: os mods são sobrepostos ao iniciar.', 'Mods',
        'Carregar mods', 'Pasta de mods', 'Escolha a pasta de mods', 'Vazio: {}',
        'Patches de terceiros', 'Pasta de patches', 'Escolha a pasta de patches', 'Atualizar',
        'Ainda não há mods em {}.', 'Mais abaixo na lista carrega depois e vence conflitos.',
        'Opções do launcher, desempenho e diagnóstico.', 'Launcher', 'Idioma do launcher',
        'Aplica-se ao reabrir o launcher.', 'Fechar o launcher ao iniciar o jogo',
        'Atalho na área de trabalho', 'Pasta do port', 'Atalho criado na área de trabalho.',
        'Desempenho', 'Pipeline de GPU em dois estágios',
        '20–30% mais rápido; desligue se o jogo ficar instável.', 'Automático (8+ threads)',
        'Ligado', 'Desligado (mais estável)', 'Leituras da GPU',
        'Com que precisão os dados escritos pela GPU voltam para o jogo.', 'Flexível (padrão)',
        'Diagnóstico', 'Estatísticas de quadros no registro (a cada 5 s)',
        'Tempo de GPU por passe no registro',
        'Camadas de validação do Vulkan (precisa do Vulkan SDK; muito mais lento)',
        'Variáveis extras', 'Pares NOME=valor separados por espaços (lista no README).', 'Sistema',
        'Copiar', 'Limpar', 'Procurar…', 'Abrir', 'Limpar o cache de shaders',
        'Se o jogo mostrar só uma tela preta, isso costuma resolver.',
        'Cache de shaders limpo. O próximo início terá travadas por alguns minutos enquanto ele é refeito.',
        'A versão {} está disponível.', 'Atualizar', 'Novidades',
        'Instalar a versão {} agora? O launcher fecha, instala e abre de novo. Saves e configurações são mantidos.',
        'Baixando a versão {}… {}%', 'Falha na atualização: {}', 'Feche o jogo antes de atualizar.',
        'Procurar atualizações ao abrir o launcher', 'Procurar atualizações',
        'Você tem a versão mais recente ({}).', 'Não foi possível procurar atualizações: {}',
        'Não foi possível instalar a atualização. Baixe-a na página de versões.', 'Patches do jogo',
        'Patches da comunidade do jogo selecionado, aplicados ao iniciar.', '{} · versão {} · {}',
        'Nativa (1920 × 1080)', 'Resolução',
        'O jogo renderiza nesse tamanho; um patch de resolução substitui a correção de texturas e reserva a memória necessária.',
        'Patches', '✓ Versão do jogo {}: os patches do jogo são aplicados',
        '⚠ Versão do jogo {}: os patches exigem a {}; nenhum aplicado', '{} · mais {} patches',
    ],
    'fr': [
        'Jouer', 'Graphismes', 'Affichage et FPS', 'Jeu et effets', 'Mods et patchs', 'Avancé',
        'Journal', 'JOUER', 'Arrêter', 'portage PC · Windows',
        'En jeu : Insert ou R3+L2\nouvre le menu du portage.', 'Vérification', 'Réglages rapides',
        'Fréquence d’images', 'Upscaler', 'Plein écran', '{} · version du jeu {}',
        'Dossier du jeu non défini', 'Choisissez le dossier du jeu (Jeu et effets)',
        '✗ Pas de eboot.bin dans le dossier du jeu (Jeu et effets)',
        '✓ Sauvegardes trouvées dans {}', '• Pas encore de sauvegarde : le jeu les créera dans {}',
        '✓ Carte graphique : {}', '• Carte graphique : non vérifiée',
        '• Vérification de la carte graphique…',
        '✗ Aucune carte Vulkan 1.3 trouvée (mettez à jour le pilote)',
        'Choisissez d’abord le dossier du jeu.',
        'Préparation du jeu ; il s’ouvrira dans sa propre fenêtre…', 'Le jeu est lancé.',
        'Redémarrage avec les nouveaux réglages…', '\nLe jeu s’est fermé (code {}).\n',
        'Impossible de lancer : {}',
        'Choisissez le dossier du jeu contenant eboot.bin (CUSA01623).',
        'Enregistré dans spartanport.ini ; le menu en jeu (Insert ou R3+L2) modifie les mêmes valeurs.',
        'Upscaling',
        'Upscaling temporel avec les vecteurs de mouvement du jeu. FSR 4 nécessite ses fichiers (ci-dessous) et un GPU avec INT8 ; sinon le jeu repasse seul à FSR 3.1.',
        'Préréglage de qualité',
        'Échelle de rendu par axe : Quality rend à 1/1,5 de la taille de sortie.',
        'Résolution de sortie',
        'Ce que produit l’upscaler ; l’interface est aussi dessinée à cette taille.',
        'Changement de résolution en direct',
        'Désactivé : les sorties autres que 1080p sont fixées par un patch au lancement (le plus rapide ; les changer en jeu le relance). Activé : changez sortie et préréglage sans relancer, avec un coût.',
        'Netteté (RCAS)', 'Intensité de la netteté', 'Vecteurs de mouvement des objets',
        'Moins de traînées sur les personnages, vêtements et armes ; coûte environ 10 % de FPS.',
        'Fichiers FSR 4', 'Télécharger les fichiers FSR 4',
        'Depuis FireBurn/Q2RTX sur GitHub (compilés à partir du code FidelityFX d’AMD sous licence MIT), environ 30 Mo, dans le dossier fsr4_shaders du portage.',
        'Installés : {} fichiers dans {}.', '{} fichiers sur {} manquants dans {}.',
        'Échec du téléchargement : {}', 'Afficher le compteur de FPS', 'FSR 4 (meilleure qualité)',
        'FSR 4.1.1 (fichiers fsr4_411 requis)', 'FSR 3.1 (tout GPU)',
        'TAA (anticrénelage en résolution native)', 'Désactivé', 'Auto (selon la carte)',
        'Désactivé (plus rapide)', 'Activé (changer sans relancer)',
        'Appliqué au lancement du jeu.', 'Images d’avance sur le GPU',
        '1 garde un rythme d’images régulier ; plus peut augmenter les FPS quand la carte est la limite.',
        'Fenêtre', 'Présentation', 'Autoriser la sortie HDR',
        'Quand le HDR est activé dans Windows et pris en charge par l’écran.',
        'Auto : fréquence de l’écran, 120 max (recommandé)', 'Aucune limite',
        'Mailbox (faible latence, sans déchirement)', 'Immediate (déchirement)', '1 (par défaut)',
        'Illimité', 'Votre dump du jeu, vos sauvegardes et les patchs.', 'Jeu', 'Dossier du jeu',
        'Choisissez le dossier contenant eboot.bin', 'Dossier des sauvegardes',
        'Choisissez le dossier des sauvegardes', 'Vide : {} (le cache des shaders y est aussi).',
        'Langue du jeu',
        'Anglais', 'Russe', 'Japonais', 'Français', 'Espagnol', 'Allemand', 'Italien',
        'Les fichiers du jeu ne sont jamais modifiés : les mods s’y superposent au lancement.',
        'Mods', 'Charger les mods', 'Dossier des mods', 'Choisissez le dossier des mods',
        'Vide : {}', 'Patchs tiers', 'Dossier des patchs', 'Choisissez le dossier des patchs',
        'Actualiser', 'Pas encore de mods dans {}.',
        'Plus bas dans la liste = chargé après, l’emporte en cas de conflit.',
        'Options du lanceur, performances et diagnostic.', 'Lanceur', 'Langue du lanceur',
        'S’applique à la prochaine ouverture du lanceur.', 'Fermer le lanceur au démarrage du jeu',
        'Raccourci sur le bureau', 'Dossier du portage', 'Raccourci créé sur le bureau.',
        'Performances', 'Pipeline GPU en deux étapes',
        '20 à 30 % plus rapide ; désactivez-le si le jeu est instable.', 'Auto (8 threads ou plus)',
        'Activé', 'Désactivé (plus stable)', 'Relectures GPU',
        'Précision du retour au jeu des données écrites par le GPU.', 'Relaxed (par défaut)',
        'Diagnostic', 'Statistiques d’images dans le journal (toutes les 5 s)',
        'Temps GPU par passe dans le journal',
        'Couches de validation Vulkan (Vulkan SDK requis ; bien plus lent)',
        'Variables supplémentaires',
        'Paires NOM=valeur séparées par des espaces (liste dans le README).', 'Système', 'Copier',
        'Effacer', 'Parcourir…', 'Ouvrir', 'Vider le cache des shaders',
        'Si le jeu n’affiche qu’un écran noir, cela aide généralement.',
        'Cache des shaders vidé. Le prochain lancement saccadera quelques minutes le temps de le reconstruire.',
        'La version {} est disponible.', 'Mettre à jour', 'Nouveautés',
        'Installer la version {} maintenant ? Le lanceur se ferme, l’installe et se rouvre. Sauvegardes et réglages sont conservés.',
        'Téléchargement de la version {}… {} %', 'Échec de la mise à jour : {}',
        'Fermez le jeu avant de mettre à jour.',
        'Rechercher les mises à jour à l’ouverture du lanceur', 'Rechercher les mises à jour',
        'Vous avez la dernière version ({}).', 'Impossible de rechercher les mises à jour : {}',
        'Impossible d’installer la mise à jour. Téléchargez-la depuis la page des versions.', '',
        '', '', '', '', '', '', '', '', '',
    ],
    'de': [
        'Spielen', 'Grafik', 'Anzeige & FPS', 'Spiel & Effekte', 'Mods & Patches', 'Erweitert',
        'Protokoll', 'SPIELEN', 'Beenden', 'nativer Port · Windows',
        'Im Spiel: Einfg oder R3+L2\nöffnet das Port-Menü.', 'Bereitschaft', 'Schnelleinstellungen',
        'Bildrate', 'Upscaler', 'Vollbild', '{} · Spielversion {}', 'Spielordner nicht gewählt',
        'Spielordner wählen (Spiel & Effekte)', '✗ Kein eboot.bin im Spielordner (Spiel & Effekte)',
        '✓ Spielstände gefunden in {}', '• Noch keine Spielstände: das Spiel legt sie an in {}',
        '✓ Grafikkarte: {}', '• Grafikkarte: nicht geprüft', '• Grafikkarte wird geprüft…',
        '✗ Keine Grafikkarte mit Vulkan 1.3 gefunden (Treiber aktualisieren)',
        'Zuerst den Spielordner wählen.',
        'Spiel wird vorbereitet; es öffnet sich in einem eigenen Fenster…', 'Das Spiel läuft.',
        'Neustart mit den neuen Einstellungen…', '\nDas Spiel wurde beendet (Code {}).\n',
        'Start fehlgeschlagen: {}', 'Spielordner mit eboot.bin wählen (CUSA01623).',
        'Gespeichert in spartanport.ini; das Menü im Spiel (Einfg oder R3+L2) ändert dieselben Werte.',
        'Hochskalierung',
        'Temporale Hochskalierung mit den Bewegungsvektoren des Spiels. FSR 4 braucht seine Dateien (unten) und eine GPU mit INT8; sonst wechselt das Spiel selbst zu FSR 3.1.',
        'Qualitätsstufe', 'Renderauflösung je Achse: Quality rendert mit 1/1,5 der Ausgabegröße.',
        'Ausgabeauflösung',
        'Was der Upscaler erzeugt; die Oberfläche wird ebenfalls in dieser Größe gezeichnet.',
        'Auflösung live ändern',
        'Aus: Ausgaben außer 1080p werden beim Start per Patch gesetzt (am schnellsten; Ändern im Spiel startet es neu). An: Ausgabe und Stufe im Spiel ohne Neustart ändern, kostet Leistung.',
        'Schärfen (RCAS)', 'Schärfestärke', 'Bewegungsvektoren von Objekten',
        'Weniger Nachziehen bei Figuren, Kleidung und Waffen; kostet etwa 10 % FPS.',
        'FSR-4-Dateien', 'FSR-4-Dateien herunterladen',
        'Von FireBurn/Q2RTX auf GitHub (aus AMDs MIT-lizenziertem FidelityFX-Quellcode gebaut), etwa 30 MB, in den Ordner fsr4_shaders des Ports.',
        'Installiert: {} Dateien in {}.', '{} von {} Dateien fehlen in {}.',
        'Download fehlgeschlagen: {}', 'FPS-Zähler anzeigen', 'FSR 4 (beste Qualität)',
        'FSR 4.1.1 (braucht fsr4_411-Dateien)', 'FSR 3.1 (jede GPU)',
        'TAA (Kantenglättung in nativer Auflösung)', 'Aus', 'Automatisch (nach Grafikkarte)',
        'Aus (schneller)', 'An (ohne Neustart ändern)', 'Gilt beim Start des Spiels.',
        'Bilder vor der GPU',
        '1 hält die Bildausgabe gleichmäßig; mehr kann die FPS erhöhen, wenn die Grafikkarte begrenzt.',
        'Fenster', 'Darstellung', 'HDR-Ausgabe erlauben',
        'Wenn HDR in Windows aktiv ist und der Bildschirm es unterstützt.',
        'Automatisch: Bildwiederholrate, max. 120 (empfohlen)', 'Keine Grenze',
        'Mailbox (geringe Latenz, ohne Tearing)', 'Immediate (mit Tearing)', '1 (Standard)',
        'Unbegrenzt', 'Dein Spiel-Dump, Spielstände und Spiel-Patches.', 'Spiel', 'Spielordner',
        'Ordner mit eboot.bin wählen', 'Spielstandordner', 'Spielstandordner wählen',
        'Leer: {} (dort liegt auch der Shader-Cache).', 'Spielsprache',
        'Englisch', 'Russisch', 'Japanisch',
        'Französisch', 'Spanisch', 'Deutsch', 'Italienisch',
        'Die Spieldateien werden nie verändert: Mods werden beim Start darübergelegt.', 'Mods',
        'Mods laden', 'Mod-Ordner', 'Mod-Ordner wählen', 'Leer: {}', 'Patches von Drittanbietern',
        'Patch-Ordner', 'Patch-Ordner wählen', 'Aktualisieren', 'Noch keine Mods in {}.',
        'Weiter unten in der Liste lädt später und gewinnt bei Konflikten.',
        'Launcher-Optionen, Leistung und Diagnose.', 'Launcher', 'Launcher-Sprache',
        'Gilt beim nächsten Öffnen des Launchers.', 'Launcher beim Spielstart schließen',
        'Desktop-Verknüpfung', 'Port-Ordner', 'Verknüpfung auf dem Desktop erstellt.', 'Leistung',
        'Zweistufige GPU-Pipeline', '20–30 % schneller; abschalten, wenn das Spiel instabil ist.',
        'Automatisch (8+ Threads)', 'An', 'Aus (stabiler)', 'GPU-Rücklesen',
        'Wie genau von der GPU geschriebene Daten ans Spiel zurückgehen.', 'Relaxed (Standard)',
        'Diagnose', 'Bildstatistik im Protokoll (alle 5 s)', 'GPU-Zeit je Durchgang im Protokoll',
        'Vulkan-Validierungsschichten (braucht das Vulkan SDK; viel langsamer)',
        'Zusätzliche Variablen', 'NAME=Wert-Paare durch Leerzeichen getrennt (Liste im README).',
        'System', 'Kopieren', 'Leeren', 'Durchsuchen…', 'Öffnen', 'Shader-Cache leeren',
        'Wenn das Spiel nur einen schwarzen Bildschirm zeigt, hilft das meistens.',
        'Shader-Cache geleert. Der nächste Start ruckelt einige Minuten, während er neu aufgebaut wird.',
        'Version {} ist verfügbar.', 'Aktualisieren', 'Neuerungen',
        'Version {} jetzt installieren? Der Launcher schließt sich, installiert sie und öffnet sich wieder. Spielstände und Einstellungen bleiben erhalten.',
        'Version {} wird heruntergeladen… {} %', 'Update fehlgeschlagen: {}',
        'Beende das Spiel vor dem Update.', 'Beim Öffnen des Launchers nach Updates suchen',
        'Nach Updates suchen', 'Du hast die neueste Version ({}).',
        'Suche nach Updates fehlgeschlagen: {}',
        'Das Update konnte nicht installiert werden. Lade es von der Release-Seite herunter.', '',
        '', '', '', '', '', '', '', '', '',
    ],
    'it': [
        'Gioca', 'Grafica', 'Schermo e FPS', 'Gioco ed effetti', 'Mod e patch', 'Avanzate',
        'Registro', 'GIOCA', 'Ferma', 'port per PC · Windows',
        'Nel gioco: Ins o R3+L2\napre il menu del port.', 'Verifica', 'Impostazioni rapide',
        'Frequenza fotogrammi', 'Upscaler', 'Schermo intero', '{} · versione del gioco {}',
        'Cartella del gioco non impostata', 'Scegli la cartella del gioco (Gioco ed effetti)',
        '✗ Nessun eboot.bin nella cartella del gioco (Gioco ed effetti)',
        '✓ Salvataggi trovati in {}', '• Ancora nessun salvataggio: il gioco li creerà in {}',
        '✓ Scheda video: {}', '• Scheda video: non verificata', '• Verifica della scheda video…',
        '✗ Nessuna scheda con Vulkan 1.3 trovata (aggiorna il driver)',
        'Scegli prima la cartella del gioco.',
        'Preparazione del gioco; si aprirà nella sua finestra…', 'Il gioco è in esecuzione.',
        'Riavvio con le nuove impostazioni…', '\nIl gioco è stato chiuso (codice {}).\n',
        'Impossibile avviare: {}', 'Scegli la cartella del gioco con eboot.bin (CUSA01623).',
        'Salvato in spartanport.ini; il menu nel gioco (Ins o R3+L2) modifica gli stessi valori.',
        'Upscaling',
        'Upscaling temporale con i vettori di movimento del gioco. FSR 4 richiede i suoi file (sotto) e una GPU con INT8; altrimenti il gioco torna da solo a FSR 3.1.',
        'Preset di qualità',
        'Scala di rendering per asse: Quality renderizza a 1/1,5 della risoluzione di uscita.',
        'Risoluzione di uscita',
        'Ciò che produce l’upscaler; anche l’interfaccia è disegnata a questa dimensione.',
        'Cambio risoluzione dal vivo',
        'Disattivo: le uscite diverse da 1080p sono impostate da una patch all’avvio (più veloce; cambiarle nel gioco lo riavvia). Attivo: cambia uscita e preset senza riavviare, con un costo.',
        'Nitidezza (RCAS)', 'Intensità della nitidezza', 'Vettori di movimento degli oggetti',
        'Meno scie su personaggi, abiti e armi; costa circa il 10% di FPS.', 'File di FSR 4',
        'Scarica i file di FSR 4',
        'Da FireBurn/Q2RTX su GitHub (compilati dal codice FidelityFX di AMD con licenza MIT), circa 30 MB, nella cartella fsr4_shaders del port.',
        'Installati: {} file in {}.', 'Mancano {} file su {} in {}.', 'Download non riuscito: {}',
        'Mostra il contatore FPS', 'FSR 4 (qualità migliore)',
        'FSR 4.1.1 (richiede i file fsr4_411)', 'FSR 3.1 (qualsiasi GPU)',
        'TAA (antialiasing a risoluzione nativa)', 'Disattivo', 'Automatico (in base alla scheda)',
        'Disattivo (più veloce)', 'Attivo (cambia senza riavviare)',
        'Applicato all’avvio del gioco.', 'Fotogrammi in anticipo sulla GPU',
        '1 mantiene uniforme il ritmo dei fotogrammi; di più può alzare gli FPS se la scheda è il limite.',
        'Finestra', 'Presentazione', 'Consenti l’uscita HDR',
        'Quando l’HDR è attivo in Windows e lo schermo lo supporta.',
        'Automatico: frequenza dello schermo, max 120 (consigliato)', 'Nessun limite',
        'Mailbox (bassa latenza, senza tearing)', 'Immediate (con tearing)', '1 (predefinito)',
        'Illimitato', 'Il tuo dump del gioco, i salvataggi e le patch.', 'Gioco',
        'Cartella del gioco', 'Scegli la cartella con eboot.bin', 'Cartella dei salvataggi',
        'Scegli la cartella dei salvataggi', 'Vuoto: {} (contiene anche la cache degli shader).',
        'Lingua del gioco',
        'Inglese', 'Russo',
        'Giapponese', 'Francese', 'Spagnolo', 'Tedesco', 'Italiano',
        'I file del gioco non vengono mai modificati: le mod si sovrappongono all’avvio.', 'Mod',
        'Carica le mod', 'Cartella delle mod', 'Scegli la cartella delle mod', 'Vuoto: {}',
        'Patch di terze parti', 'Cartella delle patch', 'Scegli la cartella delle patch',
        'Aggiorna', 'Ancora nessuna mod in {}.',
        'Più in basso nella lista si carica dopo e vince i conflitti.',
        'Opzioni del launcher, prestazioni e diagnostica.', 'Launcher', 'Lingua del launcher',
        'Si applica alla prossima apertura del launcher.', 'Chiudi il launcher all’avvio del gioco',
        'Collegamento sul desktop', 'Cartella del port', 'Collegamento creato sul desktop.',
        'Prestazioni', 'Pipeline GPU a due stadi',
        'Più veloce del 20–30%; disattivala se il gioco è instabile.', 'Automatico (8+ thread)',
        'Attivo', 'Disattivo (più stabile)', 'Riletture della GPU',
        'Quanto fedelmente i dati scritti dalla GPU tornano al gioco.', 'Relaxed (predefinito)',
        'Diagnostica', 'Statistiche dei fotogrammi nel registro (ogni 5 s)',
        'Tempo GPU per passata nel registro',
        'Livelli di validazione Vulkan (serve il Vulkan SDK; molto più lento)', 'Variabili extra',
        'Coppie NOME=valore separate da spazi (elenco nel README).', 'Sistema', 'Copia', 'Cancella',
        'Sfoglia…', 'Apri', 'Svuota la cache degli shader',
        'Se il gioco mostra solo uno schermo nero, di solito aiuta.',
        'Cache degli shader svuotata. Il prossimo avvio scatterà per qualche minuto mentre viene ricostruita.',
        'È disponibile la versione {}.', 'Aggiorna', 'Novità',
        'Installare ora la versione {}? Il launcher si chiude, la installa e si riapre. Salvataggi e impostazioni restano.',
        'Download della versione {}… {}%', 'Aggiornamento non riuscito: {}',
        'Chiudi il gioco prima di aggiornare.', 'Cerca aggiornamenti all’apertura del launcher',
        'Cerca aggiornamenti', 'Hai l’ultima versione ({}).',
        'Impossibile cercare aggiornamenti: {}',
        'Impossibile installare l’aggiornamento. Scaricalo dalla pagina delle versioni.', '', '',
        '', '', '', '', '', '', '', '',
    ],
    'pl': [
        'Graj', 'Grafika', 'Ekran i FPS', 'Gra i efekty', 'Mody i łatki', 'Zaawansowane',
        'Dziennik', 'GRAJ', 'Zatrzymaj', 'natywny port · Windows',
        'W grze: Insert lub R3+L2\notwiera menu portu.', 'Sprawdzenie', 'Szybkie ustawienia',
        'Liczba klatek', 'Upscaler', 'Pełny ekran', '{} · wersja gry {}', 'Nie wybrano folderu gry',
        'Wybierz folder gry (Gra i efekty)', '✗ Brak eboot.bin w folderze gry (Gra i efekty)',
        '✓ Znaleziono zapisy w {}', '• Brak zapisów: gra utworzy je w {}', '✓ Karta graficzna: {}',
        '• Karta graficzna: nie sprawdzono', '• Sprawdzanie karty graficznej…',
        '✗ Nie znaleziono karty z Vulkan 1.3 (zaktualizuj sterownik)',
        'Najpierw wybierz folder gry.', 'Przygotowywanie gry; otworzy się we własnym oknie…',
        'Gra działa.', 'Ponowne uruchamianie z nowymi ustawieniami…',
        '\nGra została zamknięta (kod {}).\n', 'Nie udało się uruchomić: {}',
        'Wybierz folder gry z eboot.bin (CUSA01623).',
        'Zapisywane w spartanport.ini; menu w grze (Insert lub R3+L2) zmienia te same wartości.',
        'Skalowanie',
        'Skalowanie czasowe z wektorami ruchu samej gry. FSR 4 wymaga swoich plików (poniżej) i GPU z INT8; w przeciwnym razie gra sama wraca do FSR 3.1.',
        'Profil jakości',
        'Skala renderowania na oś: Quality renderuje w 1/1,5 rozdzielczości wyjścia.',
        'Rozdzielczość wyjścia',
        'To, co tworzy upscaler; interfejs też jest rysowany w tym rozmiarze.',
        'Zmiana rozdzielczości na żywo',
        'Wył.: wyjścia inne niż 1080p ustawia łatka przy starcie (najszybciej; zmiana w grze ją restartuje). Wł.: zmieniaj wyjście i profil w grze bez restartu, kosztem wydajności.',
        'Wyostrzanie (RCAS)', 'Siła wyostrzania', 'Wektory ruchu obiektów',
        'Mniej smużenia na postaciach, ubraniach i broni; kosztuje około 10% FPS.', 'Pliki FSR 4',
        'Pobierz pliki FSR 4',
        'Z FireBurn/Q2RTX na GitHub (zbudowane z kodu AMD FidelityFX na licencji MIT), około 30 MB, do folderu fsr4_shaders portu.',
        'Zainstalowano: {} plików w {}.', 'Brakuje {} z {} plików w {}.',
        'Pobieranie nie powiodło się: {}', 'Pokazuj licznik FPS', 'FSR 4 (najlepsza jakość)',
        'FSR 4.1.1 (wymaga plików fsr4_411)', 'FSR 3.1 (każde GPU)',
        'TAA (wygładzanie w natywnej rozdzielczości)', 'Wył.', 'Automatycznie (wg karty)',
        'Wył. (szybciej)', 'Wł. (zmiana bez restartu)', 'Stosowane przy uruchomieniu gry.',
        'Klatki przed GPU',
        '1 utrzymuje równe tempo klatek; więcej może podnieść FPS, gdy ogranicza karta.', 'Okno',
        'Wyświetlanie', 'Zezwól na HDR',
        'Gdy HDR jest włączony w Windows i obsługiwany przez ekran.',
        'Auto: odświeżanie ekranu, maks. 120 (zalecane)', 'Bez limitu',
        'Mailbox (niskie opóźnienie, bez rozrywania)', 'Immediate (z rozrywaniem)', '1 (domyślnie)',
        'Bez ograniczenia', 'Twój zrzut gry, zapisy i łatki gry.', 'Gra', 'Folder gry',
        'Wybierz folder z eboot.bin', 'Folder zapisów', 'Wybierz folder zapisów',
        'Puste: {} (tam też jest pamięć podręczna shaderów).', 'Język gry',
        'Angielski', 'Rosyjski', 'Japoński',
        'Francuski', 'Hiszpański', 'Niemiecki', 'Włoski',
        'Pliki gry nigdy nie są zmieniane: mody są nakładane przy starcie.', 'Mody', 'Wczytuj mody',
        'Folder modów', 'Wybierz folder modów', 'Puste: {}', 'Łatki zewnętrzne', 'Folder łatek',
        'Wybierz folder łatek', 'Odśwież', 'Brak modów w {}.',
        'Niżej na liście wczytuje się później i wygrywa konflikty.',
        'Opcje launchera, wydajność i diagnostyka.', 'Launcher', 'Język launchera',
        'Zadziała po ponownym otwarciu launchera.', 'Zamknij launcher po uruchomieniu gry',
        'Skrót na pulpicie', 'Folder portu', 'Utworzono skrót na pulpicie.', 'Wydajność',
        'Dwuetapowy potok GPU', 'Szybciej o 20–30%; wyłącz, jeśli gra jest niestabilna.',
        'Automatycznie (8+ wątków)', 'Wł.', 'Wył. (stabilniej)', 'Odczyty z GPU',
        'Jak dokładnie dane zapisane przez GPU wracają do gry.', 'Relaxed (domyślnie)',
        'Diagnostyka', 'Statystyki klatek w dzienniku (co 5 s)', 'Czas GPU na przebieg w dzienniku',
        'Warstwy walidacji Vulkan (wymaga Vulkan SDK; dużo wolniej)', 'Dodatkowe zmienne',
        'Pary NAZWA=wartość rozdzielone spacjami (lista w README).', 'Systemowy', 'Kopiuj',
        'Wyczyść', 'Przeglądaj…', 'Otwórz', 'Wyczyść pamięć shaderów',
        'Jeśli gra pokazuje tylko czarny ekran, zwykle to pomaga.',
        'Pamięć shaderów wyczyszczona. Następne uruchomienie przez kilka minut będzie się przycinać, aż się odbuduje.',
        'Dostępna jest wersja {}.', 'Aktualizuj', 'Co nowego',
        'Zainstalować teraz wersję {}? Launcher zamknie się, zainstaluje ją i otworzy ponownie. Zapisy i ustawienia zostaną zachowane.',
        'Pobieranie wersji {}… {}%', 'Aktualizacja nie powiodła się: {}',
        'Zamknij grę przed aktualizacją.', 'Sprawdzaj aktualizacje przy otwieraniu launchera',
        'Sprawdź aktualizacje', 'Masz najnowszą wersję ({}).',
        'Nie udało się sprawdzić aktualizacji: {}',
        'Nie udało się zainstalować aktualizacji. Pobierz ją ze strony wydań.', '', '', '', '', '',
        '', '', '', '', '',
    ],
    'tr': [
        'Oyna', 'Grafik', 'Ekran ve FPS', 'Oyun ve efektler', 'Modlar ve yamalar', 'Gelişmiş',
        'Günlük', 'OYNA', 'Durdur', 'yerel port · Windows',
        'Oyunda: Insert veya R3+L2\nport menüsünü açar.', 'Hazırlık kontrolü', 'Hızlı ayarlar',
        'Kare hızı', 'Ölçekleyici', 'Tam ekran', '{} · oyun sürümü {}', 'Oyun klasörü seçilmedi',
        'Oyun klasörünü seçin (Oyun ve efektler)',
        '✗ Oyun klasöründe eboot.bin yok (Oyun ve efektler)', '✓ Kayıtlar bulundu: {}',
        '• Henüz kayıt yok: oyun bunları şurada oluşturacak: {}', '✓ Ekran kartı: {}',
        '• Ekran kartı: kontrol edilmedi', '• Ekran kartı kontrol ediliyor…',
        '✗ Vulkan 1.3 destekli ekran kartı bulunamadı (sürücüyü güncelleyin)',
        'Önce oyun klasörünü seçin.', 'Oyun hazırlanıyor; kendi penceresinde açılacak…',
        'Oyun çalışıyor.', 'Yeni ayarlarla yeniden başlatılıyor…', '\nOyun kapandı (kod {}).\n',
        'Başlatılamadı: {}', 'eboot.bin içeren oyun klasörünü seçin (CUSA01623).',
        'spartanport.ini içinde saklanır; oyun içi menü (Insert veya R3+L2) aynı değerleri değiştirir.',
        'Ölçekleme',
        'Oyunun kendi hareket vektörleriyle zamansal ölçekleme. FSR 4 için dosyaları (aşağıda) ve INT8 destekli bir GPU gerekir; aksi halde oyun kendiliğinden FSR 3.1’e döner.',
        'Kalite ön ayarı',
        'Eksen başına render ölçeği: Quality, çıkış boyutunun 1/1,5’inde render eder.',
        'Çıkış çözünürlüğü', 'Ölçekleyicinin ürettiği boyut; arayüz de bu boyutta çizilir.',
        'Canlı çözünürlük değişimi',
        'Kapalı: 1080p dışındaki çıkışlar başlangıçta bir yamayla ayarlanır (en hızlısı; oyunda değiştirmek oyunu yeniden başlatır). Açık: çıkışı ve ön ayarı oyunda yeniden başlatmadan değiştirin, performans maliyetiyle.',
        'Keskinleştirme (RCAS)', 'Keskinlik gücü', 'Nesne hareket vektörleri',
        'Karakterlerde, kıyafetlerde ve silahlarda daha az iz; yaklaşık %10 FPS’e mal olur.',
        'FSR 4 dosyaları', 'FSR 4 dosyalarını indir',
        'GitHub’daki FireBurn/Q2RTX’ten (AMD’nin MIT lisanslı FidelityFX kaynağından derlenmiş), yaklaşık 30 MB, portun fsr4_shaders klasörüne.',
        'Kurulu: {} içinde {} dosya.', '{} dosyadan {} tanesi eksik: {}.', 'İndirme başarısız: {}',
        'FPS sayacını göster', 'FSR 4 (en iyi kalite)', 'FSR 4.1.1 (fsr4_411 dosyaları gerekir)',
        'FSR 3.1 (her GPU)', 'TAA (yerel çözünürlükte kenar yumuşatma)', 'Kapalı',
        'Otomatik (ekran kartına göre)', 'Kapalı (daha hızlı)',
        'Açık (yeniden başlatmadan değiştir)', 'Oyun başlarken uygulanır.',
        'GPU’nun önündeki kareler',
        '1 kare akışını düzenli tutar; daha fazlası ekran kartı darboğazken FPS’i artırabilir.',
        'Pencere', 'Sunum', 'HDR çıkışına izin ver', 'Windows’ta HDR açık ve ekran destekliyorsa.',
        'Otomatik: ekran yenileme hızı, en fazla 120 (önerilen)', 'Sınır yok',
        'Mailbox (düşük gecikme, yırtılma yok)', 'Immediate (yırtılmalı)', '1 (varsayılan)',
        'Sınırsız', 'Oyun dökümünüz, kayıtlarınız ve oyun yamaları.', 'Oyun', 'Oyun klasörü',
        'eboot.bin içeren klasörü seçin', 'Kayıt klasörü', 'Kayıt klasörünü seçin',
        'Boş: {} (shader önbelleği de orada tutulur).', 'Oyun dili',
        'İngilizce', 'Rusça', 'Japonca',
        'Fransızca', 'İspanyolca', 'Almanca', 'İtalyanca',
        'Oyun dosyaları asla değiştirilmez: modlar başlangıçta üzerlerine eklenir.', 'Modlar',
        'Modları yükle', 'Mod klasörü', 'Mod klasörünü seçin', 'Boş: {}', 'Üçüncü taraf yamalar',
        'Yama klasörü', 'Yama klasörünü seçin', 'Yenile', '{} içinde henüz mod yok.',
        'Listede daha aşağıdaki sonra yüklenir ve çakışmalarda kazanır.',
        'Başlatıcı seçenekleri, performans ve tanılama.', 'Başlatıcı', 'Başlatıcı dili',
        'Başlatıcı yeniden açıldığında uygulanır.', 'Oyun başlayınca başlatıcıyı kapat',
        'Masaüstü kısayolu', 'Port klasörü', 'Masaüstünde kısayol oluşturuldu.', 'Performans',
        'İki aşamalı GPU hattı', '%20–30 daha hızlı; oyun kararsızsa kapatın.',
        'Otomatik (8+ iş parçacığı)', 'Açık', 'Kapalı (daha kararlı)', 'GPU geri okumaları',
        'GPU’nun yazdığı verilerin oyuna ne kadar doğru geri döndüğü.', 'Relaxed (varsayılan)',
        'Tanılama', 'Günlükte kare istatistikleri (5 sn’de bir)',
        'Günlükte geçiş başına GPU süresi',
        'Vulkan doğrulama katmanları (Vulkan SDK gerekir; çok daha yavaş)', 'Ek değişkenler',
        'Boşlukla ayrılmış AD=değer çiftleri (liste README’de).', 'Sistem', 'Kopyala', 'Temizle',
        'Gözat…', 'Aç', 'Shader önbelleğini temizle',
        'Oyun yalnızca siyah ekran gösteriyorsa bu genellikle işe yarar.',
        'Shader önbelleği temizlendi. Sonraki açılış, önbellek yeniden oluşurken birkaç dakika takılabilir.',
        '{} sürümü çıktı.', 'Güncelle', 'Yenilikler',
        '{} sürümü şimdi kurulsun mu? Başlatıcı kapanır, sürümü kurar ve yeniden açılır. Kayıtlar ve ayarlar korunur.',
        '{} sürümü indiriliyor… %{}', 'Güncelleme başarısız: {}',
        'Güncellemeden önce oyunu kapatın.', 'Başlatıcı açılırken güncellemeleri denetle',
        'Güncellemeleri denetle', 'En son sürüme sahipsiniz ({}).',
        'Güncellemeler denetlenemedi: {}', 'Güncelleme kurulamadı. Sürümler sayfasından indirin.',
        '', '', '', '', '', '', '', '', '', '',
    ],
    'zh': [
        '开始', '图形', '显示与帧率', '游戏与效果', '模组与补丁', '高级', '日志', '开始游戏', '停止', '原生移植 · Windows',
        '游戏中：Insert 或 R3+L2\n打开移植版菜单。', '就绪检查', '快速设置', '帧率', '超分辨率', '全屏', '{} · 游戏版本 {}',
        '未设置游戏文件夹', '请选择游戏文件夹（游戏与效果）', '✗ 游戏文件夹中没有 eboot.bin（游戏与效果）', '✓ 在 {} 找到存档',
        '• 暂无存档：游戏会在 {} 中创建', '✓ 显卡：{}', '• 显卡：未检查', '• 正在检查显卡…', '✗ 未找到支持 Vulkan 1.3 的显卡（请更新驱动）',
        '请先选择游戏文件夹。', '正在准备游戏，将在单独的窗口中打开…', '游戏正在运行。', '正在以新设置重新启动…', '\n— 游戏已退出（代码 {}）—\n',
        '无法启动：{}', '请选择包含 eboot.bin 的游戏文件夹（CUSA01623）。',
        '保存在 spartanport.ini 中；游戏内菜单（Insert 或 R3+L2）会修改相同的值。', '超分辨率',
        '使用游戏自身运动矢量的时间性超分辨率。FSR 4 需要其文件（见下方）和支持 INT8 的显卡；否则游戏会自动改用 FSR 3.1。', '画质预设',
        '每轴渲染比例：Quality 以输出尺寸的 1/1.5 渲染。', '输出分辨率', '超分辨率生成的尺寸；界面也以此尺寸绘制。', '实时切换分辨率',
        '关闭：1080p 以外的输出在启动时由补丁设置（最快；在游戏中更改会重启游戏）。开启：在游戏中更改输出和预设无需重启，但有性能开销。', '锐化 (RCAS)', '锐化强度',
        '物体运动矢量', '减少角色、服装和武器上的拖影；约损失 10% 帧率。', 'FSR 4 文件', '下载 FSR 4 文件',
        '来自 GitHub 上的 FireBurn/Q2RTX（由 AMD 采用 MIT 许可的 FidelityFX 源码构建），约 30 MB，放入移植版的 fsr4_shaders 文件夹。',
        '已安装：{} 个文件，位于 {}。', '{} 中缺少 {} 个文件中的 {} 个。', '下载失败：{}', '显示帧率计数器', 'FSR 4（最佳画质）',
        'FSR 4.1.1（需要 fsr4_411 文件）', 'FSR 3.1（任何显卡）', 'TAA（原生分辨率抗锯齿）', '关闭', '自动（根据显卡）', '关闭（更快）',
        '开启（无需重启即可更改）', '在游戏启动时应用。', '领先 GPU 的帧数', '1 可保持帧节奏平稳；显卡成为瓶颈时，更大的值可能提高帧率。', '窗口', '呈现方式',
        '允许 HDR 输出', '在 Windows 中开启 HDR 且显示器支持时。', '自动：显示器刷新率，最高 120（推荐）', '不限制',
        'Mailbox（低延迟，无撕裂）', 'Immediate（有撕裂）', '1（默认）', '不限', '你的游戏转储、存档和游戏补丁。', '游戏', '游戏文件夹',
        '选择包含 eboot.bin 的文件夹', '存档文件夹', '选择存档文件夹', '为空：{}（着色器缓存也保存在此）。', '游戏语言',
        '英语', '俄语', '日语', '法语', '西班牙语', '德语', '意大利语',
        '游戏文件永远不会被修改：模组在启动时叠加。', '模组', '加载模组', '模组文件夹', '选择模组文件夹', '为空：{}', '第三方补丁', '补丁文件夹',
        '选择补丁文件夹', '刷新', '{} 中还没有模组。', '列表中越靠下越晚加载，冲突时优先。', '启动器选项、性能开关和诊断。', '启动器', '启动器语言',
        '重新打开启动器后生效。', '游戏启动时关闭启动器', '桌面快捷方式', '移植版文件夹', '已在桌面创建快捷方式。', '性能', '两阶段 GPU 流水线',
        '快 20–30%；如果游戏不稳定请关闭。', '自动（8 线程以上）', '开启', '关闭（更稳定）', 'GPU 回读', 'GPU 写入的数据返回给游戏的精确程度。',
        'Relaxed（默认）', '诊断', '在日志中显示帧统计（每 5 秒）', '在日志中显示每个通道的 GPU 时间',
        'Vulkan 验证层（需要 Vulkan SDK；会慢很多）', '额外变量', '以空格分隔的 名称=值 对（列表见 README）。', '跟随系统', '复制', '清除',
        '浏览…', '打开', '清除着色器缓存', '如果游戏只显示黑屏，这通常能解决。', '着色器缓存已清除。下次启动的前几分钟会因重建缓存而卡顿。', '新版本 {} 已发布。',
        '更新', '更新内容', '现在安装版本 {} 吗？启动器会关闭、完成安装后重新打开。存档和设置会保留。', '正在下载版本 {}… {}%', '更新失败：{}',
        '请先关闭游戏再更新。', '打开启动器时检查更新', '检查更新', '已是最新版本（{}）。', '无法检查更新：{}', '无法安装更新。请从发布页面下载。', '', '',
        '', '', '', '', '', '', '', '',
    ],
    'ja': [
        'プレイ', 'グラフィック', '表示とFPS', 'ゲームとエフェクト', 'MODとパッチ', '詳細設定', 'ログ', 'プレイ', '停止',
        'ネイティブ移植 · Windows', 'ゲーム中：Insert または R3+L2\nで移植版メニューを開きます。', '準備チェック', 'クイック設定', 'フレームレート',
        'アップスケーラー', 'フルスクリーン', '{} · ゲームバージョン {}', 'ゲームフォルダー未設定', 'ゲームフォルダーを選択してください（ゲームとエフェクト）',
        '✗ ゲームフォルダーに eboot.bin がありません（ゲームとエフェクト）', '✓ セーブデータが見つかりました：{}',
        '• セーブデータはまだありません：ゲームが {} に作成します', '✓ グラフィックカード：{}', '• グラフィックカード：未確認', '• グラフィックカードを確認中…',
        '✗ Vulkan 1.3 対応のグラフィックカードが見つかりません（ドライバーを更新してください）', '先にゲームフォルダーを選択してください。',
        'ゲームを準備しています。専用のウィンドウで開きます…', 'ゲーム実行中。', '新しい設定で再起動しています…', '\n— ゲームが終了しました（コード {}）—\n',
        '起動できませんでした：{}', 'eboot.bin のあるゲームフォルダーを選択してください（CUSA01623）。',
        'spartanport.ini に保存されます。ゲーム内メニュー（Insert または R3+L2）でも同じ値を変更できます。', 'アップスケーリング',
        'ゲーム自身のモーションベクトルを使うテンポラルアップスケーリング。FSR 4 にはファイル（下記）と INT8 対応 GPU が必要です。ない場合は自動で FSR 3.1 になります。',
        '画質プリセット', '軸ごとの描画スケール：Quality は出力サイズの 1/1.5 で描画します。', '出力解像度',
        'アップスケーラーが出力するサイズ。UI もこのサイズで描画されます。', '解像度のライブ変更',
        'オフ：1080p 以外の出力は起動時にパッチで設定（最速。ゲーム内で変更すると再起動）。オン：再起動なしでゲーム内から出力とプリセットを変更できますが、負荷が増えます。',
        'シャープ化 (RCAS)', 'シャープの強さ', 'オブジェクトのモーションベクトル', 'キャラクター・衣服・武器の残像を減らします。FPS が約 10% 下がります。',
        'FSR 4 ファイル', 'FSR 4 ファイルをダウンロード',
        'GitHub の FireBurn/Q2RTX から（AMD の MIT ライセンスの FidelityFX ソースからビルド）、約 30 MB、移植版の fsr4_shaders フォルダーへ。',
        'インストール済み：{} ファイル（{}）。', '{} 中 {} ファイルが {} にありません。', 'ダウンロードに失敗しました：{}', 'FPS カウンターを表示',
        'FSR 4（最高画質）', 'FSR 4.1.1（fsr4_411 ファイルが必要）', 'FSR 3.1（すべての GPU）', 'TAA（ネイティブ解像度のアンチエイリアス）',
        'オフ', '自動（グラフィックカードに応じて）', 'オフ（高速）', 'オン（再起動なしで変更）', 'ゲーム起動時に適用されます。', 'GPU より先行するフレーム数',
        '1 はフレームペースを安定させます。GPU がボトルネックの場合、増やすと FPS が上がることがあります。', 'ウィンドウ', '表示方式', 'HDR 出力を許可',
        'Windows で HDR がオンで、ディスプレイが対応している場合。', '自動：ディスプレイのリフレッシュレート、最大 120（推奨）', '上限なし',
        'Mailbox（低遅延・ティアリングなし）', 'Immediate（ティアリングあり）', '1（既定）', '無制限', 'ゲームのダンプ、セーブデータ、ゲームパッチ。',
        'ゲーム', 'ゲームフォルダー', 'eboot.bin のあるフォルダーを選択', 'セーブフォルダー', 'セーブフォルダーを選択',
        '空欄：{}（シェーダーキャッシュもここに保存されます）。', 'ゲームの言語', '英語',
        'ロシア語', '日本語', 'フランス語', 'スペイン語', 'ドイツ語', 'イタリア語', 'ゲームファイルは変更されません。MOD は起動時に重ねて読み込まれます。',
        'MOD', 'MOD を読み込む', 'MOD フォルダー', 'MOD フォルダーを選択', '空欄：{}', 'サードパーティ製パッチ', 'パッチフォルダー',
        'パッチフォルダーを選択', '更新', '{} にはまだ MOD がありません。', 'リストの下にあるものほど後に読み込まれ、競合時に優先されます。',
        'ランチャーの設定、パフォーマンス、診断。', 'ランチャー', 'ランチャーの言語', 'ランチャーを再度開いたときに適用されます。', 'ゲーム起動時にランチャーを閉じる',
        'デスクトップのショートカット', '移植版フォルダー', 'デスクトップにショートカットを作成しました。', 'パフォーマンス', '2 段階 GPU パイプライン',
        '20〜30% 高速。ゲームが不安定ならオフにしてください。', '自動（8 スレッド以上）', 'オン', 'オフ（安定）', 'GPU リードバック',
        'GPU が書き込んだデータをどれだけ正確にゲームへ戻すか。', 'Relaxed（既定）', '診断', 'ログにフレーム統計（5 秒ごと）', 'ログにパスごとの GPU 時間',
        'Vulkan 検証レイヤー（Vulkan SDK が必要。大幅に遅くなります）', '追加の変数', 'スペース区切りの 名前=値 のペア（一覧は README）。',
        'システム', 'コピー', 'クリア', '参照…', '開く', 'シェーダーキャッシュを消去', 'ゲームが黒い画面のままのときは、たいていこれで直ります。',
        'シェーダーキャッシュを消去しました。次回起動時は再構築のため数分間カクつきます。', 'バージョン {} が利用できます。', '更新', '新機能',
        'バージョン {} を今すぐインストールしますか？ランチャーが閉じてインストールし、再び開きます。セーブデータと設定はそのままです。',
        'バージョン {} をダウンロード中… {}%', '更新に失敗しました：{}', '更新する前にゲームを終了してください。', 'ランチャー起動時に更新を確認', '更新を確認',
        '最新バージョンです（{}）。', '更新を確認できませんでした：{}', '更新をインストールできませんでした。リリースページからダウンロードしてください。', '', '',
        '', '', '', '', '', '', '', '',
    ],
    'ko': [
        '플레이', '그래픽', '디스플레이 및 FPS', '게임 및 효과', '모드 및 패치', '고급', '로그', '플레이', '중지',
        '네이티브 포트 · Windows', '게임 중: Insert 또는 R3+L2\n포트 메뉴 열기.', '준비 상태', '빠른 설정', '프레임 속도',
        '업스케일러', '전체 화면', '{} · 게임 버전 {}', '게임 폴더가 지정되지 않음', '게임 폴더를 선택하세요 (게임 및 효과)',
        '✗ 게임 폴더에 eboot.bin이 없음 (게임 및 효과)', '✓ 세이브 발견: {}', '• 아직 세이브 없음: 게임이 {}에 만듭니다',
        '✓ 그래픽 카드: {}', '• 그래픽 카드: 확인 안 됨', '• 그래픽 카드 확인 중…',
        '✗ Vulkan 1.3 그래픽 카드를 찾을 수 없음 (드라이버를 업데이트하세요)', '먼저 게임 폴더를 선택하세요.', '게임 준비 중; 별도 창에서 열립니다…',
        '게임 실행 중.', '새 설정으로 다시 시작하는 중…', '\n게임이 종료됨 (코드 {}).\n', '시작할 수 없음: {}',
        'eboot.bin이 있는 게임 폴더를 선택하세요 (CUSA01623).',
        'spartanport.ini에 저장됩니다. 게임 내 메뉴(Insert 또는 R3+L2)에서도 같은 값을 바꿉니다.', '업스케일링',
        '게임 자체의 모션 벡터를 사용한 시간적 업스케일링. FSR 4에는 파일(아래)과 INT8 지원 GPU가 필요하며, 없으면 게임이 자동으로 FSR 3.1을 사용합니다.',
        '품질 프리셋', '축별 렌더 배율: Quality는 출력 크기의 1/1.5로 렌더링합니다.', '출력 해상도',
        '업스케일러가 만드는 크기. UI도 이 크기로 그려집니다.', '실시간 해상도 변경',
        '끔: 1080p 외의 출력은 시작 시 패치로 설정 (가장 빠름, 게임에서 바꾸면 재시작). 켬: 재시작 없이 게임에서 출력과 프리셋 변경, 성능 비용 있음.',
        '선명화 (RCAS)', '선명도 강도', '오브젝트 모션 벡터', '캐릭터, 옷, 무기의 잔상이 줄어듭니다. FPS가 약 10% 감소합니다.',
        'FSR 4 파일', 'FSR 4 파일 다운로드',
        'GitHub의 FireBurn/Q2RTX에서 (AMD의 MIT 라이선스 FidelityFX 소스로 빌드), 약 30 MB, 포트의 fsr4_shaders 폴더로.',
        '설치됨: {}개 파일, 위치 {}.', '{}개 중 {}개 파일이 {}에 없음.', '다운로드 실패: {}', 'FPS 카운터 표시',
        'FSR 4 (최고 품질)', 'FSR 4.1.1 (fsr4_411 파일 필요)', 'FSR 3.1 (모든 GPU)', 'TAA (네이티브 해상도 안티에일리어싱)',
        '끔', '자동 (그래픽 카드에 따라)', '끔 (더 빠름)', '켬 (재시작 없이 변경)', '게임 시작 시 적용됩니다.', 'GPU보다 앞선 프레임',
        '1은 프레임 페이싱을 고르게 유지합니다. 그래픽 카드가 병목이면 더 높이면 FPS가 오를 수 있습니다.', '창', '표시 방식', 'HDR 출력 허용',
        'Windows에서 HDR이 켜져 있고 디스플레이가 지원할 때.', '자동: 디스플레이 주사율, 최대 120 (권장)', '제한 없음',
        'Mailbox (낮은 지연, 티어링 없음)', 'Immediate (티어링 있음)', '1 (기본)', '무제한', '게임 덤프, 세이브, 게임 패치.',
        '게임', '게임 폴더', 'eboot.bin이 있는 폴더 선택', '세이브 폴더', '세이브 폴더 선택',
        '비어 있음: {} (셰이더 캐시도 여기에 저장됩니다).', '게임 언어',
        '영어', '러시아어', '일본어', '프랑스어', '스페인어', '독일어', '이탈리아어',
        '게임 파일은 절대 바뀌지 않습니다. 모드는 시작 시 위에 덧씌워집니다.', '모드', '모드 불러오기', '모드 폴더', '모드 폴더 선택',
        '비어 있음: {}', '서드파티 패치', '패치 폴더', '패치 폴더 선택', '새로 고침', '{}에 아직 모드가 없습니다.',
        '목록에서 아래쪽일수록 나중에 불러오며 충돌 시 우선합니다.', '런처 옵션, 성능 스위치, 진단.', '런처', '런처 언어', '런처를 다시 열면 적용됩니다.',
        '게임 시작 시 런처 닫기', '바탕화면 바로가기', '포트 폴더', '바탕화면에 바로가기를 만들었습니다.', '성능', '2단계 GPU 파이프라인',
        '20–30% 더 빠름. 게임이 불안정하면 끄세요.', '자동 (8스레드 이상)', '켬', '끔 (더 안정적)', 'GPU 리드백',
        'GPU가 쓴 데이터를 게임에 얼마나 정확히 되돌려 주는지.', 'Relaxed (기본)', '진단', '로그에 프레임 통계 (5초마다)',
        '로그에 패스별 GPU 시간', 'Vulkan 검증 레이어 (Vulkan SDK 필요, 훨씬 느림)', '추가 변수',
        '공백으로 구분한 이름=값 쌍 (목록은 README).', '시스템', '복사', '지우기', '찾아보기…', '열기', '셰이더 캐시 지우기',
        '게임이 검은 화면만 보이면 보통 이것으로 해결됩니다.', '셰이더 캐시를 지웠습니다. 다음 실행은 캐시를 다시 만드는 몇 분 동안 끊길 수 있습니다.',
        '버전 {}을(를) 사용할 수 있습니다.', '업데이트', '새로운 기능',
        '지금 버전 {}을(를) 설치할까요? 런처가 닫히고 설치한 뒤 다시 열립니다. 세이브와 설정은 유지됩니다.', '버전 {} 다운로드 중… {}%',
        '업데이트 실패: {}', '업데이트하기 전에 게임을 종료하세요.', '런처를 열 때 업데이트 확인', '업데이트 확인', '최신 버전입니다 ({}).',
        '업데이트를 확인할 수 없습니다: {}', '업데이트를 설치할 수 없습니다. 릴리스 페이지에서 다운로드하세요.', '', '', '', '', '', '', '',
        '', '', '',
    ],
    'id': [
        'Main', 'Grafis', 'Tampilan & FPS', 'Game & efek', 'Mod & patch', 'Lanjutan', 'Log',
        'MAIN', 'Berhenti', 'Port PC · Windows', "Dalam game: Insert atau R3+L2\nmembuka menu port.",
        'Pemeriksaan kesiapan', 'Pengaturan cepat', 'Frame rate', 'Upscaler', 'Layar penuh', '{} · versi game {}',
        'Folder game belum diatur', 'Pilih folder game Anda (Game & efek)',
        '✗ Tidak ada eboot.bin di folder game (Game & efek)', '✓ Simpanan ditemukan di {}',
        '• Belum ada simpanan: game akan membuatnya di {}', '✓ Kartu grafis: {}',
        '• Kartu grafis: belum diperiksa', '• Memeriksa kartu grafis…',
        '✗ Tidak ditemukan kartu grafis Vulkan 1.3 (perbarui driver)', 'Pilih folder game terlebih dahulu.',
        'Menyiapkan game; game akan terbuka di jendelanya sendiri…', 'Game sedang berjalan.',
        'Memulai ulang dengan pengaturan baru…', '\nGame ditutup (kode {}).\n', 'Tidak dapat memulai: {}',
        'Pilih folder game dengan eboot.bin (CUSA01623).',
        'Tersimpan di spartanport.ini; menu dalam game (Insert atau R3+L2) mengubah nilai yang sama.',
        'Upscaling',
        "Temporal upscaling dengan vektor gerak game itu sendiri. FSR 4 memerlukan asetnya (di bawah) dan GPU dengan INT8 dot product; jika tidak, game akan otomatis beralih ke FSR 3.1.",
        'Preset kualitas', 'Skala render per sumbu: Kualitas merender pada 1/1,5 dari ukuran output.',
        'Resolusi output', 'Hasil yang dihasilkan upscaler; HUD juga digambar pada ukuran ini.',
        'Perubahan resolusi langsung',
        'Mati: output selain 1080p diatur oleh patch saat mulai (tercepat; mengubahnya dalam game akan memulai ulang game). Nyala: ubah output dan preset dalam game tanpa mulai ulang, dengan sedikit penurunan performa.',
        'Penajaman (RCAS)', 'Ketajaman', 'Vektor gerak objek',
        'Mengurangi efek ghosting pada karakter, pakaian, dan senjata; memakan sekitar 10% FPS.',
        'Aset FSR 4', 'Unduh aset FSR 4',
        "Dari FireBurn/Q2RTX di GitHub (dibuat dari kode sumber AMD FidelityFX berlisensi MIT), sekitar 30 MB, ke dalam folder fsr4_shaders port.",
        'Terinstal: {} file di {}.', '{} dari {} file hilang di {}.', 'Pengunduhan gagal: {}',
        'Tampilkan penghitung FPS', 'FSR 4 (kualitas terbaik)', 'FSR 4.1.1 (memerlukan aset fsr4_411)',
        'FSR 3.1 (semua GPU)', 'TAA (anti-aliasing resolusi asli)', 'Mati', 'Otomatis (berdasarkan kartu grafis)',
        'Mati (lebih cepat)', 'Nyala (ubah tanpa mulai ulang)', 'Diterapkan saat game dimulai.',
        'Frame mendahului GPU',
        '1 menjaga frame pacing tetap merata; lebih banyak dapat meningkatkan FPS saat kartu grafis menjadi batas performa.',
        'Jendela', 'Presentasi', 'Izinkan output HDR', 'Saat HDR aktif di Windows dan layar mendukungnya.',
        'Otomatis: refresh layar, maks 120 (disarankan)', 'Tanpa batas', 'Mailbox (latensi rendah, tanpa tearing)',
        'Immediate (dengan tearing)', '1 (default)', 'Tak terbatas', 'Dump game, simpanan, dan patch game Anda.', 'Game',
        'Folder game', 'Pilih folder dengan eboot.bin', 'Folder simpanan', 'Pilih folder simpanan',
        'Kosong: {} (cache shader juga disimpan di sana).', 'Bahasa game',
        'Inggris', 'Rusia', 'Jepang',
        'Prancis', 'Spanyol', 'Jerman', 'Italia',
        'File game tidak pernah diubah: mod diterapkan di atasnya saat mulai.', 'Mod',
        'Muat mod', 'Folder mod', 'Pilih folder mod', 'Kosong: {}', 'Patch pihak ketiga',
        'Folder patch', 'Pilih folder patch', 'Segarkan', 'Belum ada mod di {}.',
        'Yang lebih bawah dalam daftar dimuat lebih akhir dan menang jika konflik.',
        'Opsi launcher, sakelar performa, dan diagnostik.', 'Launcher', 'Bahasa launcher',
        'Berlaku saat launcher dibuka kembali.', 'Tutup launcher saat game dimulai', 'Pintasan desktop',
        'Folder port', 'Pintasan dibuat di desktop.', 'Performa', 'Pipeline GPU dua tahap',
        '20–30% lebih cepat; matikan jika game tidak stabil.', 'Otomatis (8+ thread)', 'Nyala',
        'Mati (lebih stabil)', 'Readback GPU',
        'Seberapa tepat data yang ditulis GPU disalin kembali untuk game.', 'Relaxed (default)', 'Diagnostik',
        'Statistik frame di log (setiap 5 d)', 'Waktu GPU per pass di log',
        'Vulkan validation layers (memerlukan Vulkan SDK; jauh lebih lambat)', 'Variabel tambahan',
        'Pasangan NAMA=nilai dipisahkan dengan spasi (daftar ada di README).', 'Sistem', 'Salin', 'Bersihkan',
        'Jelajahi…', 'Buka', 'Hapus cache shader',
        'Jika game hanya menampilkan layar hitam, ini biasanya membantu.',
        'Cache shader telah dihapus. Startup berikutnya akan stutter selama beberapa menit saat cache dibuat ulang.',
        'Versi {} telah tersedia.', 'Pembaruan', 'Apa yang baru',
        'Instal versi {} sekarang? Launcher akan ditutup, menginstalnya, lalu terbuka kembali. Simpanan dan pengaturan tetap tersimpan.',
        'Mengunduh versi {}… {}%', 'Pembaruan gagal: {}', 'Tutup game sebelum memperbarui.',
        'Periksa pembaruan saat launcher dibuka', 'Periksa pembaruan', 'Anda memiliki versi terbaru ({}).',
        'Tidak dapat memeriksa pembaruan: {}',
        'Tidak dapat menginstal pembaruan. Unduh dari halaman rilis.', 'Patch game',
        'Patch komunitas untuk game yang dipilih, diterapkan saat mulai.', '{} · versi {} · {}',
        'Asli (1920 × 1080)', 'Resolusi',
        'Game merender pada ukuran ini; patch resolusi menggantikan perbaikan tekstur dan memesan memori yang dibutuhkannya.',
        'Patch', '✓ Versi game {}: patch game diterapkan',
        '⚠ Versi game {}: patch game memerlukan {}; tidak ada yang diterapkan', '{} · {} patch lainnya',
    ],
}

LANGUAGE_NAMES = [('', ('System', 'Как в системе')), ('en', ('English',)), ('ru', ('Русский',)),
                  ('ar', ('العربية',)), ('es', ('Español',)), ('pt', ('Português (Brasil)',)),
                  ('fr', ('Français',)), ('de', ('Deutsch',)), ('it', ('Italiano',)),
                  ('pl', ('Polski',)), ('tr', ('Türkçe',)), ('zh', ('简体中文',)), ('ja', ('日本語',)),
                  ('ko', ('한국어',)), ('id', ('Bahasa Indonesia',))]
# Windows primary language ids (GetUserDefaultUILanguage() & 0x3ff) -> launcher language.
WINDOWS_LANGUAGES = {0x19: 'ru', 0x01: 'ar', 0x0a: 'es', 0x16: 'pt', 0x0c: 'fr', 0x07: 'de', 0x10: 'it',
                     0x15: 'pl', 0x1f: 'tr', 0x04: 'zh', 0x11: 'ja', 0x12: 'ko', 0x21: 'id'}

# Texts added after the KEYS tables were written, looked up by English text (positional tables
# need every language updated at once). Portuguese (the port's own language) and Spanish: the
# other languages fall back to English for them until someone who speaks them contributes.
EXTRA = {
    'pt': {
        'The launcher cannot write to {}, so it cannot replace itself there. Open the releases page to download the new version by hand?': 'O launcher não consegue gravar em {}, então não pode se substituir ali. Abrir a página de releases para baixar a versão nova manualmente?',
        'Version {} is out, but it has no Linux build yet.': 'A versão {} saiu, mas ainda não tem build para Linux.',
        'Show Discord Rich Presence': 'Mostrar Discord Rich Presence',
        'Shows only the game name and the time played on your Discord profile.': 'Mostra no seu perfil do Discord apenas o nome do jogo e o tempo jogado.',
        'Game multisampling': 'Multisampling do jogo',
        'Automatic ({}: off)': 'Automático ({}: desligado)',
        'Automatic ({}: on)': 'Automático ({}: ligado)',
        'Off (fixes black lines)': 'Desligado (corrige as linhas pretas)',
        "The game's 2-sample depth buffer, read as a texture by its lighting. AMD's and Intel's Windows drivers read it wrong and the lighting shows black lines, so Automatic turns multisampling off on those cards (Radeon, Arc) and keeps it on elsewhere (NVIDIA). Off and On force the choice on any card.":
            'O buffer de profundidade de 2 amostras do jogo, lido como textura pela iluminação. Os drivers da AMD e da Intel no Windows o leem errado e a iluminação mostra linhas pretas, então Automático desliga o multisampling nessas placas (Radeon, Arc) e o mantém nas demais (NVIDIA). Desligado e Ligado forçam a escolha em qualquer placa.',
        'Display': 'Tela',
        'How the game appears on your screen.': 'Como o jogo aparece na sua tela.',
        'Display mode': 'Modo de exibição',
        'Borderless covers the screen without changing the display mode.': 'Sem bordas cobre a tela inteira sem alterar o modo de vídeo.',
        'VSync': 'VSync',
        'Off allows tearing for the lowest latency.': 'Desligado permite tearing, para a menor latência.',
        'Enlarges a smaller game image to the window. With Render resolution 720p (Performance) it raises the FPS; at the size of the window it does nothing.': 'Amplia uma imagem menor do jogo até a janela. Com Resolução de renderização em 720p (Desempenho) aumenta o FPS; no tamanho da janela não faz nada.',
        'Overlay': 'Sobreposição',
        'Show performance overlay': 'Mostrar sobreposição de desempenho',
        'Items, size and corner: in-game menu (Insert / R3+L2) > Overlay.': 'Itens, tamanho e canto: menu do jogo (Insert / R3+L2) > Sobreposição.',
        'Frame rate and speed options.': 'Opções de taxa de quadros e velocidade.',
        'Frame rate limit': 'Limite de taxa de quadros',
        'The engine frame rate (below) is the highest it can go.': 'A taxa de quadros do motor (abaixo) é o máximo que ele alcança.',
        'Frames queued': 'Quadros em fila',
        '1 is the lowest input latency; more can raise FPS when the graphics card is the limit.': '1 dá a menor latência de entrada; mais pode aumentar o FPS quando a placa de vídeo é o limite.',
        'Engine frame rate': 'Taxa de quadros do motor',
        'The highest frame rate the game itself runs at (a game patch).': 'A maior taxa de quadros em que o próprio jogo roda (um patch do jogo).',
        'Render resolution': 'Resolução de renderização',
        'The size the game draws at; higher is sharper and slower. A resolution patch replaces the texture fix and reserves the memory it needs.': 'O tamanho em que o jogo desenha; maior é mais nítido e mais lento. Um patch de resolução substitui a correção de texturas e reserva a memória de que precisa.',
        'Shaders': 'Shaders',
        'Use pipeline binary cache (A/B test)': 'Usar cache binário de pipelines (teste A/B)',
        'Reuse saved Vulkan pipelines. Disable to compare FPS with normal compilation. Takes effect on the next launch; saved caches are preserved.': 'Reutiliza pipelines Vulkan salvos. Desative para comparar o FPS com a compilação normal. Vale a partir da próxima inicialização; os caches salvos são mantidos.',
        'Load shader cache in the background': 'Carregar o cache de shaders em segundo plano',
        'The game opens at once and the saved shaders load while you play; a small note in the corner shows the progress. Switch it off to wait on the loading screen instead.': 'O jogo abre na hora e os shaders salvos carregam enquanto você joga; uma pequena nota no canto mostra o progresso. Desative para esperar na tela de carregamento.',
        'Compile new shaders in the background': 'Compilar novos shaders em segundo plano',
        'Fewer stutters in new areas: the game keeps running while new shaders compile. An object or effect may appear a moment later. Switch it off if something stays missing.': 'Menos travadas em áreas novas: o jogo continua rodando enquanto novos shaders compilam. Um objeto ou efeito pode aparecer um instante depois. Desative se algo continuar faltando.',
        'Fast shader cache loading (parallel)': 'Carregamento rápido do cache de shaders (paralelo)',
        'Loading screen only: builds the cached pipelines on several CPU threads.': 'Só na tela de carregamento: monta os pipelines em cache em várias threads da CPU.',
        'GPU data': 'Dados da GPU',
        'Stream buffer memory': 'Memória do buffer de streaming',
        'System RAM can improve FPS on NVIDIA laptops. Choose GPU memory to restore the previous behavior. Applies on the next game launch.': 'A RAM do sistema pode melhorar o FPS em notebooks NVIDIA. Escolha memória da GPU para restaurar o comportamento anterior. Vale na próxima inicialização do jogo.',
        'Game reads GPU data without waiting (big FPS gain)': 'O jogo lê dados da GPU sem esperar (grande ganho de FPS)',
        'About 40 to 66 FPS in a heavy scene. Needs deferred GPU readbacks. Switch it off if lighting or objects look wrong.': 'Cerca de 40 a 66 FPS numa cena pesada. Exige leituras da GPU adiadas. Desative se a iluminação ou os objetos parecerem errados.',
        'Deferred GPU readbacks': 'Leituras da GPU adiadas',
        'Fewer waits for the GPU. Switch it off if textures or shadows look wrong.': 'Menos esperas pela GPU. Desative se texturas ou sombras parecerem erradas.',
        'Your game dump, saves and language.': 'Seu dump do jogo, saves e idioma.',
        'Cheats': 'Cheats',
        'Infinite Double Jumps': 'Pulos duplos infinitos',
        'Repeat air jumps without landing. Also available in the in-game Cheats menu.': 'Repete pulos no ar sem tocar o chão. Também está no menu de Cheats do jogo.',
        'Unlock Very Hard': 'Liberar Muito Difícil',
        'Select Very Hard without completing the game. Does not change existing save difficulty. In-game: enable before opening New Game.': 'Seleciona Muito Difícil sem terminar o jogo. Não muda a dificuldade de saves existentes. No jogo: ative antes de abrir Novo Jogo.',
        'Folders': 'Pastas',
        'Your own dump of the game (eboot.bin, sce_module, sce_sys); SpartanPort: CUSA01623, version 01.02 for the community patches.': 'Seu próprio dump do jogo (eboot.bin, sce_module, sce_sys); SpartanPort: CUSA01623, versão 01.02 para os patches da comunidade.',
        'Update folder (optional)': 'Pasta do update (opcional)',
        'Choose the folder of the update (eboot.bin, sce_sys)': 'Escolha a pasta do update (eboot.bin, sce_sys)',
        'Only if the update is dumped apart from the game: it is laid over the game folder without changing either. Leave empty when the game folder already has the update.': 'Só se o update foi dumpado separado do jogo: ele é colocado por cima da pasta do jogo sem alterar nenhuma das duas. Deixe vazio quando a pasta do jogo já tiver o update.',
        'Saves': 'Saves',
        'Language': 'Idioma',
        'Save backups': 'Backups de saves',
        'Changes applied to the game code at start.': 'Alterações aplicadas ao código do jogo na inicialização.',
        'Files layered over the game at start; the game files are never changed.': 'Arquivos colocados por cima do jogo na inicialização; os arquivos do jogo nunca são alterados.',
        'Loose-file mods, each in its own folder with dvdroot_ps4.': 'Mods de arquivos soltos, cada um na sua pasta com dvdroot_ps4.',
        'Controls': 'Controles',
        'Which key or gamepad button presses each button of the game.': 'Qual tecla ou botão do controle aciona cada botão do jogo.',
        'Gamepad names': 'Nomes dos botões do controle',
        "Keys use SDL names: letters, digits, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Up, Down, Left, Right, F1 to F12. A name the game does not know keeps the default (the log says which). L2/R2 on the gamepad stay on the triggers. Insert and R3+L2 always open the port's menu. Applied when the game starts.": 'As teclas usam nomes do SDL: letras, dígitos, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Up, Down, Left, Right, F1 a F12. Um nome que o jogo não conhece mantém o padrão (o registro diz qual). L2/R2 no controle ficam nos gatilhos. Insert e R3+L2 sempre abrem o menu do port. Aplicado quando o jogo inicia.',
        'Linux': 'Linux',
        'Window system': 'Sistema de janelas',
        'Launcher options, files and diagnostics.': 'Opções do launcher, arquivos e diagnósticos.',
        'Files': 'Arquivos',
        'GPU wait and readback report in the log (every 5 s)': 'Relatório de espera da GPU e leituras no registro (a cada 5 s)',
        'Shows where the frame waits for the GPU and which reads cause it. For performance tests.': 'Mostra onde o quadro espera pela GPU e quais leituras causam isso. Para testes de desempenho.',
        'F11 captures a frame for analysis': 'F11 captura um quadro para análise',
        'Records how the next frame is drawn into the captures folder.': 'Grava como o próximo quadro é desenhado na pasta de capturas.',
        'Home': 'Início',
        'Choose the game folder': 'Escolha a pasta do jogo',
        'Close the game before restoring saves.': 'Feche o jogo antes de restaurar os saves.',
        'No built-in patches for this game.': 'Não há patches embutidos para este jogo.',
        'GAME BUTTON': 'BOTÃO DO JOGO',
        'KEYBOARD': 'TECLADO',
        'GAMEPAD': 'CONTROLE',
        'GameMode': 'GameMode',
        'Asks GameMode (gamemoderun) for the performance governor while the game runs.': 'Pede ao GameMode (gamemoderun) o governador de desempenho enquanto o jogo roda.',
        'MangoHud overlay': 'Overlay do MangoHud',
        'Shows MangoHud (FPS, frame times, temperatures) through its Vulkan layer.': 'Mostra o MangoHud (FPS, tempos de quadro, temperaturas) pela camada Vulkan dele.',
        '{} · {} FPS engine · {} patches': '{} · motor a {} FPS · {} patches',
        'Type a name for the preset first.': 'Digite primeiro um nome para o preset.',
        'RUNNING': 'EXECUTANDO',
        '{} · version {}': '{} · versão {}',
        'Choose your game folder to play.': 'Escolha a pasta do jogo para jogar.',
        'Made before every start, newest first; the last {} are kept. Restoring backs up the current saves first.': 'Feito antes de cada início, do mais novo ao mais antigo; os últimos {} são mantidos. Restaurar faz backup dos saves atuais antes.',
        'shadPS4/GoldHEN XML patch files for this game version. Empty: {}': 'Arquivos de patch XML do shadPS4/GoldHEN para esta versão do jogo. Vazio: {}',
        '{} · version {} · {}. Resolution and frame rate patches follow Performance.': '{} · versão {} · {}. Os patches de resolução e de taxa de quadros seguem o Desempenho.',
        'Automatic lets SDL choose. Pick X11 if the window misbehaves on Wayland (it runs through XWayland).': 'O modo automático deixa o SDL escolher. Escolha X11 se a janela se comportar mal no Wayland (roda pelo XWayland).',
        '{} · 60 FPS engine · patches not available for this game build': '{} · motor a 60 FPS · patches indisponíveis para esta versão do jogo',
        'Shortcut created:': 'Atalho criado:',
        'Restore': 'Restaurar',
        'Replace the current saves with the backup {}? The current saves are backed up first.': 'Substituir os saves atuais pelo backup {}? Os saves atuais passam por backup antes.',
        'Saves restored from {}.': 'Saves restaurados de {}.',
        'Reset to defaults': 'Restaurar padrões',
        '⚠ {} file(s) replaced by more than one mod (the last one wins):': '⚠ {} arquivo(s) substituído(s) por mais de um mod (vale o último):',
        '   … and {} more': '   … e mais {}',
        'restart': 'reiniciar',
        'Could not restore: {}': 'Não foi possível restaurar: {}',
        ' Previous saves: {}.': ' Saves anteriores: {}.',
        'No Wayland session detected.': 'Nenhuma sessão Wayland detectada.',
        'Not installed.': 'Não instalado.',
        'Preset': 'Preset',
        'Load': 'Carregar',
        'Save': 'Salvar',
        'Could not save all settings. Check write permissions and free disk space, then try again.': 'Não foi possível salvar todas as configurações. Confira a permissão de escrita e o espaço livre em disco, depois tente novamente.',
        'Delete': 'Excluir',
        'No patch files for this game version yet.': 'Ainda não há arquivos de patch para esta versão do jogo.',
        '{} trigger': 'gatilho {}',
        'Diagnostics (for development)': 'Diagnósticos (para desenvolvimento)',
        'Windowed': 'Em janela',
        'Borderless': 'Sem bordas',
        'Unlimited': 'Ilimitado',
        'Automatic': 'Automático',
        'Native AA (×1.0)': 'AA nativo (×1.0)',
        'Quality (×1.5)': 'Qualidade (×1.5)',
        'Balanced (×1.7)': 'Equilibrado (×1.7)',
        'Performance (×2)': 'Desempenho (×2)',
        'Ultra Performance (×3)': 'Desempenho máximo (×3)',
        'Precise': 'Preciso',
        'System RAM (cached, default)': 'RAM do sistema (com cache, padrão)',
        'System RAM (uncached)': 'RAM do sistema (sem cache)',
        'GPU memory (previous behavior)': 'Memória da GPU (comportamento anterior)',
        '1 (lowest latency)': '1 (menor latência)',
        'Native (1080p)': 'Nativa (1080p)',
        'Graphics (experimental)': 'Gráficos (experimental)',
        'Experimental': 'Experimental',
        'Enable the upscaler (experimental)': 'Ativar o upscaler (experimental)',
        'Off (default): the game runs without temporal upscaling. These upscalers are not calibrated for this game yet: expect ghosting or flicker. FSR 3.1 needs nothing else; FSR 4 needs the assets below, and without them the game uses FSR 3.1. The options on this page only take effect with this switch on.': 'Desligado (padrão): o jogo roda sem upscaling temporal. Esses upscalers ainda não estão calibrados para este jogo: espere fantasmas (ghosting) ou cintilação. O FSR 3.1 não precisa de mais nada; o FSR 4 precisa dos assets abaixo e, sem eles, o jogo usa o FSR 3.1. As opções desta página só valem com este interruptor ligado.',
        'Add to Steam': 'Adicionar à Steam',
        'Steam is open. Close it completely (Steam > Exit) and click again: Steam rewrites its shortcuts when it closes.': 'A Steam está aberta. Feche-a por completo (Steam > Sair) e clique de novo: a Steam reescreve os atalhos quando fecha.',
        'Steam was not found for this user.': 'A Steam não foi encontrada para este usuário.',
        'The game folder has no sce_sys/pic1.png for the pictures.': 'A pasta do jogo não tem sce_sys/pic1.png para gerar as imagens.',
        'The shortcut "{}" was updated in Steam (accounts: {}), with its icon, capsules, background and logo. Open Steam to see it in the library.': 'O atalho "{}" foi atualizado na Steam (contas: {}), com ícone, capas, fundo e logotipo. Abra a Steam para vê-lo na biblioteca.',
        'The shortcut "{}" was added to Steam (accounts: {}), with its icon, capsules, background and logo. Open Steam to see it in the library.': 'O atalho "{}" foi adicionado à Steam (contas: {}), com ícone, capas, fundo e logotipo. Abra a Steam para vê-lo na biblioteca.',
        'Menu entry and icon (AppImage)': 'Entrada no menu e ícone (AppImage)',
        'Creates spartanport.desktop in ~/.local/share/applications so the launcher and the game show their icon in the dock and the menu. Turning it off removes the entry.': 'Cria o spartanport.desktop em ~/.local/share/applications para o launcher e o jogo mostrarem o ícone no dock e no menu. Desligar remove a entrada.',
        'The game dump is incomplete. Missing or empty files:\n\n{}\n\nDump the game again, or copy these files from a complete dump.': 'O dump do jogo está incompleto. Arquivos faltando ou vazios:\n\n{}\n\nFaça o dump do jogo de novo, ou copie esses arquivos de um dump completo.',
        'The files in {} look like a PKG extraction: the system files (param.sfo, icon0.png, pic1.png and the rest) are in\n\n{}\n\nCopy all of them into its sce_sys folder now? Files already there are kept.': 'Os arquivos em {} parecem vir de um PKG extraído: os arquivos de sistema (param.sfo, icon0.png, pic1.png e os demais) estão em\n\n{}\n\nCopiar todos para a pasta sce_sys dele agora? Os arquivos que já estão lá são mantidos.',
        'Could not copy the files: {}': 'Não foi possível copiar os arquivos: {}',
        '{} (empty)': '{} (vazio)',
        '{} (unreadable)': '{} (sem permissão de leitura)',
        '{} (not a param.sfo)': '{} (não é um param.sfo)',
        '{} (a folder, not a file)': '{} (é uma pasta, não um arquivo)',
        'Choose two separate folders for the game and its update, or leave the update folder empty.': 'Escolha duas pastas separadas para o jogo e o update, ou deixe a pasta do update em branco.',
        'Copying the system files…': 'Copiando os arquivos de sistema…',
        'The update folder {} is incomplete. Missing or empty files:\n\n{}\n\nChoose the folder of the update (eboot.bin, sce_sys), or leave it empty.': 'A pasta do update {} está incompleta. Arquivos faltando ou vazios:\n\n{}\n\nEscolha a pasta do update (eboot.bin, sce_sys), ou deixe em branco.',
        'In-game button icons': 'Ícones dos botões no jogo',
        'Original (default)': 'Original (padrão)',
        'Match my controller': 'Igual ao meu controle',
        'Keyboard': 'Teclado',
        "Replaces the PlayStation button icons inside the game with the ones of your controller. Works on a copy of up to three game files in the port's data folder (about 410 MB); your game folder is never changed. Applied when the game starts.": 'Troca os ícones de botões PlayStation dentro do jogo pelos do seu controle. Funciona com uma cópia de até três arquivos do jogo na pasta de dados do port (cerca de 410 MB); a pasta do jogo nunca é alterada. Aplicado quando o jogo inicia.',
        'Textures': 'Texturas',
        'Anisotropic filtering': 'Filtro anisotrópico',
        'Game default': 'Padrão do jogo',
        'Sharper floors and walls seen at an angle, at little cost on most graphics cards.': 'Chão e paredes vistos de lado ficam mais nítidos, com pouco custo na maioria das placas de vídeo.',
        'Screenshots': 'Capturas de tela',
        'Open the screenshots folder': 'Abrir a pasta das capturas',
        'F12 in the game': 'F12 no jogo',
        "Saves the game's image, without the overlay, as a PNG in the screenshots folder of the saves.": 'Salva a imagem do jogo, sem o overlay, em PNG na pasta screenshots dos saves.',
        'Vibration': 'Vibração',
        "Strength of the gamepad's vibration; 0% turns it off.": 'Força da vibração do controle; 0% desliga.',
        'Windows could not reserve the memory the game needs. Close other programs, or let the Windows page file grow (for example 16384 MB initial, 32768 MB maximum), and start again.':
            'O Windows não conseguiu reservar a memória que o jogo precisa. Feche outros programas, ou deixe o arquivo de paginação do Windows crescer (por exemplo 16384 MB inicial, 32768 MB máximo), e inicie de novo.',
        'Linux could not allocate the memory the game needs. Close other programs, check available RAM, swap space, or /dev/shm, and start again.':
            'O Linux não conseguiu alocar a memória que o jogo precisa. Feche outros programas, verifique a memória RAM disponível, swap ou /dev/shm, e inicie de novo.',
        'The game stopped while reading its files: the game dump may be incomplete. Extract the pkg again and check that the game folder is about 40 GB.':
            'O jogo parou ao ler os próprios arquivos: o dump do jogo pode estar incompleto. Extraia o pkg de novo e confira se a pasta do jogo tem cerca de 40 GB.',
        'Native · 1920 × 1080 · 16:9': 'Nativa · 1920 × 1080 · 16:9',
        'Graphics card': 'Placa de vídeo',
        'The GPU the game runs on. Choose your dedicated card if the game uses the integrated one.':
            'A GPU em que o jogo roda. Escolha a placa dedicada se o jogo estiver usando a integrada.',
        'Fast screen effects (Rage of Sparta, weapons)': 'Efeitos de tela rápidos (Rage of Sparta, armas)',
        'Keeps powers and weapon effects at full FPS. Switch it off if a screen effect (blur, distortion, the color change of a power) looks stretched, shifted or wrong.':
            'Mantém os efeitos de poderes e armas no FPS máximo. Desative se um efeito de tela (desfoque, distorção, a mudança de cor de um poder) parecer esticado, deslocado ou errado.',
        'Your own dump of the game (eboot.bin, sce_module, sce_sys): SpartanPort, any region and version.':
            'O seu próprio dump do jogo (eboot.bin, sce_module, sce_sys): SpartanPort, de qualquer região e versão.',
        'L1 + Touchpad right': 'L1 + Touchpad direito',
        "Keys use SDL names: letters, digits, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Up, Down, Left, Right, F1 to F12. A name the game does not know keeps the default (the log says which). L2/R2 on the gamepad stay on the triggers. Touchpad right opens the game's pause menu; Touchpad left (photo mode) is L1 + Touchpad right on the gamepad. Insert and R3+L2 always open the port's menu. Applied when the game starts.":
            'As teclas usam os nomes do SDL: letras, números, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Up, Down, Left, Right, F1 a F12. Um nome que o jogo não conhece mantém o padrão (o registro diz qual). L2/R2 no controle ficam nos gatilhos. Touchpad direito abre o menu de pausa do jogo; Touchpad esquerdo (modo foto) é L1 + Touchpad direito no controle. Insert e R3+L2 sempre abrem o menu do port. Vale quando o jogo inicia.',
        '{} · 60 FPS engine · no patches for this game': '{} · motor a 60 FPS · sem patches para este jogo',
        '{} not found in this eboot.bin at the last start (see Log)': '{} não encontrados neste eboot.bin na última inicialização (veja o Registro)',
        '   (game patches)': '   (patches do jogo)',
        # Button names (PS_BUTTON_NAMES, PAD_STYLES).
        'Cross': 'X', 'Circle': 'Círculo', 'Square': 'Quadrado', 'Triangle': 'Triângulo',
        'Touchpad left': 'Touchpad esquerdo', 'Touchpad right': 'Touchpad direito',
        'D-pad up': 'Direcional ↑', 'D-pad down': 'Direcional ↓', 'D-pad left': 'Direcional ←',
        'D-pad right': 'Direcional →',
        'Touchpad click': 'Clique do touchpad', 'PS button': 'Botão PS', 'Mute button': 'Botão de mudo',
        'LS (left stick click)': 'LS (clique do analógico esquerdo)',
        'RS (right stick click)': 'RS (clique do analógico direito)', 'Xbox button': 'Botão Xbox',
        # Notes of the game patches (patches/CUSA01623.xml) shown on the Game page.
        'Fixes corrupted textures in the game by increasing the Video Arena size, allowing the game to use more video memory for textures and effects. Requires setting DMEM to 2048 MB or more, or the game crashes. Enable either this patch or one resolution patch, never both.':
            'Corrige texturas corrompidas no jogo aumentando a Video Arena, para o jogo usar mais memória de vídeo em texturas e efeitos. Exige DMEM de 2048 MB ou mais, senão o jogo fecha. Ative este patch ou um patch de resolução, nunca os dois.',
        'Skips the initial logos. Thanks to illusion0001 aka illusionyy for the PS3 patch which made this very easy.':
            'Pula os logos iniciais. Agradecimentos a illusion0001, também conhecido como illusionyy, pelo patch do PS3 que tornou isso muito fácil.',
        'Allows manual skipping of cinematics any cinematic with the X button. Thanks to illusion0001 aka illusionyy for the PS3 patch which made this very easy.':
            'Permite pular qualquer cinemática com o botão X. Agradecimentos a illusion0001, também conhecido como illusionyy, pelo patch do PS3 que tornou isso muito fácil.',
        'Fixes the black screen at startup with the FPS counter running: the frame pacing index starts at 2 instead of 0, so the second frame no longer waits for a flip that only it can trigger.':
            'Corrige a tela preta na inicialização com o contador de FPS rodando: o índice de ritmo dos quadros começa em 2 em vez de 0, então o segundo quadro não espera mais por um flip que só ele pode disparar.',
        '(none)': '(nenhuma)',
        'Adaptive mouse: how far the mouse moves for one step of a rotation. Also in the in-game menu.':
            'Mouse adaptativo: quanto o mouse se move para cada passo de um giro. Também no menu do jogo.',
        'Aiming with the mouse while L2 is held (Head of Helios, Bow of Apollo). Relative: the stick follows the movement and goes back to neutral when the mouse stops. Absolute: where the mouse was when L2 was pressed is the centre, and the stick points from it to the mouse. Also in the in-game menu.':
            'Mira com o mouse enquanto L2 está pressionado (Cabeça de Hélio, Arco de Apolo). Relativo: o analógico segue o movimento e volta ao centro quando o mouse para. Absoluto: onde o mouse estava ao apertar L2 é o centro, e o analógico aponta dele até o mouse. Também no menu do jogo.',
        'Aiming with the mouse while L2 is held: lower needs more mouse movement for the same stick, higher less. Also in the in-game menu.':
            'Mira com o mouse enquanto L2 está pressionado: menor precisa de mais movimento do mouse para o mesmo analógico, maior de menos. Também no menu do jogo.',
        "Click a box, then press a key, click a mouse button or turn the wheel (it gives up after {} seconds). An input already used by another button swaps with it; the arrow puts one button back on its default. Rage of Sparta presses L3 and R3 together: tick the box above it to use one input instead of two. Grab presses L1 and Circle together. Dodge evades the way you are moving (WASD), or back when standing still. Touchpad right opens the game's pause menu. Insert always opens the port's menu and cannot be bound. Applied when the game starts.":
            'Clique em uma caixa e aperte uma tecla, clique com um botão do mouse ou gire a roda (desiste após {} segundos). Uma entrada já usada por outro botão troca com ele; a seta devolve um botão ao padrão. Rage of Sparta aperta L3 e R3 juntos: marque a caixa acima dele para usar uma entrada em vez de duas. Agarrar aperta L1 e Círculo juntos. A esquiva vai na direção em que você anda (WASD), ou para trás quando parado. O touchpad direito abre o menu de pausa do jogo. Insert sempre abre o menu do port e não pode ser atribuído. Aplicado ao iniciar o jogo.',
        'Gamepad Mapping': 'Mapeamento do controle',
        'General': 'Geral',
        'How the stick-rotation prompts are answered. Keyboard: type the turn instead, half turns from the top D S A (clockwise) or A S D, from the bottom A W D or D W A, full turns D S A W or A S D W; a key out of turn starts again. Adaptive mouse: draw the turn with the mouse.':
            'Como os eventos de girar o analógico são respondidos. Teclado: digite o giro, meias-voltas a partir de cima D S A (horário) ou A S D, a partir de baixo A W D ou D W A, voltas completas D S A W ou A S D W; uma tecla fora de ordem recomeça. Mouse adaptativo: desenhe o giro com o mouse.',
        'Joystick prompts': 'Prompts do analógico',
        'Keep the mouse in the game window': 'Manter o mouse na janela do jogo',
        'Key not supported': 'Tecla não suportada',
        'Keyboard & Mouse Mapping': 'Mapeamento de teclado e mouse',
        'Keyboard and mouse, button icons and gamepad mapping.':
            'Teclado e mouse, ícones dos botões e mapeamento do controle.',
        "L2/R2 stay on the triggers. Touchpad right opens the game's pause menu; Touchpad left (photo mode) is L1 + Touchpad right. R3+L2 always opens the port's menu. Applied when the game starts.":
            'L2/R2 ficam nos gatilhos. O touchpad direito abre o menu de pausa do jogo; o touchpad esquerdo (modo foto) é L1 + touchpad direito. R3+L2 sempre abre o menu do port. Aplicado ao iniciar o jogo.',
        'Mouse prompt distance': 'Distância do mouse nos prompts',
        'Press a key': 'Aperte uma tecla',
        'Press a key or mouse': 'Aperte uma tecla ou o mouse',
        'Rage of Sparta on one input (L3 + R3 together)':
            'Rage of Sparta em uma entrada (L3 + R3 juntos)',
        'Reset gamepad to defaults': 'Restaurar o controle ao padrão',
        'Reset keyboard & mouse to defaults': 'Restaurar teclado e mouse ao padrão',
        'Right stick sensitivity': 'Sensibilidade do analógico direito',
        'Right stick style': 'Estilo do analógico direito',
        'Which gamepad button presses each button of the game.':
            'Qual botão do controle aperta cada botão do jogo.',
        'Which key or mouse input presses each button of the game.':
            'Qual tecla ou entrada do mouse aperta cada botão do jogo.',
        'While you play, the mouse stays in the game window, so a click cannot land on another monitor or app. The in-game menu and switching windows get it back. Also in the in-game menu.':
            'Enquanto você joga, o mouse fica na janela do jogo, então um clique não cai em outro monitor ou programa. O menu do jogo e trocar de janela o liberam. Também no menu do jogo.',
        'Keyboard (native)': 'Teclado (nativo)',
        'Adaptive mouse (experimental)': 'Mouse adaptativo (experimental)',
        'Relative': 'Relativo',
        'Absolute': 'Absoluto',
        'Grab': 'Agarrar',
        'Dodge': 'Esquiva',
        'Portuguese (Brazil)': 'Português (Brasil)',
        'Reuse compiled shaders from disk': 'Reutilizar shaders compilados do disco',
        'Shaders compiled in earlier sessions load from the shader cache instead of compiling again, so objects no longer appear a few frames late after starting or loading a save, and the cache is not rewritten every session (antivirus scans each written file). The first launch with it on rebuilds the cache once. Switch it off if an object, effect or color looks wrong only after a restart and is fine on the first run of a new cache.': 'Os shaders compilados em sessões anteriores carregam do cache de shaders em vez de compilar de novo: os objetos não aparecem mais alguns quadros atrasados depois de iniciar ou carregar um save, e o cache não é reescrito a cada sessão (o antivírus verifica cada arquivo gravado). A primeira inicialização com a opção ligada recria o cache uma vez. Desative se um objeto, efeito ou cor aparecer errado só depois de reiniciar e estiver certo na primeira execução de um cache novo.',
        'Read the shader cache into memory at start': 'Ler o cache de shaders para a memória ao iniciar',
        'A background thread reads the saved shaders into memory while the game starts; a note in the lower right corner shows the progress. Loading a shader then does not hold up a frame. Needs the option above. Switch it off on a slow hard drive or with little free RAM if the first minute stutters more or the game starts slower.': 'Uma thread em segundo plano lê os shaders salvos para a memória enquanto o jogo inicia; uma nota no canto inferior direito mostra o progresso. Assim, carregar um shader não segura um quadro. Precisa da opção acima. Desative em um HD lento ou com pouca RAM livre se o primeiro minuto travar mais ou o jogo demorar mais para abrir.',
        'Show shader notes in the corner': 'Mostrar notas de shaders no canto',
        'A note in the lower right corner shows the shader cache being read at start and the shaders being compiled, so a short stutter has a visible reason. Switch it off to keep the screen clean.': 'Uma nota no canto inferior direito mostra a leitura do cache de shaders ao iniciar e os shaders sendo compilados, para que uma travada curta tenha um motivo visível. Desative para manter a tela limpa.',
        'Shader cache': 'Cache de shaders',
        'Delete shader cache': 'Apagar cache de shaders',
        'Deletes the saved shaders and pipelines of this saves folder. Use it if graphics look wrong after an update or the cache grew too big; the next start compiles everything again.': 'Apaga os shaders e pipelines salvos desta pasta de saves. Use se os gráficos ficarem errados depois de uma atualização ou se o cache ficar grande demais; a próxima inicialização compila tudo de novo.',
        'Close the game before deleting the shader cache.': 'Feche o jogo antes de apagar o cache de shaders.',
        'The shader cache is already empty.': 'O cache de shaders já está vazio.',
        'Delete the shader cache ({} MB in {})? The game compiles the shaders again, so the next start stutters for a few minutes.': 'Apagar o cache de shaders ({} MB em {})? O jogo compila os shaders de novo, então a próxima inicialização trava por alguns minutos.',
        'Could not delete all of the shader cache; another program may be using it: {}': 'Não foi possível apagar todo o cache de shaders; outro programa pode estar usando: {}',
    },
    'es': {
        'The launcher cannot write to {}, so it cannot replace itself there. Open the releases page to download the new version by hand?': 'El launcher no puede escribir en {}, así que no puede reemplazarse ahí. ¿Abrir la página de releases para descargar la versión nueva a mano?',
        'Version {} is out, but it has no Linux build yet.': 'La versión {} ya salió, pero aún no tiene compilación para Linux.',
        'Show Discord Rich Presence': 'Mostrar Discord Rich Presence',
        'Shows only the game name and the time played on your Discord profile.': 'Muestra en tu perfil de Discord solo el nombre del juego y el tiempo jugado.',
        'Game multisampling': 'Multisampling del juego',
        'Automatic ({}: off)': 'Automático ({}: desactivado)',
        'Automatic ({}: on)': 'Automático ({}: activado)',
        'Off (fixes black lines)': 'Desactivado (corrige las líneas negras)',
        "The game's 2-sample depth buffer, read as a texture by its lighting. AMD's and Intel's Windows drivers read it wrong and the lighting shows black lines, so Automatic turns multisampling off on those cards (Radeon, Arc) and keeps it on elsewhere (NVIDIA). Off and On force the choice on any card.":
            'El búfer de profundidad de 2 muestras del juego, leído como textura por su iluminación. Los controladores de AMD e Intel en Windows lo leen mal y la iluminación muestra líneas negras, así que Automático desactiva el multisampling en esas tarjetas (Radeon, Arc) y lo mantiene en las demás (NVIDIA). Desactivado y Activado fuerzan la elección en cualquier tarjeta.',
        'Display': 'Pantalla',
        'How the game appears on your screen.': 'Cómo se ve el juego en tu pantalla.',
        'Display mode': 'Modo de pantalla',
        'Borderless covers the screen without changing the display mode.':
            'Sin bordes cubre toda la pantalla sin cambiar el modo de vídeo.',
        'VSync': 'VSync',
        'Off allows tearing for the lowest latency.':
            'Desactivado permite cortes (tearing) para obtener la menor latencia.',
        'Enlarges a smaller game image to the window. With Render resolution 720p (Performance) it raises the FPS; at the size of the window it does nothing.':
            'Amplía una imagen del juego más pequeña hasta la ventana. Con Resolución de render en 720p (Rendimiento) sube los FPS; al tamaño de la ventana no hace nada.',
        'Overlay': 'Overlay',
        'Show performance overlay': 'Mostrar el overlay de rendimiento',
        'Items, size and corner: in-game menu (Insert / R3+L2) > Overlay.':
            'Elementos, tamaño y esquina: menú del juego (Insert / R3+L2) > Overlay.',
        'Frame rate and speed options.': 'Opciones de velocidad de fotogramas y de rendimiento.',
        'Frame rate limit': 'Límite de velocidad de fotogramas',
        'The engine frame rate (below) is the highest it can go.':
            'La velocidad de fotogramas del motor (abajo) es el máximo que se puede alcanzar.',
        'Frames queued': 'Fotogramas en cola',
        '1 is the lowest input latency; more can raise FPS when the graphics card is the limit.':
            '1 da la menor latencia de entrada; más puede subir los FPS cuando la tarjeta gráfica es el límite.',
        'Engine frame rate': 'Velocidad de fotogramas del motor',
        'The highest frame rate the game itself runs at (a game patch).':
            'La mayor velocidad de fotogramas a la que corre el propio juego (un parche del juego).',
        'Render resolution': 'Resolución de render',
        'The size the game draws at; higher is sharper and slower. A resolution patch replaces the texture fix and reserves the memory it needs.':
            'El tamaño al que dibuja el juego; más alto es más nítido y más lento. Un parche de resolución reemplaza la corrección de texturas y reserva la memoria que necesita.',
        'Shaders': 'Shaders',
        'Use pipeline binary cache (A/B test)': 'Usar caché binaria de pipelines (prueba A/B)',
        'Reuse saved Vulkan pipelines. Disable to compare FPS with normal compilation. Takes effect on the next launch; saved caches are preserved.':
            'Reutiliza pipelines de Vulkan guardados. Desactívalo para comparar los FPS con la compilación normal. Se aplica en el próximo inicio; las cachés guardadas se conservan.',
        'Load shader cache in the background': 'Cargar la caché de shaders en segundo plano',
        'The game opens at once and the saved shaders load while you play; a small note in the corner shows the progress. Switch it off to wait on the loading screen instead.':
            'El juego abre al instante y los shaders guardados se cargan mientras juegas; una pequeña nota en la esquina muestra el progreso. Desactívalo para esperar en la pantalla de carga.',
        'Compile new shaders in the background': 'Compilar shaders nuevos en segundo plano',
        'Fewer stutters in new areas: the game keeps running while new shaders compile. An object or effect may appear a moment later. Switch it off if something stays missing.':
            'Menos tirones en zonas nuevas: el juego sigue corriendo mientras se compilan shaders nuevos. Un objeto o efecto puede aparecer un instante después. Desactívalo si algo sigue sin aparecer.',
        'Fast shader cache loading (parallel)': 'Carga rápida de la caché de shaders (paralela)',
        'Loading screen only: builds the cached pipelines on several CPU threads.':
            'Solo en la pantalla de carga: construye los pipelines en caché en varios hilos de la CPU.',
        'GPU data': 'Datos de la GPU',
        'Stream buffer memory': 'Memoria del búfer de streaming',
        'System RAM can improve FPS on NVIDIA laptops. Choose GPU memory to restore the previous behavior. Applies on the next game launch.':
            'La RAM del sistema puede mejorar los FPS en portátiles con NVIDIA. Elige la memoria de la GPU para restaurar el comportamiento anterior. Se aplica en el próximo inicio del juego.',
        'Game reads GPU data without waiting (big FPS gain)':
            'El juego lee datos de la GPU sin esperar (gran mejora de FPS)',
        'About 40 to 66 FPS in a heavy scene. Needs deferred GPU readbacks. Switch it off if lighting or objects look wrong.':
            'Unos 40 a 66 FPS en una escena pesada. Requiere lecturas de la GPU diferidas. Desactívalo si la iluminación o los objetos se ven mal.',
        'Deferred GPU readbacks': 'Lecturas de la GPU diferidas',
        'Fewer waits for the GPU. Switch it off if textures or shadows look wrong.':
            'Menos esperas por la GPU. Desactívalo si las texturas o las sombras se ven mal.',
        'Your game dump, saves and language.': 'Tu volcado del juego, partidas e idioma.',
        'Cheats': 'Trucos',
        'Infinite Double Jumps': 'Dobles saltos infinitos',
        'Repeat air jumps without landing. Also available in the in-game Cheats menu.':
            'Repite los saltos en el aire sin tocar el suelo. También está en el menú de trucos del juego.',
        'Unlock Very Hard': 'Desbloquear Muy difícil',
        'Select Very Hard without completing the game. Does not change existing save difficulty. In-game: enable before opening New Game.':
            'Permite elegir Muy difícil sin completar el juego. No cambia la dificultad de las partidas existentes. En el juego: actívalo antes de abrir Nuevo juego.',
        'Folders': 'Carpetas',
        'Your own dump of the game (eboot.bin, sce_module, sce_sys); SpartanPort: CUSA01623, version 01.02 for the community patches.':
            'Tu propio volcado del juego (eboot.bin, sce_module, sce_sys); SpartanPort: CUSA01623, versión 01.02 para los parches de la comunidad.',
        'Update folder (optional)': 'Carpeta de la actualización (opcional)',
        'Choose the folder of the update (eboot.bin, sce_sys)':
            'Elige la carpeta de la actualización (eboot.bin, sce_sys)',
        'Only if the update is dumped apart from the game: it is laid over the game folder without changing either. Leave empty when the game folder already has the update.':
            'Solo si la actualización se volcó aparte del juego: se superpone a la carpeta del juego sin modificar ninguna de las dos. Déjala vacía cuando la carpeta del juego ya incluya la actualización.',
        'Saves': 'Partidas',
        'Language': 'Idioma',
        'Save backups': 'Copias de seguridad de partidas',
        'Changes applied to the game code at start.':
            'Cambios que se aplican al código del juego al iniciar.',
        'Files layered over the game at start; the game files are never changed.':
            'Archivos que se superponen al juego al iniciar; los archivos del juego nunca se modifican.',
        'Loose-file mods, each in its own folder with dvdroot_ps4.':
            'Mods de archivos sueltos, cada uno en su propia carpeta con dvdroot_ps4.',
        'Controls': 'Controles',
        'Which key or gamepad button presses each button of the game.':
            'Qué tecla o botón del mando activa cada botón del juego.',
        'Gamepad names': 'Nombres de los botones del mando',
        "Keys use SDL names: letters, digits, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Up, Down, Left, Right, F1 to F12. A name the game does not know keeps the default (the log says which). L2/R2 on the gamepad stay on the triggers. Insert and R3+L2 always open the port's menu. Applied when the game starts.":
            'Las teclas usan nombres de SDL: letras, dígitos, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Up, Down, Left, Right, F1 a F12. Un nombre que el juego no reconoce mantiene el predeterminado (el registro indica cuál). L2/R2 del mando se quedan en los gatillos. Insert y R3+L2 siempre abren el menú del port. Se aplica al iniciar el juego.',
        'Linux': 'Linux',
        'Window system': 'Sistema de ventanas',
        'Launcher options, files and diagnostics.': 'Opciones del lanzador, archivos y diagnóstico.',
        'Files': 'Archivos',
        'GPU wait and readback report in the log (every 5 s)':
            'Informe de esperas y lecturas de la GPU en el registro (cada 5 s)',
        'Shows where the frame waits for the GPU and which reads cause it. For performance tests.':
            'Muestra dónde espera el fotograma a la GPU y qué lecturas lo causan. Para pruebas de rendimiento.',
        'F11 captures a frame for analysis': 'F11 captura un fotograma para analizarlo',
        'Records how the next frame is drawn into the captures folder.':
            'Graba cómo se dibuja el siguiente fotograma en la carpeta de capturas.',
        'Home': 'Inicio',
        'Choose the game folder': 'Elige la carpeta del juego',
        'Close the game before restoring saves.': 'Cierra el juego antes de restaurar las partidas.',
        'No built-in patches for this game.': 'No hay parches integrados para este juego.',
        'GAME BUTTON': 'BOTÓN DEL JUEGO',
        'KEYBOARD': 'TECLADO',
        'GAMEPAD': 'MANDO',
        'GameMode': 'GameMode',
        'Asks GameMode (gamemoderun) for the performance governor while the game runs.':
            'Pide a GameMode (gamemoderun) el governor de rendimiento de la CPU mientras el juego se ejecuta.',
        'MangoHud overlay': 'Overlay de MangoHud',
        'Shows MangoHud (FPS, frame times, temperatures) through its Vulkan layer.':
            'Muestra MangoHud (FPS, tiempos de fotograma, temperaturas) mediante su capa de Vulkan.',
        '{} · {} FPS engine · {} patches': '{} · motor a {} FPS · {} parches',
        'Type a name for the preset first.': 'Escribe primero un nombre para el preajuste.',
        'RUNNING': 'EN EJECUCIÓN',
        '{} · version {}': '{} · versión {}',
        'Choose your game folder to play.': 'Elige la carpeta del juego para jugar.',
        'Made before every start, newest first; the last {} are kept. Restoring backs up the current saves first.':
            'Se hace antes de cada inicio, de la más reciente a la más antigua; se conservan las últimas {}. Antes de restaurar se hace una copia de las partidas actuales.',
        'shadPS4/GoldHEN XML patch files for this game version. Empty: {}':
            'Archivos de parche XML de shadPS4/GoldHEN para esta versión del juego. Vacío: {}',
        '{} · version {} · {}. Resolution and frame rate patches follow Performance.':
            '{} · versión {} · {}. Los parches de resolución y de velocidad de fotogramas siguen a Rendimiento.',
        'Automatic lets SDL choose. Pick X11 if the window misbehaves on Wayland (it runs through XWayland).':
            'Automático deja que SDL elija. Elige X11 si la ventana falla en Wayland (se ejecuta mediante XWayland).',
        '{} · 60 FPS engine · patches not available for this game build':
            '{} · motor a 60 FPS · parches no disponibles para esta versión del juego',
        'Shortcut created:': 'Acceso directo creado:',
        'Restore': 'Restaurar',
        'Replace the current saves with the backup {}? The current saves are backed up first.':
            '¿Reemplazar las partidas actuales por la copia {}? Antes se hace una copia de las partidas actuales.',
        'Saves restored from {}.': 'Partidas restauradas desde {}.',
        'Reset to defaults': 'Restablecer valores predeterminados',
        '⚠ {} file(s) replaced by more than one mod (the last one wins):':
            '⚠ {} archivo(s) reemplazado(s) por más de un mod (gana el último):',
        '   … and {} more': '   … y {} más',
        'restart': 'reiniciar',
        'Could not restore: {}': 'No se pudo restaurar: {}',
        ' Previous saves: {}.': ' Partidas anteriores: {}.',
        'No Wayland session detected.': 'No se detectó ninguna sesión de Wayland.',
        'Not installed.': 'No instalado.',
        'Preset': 'Preajuste',
        'Load': 'Cargar',
        'Save': 'Guardar',
        'Could not save all settings. Check write permissions and free disk space, then try again.':
            'No se pudieron guardar todos los ajustes. Comprueba los permisos de escritura y el espacio libre en disco e inténtalo de nuevo.',
        'Delete': 'Borrar',
        'No patch files for this game version yet.':
            'Aún no hay archivos de parche para esta versión del juego.',
        '{} trigger': 'gatillo {}',
        'Diagnostics (for development)': 'Diagnóstico (para desarrollo)',
        'Windowed': 'En ventana',
        'Borderless': 'Sin bordes',
        'Unlimited': 'Ilimitado',
        'Automatic': 'Automático',
        'Native AA (×1.0)': 'AA nativo (×1.0)',
        'Quality (×1.5)': 'Calidad (×1.5)',
        'Balanced (×1.7)': 'Equilibrado (×1.7)',
        'Performance (×2)': 'Rendimiento (×2)',
        'Ultra Performance (×3)': 'Rendimiento máximo (×3)',
        'Precise': 'Preciso',
        'System RAM (cached, default)': 'RAM del sistema (caché, predet.)',
        'System RAM (uncached)': 'RAM del sistema (sin caché)',
        'GPU memory (previous behavior)': 'Memoria de la GPU (anterior)',
        '1 (lowest latency)': '1 (menor latencia)',
        'Native (1080p)': 'Nativa (1080p)',
        'Graphics (experimental)': 'Gráficos (experimental)',
        'Experimental': 'Experimental',
        'Enable the upscaler (experimental)': 'Activar el escalador (experimental)',
        'Off (default): the game runs without temporal upscaling. These upscalers are not calibrated for this game yet: expect ghosting or flicker. FSR 3.1 needs nothing else; FSR 4 needs the assets below, and without them the game uses FSR 3.1. The options on this page only take effect with this switch on.':
            'Desactivado (predeterminado): el juego corre sin escalado temporal. Estos escaladores aún no están calibrados para este juego: es posible que haya estelas (ghosting) o parpadeos. FSR 3.1 no necesita nada más; FSR 4 necesita los archivos de abajo y, sin ellos, el juego usa FSR 3.1. Las opciones de esta página solo surten efecto con este interruptor activado.',
        'Add to Steam': 'Añadir a Steam',
        'Steam is open. Close it completely (Steam > Exit) and click again: Steam rewrites its shortcuts when it closes.':
            'Steam está abierto. Ciérralo por completo (Steam > Salir) y vuelve a hacer clic: Steam reescribe sus accesos directos al cerrarse.',
        'Steam was not found for this user.': 'No se encontró Steam para este usuario.',
        'The game folder has no sce_sys/pic1.png for the pictures.':
            'La carpeta del juego no tiene sce_sys/pic1.png para generar las imágenes.',
        'The shortcut "{}" was updated in Steam (accounts: {}), with its icon, capsules, background and logo. Open Steam to see it in the library.':
            'El acceso directo "{}" se actualizó en Steam (cuentas: {}), con su icono, cápsulas, fondo y logotipo. Abre Steam para verlo en la biblioteca.',
        'The shortcut "{}" was added to Steam (accounts: {}), with its icon, capsules, background and logo. Open Steam to see it in the library.':
            'El acceso directo "{}" se añadió a Steam (cuentas: {}), con su icono, cápsulas, fondo y logotipo. Abre Steam para verlo en la biblioteca.',
        'Menu entry and icon (AppImage)': 'Entrada de menú e icono (AppImage)',
        'Creates spartanport.desktop in ~/.local/share/applications so the launcher and the game show their icon in the dock and the menu. Turning it off removes the entry.':
            'Crea spartanport.desktop en ~/.local/share/applications para que el lanzador y el juego muestren su icono en el dock y en el menú. Al desactivarlo se elimina la entrada.',
        'The game dump is incomplete. Missing or empty files:\n\n{}\n\nDump the game again, or copy these files from a complete dump.':
            'El volcado del juego está incompleto. Archivos que faltan o están vacíos:\n\n{}\n\nVuelve a volcar el juego o copia esos archivos de un volcado completo.',
        'The files in {} look like a PKG extraction: the system files (param.sfo, icon0.png, pic1.png and the rest) are in\n\n{}\n\nCopy all of them into its sce_sys folder now? Files already there are kept.':
            'Los archivos de {} parecen la extracción de un PKG: los archivos del sistema (param.sfo, icon0.png, pic1.png y el resto) están en\n\n{}\n\n¿Copiarlos todos ahora a su carpeta sce_sys? Los archivos que ya están allí se conservan.',
        'Could not copy the files: {}': 'No se pudieron copiar los archivos: {}',
        '{} (empty)': '{} (vacío)',
        '{} (unreadable)': '{} (no se puede leer)',
        '{} (not a param.sfo)': '{} (no es un param.sfo)',
        '{} (a folder, not a file)': '{} (es una carpeta, no un archivo)',
        'Choose two separate folders for the game and its update, or leave the update folder empty.':
            'Elige dos carpetas distintas para el juego y su actualización, o deja vacía la carpeta de la actualización.',
        'Copying the system files…': 'Copiando los archivos del sistema…',
        'The update folder {} is incomplete. Missing or empty files:\n\n{}\n\nChoose the folder of the update (eboot.bin, sce_sys), or leave it empty.':
            'La carpeta de la actualización {} está incompleta. Archivos que faltan o están vacíos:\n\n{}\n\nElige la carpeta de la actualización (eboot.bin, sce_sys) o déjala vacía.',
        'In-game button icons': 'Iconos de botones en el juego',
        'Original (default)': 'Original (predeterminado)',
        'Match my controller': 'Igual que mi mando',
        'Keyboard': 'Teclado',
        "Replaces the PlayStation button icons inside the game with the ones of your controller. Works on a copy of up to three game files in the port's data folder (about 410 MB); your game folder is never changed. Applied when the game starts.":
            'Sustituye los iconos de botones de PlayStation dentro del juego por los de tu mando. Trabaja sobre una copia de hasta tres archivos del juego en la carpeta de datos del port (unos 410 MB); tu carpeta del juego nunca se modifica. Se aplica al iniciar el juego.',
        'Windows could not reserve the memory the game needs. Close other programs, or let the Windows page file grow (for example 16384 MB initial, 32768 MB maximum), and start again.':
            'Windows no pudo reservar la memoria que necesita el juego. Cierra otros programas, o deja que el archivo de paginación de Windows crezca (por ejemplo 16384 MB inicial, 32768 MB máximo), y vuelve a iniciarlo.',
        'Linux could not allocate the memory the game needs. Close other programs, check available RAM, swap space, or /dev/shm, and start again.':
            'Linux no pudo asignar la memoria que necesita el juego. Cierra otros programas, comprueba la memoria RAM disponible, swap o /dev/shm, y vuelve a iniciarlo.',
        'The game stopped while reading its files: the game dump may be incomplete. Extract the pkg again and check that the game folder is about 40 GB.':
            'El juego se detuvo al leer sus archivos: el dump del juego puede estar incompleto. Extrae el pkg de nuevo y comprueba que la carpeta del juego ocupa unos 40 GB.',
        'Textures': 'Texturas',
        'Anisotropic filtering': 'Filtrado anisotrópico',
        'Game default': 'Predeterminado del juego',
        'Sharper floors and walls seen at an angle, at little cost on most graphics cards.':
            'Suelos y paredes vistos en ángulo se ven más nítidos, con poco coste en la mayoría de las tarjetas gráficas.',
        'Screenshots': 'Capturas de pantalla',
        'Open the screenshots folder': 'Abrir la carpeta de capturas',
        'F12 in the game': 'F12 en el juego',
        "Saves the game's image, without the overlay, as a PNG in the screenshots folder of the saves.":
            'Guarda la imagen del juego, sin el overlay, como PNG en la carpeta screenshots de las partidas.',
        'Vibration': 'Vibración',
        "Strength of the gamepad's vibration; 0% turns it off.": 'Fuerza de la vibración del mando; 0% la desactiva.',
        '(none)': '(ninguna)',
        'Adaptive mouse: how far the mouse moves for one step of a rotation. Also in the in-game menu.':
            'Ratón adaptativo: cuánto se mueve el ratón para cada paso de un giro. También en el menú del juego.',
        'Aiming with the mouse while L2 is held (Head of Helios, Bow of Apollo). Relative: the stick follows the movement and goes back to neutral when the mouse stops. Absolute: where the mouse was when L2 was pressed is the centre, and the stick points from it to the mouse. Also in the in-game menu.':
            'Apuntar con el ratón mientras mantienes L2 (Cabeza de Helios, Arco de Apolo). Relativo: el stick sigue el movimiento y vuelve al centro cuando el ratón se detiene. Absoluto: donde estaba el ratón al pulsar L2 es el centro, y el stick apunta desde él hasta el ratón. También en el menú del juego.',
        'Aiming with the mouse while L2 is held: lower needs more mouse movement for the same stick, higher less. Also in the in-game menu.':
            'Apuntar con el ratón mientras mantienes L2: más bajo necesita más movimiento del ratón para el mismo stick, más alto menos. También en el menú del juego.',
        "Click a box, then press a key, click a mouse button or turn the wheel (it gives up after {} seconds). An input already used by another button swaps with it; the arrow puts one button back on its default. Rage of Sparta presses L3 and R3 together: tick the box above it to use one input instead of two. Grab presses L1 and Circle together. Dodge evades the way you are moving (WASD), or back when standing still. Touchpad right opens the game's pause menu. Insert always opens the port's menu and cannot be bound. Applied when the game starts.":
            'Haz clic en una casilla y luego pulsa una tecla, haz clic con un botón del ratón o gira la rueda (se cancela tras {} segundos). Una entrada ya usada por otro botón se intercambia con él; la flecha devuelve un botón a su valor predeterminado. Rage of Sparta pulsa L3 y R3 a la vez: marca la casilla de encima para usar una entrada en lugar de dos. Agarrar pulsa L1 y Círculo a la vez. Esquivar evade hacia donde te mueves (WASD), o hacia atrás si estás quieto. El touchpad derecho abre el menú de pausa del juego. Insert siempre abre el menú del port y no se puede asignar. Se aplica al iniciar el juego.',
        'Gamepad Mapping': 'Asignación del mando',
        'General': 'General',
        'How the stick-rotation prompts are answered. Keyboard: type the turn instead, half turns from the top D S A (clockwise) or A S D, from the bottom A W D or D W A, full turns D S A W or A S D W; a key out of turn starts again. Adaptive mouse: draw the turn with the mouse.':
            'Cómo se responden los eventos de girar el stick. Teclado: escribe el giro, medias vueltas desde arriba D S A (horario) o A S D, desde abajo A W D o D W A, vueltas completas D S A W o A S D W; una tecla fuera de orden vuelve a empezar. Ratón adaptativo: dibuja el giro con el ratón.',
        'Joystick prompts': 'Eventos del stick',
        'Keep the mouse in the game window': 'Mantener el ratón en la ventana del juego',
        'Key not supported': 'Tecla no compatible',
        'Keyboard & Mouse Mapping': 'Asignación de teclado y ratón',
        'Keyboard and mouse, button icons and gamepad mapping.':
            'Teclado y ratón, iconos de botones y asignación del mando.',
        "L2/R2 stay on the triggers. Touchpad right opens the game's pause menu; Touchpad left (photo mode) is L1 + Touchpad right. R3+L2 always opens the port's menu. Applied when the game starts.":
            'L2/R2 siguen en los gatillos. El touchpad derecho abre el menú de pausa del juego; el touchpad izquierdo (modo foto) es L1 + touchpad derecho. R3+L2 siempre abre el menú del port. Se aplica al iniciar el juego.',
        'Mouse prompt distance': 'Distancia del ratón en los eventos',
        'Press a key': 'Pulsa una tecla',
        'Press a key or mouse': 'Pulsa una tecla o el ratón',
        'Rage of Sparta on one input (L3 + R3 together)':
            'Rage of Sparta en una entrada (L3 + R3 a la vez)',
        'Reset gamepad to defaults': 'Restablecer el mando',
        'Reset keyboard & mouse to defaults': 'Restablecer teclado y ratón',
        'Right stick sensitivity': 'Sensibilidad del stick derecho',
        'Right stick style': 'Estilo del stick derecho',
        'Which gamepad button presses each button of the game.':
            'Qué botón del mando pulsa cada botón del juego.',
        'Which key or mouse input presses each button of the game.':
            'Qué tecla o entrada del ratón pulsa cada botón del juego.',
        'While you play, the mouse stays in the game window, so a click cannot land on another monitor or app. The in-game menu and switching windows get it back. Also in the in-game menu.':
            'Mientras juegas, el ratón se queda en la ventana del juego, así un clic no cae en otro monitor o programa. El menú del juego y cambiar de ventana lo liberan. También en el menú del juego.',
        'Keyboard (native)': 'Teclado (nativo)',
        'Adaptive mouse (experimental)': 'Ratón adaptativo (experimental)',
        'Relative': 'Relativo',
        'Absolute': 'Absoluto',
        'Grab': 'Agarrar',
        'Dodge': 'Esquivar',
        'Portuguese (Brazil)': 'Portugués (Brasil)',
        'Reuse compiled shaders from disk': 'Reutilizar shaders compilados del disco',
        'Shaders compiled in earlier sessions load from the shader cache instead of compiling again, so objects no longer appear a few frames late after starting or loading a save, and the cache is not rewritten every session (antivirus scans each written file). The first launch with it on rebuilds the cache once. Switch it off if an object, effect or color looks wrong only after a restart and is fine on the first run of a new cache.': 'Los shaders compilados en sesiones anteriores se cargan de la caché de shaders en vez de compilarse de nuevo: los objetos ya no aparecen unos fotogramas tarde al iniciar o cargar una partida, y la caché no se reescribe en cada sesión (el antivirus revisa cada archivo escrito). El primer inicio con la opción activada reconstruye la caché una vez. Desactívalo si un objeto, efecto o color se ve mal solo después de reiniciar y está bien en la primera ejecución de una caché nueva.',
        'Read the shader cache into memory at start': 'Leer la caché de shaders en memoria al iniciar',
        'A background thread reads the saved shaders into memory while the game starts; a note in the lower right corner shows the progress. Loading a shader then does not hold up a frame. Needs the option above. Switch it off on a slow hard drive or with little free RAM if the first minute stutters more or the game starts slower.': 'Un hilo en segundo plano lee los shaders guardados en memoria mientras el juego inicia; una nota en la esquina inferior derecha muestra el progreso. Así, cargar un shader no retiene un fotograma. Necesita la opción de arriba. Desactívalo en un disco duro lento o con poca RAM libre si el primer minuto se traba más o el juego tarda más en abrir.',
        'Show shader notes in the corner': 'Mostrar notas de shaders en la esquina',
        'A note in the lower right corner shows the shader cache being read at start and the shaders being compiled, so a short stutter has a visible reason. Switch it off to keep the screen clean.': 'Una nota en la esquina inferior derecha muestra la lectura de la caché de shaders al iniciar y los shaders que se están compilando, para que un tirón corto tenga un motivo visible. Desactívalo para mantener la pantalla limpia.',
        'Shader cache': 'Caché de shaders',
        'Delete shader cache': 'Borrar caché de shaders',
        'Deletes the saved shaders and pipelines of this saves folder. Use it if graphics look wrong after an update or the cache grew too big; the next start compiles everything again.': 'Borra los shaders y pipelines guardados de esta carpeta de partidas. Úsalo si los gráficos se ven mal después de una actualización o si la caché creció demasiado; el próximo inicio lo compila todo de nuevo.',
        'Close the game before deleting the shader cache.': 'Cierra el juego antes de borrar la caché de shaders.',
        'The shader cache is already empty.': 'La caché de shaders ya está vacía.',
        'Delete the shader cache ({} MB in {})? The game compiles the shaders again, so the next start stutters for a few minutes.': '¿Borrar la caché de shaders ({} MB en {})? El juego compila los shaders de nuevo, así que el próximo inicio se traba unos minutos.',
        'Could not delete all of the shader cache; another program may be using it: {}': 'No se pudo borrar toda la caché de shaders; puede que otro programa la esté usando: {}',
    },
}

EXTRA['pt'].update({
    'GUI Scale': 'Escala da interface',
    'Auto': 'Automático',
    'For launcher only. Applies after Save.':
        'Somente para o inicializador. Aplica-se após Salvar.',
    'Monitor': 'Monitor',
    'Primary (default)': 'Principal (padrão)',
    'The screen the game opens its window on. Primary is the monitor the system considers the main one.':
        'A tela em que o jogo abre a sua janela. Principal é o monitor que o sistema considera o principal.',
})

EXTRA['pt'].update({
    '1 file': '1 arquivo',
    'Change textures, sounds and text. Your game files stay untouched.': 'Troque texturas, sons e textos. Os arquivos do jogo não são alterados.',
    'Details': 'Detalhes',
    'Drop a mod folder or .zip in the mods folder.': 'Coloque a pasta ou o .zip do mod na pasta de mods.',
    'Hide': 'Ocultar',
    'Load order': 'Ordem de carga',
    'Lower in the list wins': 'O mais abaixo na lista vence',
    'No mods yet': 'Nenhum mod ainda',
    'Open folder': 'Abrir pasta',
    'Open mod folder': 'Abrir pasta do mod',
    'Open mods folder': 'Abrir pasta de mods',
    'Presets': 'Presets',
    'Website': 'Site',
    'by {}': 'por {}',
    'game patches': 'patches de jogo',
    '{} files': '{} arquivos',
    '{} of {} active': '{} de {} ativos',
    '… and {} more': '… e mais {}',
    '⚠ 1 file is changed by two mods. {} wins.': '⚠ 1 arquivo é alterado por dois mods. {} vence.',
    '⚠ {} also in {}. {} wins.': '⚠ {} também em {}. {} vence.',
    '⚠ {} files are changed by more than one mod. The lower one in the list wins.': '⚠ {} arquivos são alterados por mais de um mod. Vence o mais abaixo na lista.',
})
EXTRA['es'].update({
    '1 file': '1 archivo',
    'Change textures, sounds and text. Your game files stay untouched.': 'Cambia texturas, sonidos y textos. Los archivos del juego no se modifican.',
    'Details': 'Detalles',
    'Drop a mod folder or .zip in the mods folder.': 'Coloca la carpeta o el .zip del mod en la carpeta de mods.',
    'Hide': 'Ocultar',
    'Load order': 'Orden de carga',
    'Lower in the list wins': 'Gana el que está más abajo',
    'No mods yet': 'Aún no hay mods',
    'Open folder': 'Abrir carpeta',
    'Open mod folder': 'Abrir carpeta del mod',
    'Open mods folder': 'Abrir carpeta de mods',
    'Presets': 'Preajustes',
    'Website': 'Sitio web',
    'by {}': 'por {}',
    'game patches': 'parches del juego',
    '{} files': '{} archivos',
    '{} of {} active': '{} de {} activos',
    '… and {} more': '… y {} más',
    '⚠ 1 file is changed by two mods. {} wins.': '⚠ 1 archivo lo cambian dos mods. Gana {}.',
    '⚠ {} also in {}. {} wins.': '⚠ {} también en {}. Gana {}.',
    '⚠ {} files are changed by more than one mod. The lower one in the list wins.': '⚠ {} archivos los cambian varios mods. Gana el que está más abajo.',
    'Monitor': 'Monitor',
    'Primary (default)': 'Principal (predeterminado)',
    'The screen the game opens its window on. Primary is the monitor the system considers the main one.':
        'La pantalla en la que el juego abre su ventana. Principal es el monitor que el sistema considera el principal.',
})

EXTRA['pt'].update({
    'Left stick click': 'Clique do analógico esquerdo',
    'Right stick click': 'Clique do analógico direito',
    'Plus (+)': 'Mais (+)',
    'Minus (−)': 'Menos (−)',
    'Home button': 'Botão Home',
    'Capture': 'Captura',
})
EXTRA['es'].update({
    'Left stick click': 'Clic del stick izquierdo',
    'Right stick click': 'Clic del stick derecho',
    'Plus (+)': 'Más (+)',
    'Minus (−)': 'Menos (−)',
    'Home button': 'Botón Home',
    'Capture': 'Captura',
})

EXTRA['pt'].update({
    'Device': 'Dispositivo',
    'Keyboard & mouse': 'Teclado e mouse',
    'Gamepad': 'Controle',
    'Controller type': 'Tipo de controle',
})
EXTRA['es'].update({
    'Device': 'Dispositivo',
    'Keyboard & mouse': 'Teclado y ratón',
    'Gamepad': 'Mando',
    'Controller type': 'Tipo de mando',
})

EXTRA['es'].update({
    'Allows manual skipping of cinematics any cinematic with the X button. Thanks to illusion0001 aka illusionyy for the PS3 patch which made this very easy.':
        'Permite saltar manualmente cualquier cinemática con el botón X. Gracias a illusion0001, alias illusionyy, por el parche de PS3 que lo hizo muy fácil.',
    'Circle':
        'Círculo',
    'Cross':
        'X',
    'D-pad down':
        'Cruceta abajo',
    'D-pad left':
        'Cruceta izquierda',
    'D-pad right':
        'Cruceta derecha',
    'D-pad up':
        'Cruceta arriba',
    'Fast screen effects (Rage of Sparta, weapons)':
        'Efectos de pantalla rápidos (Rage of Sparta, armas)',
    'Fixes corrupted textures in the game by increasing the Video Arena size, allowing the game to use more video memory for textures and effects. Requires setting DMEM to 2048 MB or more, or the game crashes. Enable either this patch or one resolution patch, never both.':
        'Corrige las texturas corruptas del juego aumentando el tamaño del Video Arena, para que el juego use más memoria de vídeo en texturas y efectos. Requiere DMEM de 2048 MB o más, o el juego se cierra. Activa este parche o un parche de resolución, nunca los dos.',
    'Fixes the black screen at startup with the FPS counter running: the frame pacing index starts at 2 instead of 0, so the second frame no longer waits for a flip that only it can trigger.':
        'Corrige la pantalla negra al iniciar con el contador de FPS en marcha: el índice de ritmo de fotogramas empieza en 2 en lugar de 0, así el segundo fotograma ya no espera un flip que solo él puede provocar.',
    'For launcher only. Applies after Save.':
        'Solo para el launcher. Se aplica después de Guardar.',
    'GUI Scale':
        'Escala de la interfaz',
    'Graphics card':
        'Tarjeta gráfica',
    'Keeps powers and weapon effects at full FPS. Switch it off if a screen effect (blur, distortion, the color change of a power) looks stretched, shifted or wrong.':
        'Mantiene los poderes y los efectos de las armas a FPS completos. Desactívalo si un efecto de pantalla (desenfoque, distorsión, el cambio de color de un poder) se ve estirado, desplazado o mal.',
    'L1 + Touchpad right':
        'L1 + touchpad derecho',
    'LS (left stick click)':
        'LS (clic del stick izquierdo)',
    'Mute button':
        'Botón de silencio',
    'PS button':
        'Botón PS',
    'RS (right stick click)':
        'RS (clic del stick derecho)',
    'Skips the initial logos. Thanks to illusion0001 aka illusionyy for the PS3 patch which made this very easy.':
        'Salta los logotipos iniciales. Gracias a illusion0001, alias illusionyy, por el parche de PS3 que lo hizo muy fácil.',
    'Square':
        'Cuadrado',
    'The GPU the game runs on. Choose your dedicated card if the game uses the integrated one.':
        'La GPU en la que se ejecuta el juego. Elige tu tarjeta dedicada si el juego usa la integrada.',
    'Touchpad click':
        'Clic del touchpad',
    'Touchpad left':
        'Touchpad izquierdo',
    'Touchpad right':
        'Touchpad derecho',
    'Triangle':
        'Triángulo',
    'Xbox button':
        'Botón Xbox',
    'Your own dump of the game (eboot.bin, sce_module, sce_sys): SpartanPort, any region and version.':
        'Tu propio volcado del juego (eboot.bin, sce_module, sce_sys): SpartanPort, cualquier región y versión.',
    '{} not found in this eboot.bin at the last start (see Log)':
        '{} no se encontró en este eboot.bin en el último inicio (ver Registro)',
    '{} · 60 FPS engine · no patches for this game':
        '{} · motor a 60 FPS · sin parches para este juego',
    'Native · 1920 × 1080 · 16:9':
        'Nativa · 1920 × 1080 · 16:9',
})

EXTRA['pt'].update({
    'Aspect ratio': 'Proporção da tela',
    'Shows the render resolutions of this shape only.': 'Mostra só as resoluções de render desta proporção.',
})
EXTRA['es'].update({
    'Aspect ratio': 'Relación de aspecto',
    'Shows the render resolutions of this shape only.': 'Muestra solo las resoluciones de render de esta proporción.',
})

EXTRA['pt'].update({'Polish': 'Polonês'})
EXTRA['es'].update({'Polish': 'Polaco'})
for _lang, _text in (('pl', 'Polski'), ('fr', 'Polonais'), ('de', 'Polnisch'),
                     ('it', 'Polacco'), ('tr', 'Lehçe'), ('ar', 'البولندية'),
                     ('zh', '波兰语'), ('ja', 'ポーランド語'), ('ko', '폴란드어'),
                     ('id', 'Polandia')):
    EXTRA.setdefault(_lang, {})['Polish'] = _text

# Simplified Chinese: current launcher pages, controls and diagnostics.
EXTRA.setdefault('zh', {})['Portuguese (Brazil)'] = '葡萄牙语（巴西）'
EXTRA.setdefault('zh', {}).update({'   (game patches)': '   （游戏补丁）',
 '   … and {} more': '   ……另有 {} 项',
 ' Previous saves: {}.': ' 原存档备份：{}。',
 '(none)': '（无）',
 '1 (lowest latency)': '1（最低延迟）',
 '1 file': '1 个文件',
 '1 is the lowest input latency; more can raise FPS when the graphics card is the limit.': '1 '
                                                                                           '的输入延迟最低；显卡成为性能瓶颈时，增加此值可能提高帧率。',
 'About 40 to 66 FPS in a heavy scene. Needs deferred GPU readbacks. Switch it off if lighting or objects look wrong.': '高负载场景约从 '
                                                                                                                        '40 '
                                                                                                                        '提升至 '
                                                                                                                        '66 '
                                                                                                                        '帧。需要延迟 '
                                                                                                                        'GPU '
                                                                                                                        '回读；若光照或物体异常，请关闭。',
 'Absolute': '绝对位置',
 'Adaptive mouse (experimental)': '自适应鼠标（实验性）',
 'Adaptive mouse: how far the mouse moves for one step of a rotation. Also in the in-game menu.': '自适应鼠标：每次旋转步进所需的鼠标移动距离。也可在游戏内菜单调整。',
 'Add to Steam': '添加到 Steam',
 'Aiming with the mouse while L2 is held (Head of Helios, Bow of Apollo). Relative: the stick follows the movement and goes back to neutral when the mouse stops. Absolute: where the mouse was when L2 was pressed is the centre, and the stick points from it to the mouse. Also in the in-game menu.': '按住 '
                                                                                                                                                                                                                                                                                                          'L2 '
                                                                                                                                                                                                                                                                                                          '时用鼠标瞄准（赫利俄斯之首、阿波罗之弓）。相对模式：摇杆跟随鼠标移动，停止移动时回中。绝对模式：以按下 '
                                                                                                                                                                                                                                                                                                          'L2 '
                                                                                                                                                                                                                                                                                                          '时的鼠标位置为中心，按鼠标偏移控制摇杆。也可在游戏内菜单调整。',
 'Aiming with the mouse while L2 is held: lower needs more mouse movement for the same stick, higher less. Also in the in-game menu.': '按住 '
                                                                                                                                       'L2 '
                                                                                                                                       '用鼠标瞄准时，数值越低，所需移动距离越大；越高则越小。也可在游戏内菜单调整。',
 'Allows manual skipping of cinematics any cinematic with the X button. Thanks to illusion0001 aka illusionyy for the PS3 patch which made this very easy.': '允许按 '
                                                                                                                                                             'X '
                                                                                                                                                             '键跳过过场动画。感谢 '
                                                                                                                                                             'illusion0001（illusionyy）的 '
                                                                                                                                                             'PS3 '
                                                                                                                                                             '补丁为此提供便利。',
 'Anisotropic filtering': '各向异性过滤',
 'Asks GameMode (gamemoderun) for the performance governor while the game runs.': '游戏运行时请求 '
                                                                                  'GameMode（gamemoderun）使用性能模式。',
 'Aspect ratio': '宽高比',
 'Auto': '自动',
 'Automatic': '自动',
 'Automatic ({}: off)': '自动（{}：关闭）',
 'Automatic ({}: on)': '自动（{}：开启）',
 'Automatic lets SDL choose. Pick X11 if the window misbehaves on Wayland (it runs through XWayland).': '自动由 '
                                                                                                        'SDL '
                                                                                                        '选择。Wayland '
                                                                                                        '下窗口异常时可选择 '
                                                                                                        'X11（通过 '
                                                                                                        'XWayland '
                                                                                                        '运行）。',
 'Balanced (×1.7)': '均衡（×1.7）',
 'Borderless': '无边框',
 'Borderless covers the screen without changing the display mode.': '无边框模式铺满屏幕，不改变显示模式。',
 'Capture': '截屏键',
 'Change textures, sounds and text. Your game files stay untouched.': '替换纹理、声音和文字，原始游戏文件保持不变。',
 'Changes applied to the game code at start.': '启动时应用到游戏代码的修改。',
 'Cheats': '作弊选项',
 'Choose the folder of the update (eboot.bin, sce_sys)': '选择更新目录（包含 eboot.bin、sce_sys）',
 'Choose the game folder': '选择游戏目录',
 'Choose two separate folders for the game and its update, or leave the update folder empty.': '游戏和更新请选择两个不同目录，或将更新目录留空。',
 'Choose your game folder to play.': '请选择游戏目录以开始游玩。',
 'Circle': '圆圈',
 "Click a box, then press a key, click a mouse button or turn the wheel (it gives up after {} seconds). An input already used by another button swaps with it; the arrow puts one button back on its default. Rage of Sparta presses L3 and R3 together: tick the box above it to use one input instead of two. Grab presses L1 and Circle together. Dodge evades the way you are moving (WASD), or back when standing still. Touchpad right opens the game's pause menu. Insert always opens the port's menu and cannot be bound. Applied when the game starts.": '点击输入框后按键、点击鼠标按钮或滚动滚轮（{} '
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   '秒后自动取消）。已被使用的输入会与原绑定互换；箭头可单独恢复默认绑定。斯巴达之怒同时按下 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   'L3 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   '和 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   'R3：勾选上方选项可改用单个输入。抓取同时按下 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   'L1 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   '和圆圈。闪避沿移动方向（WASD）进行，站立时向后闪避。触摸板右侧打开暂停菜单。Insert '
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   '固定打开移植版菜单，不能绑定。下次启动游戏生效。',
 'Close the game before restoring saves.': '恢复存档前请先退出游戏。',
 'Community patches of the selected game, applied at start.': '所选游戏的社区补丁，启动时应用。',
 'Compile new shaders in the background': '后台编译新着色器',
 'Controller type': '手柄类型',
 'Controls': '控制',
 'Copying the system files…': '正在复制系统文件……',
 'Could not copy the files: {}': '无法复制文件：{}',
 'Could not restore: {}': '无法恢复：{}',
 'Could not save all settings. Check write permissions and free disk space, then try again.': '部分设置保存失败。请检查写入权限和磁盘剩余空间后重试。',
 'Creates spartanport.desktop in ~/.local/share/applications so the launcher and the game show their icon in the dock and the menu. Turning it off removes the entry.': '在 '
                                                                                                                                                                        '~/.local/share/applications '
                                                                                                                                                                        '创建 '
                                                                                                                                                                        'spartanport.desktop，使启动器和游戏在程序坞与菜单中显示图标。关闭时会删除该条目。',
 'Cross': '叉号',
 'D-pad down': '方向键下',
 'D-pad left': '方向键左',
 'D-pad right': '方向键右',
 'D-pad up': '方向键上',
 'Deferred GPU readbacks': '延迟 GPU 回读',
 'Delete': '删除',
 'Details': '详细信息',
 'Device': '设备',
 'Diagnostics (for development)': '诊断（开发用途）',
 'Display': '显示',
 'Display mode': '显示模式',
 'Dodge': '闪避',
 'Drop a mod folder or .zip in the mods folder.': '将模组文件夹或 .zip 放入模组目录。',
 'Enable the upscaler (experimental)': '启用超分辨率（实验性）',
 'Engine frame rate': '引擎帧率',
 'Enlarges a smaller game image to the window. With Render resolution 720p (Performance) it raises the FPS; at the size of the window it does nothing.': '将较低分辨率的游戏画面放大到窗口大小。渲染分辨率设为 '
                                                                                                                                                         '720p '
                                                                                                                                                         '时可提高帧率；与窗口大小相同时无作用。',
 'Experimental': '实验性',
 'F11 captures a frame for analysis': '按 F11 捕获一帧用于分析',
 'F12 in the game': '游戏中按 F12',
 'Fast screen effects (Rage of Sparta, weapons)': '快速屏幕特效（斯巴达之怒、武器）',
 'Fast shader cache loading (parallel)': '快速加载着色器缓存（并行）',
 'Fewer stutters in new areas: the game keeps running while new shaders compile. An object or effect may appear a moment later. Switch it off if something stays missing.': '进入新区域时减少卡顿：编译新着色器时游戏继续运行，物体或特效可能稍后出现。若内容持续缺失，请关闭。',
 'Fewer waits for the GPU. Switch it off if textures or shadows look wrong.': '减少等待 GPU；若纹理或阴影异常，请关闭。',
 'Files': '文件',
 'Files layered over the game at start; the game files are never changed.': '启动时覆盖加载的文件；原始游戏文件不会改变。',
 'Fixes corrupted textures in the game by increasing the Video Arena size, allowing the game to use more video memory for textures and effects. Requires setting DMEM to 2048 MB or more, or the game crashes. Enable either this patch or one resolution patch, never both.': '增大视频内存区，修复纹理损坏，让游戏使用更多显存存放纹理与特效。DMEM '
                                                                                                                                                                                                                                                                               '需至少 '
                                                                                                                                                                                                                                                                               '2048 '
                                                                                                                                                                                                                                                                               'MB，否则会崩溃。此补丁与分辨率补丁只能启用其中一个。',
 'Fixes the black screen at startup with the FPS counter running: the frame pacing index starts at 2 instead of 0, so the second frame no longer waits for a flip that only it can trigger.': '修复启动时帧率计数仍在运行但画面黑屏的问题：帧调度索引从 '
                                                                                                                                                                                              '2 '
                                                                                                                                                                                              '开始，避免第二帧等待只能由自身触发的翻转事件。',
 'Folders': '目录',
 'For launcher only. Applies after Save.': '仅影响启动器，保存后生效。',
 'Frame rate and speed options.': '帧率与运行速度选项。',
 'Frame rate limit': '帧率上限',
 'Frames queued': '排队帧数',
 'GAME BUTTON': '游戏按键',
 'GAMEPAD': '手柄',
 'GPU data': 'GPU 数据',
 'GPU memory (previous behavior)': '显存（旧版方式）',
 'GPU wait and readback report in the log (every 5 s)': '在日志中记录 GPU 等待与回读（每 5 秒）',
 'GUI Scale': '界面缩放',
 'Game default': '游戏默认',
 'Game multisampling': '游戏多重采样',
 'Game patches': '游戏补丁',
 'Game reads GPU data without waiting (big FPS gain)': '游戏读取 GPU 数据时不等待（大幅提高帧率）',
 'GameMode': 'GameMode',
 'Gamepad': '手柄',
 'Gamepad Mapping': '手柄映射',
 'Gamepad names': '手柄按键名称',
 'General': '常规',
 'Grab': '抓取',
 'Graphics (experimental)': '图形（实验性）',
 'Graphics card': '显卡',
 'Hide': '隐藏',
 'Home': '主页',
 'Home button': 'Home 键',
 'How the game appears on your screen.': '游戏在屏幕上的显示方式。',
 'How the stick-rotation prompts are answered. Keyboard: type the turn instead, half turns from the top D S A (clockwise) or A S D, from the bottom A W D or D W A, full turns D S A W or A S D W; a key out of turn starts again. Adaptive mouse: draw the turn with the mouse.': '旋转摇杆提示的输入方式。键盘模式：依次按键完成旋转。从上方开始半圈按 '
                                                                                                                                                                                                                                                                                   'D '
                                                                                                                                                                                                                                                                                   'S '
                                                                                                                                                                                                                                                                                   'A（顺时针）或 '
                                                                                                                                                                                                                                                                                   'A '
                                                                                                                                                                                                                                                                                   'S '
                                                                                                                                                                                                                                                                                   'D，从下方开始按 '
                                                                                                                                                                                                                                                                                   'A '
                                                                                                                                                                                                                                                                                   'W '
                                                                                                                                                                                                                                                                                   'D '
                                                                                                                                                                                                                                                                                   '或 '
                                                                                                                                                                                                                                                                                   'D '
                                                                                                                                                                                                                                                                                   'W '
                                                                                                                                                                                                                                                                                   'A；整圈按 '
                                                                                                                                                                                                                                                                                   'D '
                                                                                                                                                                                                                                                                                   'S '
                                                                                                                                                                                                                                                                                   'A '
                                                                                                                                                                                                                                                                                   'W '
                                                                                                                                                                                                                                                                                   '或 '
                                                                                                                                                                                                                                                                                   'A '
                                                                                                                                                                                                                                                                                   'S '
                                                                                                                                                                                                                                                                                   'D '
                                                                                                                                                                                                                                                                                   'W。顺序错误会重新开始。自适应鼠标模式则用鼠标画出旋转轨迹。',
 'In-game button icons': '游戏内按键图标',
 'Infinite Double Jumps': '无限空中跳跃',
 'Items, size and corner: in-game menu (Insert / R3+L2) > Overlay.': '显示项目、大小和位置：游戏内菜单（Insert／R3+L2）→ 性能叠加层。',
 'Joystick prompts': '摇杆提示输入',
 'KEYBOARD': '键盘',
 'Keep the mouse in the game window': '将鼠标限制在游戏窗口内',
 'Keeps powers and weapon effects at full FPS. Switch it off if a screen effect (blur, distortion, the color change of a power) looks stretched, shifted or wrong.': '让技能和武器特效保持全帧率。如果模糊、扭曲、技能变色等屏幕特效拉伸、错位或异常，请关闭。',
 'Key not supported': '不支持此按键',
 'Keyboard': '键盘',
 'Keyboard & Mouse Mapping': '键鼠映射',
 'Keyboard & mouse': '键盘与鼠标',
 'Keyboard (native)': '键盘（原生）',
 'Keyboard and mouse, button icons and gamepad mapping.': '键鼠、按键图标与手柄映射。',
 "Keys use SDL names: letters, digits, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Up, Down, Left, Right, F1 to F12. A name the game does not know keeps the default (the log says which). L2/R2 on the gamepad stay on the triggers. Insert and R3+L2 always open the port's menu. Applied when the game starts.": '按键使用 '
                                                                                                                                                                                                                                                                                                                                        'SDL '
                                                                                                                                                                                                                                                                                                                                        '名称：字母、数字、Space、Return、Tab、Left '
                                                                                                                                                                                                                                                                                                                                        'Shift、Left '
                                                                                                                                                                                                                                                                                                                                        'Ctrl、Left '
                                                                                                                                                                                                                                                                                                                                        'Alt、Backspace、Up、Down、Left、Right、F1 '
                                                                                                                                                                                                                                                                                                                                        '至 '
                                                                                                                                                                                                                                                                                                                                        'F12。无法识别的名称会保留默认值，并记录到日志。手柄 '
                                                                                                                                                                                                                                                                                                                                        'L2/R2 '
                                                                                                                                                                                                                                                                                                                                        '保留为扳机。Insert '
                                                                                                                                                                                                                                                                                                                                        '和 '
                                                                                                                                                                                                                                                                                                                                        'R3+L2 '
                                                                                                                                                                                                                                                                                                                                        '固定打开移植版菜单。下次启动游戏生效。',
 "Keys use SDL names: letters, digits, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Up, Down, Left, Right, F1 to F12. A name the game does not know keeps the default (the log says which). L2/R2 on the gamepad stay on the triggers. Touchpad right opens the game's pause menu; Touchpad left (photo mode) is L1 + Touchpad right on the gamepad. Insert and R3+L2 always open the port's menu. Applied when the game starts.": '按键使用 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      'SDL '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      '名称：字母、数字、Space、Return、Tab、Left '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      'Shift、Left '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      'Ctrl、Left '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      'Alt、Backspace、Up、Down、Left、Right、F1 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      '至 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      'F12。无法识别的名称会保留默认值，并记录到日志。手柄 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      'L2/R2 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      '保留为扳机。触摸板右侧打开暂停菜单；手柄按 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      'L1 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      '+ '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      '触摸板右侧进入拍照模式。Insert '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      '和 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      'R3+L2 '
                                                                                                                                                                                                                                                                                                                                                                                                                                                      '固定打开移植版菜单。下次启动游戏生效。',
 'L1 + Touchpad right': 'L1 + 触摸板右侧',
 "L2/R2 stay on the triggers. Touchpad right opens the game's pause menu; Touchpad left (photo mode) is L1 + Touchpad right. R3+L2 always opens the port's menu. Applied when the game starts.": 'L2/R2 '
                                                                                                                                                                                                 '保留为扳机。触摸板右侧打开暂停菜单；L1 '
                                                                                                                                                                                                 '+ '
                                                                                                                                                                                                 '触摸板右侧进入拍照模式。R3+L2 '
                                                                                                                                                                                                 '固定打开移植版菜单。下次启动游戏生效。',
 'LS (left stick click)': 'LS（按下左摇杆）',
 'Language': '语言',
 'Launcher options, files and diagnostics.': '启动器选项、文件和诊断。',
 'Left stick click': '按下左摇杆',
 'Linux': 'Linux',
 'Load': '加载',
 'Load order': '加载顺序',
 'Load shader cache in the background': '后台加载着色器缓存',
 'Loading screen only: builds the cached pipelines on several CPU threads.': '仅在加载画面使用多个 CPU 线程构建缓存管线。',
 'Loose-file mods, each in its own folder with dvdroot_ps4.': '散装文件模组，每个模组独立放在包含 dvdroot_ps4 的文件夹中。',
 'Lower in the list wins': '列表下方优先',
 'Made before every start, newest first; the last {} are kept. Restoring backs up the current saves first.': '每次启动前备份，按新到旧排列；保留最近 '
                                                                                                             '{} '
                                                                                                             '份。恢复前会先备份当前存档。',
 'MangoHud overlay': 'MangoHud 性能叠加层',
 'Match my controller': '匹配当前手柄',
 'Menu entry and icon (AppImage)': '菜单条目和图标（AppImage）',
 'Minus (−)': '减号（−）',
 'Mouse prompt distance': '鼠标旋转提示距离',
 'Mute button': '静音键',
 'Native (1080p)': '原生（1080p）',
 'Native (1920 × 1080)': '原生（1920 × 1080）',
 'Native AA (×1.0)': '原生抗锯齿（×1.0）',
 'Native · 1920 × 1080 · 16:9': '原生 · 1920 × 1080 · 16:9',
 'No Wayland session detected.': '未检测到 Wayland 会话。',
 'No built-in patches for this game.': '此游戏没有内置补丁。',
 'No mods yet': '暂无模组',
 'No patch files for this game version yet.': '此游戏版本暂无补丁文件。',
 'Not installed.': '未安装。',
 'Off (default): the game runs without temporal upscaling. These upscalers are not calibrated for this game yet: expect ghosting or flicker. FSR 3.1 needs nothing else; FSR 4 needs the assets below, and without them the game uses FSR 3.1. The options on this page only take effect with this switch on.': '默认关闭，游戏不使用时域超分辨率。这些算法尚未针对本游戏校准，可能出现拖影或闪烁。FSR '
                                                                                                                                                                                                                                                                                                                '3.1 '
                                                                                                                                                                                                                                                                                                                '无需额外文件；FSR '
                                                                                                                                                                                                                                                                                                                '4 '
                                                                                                                                                                                                                                                                                                                '需要下方资源，缺失时会回退到 '
                                                                                                                                                                                                                                                                                                                'FSR '
                                                                                                                                                                                                                                                                                                                '3.1。本页选项仅在此开关开启时生效。',
 'Off (fixes black lines)': '关闭（修复黑线）',
 'Off allows tearing for the lowest latency.': '关闭可降低延迟，但可能出现画面撕裂。',
 'Only if the update is dumped apart from the game: it is laid over the game folder without changing either. Leave empty when the game folder already has the update.': '仅在更新与本体分开提取时填写：启动时覆盖加载，两个原始目录均不会改变。游戏目录已整合更新时请留空。',
 'Open folder': '打开目录',
 'Open mod folder': '打开模组目录',
 'Open mods folder': '打开模组目录',
 'Open the screenshots folder': '打开截图目录',
 'Original (default)': '原始图标（默认）',
 'Overlay': '性能叠加层',
 'PLAY': '开始游戏',
 'PS button': 'PS 键',
 'Patches': '补丁',
 'Performance (×2)': '性能（×2）',
 'Play': '开始游戏',
 'Plus (+)': '加号（+）',
 'Precise': '精确',
 'Preset': '预设',
 'Presets': '预设方案',
 'Press a key': '请按键',
 'Press a key or mouse': '请按键或点击鼠标',
 'Quality (×1.5)': '质量（×1.5）',
 'RS (right stick click)': 'RS（按下右摇杆）',
 'RUNNING': '运行中',
 'Rage of Sparta on one input (L3 + R3 together)': '单键释放斯巴达之怒（同时按下 L3 + R3）',
 'Records how the next frame is drawn into the captures folder.': '将下一帧的绘制过程记录到 captures 目录。',
 'Relative': '相对移动',
 'Render resolution': '渲染分辨率',
 'Repeat air jumps without landing. Also available in the in-game Cheats menu.': '可连续空中跳跃，无需落地。游戏内作弊菜单也可设置。',
 'Replace the current saves with the backup {}? The current saves are backed up first.': '用备份 {} '
                                                                                         '替换当前存档？操作前会先备份当前存档。',
 "Replaces the PlayStation button icons inside the game with the ones of your controller. Works on a copy of two game files in the port's data folder (about 70 MB); your game folder is never changed. Applied when the game starts.": '将游戏内 '
                                                                                                                                                                                                                                        'PlayStation '
                                                                                                                                                                                                                                        '按键图标替换为所选手柄图标。在移植版 '
                                                                                                                                                                                                                                        'data '
                                                                                                                                                                                                                                        '目录生成两个游戏文件的副本（约 '
                                                                                                                                                                                                                                        '70 '
                                                                                                                                                                                                                                        'MB），不改变原始游戏目录。下次启动游戏生效。',
 'Reset gamepad to defaults': '恢复默认手柄绑定',
 'Reset keyboard & mouse to defaults': '恢复默认键鼠绑定',
 'Reset to defaults': '恢复默认',
 'Resolution': '分辨率',
 'Restore': '恢复',
 'Reuse saved Vulkan pipelines. Disable to compare FPS with normal compilation. Takes effect on the next launch; saved caches are preserved.': '复用已保存的 '
                                                                                                                                               'Vulkan '
                                                                                                                                               '管线。关闭可与常规编译方式对比帧率。下次启动生效，已有缓存会保留。',
 'Right stick click': '按下右摇杆',
 'Right stick sensitivity': '右摇杆灵敏度',
 'Right stick style': '右摇杆模式',
 'Save': '保存',
 'Save backups': '存档备份',
 'Saves': '存档',
 'Saves restored from {}.': '已从 {} 恢复存档。',
 "Saves the game's image, without the overlay, as a PNG in the screenshots folder of the saves.": '将不含性能叠加层的游戏画面保存为 '
                                                                                                  'PNG，位于用户存档目录中的 '
                                                                                                  'screenshots '
                                                                                                  '文件夹。',
 'Screenshots': '截图',
 'Select Very Hard without completing the game. Does not change existing save difficulty. In-game: enable before opening New Game.': '无需通关即可选择极难模式，不改变已有存档难度。请在打开“新游戏”前启用。',
 'Shaders': '着色器',
 'Sharper floors and walls seen at an angle, at little cost on most graphics cards.': '斜视地面和墙壁时纹理更清晰，多数显卡的性能开销很小。',
 'Shortcut created:': '已创建快捷方式：',
 'Show Discord Rich Presence': '显示 Discord 游玩状态',
 'Show performance overlay': '显示性能叠加层',
 'Shows MangoHud (FPS, frame times, temperatures) through its Vulkan layer.': '通过 Vulkan 图层显示 '
                                                                              'MangoHud（帧率、帧时间、温度）。',
 'Shows only the game name and the time played on your Discord profile.': '仅在 Discord 个人资料中显示游戏名称与游玩时长。',
 'Shows the render resolutions of this shape only.': '只显示所选宽高比的渲染分辨率。',
 'Shows where the frame waits for the GPU and which reads cause it. For performance tests.': '记录每帧等待 GPU '
                                                                                             '的位置及相关读取，供性能测试使用。',
 'Skips the initial logos. Thanks to illusion0001 aka illusionyy for the PS3 patch which made this very easy.': '跳过开场标志。感谢 '
                                                                                                                'illusion0001（illusionyy）的 '
                                                                                                                'PS3 '
                                                                                                                '补丁为此提供便利。',
 'Square': '方块',
 'Steam is open. Close it completely (Steam > Exit) and click again: Steam rewrites its shortcuts when it closes.': 'Steam '
                                                                                                                    '正在运行。请完全退出（Steam '
                                                                                                                    '→ '
                                                                                                                    '退出）后重试；Steam '
                                                                                                                    '关闭时会重写快捷方式。',
 'Steam was not found for this user.': '未找到当前用户的 Steam。',
 'Stream buffer memory': '流式缓冲区内存',
 "Strength of the gamepad's vibration; 0% turns it off.": '手柄震动强度；0% 为关闭。',
 'System RAM (cached, default)': '系统内存（缓存，默认）',
 'System RAM (uncached)': '系统内存（不缓存）',
 'System RAM can improve FPS on NVIDIA laptops. Choose GPU memory to restore the previous behavior. Applies on the next game launch.': '使用系统内存可提高部分 '
                                                                                                                                       'NVIDIA '
                                                                                                                                       '笔记本的帧率。选择显存可恢复旧版方式。下次启动游戏生效。',
 'Textures': '纹理',
 'The GPU the game runs on. Choose your dedicated card if the game uses the integrated one.': '运行游戏的显卡。若误用集成显卡，请选择独立显卡。',
 'The engine frame rate (below) is the highest it can go.': '最高不会超过下方设置的引擎帧率。',
 'The files in {} look like a PKG extraction: the system files (param.sfo, icon0.png, pic1.png and the rest) are in\n\n{}\n\nCopy all of them into its sce_sys folder now? Files already there are kept.': '{} '
                                                                                                                                                                                                           '中的文件看起来来自 '
                                                                                                                                                                                                           'PKG '
                                                                                                                                                                                                           '提取，系统文件（param.sfo、icon0.png、pic1.png '
                                                                                                                                                                                                           '等）位于\n'
                                                                                                                                                                                                           '\n'
                                                                                                                                                                                                           '{}\n'
                                                                                                                                                                                                           '\n'
                                                                                                                                                                                                           '现在将它们复制到 '
                                                                                                                                                                                                           'sce_sys '
                                                                                                                                                                                                           '目录？已有文件会保留。',
 'The game dump is incomplete. Missing or empty files:\n\n{}\n\nDump the game again, or copy these files from a complete dump.': '游戏文件不完整，以下文件缺失或为空：\n'
                                                                                                                                 '\n'
                                                                                                                                 '{}\n'
                                                                                                                                 '\n'
                                                                                                                                 '请重新提取游戏，或从完整游戏副本中复制这些文件。',
 'The game folder has no sce_sys/pic1.png for the pictures.': '游戏目录缺少 sce_sys/pic1.png，无法显示背景图。',
 'The game opens at once and the saved shaders load while you play; a small note in the corner shows the progress. Switch it off to wait on the loading screen instead.': '游戏立即打开，缓存着色器在游玩时加载，角落显示进度。关闭则在加载画面等待完成。',
 'The game renders at this size; a resolution patch replaces the texture fix and reserves the memory it needs.': '游戏按此尺寸渲染；分辨率补丁会替代纹理修复，并预留所需内存。',
 'The game stopped while reading its files: the game dump may be incomplete. Extract the pkg again and check that the game folder is about 40 GB.': '读取游戏文件时停止运行，游戏文件可能不完整。请重新提取 '
                                                                                                                                                    'PKG，并确认游戏目录约为 '
                                                                                                                                                    '40 '
                                                                                                                                                    'GB。',
 "The game's 2-sample depth buffer, read as a texture by its lighting. AMD's and Intel's Windows drivers read it wrong and the lighting shows black lines, so Automatic turns multisampling off on those cards (Radeon, Arc) and keeps it on elsewhere (NVIDIA). Off and On force the choice on any card.": '游戏使用双重采样深度缓冲作为光照纹理。AMD '
                                                                                                                                                                                                                                                                                                            '和 '
                                                                                                                                                                                                                                                                                                            'Intel '
                                                                                                                                                                                                                                                                                                            '的 '
                                                                                                                                                                                                                                                                                                            'Windows '
                                                                                                                                                                                                                                                                                                            '驱动可能读取错误并产生黑线；自动模式会在 '
                                                                                                                                                                                                                                                                                                            'Radeon、Arc '
                                                                                                                                                                                                                                                                                                            '显卡上关闭多重采样，在 '
                                                                                                                                                                                                                                                                                                            'NVIDIA '
                                                                                                                                                                                                                                                                                                            '上保留。也可手动强制开启或关闭。',
 'The highest frame rate the game itself runs at (a game patch).': '游戏引擎自身运行的最高帧率（通过游戏补丁实现）。',
 'The shortcut "{}" was added to Steam (accounts: {}), with its icon, capsules, background and logo. Open Steam to see it in the library.': '快捷方式“{}”已添加到 '
                                                                                                                                            'Steam（账户：{}），包含图标、封面、背景和标志。打开 '
                                                                                                                                            'Steam '
                                                                                                                                            '即可在库中查看。',
 'The shortcut "{}" was updated in Steam (accounts: {}), with its icon, capsules, background and logo. Open Steam to see it in the library.': '快捷方式“{}”已在 '
                                                                                                                                              'Steam '
                                                                                                                                              '中更新（账户：{}），包含图标、封面、背景和标志。打开 '
                                                                                                                                              'Steam '
                                                                                                                                              '即可在库中查看。',
 'The size the game draws at; higher is sharper and slower. A resolution patch replaces the texture fix and reserves the memory it needs.': '游戏绘制画面的尺寸；越高越清晰，但性能开销越大。分辨率补丁会替代纹理修复，并预留所需内存。',
 'The update folder {} is incomplete. Missing or empty files:\n\n{}\n\nChoose the folder of the update (eboot.bin, sce_sys), or leave it empty.': '更新目录 '
                                                                                                                                                  '{} '
                                                                                                                                                  '不完整，以下文件缺失或为空：\n'
                                                                                                                                                  '\n'
                                                                                                                                                  '{}\n'
                                                                                                                                                  '\n'
                                                                                                                                                  '请选择包含 '
                                                                                                                                                  'eboot.bin、sce_sys '
                                                                                                                                                  '的更新目录，或留空。',
 'Touchpad click': '点击触摸板',
 'Touchpad left': '触摸板左侧',
 'Touchpad right': '触摸板右侧',
 'Triangle': '三角',
 'Type a name for the preset first.': '请先输入预设名称。',
 'Ultra Performance (×3)': '极致性能（×3）',
 'Unlimited': '无限制',
 'Unlock Very Hard': '解锁极难模式',
 'Update folder (optional)': '更新目录（可选）',
 'Use pipeline binary cache (A/B test)': '使用管线二进制缓存（对比测试）',
 'VSync': '垂直同步',
 'Vibration': '震动',
 'Website': '网站',
 'Which gamepad button presses each button of the game.': '选择触发各游戏动作的手柄按键。',
 'Which key or gamepad button presses each button of the game.': '选择触发各游戏动作的键盘或手柄按键。',
 'Which key or mouse input presses each button of the game.': '选择触发各游戏动作的键盘或鼠标输入。',
 'While you play, the mouse stays in the game window, so a click cannot land on another monitor or app. The in-game menu and switching windows get it back. Also in the in-game menu.': '游玩时将鼠标限制在游戏窗口，避免点到其他屏幕或程序。打开游戏内菜单或切换窗口时释放鼠标。也可在游戏内菜单调整。',
 'Window system': '窗口系统',
 'Windowed': '窗口化',
 'Windows could not reserve the memory the game needs. Close other programs, or let the Windows page file grow (for example 16384 MB initial, 32768 MB maximum), and start again.': 'Windows '
                                                                                                                                                                                    '无法预留游戏所需内存。请关闭其他程序，或增大系统分页文件（例如初始 '
                                                                                                                                                                                    '16384 '
                                                                                                                                                                                    'MB、最大 '
                                                                                                                                                                                    '32768 '
                                                                                                                                                                                    'MB）后重试。',
 'Linux could not allocate the memory the game needs. Close other programs, check available RAM, swap space, or /dev/shm, and start again.': 'Linux 无法为游戏分配所需的内存。请关闭其他程序，检查可用的 RAM、swap 交换空间或 /dev/shm，然后重新启动。',
 'Xbox button': 'Xbox 键',
 'Your game dump, saves and language.': '游戏文件、存档和语言。',
 'Your own dump of the game (eboot.bin, sce_module, sce_sys): God of War III Remastered, any region and version.': '你自己的游戏文件（eboot.bin、sce_module、sce_sys）；支持各地区和版本的战神 '
                                                                                                                   '3 '
                                                                                                                   '重制版。',
 'Your own dump of the game (eboot.bin, sce_module, sce_sys): SpartanPort, any region and version.': '你自己的战神 '
                                                                                                     '3 '
                                                                                                     '重制版游戏文件（eboot.bin、sce_module、sce_sys），支持各地区和版本。',
 'Your own dump of the game (eboot.bin, sce_module, sce_sys); God of War III Remastered: CUSA01623, version 01.02 for the community patches.': '你自己的游戏文件（eboot.bin、sce_module、sce_sys）；社区补丁要求战神 '
                                                                                                                                               '3 '
                                                                                                                                               '重制版 '
                                                                                                                                               'CUSA01623、01.02 '
                                                                                                                                               '版本。',
 'Your own dump of the game (eboot.bin, sce_module, sce_sys); SpartanPort: CUSA01623, version 01.02 for the community patches.': '你自己的战神 '
                                                                                                                                 '3 '
                                                                                                                                 '重制版游戏文件（eboot.bin、sce_module、sce_sys）；社区补丁要求 '
                                                                                                                                 'CUSA01623、01.02 '
                                                                                                                                 '版本。',
 'by {}': '作者：{}',
 'game patches': '游戏补丁',
 'restart': '需重启',
 'shadPS4/GoldHEN XML patch files for this game version. Empty: {}': '适用于此游戏版本的 shadPS4／GoldHEN XML '
                                                                     '补丁。留空时使用：{}',
 '{} (a folder, not a file)': '{}（应为文件夹，而非文件）',
 '{} (empty)': '{}（为空）',
 '{} (not a param.sfo)': '{}（不是 param.sfo）',
 '{} (unreadable)': '{}（无法读取）',
 '{} files': '{} 个文件',
 '{} not found in this eboot.bin at the last start (see Log)': '上次启动时未在此 eboot.bin 中找到 {}（见日志）',
 '{} of {} active': '已启用 {}／{}',
 '{} trigger': '{} 扳机',
 '{} · 60 FPS engine · no patches for this game': '{} · 60 帧引擎 · 此游戏无可用补丁',
 '{} · 60 FPS engine · patches not available for this game build': '{} · 60 帧引擎 · 此游戏版本无可用补丁',
 '{} · version {}': '{} · 版本 {}',
 '{} · version {} · {}': '{} · 版本 {} · {}',
 '{} · version {} · {}. Resolution and frame rate patches follow Performance.': '{} · 版本 {} · '
                                                                                '{}。分辨率和帧率补丁以“性能”页为准。',
 '{} · {} FPS engine · {} patches': '{} · {} 帧引擎 · {} 个补丁',
 '{} · {} more patches': '{} · 另有 {} 个补丁',
 '… and {} more': '……另有 {} 项',
 '⚠ 1 file is changed by two mods. {} wins.': '⚠ 两个模组修改了同一个文件，{} 优先。',
 '⚠ Game version {}: the game patches need {}; none applied': '⚠ 游戏版本 {}：补丁要求版本 {}，未应用任何补丁',
 '⚠ {} also in {}. {} wins.': '⚠ {} 也存在于 {}，{} 优先。',
 '⚠ {} file(s) replaced by more than one mod (the last one wins):': '⚠ 有 {} 个文件被多个模组替换（最后加载的优先）：',
 '⚠ {} files are changed by more than one mod. The lower one in the list wins.': '⚠ 有 {} 个文件被多个模组修改，列表下方的优先。',
 '✓ Game version {}: the game patches apply': '✓ 游戏版本 {}：可应用游戏补丁'})
# The Trophies page: its texts (and the trophy names) live in spartanport_trophies.py.
import spartanport_trophies  # noqa: E402

for _language, _texts in spartanport_trophies.UI.items():
    EXTRA.setdefault(_language, {}).update(_texts)


EXTRA['zh'].update({
 'Version {} is out, but it has no Linux build yet.': '版本 {} 已发布，但还没有 Linux 版本。',
 'The launcher cannot write to {}, so it cannot replace itself there. Open the releases page to download the new version by hand?': '启动器无法写入 {}，因此无法在此处替换自身。是否打开发布页面手动下载新版本？',
    'Monitor': '显示器',
    'Primary (default)': '主显示器（默认）',
    'The screen the game opens its window on. Primary is the monitor the system considers the main one.':
        '游戏窗口打开所在的屏幕。主显示器是系统指定的主要屏幕。',
    "Replaces the PlayStation button icons inside the game with the ones of your controller. Works on a copy of up to three game files in the port's data folder (about 410 MB); your game folder is never changed. Applied when the game starts.":
        '将游戏中的 PlayStation 按键图标替换为你的控制器图标。使用移植版数据文件夹内最多三个游戏文件的副本（约 410 MB），不会更改游戏文件夹。启动游戏时生效。',
})

# The note under Game language: the game keeps its own text and speech languages (#842).
# The menu names are the game's own, from its R_LANG_*.WAD.
_GAME_LANGUAGE_NOTE = "Used on the game's first start. After that, the game keeps its own text and speech languages in its settings save: change them in the game, in Options → Game Settings → Game Text and Options → Audio → Speech."
for _language, _text in {
    'pt': 'Vale no primeiro início do jogo. Depois, o jogo guarda os próprios idiomas de texto e de falas no save de configurações: troque no jogo, em Opções → Configurações de jogo → Texto de jogo e Opções → Áudio → Falas.',
    'es': 'Se usa en el primer inicio del juego. Después, el juego guarda sus propios idiomas de texto y de diálogos en su partida de ajustes: cambialos en el juego, en Opciones → Ajustes de juego → Texto del juego y Opciones → Audio → Diálogos.',
    'zh': '仅在游戏首次启动时生效。之后游戏会在自己的设置存档中保存文字和台词语言：请在游戏中通过選項 → 遊戲設定 → 遊戲資訊 和 選項 → 聲音 → 台詞 更改。',
    'pl': 'Używany przy pierwszym uruchomieniu gry. Potem gra zapisuje własny język tekstu i dialogów w zapisie ustawień: zmień je w grze, w Opcje → Ustawienia gry → Tekst gry oraz Opcje → Dźwięk → Dialogi.',
    'fr': 'Utilisée au premier lancement du jeu. Ensuite, le jeu garde ses propres langues de texte et de voix dans sa sauvegarde de réglages : changez-les dans le jeu, dans Options → Paramètres jeu → Texte du jeu et Options → Audio → Voix.',
    'de': 'Gilt beim ersten Start des Spiels. Danach speichert das Spiel seine eigene Text- und Sprachausgabe-Sprache im Einstellungsspeicherstand: ändere sie im Spiel unter Optionen → Spieleinstellungen → Spieltext und Optionen → Audio → Sprachausgabe.',
    'it': 'Usata al primo avvio del gioco. Poi il gioco conserva le proprie lingue di testo e dei dialoghi nel salvataggio delle impostazioni: cambiale nel gioco, in Opzioni → Impostazioni di gioco → Testo di gioco e Opzioni → Audio → Dialoghi.',
    'tr': 'Oyunun ilk açılışında kullanılır. Sonrasında oyun kendi metin ve konuşma dillerini ayar kaydında tutar: bunları oyunda, Seçenekler → Oyun Ayarları → Oyun Metni ve Seçenekler → Ses → Konuşma bölümünden değiştirin.',
    'ja': 'ゲームの初回起動時に使われます。その後は、ゲームが設定セーブにテキストと台詞の言語を保存します。ゲーム内の オプション → ゲーム設定 → テキスト表示 と オプション → 音声設定 → 台詞 で変更してください。',
    'ko': '게임을 처음 시작할 때 사용됩니다. 그 후에는 게임이 설정 저장 데이터에 텍스트와 음성 언어를 저장합니다. 게임 안의 옵션 → 게임 설정 → 게임 텍스트 및 옵션 → 오디오 → 음성에서 변경하세요.',
    'id': 'Digunakan pada saat pertama kali game dimulai. Setelah itu, game menyimpan bahasa teks dan suaranya sendiri di penyimpanan pengaturan: ubah di dalam game, di Options → Game Settings → Game Text dan Options → Audio → Speech.',
}.items():
    EXTRA.setdefault(_language, {})[_GAME_LANGUAGE_NOTE] = _text


EXTRA.setdefault('id', {}).update({
    'The launcher cannot write to {}, so it cannot replace itself there. Open the releases page to download the new version by hand?':
        'Launcher tidak dapat menulis ke {}, sehingga tidak dapat mengganti dirinya sendiri di sana. Buka halaman rilis untuk mengunduh versi baru secara manual?',
    'Version {} is out, but it has no Linux build yet.':
        'Versi {} sudah rilis, tetapi belum ada build untuk Linux.',
    'Polish': 'Polandia',
    'Portuguese (Brazil)': 'Portugis (Brasil)',
    'Aspect ratio': 'Rasio aspek',
    'Shows the render resolutions of this shape only.': 'Hanya menampilkan resolusi render dari rasio ini.',
    'Left stick click': 'Klik stik kiri',
    'Right stick click': 'Klik stik kanan',
    'Plus (+)': 'Plus (+)',
    'Minus (−)': 'Minus (−)',
    'Home button': 'Tombol Home',
    'Capture': 'Tangkapan',
    'Circle': 'Lingkaran',
    'Cross': 'Silang (X)',
    'Square': 'Kotak',
    'Triangle': 'Segitiga',
    'D-pad down': 'D-pad bawah',
    'D-pad left': 'D-pad kiri',
    'D-pad right': 'D-pad kanan',
    'D-pad up': 'D-pad atas',
    'Touchpad click': 'Klik touchpad',
    'Touchpad left': 'Touchpad kiri',
    'Touchpad right': 'Touchpad kanan',
    'L1 + Touchpad right': 'L1 + Touchpad kanan',
    'LS (left stick click)': 'LS (klik stik kiri)',
    'RS (right stick click)': 'RS (klik stik kanan)',
    'Mute button': 'Tombol bisu',
    'PS button': 'Tombol PS',
    'Xbox button': 'Tombol Xbox',
    'Match my controller': 'Sesuaikan controller saya',
    'Device': 'Perangkat',
    'Keyboard & mouse': 'Keyboard & mouse',
    'Gamepad': 'Gamepad',
    'Controller type': 'Tipe controller',
    'Reset gamepad to defaults': 'Atur ulang gamepad ke default',
    'Reset keyboard & mouse to defaults': 'Atur ulang keyboard & mouse ke default',
    'Reset to defaults': 'Atur ulang ke default',
    'Which gamepad button presses each button of the game.': 'Tombol gamepad mana yang menekan setiap tombol game.',
    'Which key or gamepad button presses each button of the game.': 'Tombol keyboard atau gamepad mana yang menekan setiap tombol game.',
    'Which key or mouse input presses each button of the game.': 'Tombol keyboard atau input mouse mana yang menekan setiap tombol game.',
    'Keyboard and mouse, button icons and gamepad mapping.': 'Keyboard dan mouse, ikon tombol, dan pemetaan gamepad.',
    "Keys use SDL names: letters, digits, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Up, Down, Left, Right, F1 to F12. A name the game does not know keeps the default (the log says which). L2/R2 on the gamepad stay on the triggers. Insert and R3+L2 always open the port's menu. Applied when the game starts.":
        "Tombol menggunakan nama SDL: huruf, angka, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Atas, Bawah, Kiri, Kanan, F1 hingga F12. Nama yang tidak dikenali game tetap memakai default (dicatat di log). L2/R2 pada gamepad tetap pada trigger. Insert dan R3+L2 selalu membuka menu port. Diterapkan saat game dimulai.",
    "Keys use SDL names: letters, digits, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Up, Down, Left, Right, F1 to F12. A name the game does not know keeps the default (the log says which). L2/R2 on the gamepad stay on the triggers. Touchpad right opens the game's pause menu; Touchpad left (photo mode) is L1 + Touchpad right on the gamepad. Insert and R3+L2 always open the port's menu. Applied when the game starts.":
        "Tombol menggunakan nama SDL: huruf, angka, Space, Return, Tab, Left Shift, Left Ctrl, Left Alt, Backspace, Atas, Bawah, Kiri, Kanan, F1 hingga F12. Nama yang tidak dikenali game tetap memakai default (dicatat di log). L2/R2 pada gamepad tetap pada trigger. Touchpad kanan membuka menu jeda game; Touchpad kiri (mode foto) adalah L1 + Touchpad kanan pada gamepad. Insert dan R3+L2 selalu membuka menu port. Diterapkan saat game dimulai.",
    "L2/R2 stay on the triggers. Touchpad right opens the game's pause menu; Touchpad left (photo mode) is L1 + Touchpad right. R3+L2 always opens the port's menu. Applied when the game starts.":
        "L2/R2 tetap pada trigger. Touchpad kanan membuka menu jeda game; Touchpad kiri (mode foto) adalah L1 + Touchpad kanan. R3+L2 selalu membuka menu port. Diterapkan saat game dimulai.",
    "Replaces the PlayStation button icons inside the game with the ones of your controller. Works on a copy of up to three game files in the port's data folder (about 410 MB); your game folder is never changed. Applied when the game starts.":
        "Mengganti ikon tombol PlayStation di dalam game dengan ikon controller Anda. Bekerja pada salinan hingga tiga file game di folder data port (sekitar 410 MB); folder game Anda tidak pernah diubah. Diterapkan saat game dimulai.",
    'Monitor': 'Monitor',
    'Primary (default)': 'Utama (default)',
    'The screen the game opens its window on. Primary is the monitor the system considers the main one.':
        'Layar tempat jendela game dibuka. Utama adalah monitor yang dianggap sistem sebagai yang utama.',
    "Strength of the gamepad's vibration; 0% turns it off.":
        "Kekuatan getaran gamepad; 0% mematikannya.",
    'Vibration': 'Getaran',
    'Press a key': 'Tekan tombol',
    'Press a key or mouse': 'Tekan tombol atau mouse',
    'Mouse prompt distance': 'Jarak petunjuk mouse',
    'Right stick sensitivity': 'Sensitivitas stik kanan',
    'Right stick style': 'Gaya stik kanan',
    'Absolute': 'Absolut',
    'Relative': 'Relatif',
    'Grab': 'Tangkap',
    'Dodge': 'Menghindar',
    '{} trigger': 'Trigger {}',
    'Textures': 'Tekstur',
    'Anisotropic filtering': 'Penyaringan anisotropik',
    'Game default': 'Default game',
    'Sharper floors and walls seen at an angle, at little cost on most graphics cards.':
        'Lantai dan dinding lebih tajam saat dilihat dari sudut miring, dengan sedikit beban pada sebagian besar kartu grafis.',
    'restart': 'mulai ulang',
    'Overlay': 'Overlay',
    'Show performance overlay': 'Tampilkan overlay performa',
    'Items, size and corner: in-game menu (Insert / R3+L2) > Overlay.':
        'Item, ukuran, dan sudut posisi: menu dalam game (Insert / R3+L2) > Overlay.',
    'Screenshots': 'Tangkapan layar',
    'F12 in the game': 'F12 di dalam game',
    'Open the screenshots folder': 'Buka folder tangkapan layar',
    "Saves the game's image, without the overlay, as a PNG in the screenshots folder of the saves.":
        "Menyimpan gambar game, tanpa overlay, sebagai file PNG di folder screenshots pada folder simpanan.",
    'Display': 'Layar',
    'How the game appears on your screen.': 'Bagaimana game tampil di layar Anda.',
    'Display mode': 'Mode tampilan',
    'Borderless covers the screen without changing the display mode.':
        'Tanpa batas (borderless) menutupi layar tanpa mengubah mode tampilan.',
    'VSync': 'VSync',
    'Off allows tearing for the lowest latency.': 'Mati mengizinkan screen tearing untuk latensi terendah.',
    'Native (1080p)': 'Asli (1080p)',
    'Native AA (×1.0)': 'AA Asli (×1.0)',
    'Native · 1920 × 1080 · 16:9': 'Asli · 1920 × 1080 · 16:9',
    'Quality (×1.5)': 'Kualitas (×1.5)',
    'Balanced (×1.7)': 'Seimbang (×1.7)',
    'Performance (×2)': 'Performa (×2)',
    'Ultra Performance (×3)': 'Ultra Performa (×3)',
    'Original (default)': 'Asli (default)',
    'Show Discord Rich Presence': 'Tampilkan Discord Rich Presence',
    'Shows only the game name and the time played on your Discord profile.':
        'Hanya menampilkan nama game dan lama waktu bermain di profil Discord Anda.',
    'Game multisampling': 'Multisampling game',
    'Automatic ({}: off)': 'Otomatis ({}: mati)',
    'Automatic ({}: on)': 'Otomatis ({}: nyala)',
    'Off (fixes black lines)': 'Mati (memperbaiki garis hitam)',
    "The game's 2-sample depth buffer, read as a texture by its lighting. AMD's and Intel's Windows drivers read it wrong and the lighting shows black lines, so Automatic turns multisampling off on those cards (Radeon, Arc) and keeps it on elsewhere (NVIDIA). Off and On force the choice on any card.":
        "Buffer kedalaman 2-sampel game dibaca sebagai tekstur oleh pencahayaannya. Driver Windows AMD dan Intel salah membacanya sehingga pencahayaan menampilkan garis hitam; Otomatis mematikan multisampling pada kartu tersebut (Radeon, Arc) dan menyalakannya pada kartu lain (NVIDIA). Mati dan Nyala memaksa pilihan pada kartu grafis apa pun.",
    'GUI Scale': 'Skala Antarmuka',
    'For launcher only. Applies after Save.': 'Hanya untuk launcher. Berlaku setelah Disimpan.',
    'Windowed': 'Berjendela',
    'Window system': 'Sistem jendela',
    'No Wayland session detected.': 'Tidak ada sesi Wayland yang terdeteksi.',
    'MangoHud overlay': 'Overlay MangoHud',
    'Shows MangoHud (FPS, frame times, temperatures) through its Vulkan layer.':
        'Menampilkan MangoHud (FPS, frame time, suhu) melalui lapisan Vulkan-nya.',
    'Records how the next frame is drawn into the captures folder.':
        'Merekam bagaimana frame berikutnya digambar ke dalam folder captures.',
    'Shows where the frame waits for the GPU and which reads cause it. For performance tests.':
        'Menunjukkan di mana frame menunggu GPU dan pembacaan mana yang menyebabkannya. Untuk uji performa.',
    'Enlarges a smaller game image to the window. With Render resolution 720p (Performance) it raises the FPS; at the size of the window it does nothing.':
        'Memperbesar gambar game yang lebih kecil ke ukuran jendela. Dengan Resolusi render 720p (Performa) ini meningkatkan FPS; pada ukuran jendela yang sama tidak ada efeknya.',
    'Off (default): the game runs without temporal upscaling. These upscalers are not calibrated for this game yet: expect ghosting or flicker. FSR 3.1 needs nothing else; FSR 4 needs the assets below, and without them the game uses FSR 3.1. The options on this page only take effect with this switch on.':
        'Mati (default): game berjalan tanpa temporal upscaling. Upscaler ini belum dikalibrasi untuk game ini: kemungkinan ada efek ghosting atau flicker. FSR 3.1 tidak memerlukan apa pun lagi; FSR 4 memerlukan aset di bawah, dan tanpanya game akan menggunakan FSR 3.1. Opsi pada halaman ini hanya berlaku jika sakelar ini aktif.',
    'Frame rate and speed options.': 'Opsi frame rate dan kecepatan.',
    'Frame rate limit': 'Batas frame rate',
    'The engine frame rate (below) is the highest it can go.':
        'Frame rate engine (di bawah) adalah batas tertinggi yang dapat dicapai.',
    'Frames queued': 'Frame dalam antrean',
    '1 (lowest latency)': '1 (latensi terendah)',
    'Unlimited': 'Tak terbatas',
    '1 is the lowest input latency; more can raise FPS when the graphics card is the limit.':
        '1 memberikan latensi input terendah; lebih banyak dapat menaikkan FPS saat kartu grafis menjadi batas performa.',
    'Engine frame rate': 'Frame rate engine',
    'The highest frame rate the game itself runs at (a game patch).':
        'Frame rate tertinggi tempat game itu sendiri berjalan (sebuah patch game).',
    'Render resolution': 'Resolusi render',
    'The size the game draws at; higher is sharper and slower. A resolution patch replaces the texture fix and reserves the memory it needs.':
        'Ukuran resolusi gambar game; semakin tinggi semakin tajam dan berat. Patch resolusi menggantikan perbaikan tekstur dan memesan memori yang dibutuhkannya.',
    'Shaders': 'Shader',
    'Use pipeline binary cache (A/B test)': 'Gunakan cache biner pipeline (uji A/B)',
    'Reuse saved Vulkan pipelines. Disable to compare FPS with normal compilation. Takes effect on the next launch; saved caches are preserved.':
        'Gunakan kembali pipeline Vulkan yang tersimpan. Nonaktifkan untuk membandingkan FPS dengan kompilasi normal. Berlaku pada peluncuran berikutnya; cache tersimpan tetap dipertahankan.',
    'Load shader cache in the background': 'Muat cache shader di latar belakang',
    'The game opens at once and the saved shaders load while you play; a small note in the corner shows the progress. Switch it off to wait on the loading screen instead.':
        'Game langsung terbuka dan shader tersimpan dimuat saat Anda bermain; catatan kecil di sudut menunjukkan prosesnya. Matikan jika ingin menunggu di layar pemuatan (loading screen).',
    'Compile new shaders in the background': 'Kompilasi shader baru di latar belakang',
    'Fewer stutters in new areas: the game keeps running while new shaders compile. An object or effect may appear a moment later. Switch it off if something stays missing.':
        'Mengurangi stutter di area baru: game terus berjalan saat shader baru dikompilasi. Objek atau efek mungkin muncul sesaat kemudian. Matikan jika ada yang hilang terus-menerus.',
    'Fast shader cache loading (parallel)': 'Pemuatan cache shader cepat (paralel)',
    'Loading screen only: builds the cached pipelines on several CPU threads.':
        'Hanya di layar pemuatan: membangun pipeline yang di-cache pada beberapa thread CPU.',
    'GPU data': 'Data GPU',
    'Graphics card': 'Kartu grafis',
    'The GPU the game runs on. Choose your dedicated card if the game uses the integrated one.':
        'GPU tempat game berjalan. Pilih kartu diskrit (dedicated) jika game menggunakan kartu grafis terintegrasi.',
    'Stream buffer memory': 'Memori stream buffer',
    'GPU memory (previous behavior)': 'Memori GPU (perilaku sebelumnya)',
    'System RAM (cached, default)': 'RAM Sistem (di-cache, default)',
    'System RAM (uncached)': 'RAM Sistem (tidak di-cache)',
    'System RAM can improve FPS on NVIDIA laptops. Choose GPU memory to restore the previous behavior. Applies on the next game launch.':
        'RAM Sistem dapat meningkatkan FPS pada laptop NVIDIA. Pilih memori GPU untuk mengembalikan perilaku sebelumnya. Berlaku pada peluncuran game berikutnya.',
    'Game reads GPU data without waiting (big FPS gain)':
        'Game membaca data GPU tanpa menunggu (peningkatan FPS besar)',
    'About 40 to 66 FPS in a heavy scene. Needs deferred GPU readbacks. Switch it off if lighting or objects look wrong.':
        'Sekitar 40 hingga 66 FPS pada adegan berat. Memerlukan readback GPU yang ditangguhkan. Matikan jika pencahayaan atau objek terlihat salah.',
    'Deferred GPU readbacks': 'Readback GPU ditangguhkan',
    'Fewer waits for the GPU. Switch it off if textures or shadows look wrong.':
        'Lebih sedikit waktu tunggu untuk GPU. Matikan jika tekstur atau bayangan terlihat salah.',
    'Fast screen effects (Rage of Sparta, weapons)':
        'Efek layar cepat (Rage of Sparta, senjata)',
    'Keeps powers and weapon effects at full FPS. Switch it off if a screen effect (blur, distortion, the color change of a power) looks stretched, shifted or wrong.':
        'Menjaga kekuatan dan efek senjata tetap pada FPS penuh. Matikan jika efek layar (blur, distorsi, perubahan warna kekuatan) terlihat melar, bergeser, atau salah.',
    'Precise': 'Tepat (Precise)',
    'Cheats': 'Cheat',
    'Infinite Double Jumps': 'Lompatan Ganda Tak Terbatas',
    'Repeat air jumps without landing. Also available in the in-game Cheats menu.':
        'Ulangi lompatan di udara tanpa harus mendarat. Juga tersedia di menu Cheat dalam game.',
    'Rage of Sparta on one input (L3 + R3 together)':
        'Rage of Sparta dalam satu input (L3 + R3 bersamaan)',
    'Unlock Very Hard': 'Buka Tingkat Kesulitan Sangat Sulit',
    'Select Very Hard without completing the game. Does not change existing save difficulty. In-game: enable before opening New Game.':
        'Pilih tingkat Sangat Sulit tanpa menamatkan game. Tidak mengubah kesulitan simpanan yang sudah ada. Dalam game: aktifkan sebelum membuka Game Baru.',
    'Folders': 'Folder',
    'Your game dump, saves and language.': 'Dump game, simpanan, dan bahasa Anda.',
    'Your own dump of the game (eboot.bin, sce_module, sce_sys): SpartanPort, any region and version.':
        'Dump game milik Anda sendiri (eboot.bin, sce_module, sce_sys): SpartanPort, wilayah dan versi apa pun.',
    'Your own dump of the game (eboot.bin, sce_module, sce_sys); SpartanPort: CUSA01623, version 01.02 for the community patches.':
        'Dump game milik Anda sendiri (eboot.bin, sce_module, sce_sys); SpartanPort: CUSA01623, versi 01.02 untuk patch komunitas.',
    'Update folder (optional)': 'Folder pembaruan (opsional)',
    'Only if the update is dumped apart from the game: it is laid over the game folder without changing either. Leave empty when the game folder already has the update.':
        'Hanya jika pembaruan di-dump terpisah dari game: pembaruan akan diterapkan di atas folder game tanpa mengubah keduanya. Biarkan kosong jika folder game sudah memiliki pembaruan.',
    'The update folder {} is incomplete. Missing or empty files:\n\n{}\n\nChoose the folder of the update (eboot.bin, sce_sys), or leave it empty.':
        'Folder pembaruan {} tidak lengkap. File hilang atau kosong:\n\n{}\n\nPilih folder pembaruan (eboot.bin, sce_sys), atau biarkan kosong.',
    'The game dump is incomplete. Missing or empty files:\n\n{}\n\nDump the game again, or copy these files from a complete dump.':
        'Dump game tidak lengkap. File hilang atau kosong:\n\n{}\n\nDump game kembali, atau salin file-file ini dari dump yang lengkap.',
    'The files in {} look like a PKG extraction: the system files (param.sfo, icon0.png, pic1.png and the rest) are in\n\n{}\n\nCopy all of them into its sce_sys folder now? Files already there are kept.':
        'File dalam {} terlihat seperti ekstraksi PKG: file sistem (param.sfo, icon0.png, pic1.png dan lainnya) berada di\n\n{}\n\nSalin semuanya ke dalam folder sce_sys sekarang? File yang sudah ada tetap dipertahankan.',
    'The game folder has no sce_sys/pic1.png for the pictures.':
        'Folder game tidak memiliki sce_sys/pic1.png untuk gambar latar.',
    'The game stopped while reading its files: the game dump may be incomplete. Extract the pkg again and check that the game folder is about 40 GB.':
        'Game berhenti saat membaca file-filenya: dump game mungkin tidak lengkap. Ekstrak pkg kembali dan pastikan folder game berukuran sekitar 40 GB.',
    'Saves': 'Simpanan',
    'Save backups': 'Cadangan simpanan',
    'Restore': 'Pulihkan',
    'Save': 'Simpan',
    'Made before every start, newest first; the last {} are kept. Restoring backs up the current saves first.':
        'Dibuat sebelum setiap peluncuran, yang terbaru di awal; {} terakhir disimpan. Memulihkan akan mencadangkan simpanan saat ini terlebih dahulu.',
    'Replace the current saves with the backup {}? The current saves are backed up first.':
        'Ganti simpanan saat ini dengan cadangan {}? Simpanan saat ini akan dicadangkan terlebih dahulu.',
    'Saves restored from {}.': 'Simpanan dipulihkan dari {}.',
    'RUNNING': 'BERJALAN',
    'Fixes corrupted textures in the game by increasing the Video Arena size, allowing the game to use more video memory for textures and effects. Requires setting DMEM to 2048 MB or more, or the game crashes. Enable either this patch or one resolution patch, never both.':
        'Memperbaiki tekstur yang rusak dalam game dengan meningkatkan ukuran Video Arena, memungkinkan game menggunakan lebih banyak memori video untuk tekstur dan efek. Memerlukan pengaturan DMEM ke 2048 MB atau lebih, jika tidak game akan crash. Aktifkan patch ini atau salah satu patch resolusi, jangan keduanya.',
    'Skips the initial logos. Thanks to illusion0001 aka illusionyy for the PS3 patch which made this very easy.':
        'Melewati logo pembuka. Terima kasih kepada illusion0001 alias illusionyy atas patch PS3 yang membuat ini sangat mudah.',
    'Allows manual skipping of cinematics any cinematic with the X button. Thanks to illusion0001 aka illusionyy for the PS3 patch which made this very easy.':
        'Mengizinkan melewati sinematik apa pun secara manual dengan tombol X. Terima kasih kepada illusion0001 alias illusionyy atas patch PS3 yang membuat ini sangat mudah.',
    'Fixes the black screen at startup with the FPS counter running: the frame pacing index starts at 2 instead of 0, so the second frame no longer waits for a flip that only it can trigger.':
        'Memperbaiki layar hitam saat startup dengan penghitung FPS berjalan: indeks frame pacing dimulai dari 2, bukan 0, sehingga frame kedua tidak lagi menunggu flip yang hanya bisa dipicu olehnya.',
    'No built-in patches for this game.': 'Tidak ada patch bawaan untuk game ini.',
    'No patch files for this game version yet.': 'Belum ada file patch untuk versi game ini.',
    'shadPS4/GoldHEN XML patch files for this game version. Empty: {}':
        'File patch XML shadPS4/GoldHEN untuk versi game ini. Kosong: {}',
    '{} · 60 FPS engine · no patches for this game': '{} · engine 60 FPS · tidak ada patch untuk game ini',
    '{} · 60 FPS engine · patches not available for this game build':
        '{} · engine 60 FPS · patch tidak tersedia untuk build game ini',
    '{} · version {}': '{} · versi {}',
    '{} · version {} · {}. Resolution and frame rate patches follow Performance.':
        '{} · versi {} · {}. Patch resolusi dan frame rate mengikuti Performa.',
    '{} · {} FPS engine · {} patches': '{} · engine {} FPS · {} patch',
    'game patches': 'patch game',
    'Loose-file mods, each in its own folder with dvdroot_ps4.':
        'Mod file lepas, masing-masing dalam foldernya sendiri dengan dvdroot_ps4.',
    'Change textures, sounds and text. Your game files stay untouched.':
        'Ubah tekstur, suara, dan teks. File game Anda tetap tidak tersentuh.',
    'Drop a mod folder or .zip in the mods folder.': 'Tarik folder mod atau file .zip ke dalam folder mods.',
    'No mods yet': 'Belum ada mod',
    'Details': 'Detail',
    'Hide': 'Sembunyikan',
    'Load order': 'Urutan muat',
    'Lower in the list wins': 'Yang lebih bawah dalam daftar menang',
    'Open folder': 'Buka folder',
    'Open mod folder': 'Buka folder mod',
    'Open mods folder': 'Buka folder mod',
    'Presets': 'Preset',
    'Preset': 'Preset',
    'Type a name for the preset first.': 'Ketik nama untuk preset terlebih dahulu.',
    'Website': 'Situs web',
    'by {}': 'oleh {}',
    '{} files': '{} file',
    '{} of {} active': '{} dari {} aktif',
    '… and {} more': '… dan {} lainnya',
    '⚠ 1 file is changed by two mods. {} wins.': '⚠ 1 file diubah oleh dua mod. {} menang.',
    '⚠ {} also in {}. {} wins.': '⚠ {} juga ada di {}. {} menang.',
    '⚠ {} file(s) replaced by more than one mod (the last one wins):':
        '⚠ {} file digantikan oleh lebih dari satu mod (yang terakhir menang):',
    '⚠ {} files are changed by more than one mod. The lower one in the list wins.':
        '⚠ {} file diubah oleh lebih dari satu mod. Yang lebih bawah dalam daftar menang.',
    '1 file': '1 file',
    'Load': 'Muat',
    'Launcher options, files and diagnostics.': 'Opsi launcher, file, dan diagnostik.',
    'Language': 'Bahasa',
    'Linux': 'Linux',
    'Menu entry and icon (AppImage)': 'Entri menu dan ikon (AppImage)',
    'Not installed.': 'Belum terinstal.',
    'Steam is open. Close it completely (Steam > Exit) and click again: Steam rewrites its shortcuts when it closes.':
        'Steam sedang terbuka. Tutup sepenuhnya (Steam > Keluar) dan klik lagi: Steam menulis ulang pintasannya saat ditutup.',
    'Steam was not found for this user.': 'Steam tidak ditemukan untuk pengguna ini.',
    'Shortcut created:': 'Pintasan dibuat:',
    'The shortcut "{}" was added to Steam (accounts: {}), with its icon, capsules, background and logo. Open Steam to see it in the library.':
        'Pintasan "{}" telah ditambahkan ke Steam (akun: {}), dengan ikon, kapsul, latar belakang, dan logonya. Buka Steam untuk melihatnya di perpustakaan.',
    'The shortcut "{}" was updated in Steam (accounts: {}), with its icon, capsules, background and logo. Open Steam to see it in the library.':
        'Pintasan "{}" telah diperbarui di Steam (akun: {}), dengan ikon, kapsul, latar belakang, dan logonya. Buka Steam untuk melihatnya di perpustakaan.',
    'While you play, the mouse stays in the game window, so a click cannot land on another monitor or app. The in-game menu and switching windows get it back. Also in the in-game menu.':
        'Saat Anda bermain, mouse tetap berada di dalam jendela game, sehingga klik tidak akan mengenai monitor atau aplikasi lain. Menu dalam game dan berpindah jendela akan mengembalikannya. Juga ada di menu dalam game.',
    'Windows could not reserve the memory the game needs. Close other programs, or let the Windows page file grow (for example 16384 MB initial, 32768 MB maximum), and start again.':
        'Windows tidak dapat memesan memori yang dibutuhkan game. Tutup program lain, atau perbesar pagefile Windows (misalnya 16384 MB awal, 32768 MB maksimum), lalu mulai kembali.',
    'Linux could not allocate the memory the game needs. Close other programs, check available RAM, swap space, or /dev/shm, and start again.':
        'Linux tidak dapat mengalokasikan memori yang dibutuhkan game. Tutup program lain, periksa RAM yang tersedia, swap, atau /dev/shm, lalu mulai kembali.',
    '{} (a folder, not a file)': '{} (sebuah folder, bukan file)',
    '{} (empty)': '{} (kosong)',
    '{} (not a param.sfo)': '{} (bukan param.sfo)',
    '{} (unreadable)': '{} (tidak dapat dibaca)',
    '{} not found in this eboot.bin at the last start (see Log)':
        '{} tidak ditemukan di eboot.bin ini pada peluncuran terakhir (lihat Log)',
    '   (game patches)': '   (patch game)',
    '   … and {} more': '   … dan {} lainnya',
    ' Previous saves: {}.': ' Simpanan sebelumnya: {}.',
    '(none)': '(tidak ada)',
    'Adaptive mouse (experimental)': 'Mouse adaptif (eksperimental)',
    'Adaptive mouse: how far the mouse moves for one step of a rotation. Also in the in-game menu.':
        'Mouse adaptif: seberapa jauh mouse bergerak untuk satu langkah putaran. Juga ada di menu dalam game.',
    'Add to Steam': 'Tambahkan ke Steam',
    'Aiming with the mouse while L2 is held (Head of Helios, Bow of Apollo). Relative: the stick follows the movement and goes back to neutral when the mouse stops. Absolute: where the mouse was when L2 was pressed is the centre, and the stick points from it to the mouse. Also in the in-game menu.':
        'Membidik dengan mouse saat L2 ditahan (Head of Helios, Bow of Apollo). Relatif: stik mengikuti gerakan dan kembali ke netral saat mouse berhenti. Absolut: posisi mouse saat L2 ditekan menjadi titik tengah, dan stik mengarah darinya ke mouse. Juga ada di menu dalam game.',
    'Aiming with the mouse while L2 is held: lower needs more mouse movement for the same stick, higher less. Also in the in-game menu.':
        'Membidik dengan mouse saat L2 ditahan: nilai lebih rendah memerlukan lebih banyak gerakan mouse untuk pergerakan stik yang sama, lebih tinggi lebih sedikit. Juga ada di menu dalam game.',
    'Asks GameMode (gamemoderun) for the performance governor while the game runs.':
        'Meminta GameMode (gamemoderun) untuk pengatur performa saat game berjalan.',
    'Auto': 'Otomatis',
    'Automatic': 'Otomatis',
    'Automatic lets SDL choose. Pick X11 if the window misbehaves on Wayland (it runs through XWayland).':
        'Otomatis membiarkan SDL memilih. Pilih X11 jika jendela bermasalah di Wayland (berjalan melalui XWayland).',
    'Borderless': 'Tanpa batas',
    'Changes applied to the game code at start.': 'Perubahan yang diterapkan ke kode game saat mulai.',
    'Choose the folder of the update (eboot.bin, sce_sys)': 'Pilih folder pembaruan (eboot.bin, sce_sys)',
    'Choose the game folder': 'Pilih folder game',
    'Choose two separate folders for the game and its update, or leave the update folder empty.':
        'Pilih dua folder terpisah untuk game dan pembaruannya, atau biarkan folder pembaruan kosong.',
    'Choose your game folder to play.': 'Pilih folder game Anda untuk bermain.',
    "Click a box, then press a key, click a mouse button or turn the wheel (it gives up after {} seconds). An input already used by another button swaps with it; the arrow puts one button back on its default. Rage of Sparta presses L3 and R3 together: tick the box above it to use one input instead of two. Grab presses L1 and Circle together. Dodge evades the way you are moving (WASD), or back when standing still. Touchpad right opens the game's pause menu. Insert always opens the port's menu and cannot be bound. Applied when the game starts.":
        "Klik kotak, lalu tekan tombol, klik tombol mouse, atau putar roda mouse (waktu habis setelah {} detik). Input yang sudah digunakan oleh tombol lain akan ditukar; panah mengembalikan tombol ke default-nya. Rage of Sparta menekan L3 dan R3 bersamaan: centang kotak di atasnya untuk memakai satu input saja. Grab menekan L1 dan Lingkaran bersamaan. Dodge menghindar ke arah gerakan Anda (WASD), atau mundur jika diam. Touchpad kanan membuka menu jeda game. Insert selalu membuka menu port dan tidak dapat diubah. Diterapkan saat game dimulai.",
    'Close the game before restoring saves.': 'Tutup game sebelum memulihkan simpanan.',
    'Controls': 'Kontrol',
    'Copying the system files…': 'Menyalin file sistem…',
    'Could not copy the files: {}': 'Tidak dapat menyalin file: {}',
    'Could not restore: {}': 'Tidak dapat memulihkan: {}',
    'Could not save all settings. Check write permissions and free disk space, then try again.':
        'Tidak dapat menyimpan semua pengaturan. Periksa izin penulisan dan ruang disk yang kosong, lalu coba lagi.',
    'Creates spartanport.desktop in ~/.local/share/applications so the launcher and the game show their icon in the dock and the menu. Turning it off removes the entry.':
        'Membuat spartanport.desktop di ~/.local/share/applications agar launcher dan game menampilkan ikon di dock dan menu. Mematikannya akan menghapus entri ini.',
    'Delete': 'Hapus',
    'Diagnostics (for development)': 'Diagnostik (untuk pengembangan)',
    'Enable the upscaler (experimental)': 'Aktifkan upscaler (eksperimental)',
    'Experimental': 'Eksperimental',
    'F11 captures a frame for analysis': 'F11 menangkap satu frame untuk analisis',
    'Files': 'File',
    'Files layered over the game at start; the game files are never changed.':
        'File yang diterapkan di atas game saat mulai; file game tidak pernah diubah.',
    'GAME BUTTON': 'TOMBOL GAME',
    'GAMEPAD': 'GAMEPAD',
    'GPU wait and readback report in the log (every 5 s)':
        'Laporan tunggu dan readback GPU di log (setiap 5 detik)',
    'GameMode': 'GameMode',
    'Gamepad Mapping': 'Pemetaan Gamepad',
    'Gamepad names': 'Nama gamepad',
    'General': 'Umum',
    'Graphics (experimental)': 'Grafis (eksperimental)',
    'Home': 'Beranda',
    'How the stick-rotation prompts are answered. Keyboard: type the turn instead, half turns from the top D S A (clockwise) or A S D, from the bottom A W D or D W A, full turns D S A W or A S D W; a key out of turn starts again. Adaptive mouse: draw the turn with the mouse.':
        'Bagaimana petunjuk putaran stik dijawab. Keyboard: tekan arah putarannya, setengah putaran dari atas D S A (searah jarum jam) atau A S D, dari bawah A W D atau D W A, putaran penuh D S A W atau A S D W; tombol yang salah mengulang dari awal. Mouse adaptif: gambar putaran menggunakan mouse.',
    'In-game button icons': 'Ikon tombol dalam game',
    'Joystick prompts': 'Petunjuk joystick',
    'KEYBOARD': 'KEYBOARD',
    'Keep the mouse in the game window': 'Kunci mouse di dalam jendela game',
    'Key not supported': 'Tombol tidak didukung',
    'Keyboard': 'Keyboard',
    'Keyboard & Mouse Mapping': 'Pemetaan Keyboard & Mouse',
    'Keyboard (native)': 'Keyboard (asli)',
})

EXTRA['zh'].update({
    'A background thread reads the saved shaders into memory while the game starts; a note in the lower right corner shows the progress. Loading a shader then does not hold up a frame. Needs the option above. Switch it off on a slow hard drive or with little free RAM if the first minute stutters more or the game starts slower.': '游戏启动时，后台线程会将已保存的着色器读入内存，右下角显示进度。随后加载着色器时无需因磁盘读取而阻塞帧。需要启用上一项。如果硬盘较慢或可用内存不足，导致启动变慢或第一分钟更卡顿，请关闭此项。',
    'A note in the lower right corner shows the shader cache being read at start and the shaders being compiled, so a short stutter has a visible reason. Switch it off to keep the screen clean.': '右下角显示启动时的着色器缓存读取进度和着色器编译状态，便于了解短暂卡顿的原因。关闭此项可隐藏提示。',
    'Close the game before deleting the shader cache.': '删除着色器缓存前请先关闭游戏。',
    'Could not delete all of the shader cache; another program may be using it: {}': '无法删除全部着色器缓存，其他程序可能正在使用：{}',
    'Delete shader cache': '删除着色器缓存',
    'Delete the shader cache ({} MB in {})? The game compiles the shaders again, so the next start stutters for a few minutes.': '删除着色器缓存（{} MB，位于 {}）？游戏会重新编译着色器，下次启动后的几分钟可能出现卡顿。',
    'Deletes the saved shaders and pipelines of this saves folder. Use it if graphics look wrong after an update or the cache grew too big; the next start compiles everything again.': '删除此存档文件夹中保存的着色器和管线。如果更新后画面异常或缓存过大，可以使用此功能。下次启动时会重新编译。',
    'Read the shader cache into memory at start': '启动时将着色器缓存读入内存',
    'Reuse compiled shaders from disk': '重用磁盘中已编译的着色器',
    'Shader cache': '着色器缓存',
    'Shaders compiled in earlier sessions load from the shader cache instead of compiling again, so objects no longer appear a few frames late after starting or loading a save, and the cache is not rewritten every session (antivirus scans each written file). The first launch with it on rebuilds the cache once. Switch it off if an object, effect or color looks wrong only after a restart and is fine on the first run of a new cache.': '从缓存加载先前会话编译的着色器，避免重新编译，减少启动游戏或加载存档后物体延迟出现，并避免每次重写缓存（杀毒软件会扫描每个写入的文件）。首次启用时会重建一次缓存。如果仅在重启后出现物体、特效或颜色异常，而新缓存首次运行正常，请关闭此项。',
    'Show shader notes in the corner': '在角落显示着色器提示',
    'The shader cache is already empty.': '着色器缓存已经为空。',
})

EXTRA['id'].update({
    'A background thread reads the saved shaders into memory while the game starts; a note in the lower right corner shows the progress. Loading a shader then does not hold up a frame. Needs the option above. Switch it off on a slow hard drive or with little free RAM if the first minute stutters more or the game starts slower.': 'Thread latar belakang membaca shader tersimpan ke memori saat game dimulai; kemajuannya ditampilkan di sudut kanan bawah. Pemuatan shader berikutnya tidak perlu menunggu pembacaan disk. Memerlukan opsi di atas. Nonaktifkan jika hard disk lambat atau RAM terbatas membuat menit pertama lebih tersendat atau waktu mulai lebih lama.',
    'A note in the lower right corner shows the shader cache being read at start and the shaders being compiled, so a short stutter has a visible reason. Switch it off to keep the screen clean.': 'Catatan di sudut kanan bawah menampilkan pembacaan cache saat mulai dan kompilasi shader, agar penyebab jeda singkat terlihat. Nonaktifkan untuk menyembunyikannya.',
    'Close the game before deleting the shader cache.': 'Tutup game sebelum menghapus cache shader.',
    'Could not delete all of the shader cache; another program may be using it: {}': 'Tidak dapat menghapus seluruh cache shader; program lain mungkin sedang menggunakannya: {}',
    'Delete shader cache': 'Hapus cache shader',
    'Delete the shader cache ({} MB in {})? The game compiles the shaders again, so the next start stutters for a few minutes.': 'Hapus cache shader ({} MB di {})? Game akan mengompilasi ulang shader, sehingga beberapa menit setelah mulai berikutnya dapat tersendat.',
    'Deletes the saved shaders and pipelines of this saves folder. Use it if graphics look wrong after an update or the cache grew too big; the next start compiles everything again.': 'Menghapus shader dan pipeline tersimpan dari folder penyimpanan ini. Gunakan jika grafik bermasalah setelah pembaruan atau cache terlalu besar; saat mulai berikutnya semuanya dikompilasi ulang.',
    'Read the shader cache into memory at start': 'Baca cache shader ke memori saat mulai',
    'Reuse compiled shaders from disk': 'Gunakan kembali shader terkompilasi dari disk',
    'Shader cache': 'Cache shader',
    'Shaders compiled in earlier sessions load from the shader cache instead of compiling again, so objects no longer appear a few frames late after starting or loading a save, and the cache is not rewritten every session (antivirus scans each written file). The first launch with it on rebuilds the cache once. Switch it off if an object, effect or color looks wrong only after a restart and is fine on the first run of a new cache.': 'Shader dari sesi sebelumnya dimuat dari cache tanpa dikompilasi ulang, sehingga objek tidak terlambat muncul setelah game dimulai atau penyimpanan dimuat, dan cache tidak ditulis ulang setiap sesi (antivirus memindai setiap berkas yang ditulis). Saat pertama diaktifkan, cache dibangun ulang sekali. Nonaktifkan jika objek, efek, atau warna hanya bermasalah setelah mulai ulang tetapi normal saat cache baru pertama digunakan.',
    'Show shader notes in the corner': 'Tampilkan catatan shader di sudut',
    'The launcher cannot write to {}, so it cannot replace itself there. Open the releases page to download the new version by hand?': 'Launcher tidak dapat menulis ke {}, sehingga tidak dapat mengganti dirinya di sana. Buka halaman rilis untuk mengunduh versi baru secara manual?',
    'The shader cache is already empty.': 'Cache shader sudah kosong.',
    'Version {} is out, but it has no Linux build yet.': 'Versi {} telah tersedia, tetapi belum memiliki build Linux.',
})

_tables = {}


def table(language):
    """English text -> translation for one language ({} for English and Russian)."""
    if language not in _tables:
        texts = TRANSLATIONS.get(language)
        if texts and len(texts) != len(KEYS):
            raise ValueError(f'launcher translation {language}: {len(texts)} texts for {len(KEYS)} keys')
        _tables[language] = {**(dict(zip(KEYS, texts)) if texts else {}), **EXTRA.get(language, {})}
    return _tables[language]
