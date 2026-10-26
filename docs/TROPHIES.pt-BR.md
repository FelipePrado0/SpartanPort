# Troféus

[English](TROPHIES.md) | **Português** | [Español](TROPHIES.es.md)

O jogo chama a biblioteca de troféus do PlayStation (`sceNpTrophy*`). Este port responde a essas
chamadas, guarda os troféus desbloqueados no disco e mostra tudo ao jogador: uma notificação quando
um troféu é desbloqueado, uma janela de troféus dentro do jogo e uma página **Trophies** no launcher.

Tudo é lido da pasta do jogo do próprio jogador (`sce_sys/trophy/trophy00.trp`); nenhum dado do
jogo faz parte do port.

**Índice:** [O que o jogador vê](#o-que-o-jogador-vê) · [Ordem e raridade](#ordem-e-raridade) ·
[Notificações](#notificações) · [Idioma](#idioma) · [Estado salvo](#estado-salvo) ·
[Ids dos troféus](#ids-dos-troféus) · [O que está neste repositório](#o-que-está-neste-repositório-e-o-que-não-está) ·
[Como funciona](#como-funciona) · [Limites](#limites)

## O que o jogador vê

| Onde | O quê |
|------|-------|
| No jogo, quando um troféu é desbloqueado | Uma notificação no tema escolhido na janela de troféus (abaixo). O padrão, **Original**, é um cartão no canto superior direito (abaixo do contador de FPS quando ele está ligado) com o ícone, o nome e a descrição do troféu; ele entra deslizando, fica 5 segundos e some. |
| No jogo, **F8** ou **R3+R2** | A janela de troféus, em duas páginas. **Troféus**: todos os troféus com o ícone, um troféuzinho na cor da classe, o nome, o que fazer para desbloquear, o estado e quantos jogadores conseguiram; um troféu desbloqueado ganha a moldura da cor da classe (bronze, prata, ouro e uma platina que brilha). Acima da lista: a contagem, um filtro por estado (todos / desbloqueados / bloqueados), um por classe (todas / platina / ouro / prata / bronze, com quantos de cada estão desbloqueados; os dois se combinam) e a ordem. **Notificações**: o idioma, e a aparência e o som da notificação, com prévias. |
| Launcher, página **Trophies** | A mesma lista, com a categoria (platina, ouro, prata, bronze) e um filtro. |

Teclas da página **Troféus**: Cima/Baixo, W/S, PageUp/PageDown, Home/End, roda do mouse ou
direcional rolam; Esquerda/Direita, A/D ou direcional Esquerda/Direita trocam o filtro de estado;
Quadrado ou G, o filtro de classe; Triângulo ou Tab trocam a ordem; L1/R1 ou Q/E vão para a página
**Notificações**; F8, Esc, Backspace, Enter, Bola/X ou R3+R2 fecham a janela.
A página **Notificações** é usada como o menu do Insert (direcional ou setas, X ou Enter, o mouse);
L1/R1 ou Q/E voltam para a lista; F8, Esc ou R3+R2 fecham. Com a janela aberta o jogo não recebe
comandos (como no menu do Insert), e o jogo fica parado até ela fechar, como com o menu do
Insert: Jogo > **Pausar o jogo** (ligado por padrão) desliga isso. O R3 apertado com o R2 ou o L2
não chega ao jogo, então abrir a janela ou o menu não usa o item nem a magia.

A platina (id 0) é desbloqueada junto com o último dos outros troféus, como no PlayStation.

## Ordem e raridade

Cada troféu mostra a parcela de jogadores que o conseguiu e o nível de raridade, como o PlayStation
mostra: **Comum** (mais de 50%), **Raro** (mais de 15%), **Muito raro** (mais de 5%) e
**Ultrarraro**. A ordem é a lista do jogo, **Mais conquistados primeiro** ou **Mais raros primeiro**.

O port não se conecta à PSN, então os números são fixos: a parcela dos jogadores acompanhados pela
[Exophase](https://www.exophase.com/game/god-of-war-iii-remastered-ps4/trophies/) que conseguiu cada
troféu, como listado em outubro de 2026 (`gpu/shim/spartanport_trophy_rarity.h`). Eles só aparecem
para a lista de 35 troféus deste jogo; outro pacote mantém a ordem da lista e não mostra porcentagem.

## Notificações

A página **Notificações** da janela de troféus (F8 ou R3+R2, depois L1/R1) escolhe como um troféu
desbloqueado aparece e soa. Nada muda para quem não a abrir: a notificação **Original** continua
sendo o padrão, como era, e nenhum som toca sem um arquivo de som.

| Opção | O que faz |
|-------|-----------|
| Tema | **Original** (o cartão do port, acima), **Cartão** (um cartão escuro no canto superior direito com a arte do troféu, o troféuzinho da classe e um selo animado; a platina ganha uma animação desenhada curta), **Cartão, platina cinematográfica** (a platina fica mais tempo, com luz em volta) ou **Pop-up no canto** (um retângulo que sobe no canto inferior direito com a arte emoldurada na cor da classe, o nome e o que o troféu pede). |
| Sem som, Volume | Só o som da notificação; o som do jogo não é alterado. |
| Tempo na tela | De 0 a 20 segundos; "Padrão do tema" mantém o tempo do tema. |
| Brilho da platina | Só no Cartão, platina cinematográfica: a intensidade da luz. |
| Animar a platina | Desligado: a platina mostra a própria imagem, como os outros troféus. |
| Escrever a classe | Escreve o nome da classe (Bronze, Prata...) antes da segunda linha. |
| Prévia | Bronze, Prata, Ouro, Platina, os quatro em sequência ou três de uma vez, pela mesma fila, desenho e som de um desbloqueio de verdade. Nada é desbloqueado nem salvo. |

A classe de cada troféu é deduzida pela cor do ícone, como o launcher faz (as classes no pacote são
criptografadas); para os 35 troféus deste jogo ela bate com a lista do próprio pacote. A página
fica no idioma do menu (abaixo).

### Seus próprios arquivos

Coloque-os em `user/custom_trophy` (dentro da sua pasta de saves, então as atualizações os mantêm).
Nenhum faz parte do port: sem eles, são usados a arte desenhada, as fontes embutidas e nenhum som.

| Arquivo | Efeito |
|---------|--------|
| `bronze`, `silver`, `gold`, `platinum` `.wav .mp3 .ogg .flac .m4a .opus` | O som daquela classe (no máximo 20 segundos). `trophy.*` vale para toda classe sem som próprio. O dispositivo de áudio só é aberto quando há um arquivo de som, e nunca com `SPARTAN_AUDIO=none`. **Recarregar sons** lê os arquivos de novo durante o jogo. |
| `<classe>_anim.gif`, `.apng`, `.png`, `.webp`, `.jpg` | Substitui a arte da notificação daquela classe (Cartão e Pop-up no canto). Um GIF ou PNG animado toca uma vez no próprio tempo; uma imagem parada aparece como está. `trophy_anim.*` vale para toda classe sem arquivo próprio. |
| `<classe>_anim.png` + `<classe>_anim.ini` | Uma folha de quadros: `frames=`, `columns=`, `fps=`, `loop=1` para repetir, `delay_ms=` para começar depois. |
| `badge.png` | O disco à direita do Cartão. |
| `font.ttf`, `font-light.ttf` | As fontes do Cartão (o nome, a segunda linha). |
| `notifications.ini` | As opções acima, gravadas pela página. Também guarda os textos, em qualquer idioma: `text_unlocked=` (a segunda linha do Cartão), `text_banner=` (a primeira linha do cartão Original), `name_bronze=`, `name_silver=`, `name_gold=`, `name_platinum=`. |

Imagens e animações são lidas quando o jogo abre. Mais de 8192 pixels de lado, mais de 20 segundos
ou 600 quadros, ou mais de 64 MB de quadros decodificados são recusados ou cortados. Os arquivos são
abertos só como arquivos locais: um "som" que na verdade é uma playlist ou um link não é seguido. Um
arquivo quebrado ou ausente é ignorado e registrado no log; o jogo nunca para por causa dele.

## Idioma

O launcher grava `user/trophy_texts.txt` toda vez que abre o jogo, no idioma do launcher, e o mesmo
em todos os idiomas em `user/trophy_texts/<idioma>.txt`. O jogo lê o primeiro ao abrir.
**Idioma**, no topo da aba **Jogo** do menu do Insert e da página **Notificações** da janela de
troféus, troca na hora o menu, a janela de troféus e os nomes dos troféus, lendo o segundo; a
escolha fica em `spartanport.ini` (`ui_language`), e **Igual ao launcher** volta a seguir o launcher.

O menu e a janela de troféus são desenhados em inglês, português, espanhol, francês, alemão,
italiano, polonês, turco, indonésio, russo e chinês (o chinês com uma fonte CJK instalada, veja o
README). Árabe, japonês e coreano os deixam em inglês, porque a fonte do overlay não desenha essas
escritas; os nomes dos troféus continuam vindo nesses idiomas no launcher.

Idiomas: inglês, russo, árabe, espanhol, português, francês, alemão, italiano, polonês, turco,
chinês, japonês e coreano (`launcher-rust/assets/i18n.json`, gerado a partir do
`launcher/spartanport_trophies.py` pelo `tools/launcher_i18n.py`). Os nomes e descrições são os do
próprio jogo, como a lista de troféus do PS4 os mostra em cada idioma, então dá para procurar um
troféu pelo nome que o jogo dá a ele: inglês, português (Brasil), espanhol (a lista da América
Latina), francês, alemão, italiano, polonês, turco, russo, japonês e coreano. **Árabe e chinês
simplificado são traduções**: o jogo não tem lista nesses idiomas. Um idioma sem lista usa inglês.

Sem o launcher, coloque o mesmo arquivo à mão na pasta de saves (`user/` ou `SPARTAN_USER_DIR`):
`trophy_texts.txt`, ou `trophy_names.txt` (mesmo formato, usado quando não há `trophy_texts.txt`).

### Formato do arquivo

```
# comentário
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

`ID=Nome | Descrição` (a descrição é opcional), UTF-8, um troféu por linha. Um nome tem até 63 bytes
e uma descrição até 159. Um troféu sem linha aparece como `Trophy ID N`. Linhas `@` ausentes usam os
rótulos em inglês.

## Estado salvo

`user/trophies.dat`: 16 bytes, quatro palavras de 32 bits little-endian. O troféu `id` é o bit
`id % 32` da palavra `id / 32`. Apague o arquivo para zerar os troféus. Ele é separado dos saves do
próprio jogo.

## Ids dos troféus

Os ids são os números nos nomes dos ícones do pacote (`TROP000.PNG` ... `TROP034.PNG`): 35 troféus,
id 0 a platina, 1-18 bronze, 19-29 prata, 30-34 ouro (pela cor dos ícones). Os textos estão na ordem
da lista do PlayStation, que é a mesma.

## O que está neste repositório, e o que não está

Muita gente pergunta o que pode ser compartilhado. Os recursos de troféus foram feitos para que nada
neste repositório pertença à Sony ou aos criadores do jogo:

- **Nenhum arquivo do jogo.** Os ícones, o pacote de troféus e o jogo são lidos da cópia do próprio
  jogador durante a execução; nenhum deles é copiado para o port.
- **Nenhum logo, som ou imagem do PlayStation.** A arte da notificação (troféuzinhos, selo, a
  animação da platina) é desenhada com formas no código. A aparência do Cartão é do próprio port; ela
  só segue o layout comum de uma notificação (arte à esquerda, duas linhas de texto, um selo).
- **Os textos** são os nomes e descrições da lista pública de troféus do jogo, as linhas curtas que
  todo site de troféus mostra e que o port já trazia em inglês e português. É com eles que o jogador
  encontra um troféu pelo nome; nenhum outro texto do jogo está incluído.
- **Os números de raridade** são estatísticas (uma parcela de jogadores), com a fonte citada acima.
  Um número sobre quantas pessoas fizeram algo é um fato, não uma obra.
- **Seus próprios arquivos** em `user/custom_trophy` (um som de que você gosta, uma imagem, um logo)
  ficam no seu computador. Eles não fazem parte do port, e você não deve enviá-los a este repositório
  nem compartilhá-los se pertencerem a outra pessoa.

Isto não é aconselhamento jurídico; é como o port mantém o próprio repositório limpo.

## Como funciona

- `src/runtime_services.c`: as funções `sceNpTrophy*`. No `RegisterContext` lê o `trophy00.trp`
  (cabeçalho big endian: magic `DC A2 4D 00`, número de arquivos em `0x10`, tamanho da entrada em
  `0x14`, entradas a partir de `0x60`: nome, offset, tamanho) e o arquivo de textos. `UnlockTrophy`
  marca o bit, salva o estado e mostra a notificação; a platina é dada quando todos os outros estão
  marcados. `GetTrophyUnlockState` devolve os bits salvos e o número de troféus.
- `gpu/spartanportgpu.h`: `spartanportgpu_toast_trophy` (a notificação, com o id do troféu) e
  `spartanportgpu_trophy_setup` (a janela: textos, ícones e um ponteiro para o estado ao vivo),
  entregues uma vez pelo runtime.
- `gpu/shim/spartanport_overlay.cpp`: desenha a notificação Original e a janela de troféus com Dear
  ImGui, decodifica os ícones com `SpartanPng` (numa thread separada), deduz a classe de cada troféu
  pelo ícone e trata as teclas e botões da janela.
- `gpu/shim/spartanport_trophy_toast.{h,cpp}`: os outros temas: a fila, o desenho e uma thread para
  os sons (um stream de áudio SDL próprio, ao lado do do jogo).
- `gpu/shim/spartanport_trophy_toast_model.{h,cpp}`: `notifications.ini`, as configurações de
  animação, a linha do tempo e a classe pela cor. Sem janela, GPU ou áudio, por isso tem testes.
- `gpu/shim/spartanport_trophy_media.{h,cpp}`: os sons e imagens do jogador decodificados com FFmpeg,
  com os limites acima.
- `gpu/shim/spartanport_trophy_rarity.h`: a parcela de jogadores de cada troféu.
- `gpu/shim/spartanport_trophy_texts.{h,cpp}`: lê `user/trophy_texts/<idioma>.txt` para o idioma
  escolhido no jogo.
- `gpu/shim/spartanport_ui_language.h`, `gpu/shim/ui_language/`: os textos do menu e da janela de
  troféus em cada idioma, com o texto em inglês como chave.
- `gpu/tools/overlay_preview.cpp` (`overlay-preview`): desenha o menu, a janela de troféus e as
  notificações fora do jogo, com troféus inventados, e a lista de saves do jogo, com saves
  inventados, para trabalhar na aparência sem o jogo.
- `launcher-rust/assets/i18n.json`: os textos e rótulos em todos os idiomas, gerados a partir do
  `launcher/spartanport_trophies.py` e do `launcher/spartanport_lang.py` pelo `tools/launcher_i18n.py`.
- `launcher-rust/src/core/trophies.rs`: o leitor do `.trp` e do `trophies.dat`, e a parcela de
  jogadores (a mesma tabela do jogo).
- `launcher-rust/src/core/env.rs`: `write_trophy_texts`, os arquivos que o jogo lê.
- `launcher-rust/src/ui/pages/trophies.rs`: a página Troféus do launcher, com a mesma ordem e
  raridade da janela do jogo.
- `tests/test_trophies.py`, `tests/test_trophy_toast.cpp`, `tests/test_ui_language.cpp`,
  `tests/test_services.c`: testes unitários. Eles montam os próprios arquivos e não precisam de
  arquivos do jogo; `trophy-toast-test PASTA_TEMP trophy00.trp` também confere as classes dos ícones
  do próprio jogo.

## Limites

- Só os troféus desbloqueados por este port são conhecidos. Troféus ganhos num PlayStation não são
  importados, e com um save já avançado o jogo normalmente não desbloqueia de novo o que já tinha
  feito.
- Os nomes e descrições do pacote são criptografados (`TROP*.ESFM`) e não são lidos: os textos acima
  vêm embutidos no launcher.
- Não há sincronização de troféus com a PSN nem perfil online. A raridade é um retrato fixo e não muda
  enquanto você joga.
- R3+R2 pode ser apertado sem querer em combate; o menu do Insert usa R3+L2 pelo mesmo motivo.
