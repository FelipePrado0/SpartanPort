# Ultrawide (21:9 and 32:9) mod — beta

**English** | [Português (Brasil)](#português-brasil)

The ultrawide support is a **mod**: the folder [`extras/mods/Ultrawide`](../extras/mods/Ultrawide)
(install steps in its README). It uses the patch support of mods: a mod folder can carry game patches
(XML files with signatures, in its `patches/` folder), applied while the mod is ticked on the Mods
page. See [MODS.md](MODS.md).

The game's camera has a fixed *horizontal* field of view. Raising the render resolution to an ultrawide
size without touching the camera only crops the 16:9 picture top and bottom, so it looks zoomed in.
Each `Resolution Patch - <width>x<height>` entry of the mod fixes that: the picture fills the whole screen,
the world keeps its proportions, and the vertical view is the same as at 16:9.

| Render resolution | Aspect | Status |
|---|---|---|
| 3440x1440 | 21:9 | **Tested in game** (3440x1440 monitor, build `6e72330e…`) |
| 2560x1080 | 21:9 | **Not tested** |
| 3840x1600 | 21:9 | **Not tested** |
| 5120x1440 | 32:9 | **Works** (tester report, gameplay) |
| 1920x1200 | 16:10 | **Works** (tester report, with screenshots; taller than 16:9: no field of view change) |
| 2560x1600 | 16:10 | **Works** (tester report, gameplay; taller than 16:9: no field of view change) |

The author only has a 3440x1440 monitor; 5120x1440, 1920x1200 and 2560x1600 were tested by other players in
issue #180 (thank you!). 2560x1080 and 3840x1600 come from the same code but nobody has reported them yet.
**If you try one, please say how it looks** (an issue):
the resolution, what you see, and whether anything is cropped, stretched or missing at the edges.

### Known limitations (outside 16:9)

Reported by testers; they come from the game, which is built for 16:9:

- **Startup logo screens** are stretched to the whole screen.
- **Pre-rendered videos** (cutscenes, intros) keep their native 16:9 size, with black bars at the sides. The
  game picture can show around them in some builds; the test build by devLucasEmiliano clears it to black.
- **Menus** show a slightly darker box over the original 16:9 area, and on the start menu Kratos is half
  covered by the fade. That fade is drawn by the game to delimit its 16:9 screen; it is only cosmetic and
  there is no setting to remove it.
- Gameplay and HUD are fine on every size reported so far.

## Using it

Put `Ultrawide.zip` (made with `python tools/zip_mod.py extras/mods/Ultrawide`) or the `Ultrawide` folder in
the mods folder; a zip is unpacked by itself when the launcher or the game starts. The mod's resolutions then show up in **Render
resolution** (launcher, **Home** and **Performance**) and in the in-game menu (Insert →
Performance → Render resolution), after the built-in ones, as `3440x1440 (mod)` and so on. Pick one
and **PLAY** (in the game menu: "Apply and restart"). The **Engine frame rate** (60, 120 or 240) is a
separate choice that works with any resolution. Untick the mod to go back to the built-in sizes.

What you get:

- The world and the HUD fill the screen; characters keep their proportions. Menus are in the right proportion.
- **Cutscenes keep their borders.** They are pre-rendered 16:9 videos, so they play with black bars at
  the sides (above and below at 16:10). Stretching them to fill the screen would distort the picture, and
  they cannot be re-rendered. The game draws a cutscene in a 16:9 picture in the middle and never clears the
  rest of the screen (at 16:9 there is no rest), so the last game frame used to stay in the bars. While the
  video decoder hands out pictures, the presenter now draws everything outside that picture black
  (`video_core/renderer_vulkan/movie_bars.h`; render sizes within 1% of 16:9 are left alone).
- **Menus are 16:9.** The game fits its menus and HUD to a 16:9 area in the middle, so the darkened
  background of a menu does not reach the sides (the top and bottom at 16:10).
- At 32:9 the field of view is multiplied by 2 horizontally. It is rectilinear, so objects at the very
  edges look stretched, as in any game with a very wide field of view.

## 16:10 (taller than 16:9)

The camera keeps the horizontal view, so on a taller screen the picture simply shows more at the top and
bottom, without distortion. The 16:10 entries therefore only set the resolution and the aspect ratio
(`k = 1`: no field of view change, no caves). 4:3 is not included.

## What it changes (wider than 16:9)

1. The render resolution, as the built-in `Resolution Patch` entries do, and the Video Arena size.
2. The display aspect ratio, two floats in the video mode (`16/9` → `width/height`).
3. The tangent `t` of the field of view, multiplied by `k = (width/height) / (16/9)` (43/32 at
   3440x1440), in the four places the cameras read it. The projection is `x = 2/t`, `y = 2A/t`
   (`A` = aspect ratio), so with the new `A` and `t·k` the vertical view stays the one of a 16:9 picture and
   the horizontal one widens. The same `t` feeds the culling vectors (two builders) and the standard
   camera's `tan(fov/2)`, so nothing is culled at the sides.

There is no room for a multiplication in place. Each of the four sites becomes a `jmp` into two chained
15-byte NOP pads between functions ("caves": the first runs the replaced instruction, the second
multiplies by `k` and jumps back), and a ninth pad holds the constant `k`. Every site and pad is found
by a signature (`Pattern`/`At`) like the built-in patches; a build that does not match skips the patch.

## Other resolutions

`tools/make_ultrawide_patch.py` writes the entries of the mod (it needs the 6e72 `eboot.bin` and capstone):

```
python tools/make_ultrawide_patch.py path/to/eboot.bin 3840 1080 --mod extras/mods/Ultrawide/patches/Ultrawide.xml
```

The two numbers are the width and the height. A new size is marked "not tested" in the patch note until it
is added to `TESTED_IN_GAME` in the tool. Nothing else needs to change: the mod's resolutions are listed
by the launcher and the game menu from its patch file.

## Reference build addresses

The `Address` attributes of the mod's lines are addresses in the `6e72…` build (the author did not have
the `d85c8135…` build). The signatures are what is used. To turn them into the reference build's
addresses, run with that build's `eboot.bin`, once per size:

