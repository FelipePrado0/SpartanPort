# Trofeos

[English](TROPHIES.md) | [Português](TROPHIES.pt-BR.md) | **Español**

El juego llama a la biblioteca de trofeos de PlayStation (`sceNpTrophy*`). Este port responde a esas
llamadas, guarda en el disco los trofeos desbloqueados y se los muestra al jugador: una notificación
cuando se desbloquea un trofeo, una ventana de trofeos dentro del juego y una página **Trophies** en
el launcher.

Todo se lee de la carpeta del juego del propio jugador (`sce_sys/trophy/trophy00.trp`); ningún dato
del juego forma parte del port.

**Índice:** [Lo que ve el jugador](#lo-que-ve-el-jugador) · [Orden y rareza](#orden-y-rareza) ·
[Notificaciones](#notificaciones) · [Idioma](#idioma) · [Estado guardado](#estado-guardado) ·
[Ids de los trofeos](#ids-de-los-trofeos) · [Qué hay en este repositorio](#qué-hay-en-este-repositorio-y-qué-no) ·
[Cómo funciona](#cómo-funciona) · [Límites](#límites)

## Lo que ve el jugador

| Dónde | Qué |
|-------|-----|
| En el juego, al desbloquear un trofeo | Una notificación en el tema elegido en la ventana de trofeos (abajo). El predeterminado, **Original**, es una tarjeta arriba a la derecha (debajo del contador de FPS cuando está activo) con el icono, el nombre y la descripción del trofeo; entra deslizándose, se queda 5 segundos y se desvanece. |
| En el juego, **F8** o **R3+R2** | La ventana de trofeos, en dos páginas. **Trofeos**: todos los trofeos con su icono, una copa del color de su grado, su nombre, qué hacer para desbloquearlo, su estado y cuántos jugadores lo consiguieron; un trofeo desbloqueado lleva el marco del color de su grado (bronce, plata, oro y un platino que brilla). Encima de la lista: el recuento, un filtro por estado (todos / desbloqueados / bloqueados), otro por grado (todos / platino / oro / plata / bronce, con cuántos de cada uno están desbloqueados; los dos se combinan) y el orden. **Notificaciones**: el idioma, y el aspecto y el sonido de la notificación, con vistas previas. |
| Launcher, página **Trophies** | La misma lista, con la categoría (platino, oro, plata, bronce) y un filtro. |

Teclas de la página **Trofeos**: Arriba/Abajo, W/S, RePág/AvPág, Inicio/Fin, la rueda del ratón o la
cruceta desplazan; Izquierda/Derecha, A/D o la cruceta Izquierda/Derecha cambian el filtro de
estado; Cuadrado o G, el de grado; Triángulo o Tab cambian el orden; L1/R1 o Q/E van a la página
**Notificaciones**; F8, Esc, Retroceso, Enter, Círculo/Equis o R3+R2 la cierran.
La página **Notificaciones** se usa como el menú de Insert (cruceta o flechas, Equis o Enter, el
ratón); L1/R1 o Q/E vuelven a la lista; F8, Esc o R3+R2 la cierran. Con la ventana abierta el juego
no recibe controles (como en el menú de Insert), y el juego se detiene hasta que se cierra, como
con el menú de Insert: Juego > **Pausar el juego** (activado por defecto) lo desactiva. R3
pulsado con R2 o L2 no llega al juego, así que abrir la ventana o el menú no usa el objeto ni la
magia.

El platino (id 0) se desbloquea con el último de los demás trofeos, como en PlayStation.

## Orden y rareza

Cada trofeo muestra la parte de jugadores que lo consiguió y su nivel de rareza, como lo muestra
PlayStation: **Común** (más del 50%), **Raro** (más del 15%), **Muy raro** (más del 5%) y
**Ultrarraro**. El orden es la lista del juego, **Más obtenidos primero** o **Más raros primero**.

El port no se conecta a la PSN, así que los números son fijos: la parte de los jugadores que sigue
[Exophase](https://www.exophase.com/game/god-of-war-iii-remastered-ps4/trophies/) que consiguió cada
trofeo, según su lista de octubre de 2026 (`gpu/shim/spartanport_trophy_rarity.h`). Solo se muestran
para la lista de 35 trofeos de este juego; otro paquete conserva el orden de la lista y no muestra
porcentajes.

## Notificaciones

La página **Notificaciones** de la ventana de trofeos (F8 o R3+R2, luego L1/R1) elige cómo se ve y
suena un desbloqueo. Nada cambia para quien no la abra: la notificación **Original** sigue siendo la
predeterminada, como era, y no suena nada sin un archivo de sonido.

| Opción | Qué hace |
|--------|----------|
| Tema | **Original** (la tarjeta del port, arriba), **Tarjeta** (una tarjeta oscura arriba a la derecha con el arte del trofeo, la copa del grado y una insignia animada; el platino recibe una breve animación dibujada), **Tarjeta, platino cinematográfico** (el platino se queda más tiempo, con luz alrededor) o **Ventana en la esquina** (un rectángulo que sube en la esquina inferior derecha con el arte enmarcado en el color del grado, el nombre y lo que pide el trofeo). |
| Silenciar, Volumen | Solo el sonido de la notificación; el sonido del juego no se toca. |
| Tiempo en pantalla | De 0 a 20 segundos; "Predeterminado del tema" conserva el tiempo del tema. |
| Brillo del platino | Solo en Tarjeta, platino cinematográfico: la intensidad de su luz. |
| Animar el platino | Desactivado: el platino muestra su propia imagen, como los demás trofeos. |
| Escribir el grado | Escribe el nombre del grado (Bronce, Plata...) antes de la segunda línea. |
| Vista previa | Bronce, Plata, Oro, Platino, los cuatro seguidos o tres a la vez, por la misma cola, dibujo y sonido de un desbloqueo real. No se desbloquea ni se guarda nada. |

El grado de cada trofeo se deduce del color de su icono, como hace el launcher (los grados del
paquete están cifrados); para los 35 trofeos de este juego coincide con la lista del propio paquete.
La página está en el idioma del menú (abajo).

### Tus propios archivos

Ponlos en `user/custom_trophy` (dentro de tu carpeta de partidas guardadas, así las actualizaciones
los conservan). Ninguno forma parte del port: sin ellos se usan el arte dibujado, las fuentes
integradas y ningún sonido.

| Archivo | Efecto |
|---------|--------|
| `bronze`, `silver`, `gold`, `platinum` `.wav .mp3 .ogg .flac .m4a .opus` | El sonido de ese grado (20 segundos como máximo). `trophy.*` sirve para todo grado sin sonido propio. El dispositivo de audio solo se abre cuando hay un archivo de sonido, y nunca con `SPARTAN_AUDIO=none`. **Recargar sonidos** los vuelve a leer mientras juegas. |
| `<grado>_anim.gif`, `.apng`, `.png`, `.webp`, `.jpg` | Reemplaza el arte de la notificación de ese grado (Tarjeta y Ventana en la esquina). Un GIF o un PNG animado se reproduce una vez con su propio tiempo; una imagen fija se muestra tal cual. `trophy_anim.*` sirve para todo grado sin archivo propio. |
| `<grado>_anim.png` + `<grado>_anim.ini` | Una hoja de fotogramas: `frames=`, `columns=`, `fps=`, `loop=1` para repetir, `delay_ms=` para empezar más tarde. |
| `badge.png` | El disco a la derecha de la Tarjeta. |
| `font.ttf`, `font-light.ttf` | Las fuentes de la Tarjeta (el nombre, la segunda línea). |
| `notifications.ini` | Las opciones de arriba, escritas por la página. También guarda los textos, en cualquier idioma: `text_unlocked=` (la segunda línea de la Tarjeta), `text_banner=` (la primera línea de la tarjeta Original), `name_bronze=`, `name_silver=`, `name_gold=`, `name_platinum=`. |

Las imágenes y animaciones se leen al iniciar el juego. Más de 8192 píxeles de lado, más de 20
segundos o 600 fotogramas, o más de 64 MB de fotogramas decodificados se rechazan o se recortan. Los
archivos se abren solo como archivos locales: un "sonido" que en realidad es una lista de
reproducción o un enlace no se sigue. Un archivo dañado o ausente se omite y se anota en el registro;
el juego nunca se detiene por él.

## Idioma

El launcher escribe `user/trophy_texts.txt` cada vez que inicia el juego, en el idioma del launcher,
y lo mismo en todos los idiomas en `user/trophy_texts/<idioma>.txt`. El juego lee el primero al
iniciar. **Idioma**, arriba de la pestaña **Juego** del menú de Insert y de la página
**Notificaciones** de la ventana de trofeos, cambia al instante el menú, la ventana de trofeos y los
nombres de los trofeos, leyendo el segundo; la elección queda en `spartanport.ini` (`ui_language`), e
**Igual que el launcher** vuelve a seguir el launcher.

El menú y la ventana de trofeos se dibujan en inglés, portugués, español, francés, alemán, italiano,
polaco, turco, indonesio, ruso y chino (el chino con una fuente CJK instalada, consulta el README).
Árabe, japonés y coreano los dejan en inglés, porque la fuente del overlay no dibuja esas escrituras;
los nombres de los trofeos siguen llegando en esos idiomas en el launcher.

Idiomas: inglés, ruso, árabe, español, portugués, francés, alemán, italiano, polaco, turco, chino,
japonés y coreano (`launcher-rust/assets/i18n.json`, generado a partir de
`launcher/spartanport_trophies.py` por `tools/launcher_i18n.py`). Los nombres y descripciones son los del
propio juego, como los muestra la lista de trofeos de PS4 en cada idioma, así que un trofeo se puede
buscar por el nombre que le da el juego: inglés, portugués (Brasil), español (la lista de
Latinoamérica), francés, alemán, italiano, polaco, turco, ruso, japonés y coreano. **Árabe y chino
simplificado son traducciones**: el juego no tiene lista en esos idiomas. Un idioma sin lista usa
inglés.

Sin el launcher, pon el mismo archivo a mano en la carpeta de partidas (`user/` o
`SPARTAN_USER_DIR`): `trophy_texts.txt`, o `trophy_names.txt` (mismo formato, se usa cuando no hay
`trophy_texts.txt`).

### Formato del archivo

```
# comentario
@heading=Trophies
@notice=TROPHY UNLOCKED
@unlocked=UNLOCKED
@locked=LOCKED
@all=All trophies
@only_unlocked=Unlocked
@only_locked=Locked
@counter={} of {} unlocked
@hint=Up/Down: scroll   Left/Right: filter   F8 / R3+R2 / Esc: close
@notifications=Notifications
@order_list=List order
@order_common=Most earned first
@order_rare=Rarest first
@rarity={}% of players
@common=Common
@rare=Rare
@very_rare=Very rare
@ultra_rare=Ultra rare
@hint_order=Triangle/Tab: order   L1/R1: notifications
@hint_notifications=L1/R1: trophies   F8 / R3+R2 / Esc: close
0=King of the Hill | Unlock all trophies
1=Releasing the Floodgates | Kill Poseidon
```

`ID=Nombre | Descripción` (la descripción es opcional), UTF-8, un trofeo por línea. Un nombre ocupa
hasta 63 bytes y una descripción hasta 159. Un trofeo sin línea se muestra como `Trophy ID N`. Las
líneas `@` que faltan usan los rótulos en inglés.

## Estado guardado

`user/trophies.dat`: 16 bytes, cuatro palabras de 32 bits little-endian. El trofeo `id` es el bit
`id % 32` de la palabra `id / 32`. Borra el archivo para reiniciar los trofeos. Está separado de las
partidas guardadas del propio juego.

## Ids de los trofeos

Los ids son los números de los nombres de los iconos del paquete (`TROP000.PNG` ... `TROP034.PNG`):
35 trofeos, id 0 el platino, 1-18 bronce, 19-29 plata, 30-34 oro (por el color de los iconos). Los
textos siguen el orden de la lista de PlayStation, que es el mismo.

## Qué hay en este repositorio, y qué no

Muchos jugadores preguntan qué se puede compartir. Las funciones de trofeos están hechas para que
nada en este repositorio pertenezca a Sony ni a los creadores del juego:

- **Ningún archivo del juego.** Los iconos, el paquete de trofeos y el juego se leen de la copia del
  propio jugador durante la ejecución; ninguno se copia al port.
- **Ningún logotipo, sonido ni imagen de PlayStation.** El arte de la notificación (copas, insignia,
  la animación del platino) se dibuja con formas en el código. El aspecto de la Tarjeta es propio del
  port; solo sigue la disposición habitual de una notificación (arte a la izquierda, dos líneas de
  texto, una insignia).
- **Los textos** son los nombres y descripciones de la lista pública de trofeos del juego, las líneas
  cortas que muestra cualquier sitio de trofeos y que el port ya incluía en inglés y portugués. Son lo
  que permite encontrar un trofeo por su nombre; no se incluye ningún otro texto del juego.
- **Los números de rareza** son estadísticas (una parte de jugadores), con su fuente citada arriba.
  Un número sobre cuántas personas hicieron algo es un hecho, no una obra.
- **Tus propios archivos** en `user/custom_trophy` (un sonido que te guste, una imagen, un logotipo)
  se quedan en tu equipo. No forman parte del port, y no deberías subirlos a este repositorio ni
  compartirlos si pertenecen a otra persona.

Esto no es asesoramiento legal; es como el port mantiene limpio su propio repositorio.

## Cómo funciona

- `src/runtime_services.c`: las funciones `sceNpTrophy*`. En `RegisterContext` lee `trophy00.trp`
  (cabecera big endian: magic `DC A2 4D 00`, número de archivos en `0x10`, tamaño de entrada en
  `0x14`, entradas desde `0x60`: nombre, offset, tamaño) y el archivo de textos. `UnlockTrophy` marca
  el bit, guarda el estado y muestra la notificación; el platino se concede cuando todos los demás
  están marcados. `GetTrophyUnlockState` devuelve los bits guardados y el número de trofeos.
- `gpu/spartanportgpu.h`: `spartanportgpu_toast_trophy` (la notificación, con el id del trofeo) y
  `spartanportgpu_trophy_setup` (la ventana: textos, iconos y un puntero al estado en vivo),
  entregados una vez por el runtime.
- `gpu/shim/spartanport_overlay.cpp`: dibuja la notificación Original y la ventana de trofeos con Dear
  ImGui, decodifica los iconos con `SpartanPng` (en un hilo aparte), deduce el grado de cada trofeo
  por su icono y atiende las teclas y botones de la ventana.
- `gpu/shim/spartanport_trophy_toast.{h,cpp}`: los demás temas: la cola, el dibujo y un hilo para los
  sonidos (un flujo de audio SDL propio, junto al del juego).
- `gpu/shim/spartanport_trophy_toast_model.{h,cpp}`: `notifications.ini`, los ajustes de animación,
  la línea de tiempo y el grado por color. Sin ventana, GPU ni audio, por eso tiene pruebas.
- `gpu/shim/spartanport_trophy_media.{h,cpp}`: los sonidos e imágenes del jugador decodificados con
  FFmpeg, con los límites de arriba.
- `gpu/shim/spartanport_trophy_rarity.h`: la parte de jugadores de cada trofeo.
- `gpu/shim/spartanport_trophy_texts.{h,cpp}`: lee `user/trophy_texts/<idioma>.txt` para el idioma
  elegido en el juego.
- `gpu/shim/spartanport_ui_language.h`, `gpu/shim/ui_language/`: los textos del menú y de la ventana
  de trofeos en cada idioma, con el texto en inglés como clave.
- `gpu/tools/overlay_preview.cpp` (`overlay-preview`): dibuja el menú, la ventana de trofeos y las
  notificaciones fuera del juego, con trofeos inventados, y la lista de partidas guardadas del
  juego, con partidas inventadas, para trabajar en su aspecto sin el juego.
- `launcher-rust/assets/i18n.json`: los textos y etiquetas en todos los idiomas, generados a partir
  de `launcher/spartanport_trophies.py` y `launcher/spartanport_lang.py` por `tools/launcher_i18n.py`.
- `launcher-rust/src/core/trophies.rs`: el lector del `.trp` y de `trophies.dat`, y la parte de
  jugadores (la misma tabla del juego).
- `launcher-rust/src/core/env.rs`: `write_trophy_texts`, los archivos que lee el juego.
- `launcher-rust/src/ui/pages/trophies.rs`: la página Trofeos del launcher, con el mismo orden y
  rareza que la ventana del juego.
- `tests/test_trophies.py`, `tests/test_trophy_toast.cpp`, `tests/test_ui_language.cpp`,
  `tests/test_services.c`: pruebas unitarias. Crean sus propios archivos y no necesitan archivos del
  juego; `trophy-toast-test CARPETA_TEMP trophy00.trp` también comprueba los grados de los iconos del
  propio juego.

## Límites

- Solo se conocen los trofeos desbloqueados con este port. Los trofeos obtenidos en una PlayStation
  no se importan, y con una partida ya avanzada el juego normalmente no vuelve a desbloquear lo que
  ya había hecho.
- Los nombres y descripciones del paquete están cifrados (`TROP*.ESFM`) y no se leen: los textos de
  arriba vienen integrados en el launcher.
- No hay sincronización de trofeos con la PSN ni perfil en línea. La rareza es una instantánea fija y
  no cambia mientras juegas.
- R3+R2 puede pulsarse sin querer en combate; el menú de Insert usa R3+L2 por el mismo motivo.
