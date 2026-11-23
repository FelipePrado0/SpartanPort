[English](CONTROLS.md) | [Português (Brasil)](CONTROLS.pt-BR.md) | **Español**

# Controles

Las teclas por defecto están en el [README](../README.es.md#controles). Esta página explica las
opciones de la página **Controls** del launcher.

## Teclado y mouse

**Rage of Sparta en una tecla** (activado por defecto) deja Z y X sin uso. Los **Joystick prompts**
responden a los quick-time events de girar el stick sin stick: escribí el giro (medias vueltas D S
A, A S D, A W D o D W A; vueltas completas D S A W o A S D W) o, con *Adaptive mouse*, dibujalo con
el mouse. Mientras jugás, el mouse queda dentro de la ventana del juego; el menú del juego, la tecla
de pausa (Esc) y otras ventanas lo liberan, y Esc de nuevo o un clic en la ventana lo vuelve a
tomar. Con L2 apretado (R), el mouse apunta con el stick derecho, para la Cabeza de Helios o el Arco
de Apolo: *Relative* sigue el movimiento, *Absolute* apunta desde donde estaba el mouse al apretar
L2, y *Right stick sensitivity* define cuánto mouse hace falta. Sin el launcher, el juego usa sus
propias teclas: E y Q para Cuadrado y Triángulo, 1 y 3 para L1 y R1, F para R2, C para R3, I, K, J,
L para la cruceta, Enter para Options, Tab y Retroceso para el touchpad.

## Teclas alternativas

Cada botón puede tener una segunda y una tercera tecla o entrada del mouse, en la segunda y la
tercera casilla de su fila en la página Controls (el botón x al lado de una casilla la borra).
Todas presionan el botón, así que podés dejar el Cuadrado en el clic izquierdo para el combate y
presionarlo también con la J, o con el Keypad 4, en los minijuegos. En la lira de las Musas, por
ejemplo, dejá Espacio, Shift izquierdo y los dos botones del mouse y agregá K, L, J e I, dispuestas
como los cuatro botones (I arriba, J a la izquierda, L a la derecha, K abajo). Una tecla que ya usa
otro botón se intercambia con él, como en las teclas principales. Los íconos de teclado en el juego
muestran la tecla principal; *Keys on the icons* (página Controls, con el estilo Keyboard o Match
my controller) puede mostrar la alternativa, o la alternativa solo en la lira (los demás íconos
conservan la principal). Los íconos dibujan la primera alternativa definida: la segunda casilla, o
la tercera si la segunda está vacía. Sin el launcher, uní los nombres con `|` en `SPARTAN_KEY_MAP`,
por ejemplo `cross=Space|K|Keypad 2,square=Mouse Left|J`.

## Gamepads

En un control sin touchpad (Xbox y otros), View abre el menú de pausa (el clic del touchpad derecho)
y L1 + View abre el modo foto (el clic izquierdo). En un DualSense el touchpad funciona como en la
PS4, y Share/Create funciona como View. Detalles del DualSense: [DUALSENSE.md](DUALSENSE.md) (en
inglés).

En el menú del port: L1/R1 o Q/E cambian de pestaña. En la lista de partidas: las flechas o la
cruceta eligen, Enter o Cruz confirman, Esc o Círculo cancelan.

### Controles USB genéricos

En **Controls → Set up a controller**, seleccioná el joystick y elegí **Configure** junto a
cualquier botón, dirección de la cruceta o eje de stick. También aparecen los joysticks sin un
mapeo de gamepad de SDL. Cada entrada detectada se guarda al instante; podés empezar por cualquier
fila y corregir un solo control después. Los mapeos existentes se muestran al volver a abrir el
editor. **Clear** borra solo esa fila, y **Cancel capture** deja la edición pendiente sin cambios y
conserva lo ya guardado. La vista previa en vivo muestra el resultado en cualquier momento. Cerrá
la ventana al terminar.

Los perfiles se guardan por GUID de dispositivo de SDL y sistema operativo en `gamecontrollerdb.txt`
en la carpeta de datos del port. Se cargan en el próximo inicio del juego, incluso con `--play` y
accesos directos. Los mapeos explícitos en `SDL_GAMECONTROLLERCONFIG` y
`SDL_GAMECONTROLLERCONFIG_FILE` tienen prioridad. El remapeo de botones del juego sigue aplicándose;
usá **Reset to defaults** si querés que los botones coincidan con la configuración guiada. SDL tiene
que poder leer el joystick; el mapeador no instala drivers.

## Ataques de cambio

**Ataques de cambio con L1 + cruceta y L1 + Cruz** (desactivado por defecto, página Controls): L1 +
una dirección de la cruceta cambia a esa arma con su ataque de cambio, y el arma que Kratos ya tiene
en la mano repite su ataque; L1 + Cruz hace el ataque de cambio del arma en la mano en lugar de
pasar a la siguiente arma. El ataque de cambio de la misma arma otra vez espera 0,8 segundos después
del último (el de otra arma no espera). La cruceta sola cambia como antes. Los tutoriales de cambio
de arma lo dicen así en todos los idiomas (en copias de los archivos de idioma en la carpeta de
datos del port). Sin el launcher, definí `SPARTAN_SWITCH_ATTACKS=1`. Cómo elige el juego los
movimientos de Kratos, para otros cambios: [moves.md](moves.md) (en inglés).

## Cámara libre (experimental)

Desactivada por defecto. Activala con *Free camera (experimental)* en la página Controls del
launcher, en la sección *Controller* del menú del port (Insert) o en Opciones → Controles del juego.
Así el stick derecho, o el mouse mientras el juego lo retiene, gira la cámara alrededor de Kratos
por completo y por encima de él, y la cámara mantiene ese ángulo mientras él se mueve; el mouse
sigue la *Right stick sensitivity*, y con L2 presionado apunta como antes. Kratos se mueve hacia
donde mira la cámara, y L1 + stick derecho esquiva.

En el juego, G (remapeable) o un toque solo de R3 la desactiva y la activa: desactivada, es la
cámara del propio juego, para las peleas contra jefes y las escenas guionadas. El juego retoma la
cámara solo en sus cortes de cámara, los quick-time events, la pantalla de título y el menú de
pausa. La cámara se detiene antes de paredes y rocas (la colisión del propio nivel, no las paredes
invisibles al borde de los precipicios), sube por encima de Kratos cuando está contra una pared y
no baja por debajo de sus pies. Muchos lugares se hicieron solo para la cámara del juego, así que
otros ángulos pueden mostrar partes sin terminar del escenario. Sin el launcher, definí
`SPARTAN_FREE_CAMERA=1`; `SPARTAN_FREE_CAMERA_LIMITS=yaw,pitch` (grados) limita cuánto gira.

## Íconos de botones en el juego

Desactivados por defecto. Se eligen en la página Controls del launcher, en *In-game button icons*:
*Match my controller*, *Xbox*, *Nintendo* o *Keyboard*. Reemplazan los íconos de PlayStation en los
tutoriales, la lista de movimientos, las indicaciones de quick-time events y en la lira de las Musas
en la Cámara de la Llama; el dibujo del control en el menú de pausa sigue siendo un DualShock 4.
*Keyboard* muestra la tecla que presiona cada botón, también en la lira, según tu mapeo del teclado
(una tecla cambiada aparece en el próximo inicio); *Match my controller* también muestra las teclas
cuando no hay un control conectado.

*Key colors* (en el mismo lugar) pinta las teclas de X, Círculo, Cuadrado y Triángulo, en la lira y
en los demás íconos de teclado: *PlayStation colors*, o *Custom* con un color de una paleta corta
para cada botón; los demás íconos y el *Gray* por defecto no cambian.

El launcher escribe una copia parcheada de hasta tres archivos del juego (unos 410 MB) en la carpeta
de datos del port y nunca modifica tu carpeta del juego. Un control nuevo se detecta después de
reiniciar. Sin el launcher, definí `SPARTAN_GLYPH_STYLE` como `auto`, `xbox`, `nintendo` o
`keyboard` antes de iniciar `run.sh`.