```
python tools/make_ultrawide_patch.py --rebase path/to/eboot_d85c.bin 3440 1440 --mod extras/mods/Ultrawide/patches/Ultrawide.xml
```

---

## Português (Brasil)

O suporte ultrawide é um **mod**: a pasta [`extras/mods/Ultrawide`](../extras/mods/Ultrawide) (o passo a
passo está no README dela). Ele usa o suporte a patches dos mods: uma pasta de mod pode trazer patches de
jogo (arquivos XML com assinaturas, na pasta `patches/`), aplicados enquanto o mod está marcado na página
Mods. Veja [MODS.md](MODS.md).

A câmera do jogo tem campo de visão *horizontal* fixo. Aumentar a resolução para um tamanho ultrawide sem
mexer na câmera só corta a imagem de 16:9 em cima e embaixo, e ela parece com zoom. Cada entrada
`Resolution Patch - <largura>x<altura>` do mod corrige isso: a imagem enche a tela, o mundo mantém as
proporções e a visão vertical é a mesma do 16:9.

O **3440x1440** foi testado por quem fez (só tem esse monitor); 5120x1440, 1920x1200 e 2560x1600 foram testados por
outros jogadores na issue #180 e funcionam. 2560x1080 e 3840x1600 usam o mesmo código e ninguém relatou ainda; se
testar, conte como ficou numa issue. Os tamanhos 16:10 (1920x1200 e 2560x1600): como a câmera mantém o campo horizontal, uma imagem mais alta só
mostra mais em cima e embaixo, então essas entradas mudam apenas a resolução e o aspect ratio.

**Como usar:** coloque o `Ultrawide.zip` (gerado com `python tools/zip_mod.py extras/mods/Ultrawide`) ou a pasta
`Ultrawide` na pasta de mods; o zip é extraído sozinho ao abrir o launcher ou o jogo. As resoluções do mod aparecem em **Render
resolution** (launcher e menu do jogo, tecla Insert) depois das embutidas, como `3440x1440 (mod)`. Escolha uma
e **PLAY**. O **Engine frame rate** (60, 120 ou 240) é uma escolha à parte e vale com qualquer resolução.

**As cutscenes continuam com bordas:** são vídeos 16:9 pré-renderizados, então tocam com barras pretas dos
lados (em cima e embaixo no 16:10); esticá-los deformaria a imagem. O jogo desenha a cutscene num quadro 16:9
no meio e nunca limpa o resto da tela, então o último frame do jogo ficava nas barras; enquanto o decodificador
de vídeo entrega quadros, o presenter agora pinta de preto tudo fora desse quadro. **Os menus são 16:9:** o
fundo escurecido de um menu não chega aos lados (nem em cima e embaixo no 16:10). No 32:9 o campo de visão horizontal fica multiplicado por 2, e
objetos nas bordas extremas parecem esticados.

**Limitações conhecidas (fora do 16:9),** relatadas pelos testers; vêm do próprio jogo, feito para 16:9: as
telas de logo da abertura ficam esticadas; os vídeos pré-renderizados mantêm o tamanho 16:9 com barras pretas; e
os menus mostram uma caixa um pouco mais escura sobre a área 16:9 original (um "fade" que o jogo desenha para
delimitar a tela; é só estético). O gameplay e o HUD estão bons em todos os tamanhos relatados.
