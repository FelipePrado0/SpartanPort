[English](CONTROLS.md) | **Português (Brasil)** | [Español](CONTROLS.es.md)

# Controles

As teclas padrão estão no [README](../README.pt-BR.md#controles). Esta página explica as opções da
tela **Controls** do launcher.

## Teclado e mouse

**Rage of Sparta em uma tecla** (ligado por padrão) deixa Z e X sem uso. Os **Joystick prompts**
respondem aos eventos rápidos de girar o analógico sem analógico: digite o giro (meias-voltas D S A,
A S D, A W D ou D W A; voltas completas D S A W ou A S D W) ou, com *Adaptive mouse*, desenhe-o com
o mouse. Enquanto você joga, o mouse fica preso na janela do jogo; o menu do jogo, a tecla de pausa
(Esc) e outras janelas o liberam, e Esc de novo ou um clique na janela o prende outra vez. Com L2
pressionado (R), o mouse mira com o analógico direito, para a Cabeça de Hélio ou o Arco de Apolo:
*Relative* segue o movimento, *Absolute* aponta a partir de onde o mouse estava ao apertar L2, e
*Right stick sensitivity* define quanto mouse é preciso. Sem o launcher, o jogo usa as próprias
teclas: E e Q para Quadrado e Triângulo, 1 e 3 para L1 e R1, F para R2, C para R3, I, K, J, L para o
direcional, Enter para Options, Tab e Backspace para o touchpad.

## Teclas alternativas

Todo botão pode ter uma segunda e uma terceira tecla ou entrada do mouse, na segunda e na terceira
caixa da linha dele na tela Controls (o botão x ao lado de uma caixa a limpa). Todas apertam o
botão, então você pode manter o Quadrado no clique esquerdo para o combate e também apertá-lo com o
J, ou com o Keypad 4, nos minigames. Na lira das Musas, por exemplo, mantenha Espaço, Shift esquerdo
e os dois botões do mouse e acrescente K, L, J e I, dispostas como os quatro botões (I em cima, J à
esquerda, L à direita, K embaixo). Uma tecla que já é de outro botão troca com ele, como nas teclas
principais. Os ícones de teclado no jogo mostram a tecla principal; *Teclas nos ícones* (tela
Controls, com o estilo Teclado ou Igual ao meu controle) pode mostrar a alternativa no lugar, ou a
alternativa só na lira (os outros ícones continuam com a principal). Os ícones desenham a primeira
alternativa preenchida: a segunda caixa, ou a terceira se a segunda estiver vazia. Sem o launcher,
junte os nomes com `|` no `SPARTAN_KEY_MAP`, por exemplo `cross=Space|K|Keypad 2,square=Mouse Left|J`.

## Gamepads

Em um controle sem touchpad (Xbox e outros), View abre o menu de pausa (o clique direito do
touchpad) e L1 + View abre o modo foto (o clique esquerdo). No DualSense o touchpad funciona como
no PS4, e Share/Create funciona como View. Detalhes do DualSense: [DUALSENSE.md](DUALSENSE.md) (em
inglês).

No menu do port: L1/R1 ou Q/E trocam de aba. Na lista de saves: setas ou direcional escolhem, Enter
ou Xis confirmam, Esc ou Círculo cancelam.

### Controles USB genéricos

Em **Controls → Set up a controller**, selecione o joystick e escolha **Configure** ao lado de
qualquer botão, direção do direcional ou eixo de analógico. Joysticks sem mapeamento de gamepad do
SDL também aparecem na lista. Cada entrada detectada é salva na hora; você pode começar por qualquer
linha e corrigir só um controle depois. Os mapeamentos existentes aparecem ao reabrir o editor.
**Clear** remove só aquela linha, e **Cancel capture** deixa a edição pendente como estava, mantendo
o que já foi salvo. A prévia ao vivo mostra o resultado a qualquer momento. Feche a janela ao
terminar.

Os perfis são salvos por GUID de dispositivo do SDL e sistema operacional no `gamecontrollerdb.txt`
da pasta de dados do port. Eles carregam na próxima abertura do jogo, inclusive pelo `--play` e por
atalhos. Mapeamentos explícitos em `SDL_GAMECONTROLLERCONFIG` e `SDL_GAMECONTROLLERCONFIG_FILE` têm
prioridade. O remapeamento de botões do jogo continua valendo; use **Reset to defaults** se quiser
que os botões fiquem iguais à configuração guiada. O SDL precisa conseguir ler o joystick; o
mapeador não instala drivers.

## Ataques de troca

**Ataques de troca com L1 + direcional e L1 + Xis** (desligado por padrão, tela Controls): L1 + uma
direção do direcional troca para essa arma com o ataque de troca dela, e a arma que já está na mão
de Kratos repete o ataque; L1 + Xis faz o ataque de troca da arma na mão em vez de passar para a
próxima arma. O ataque de troca da mesma arma de novo espera 0,8 segundo depois do último (o de
outra arma não espera). O direcional sozinho troca como antes. Os tutoriais de troca de arma passam
a dizer isso em todos os idiomas (em cópias dos arquivos de idioma na pasta de dados do port). Sem o
launcher, defina `SPARTAN_SWITCH_ATTACKS=1`. Como o jogo escolhe os golpes de Kratos, para outras
mudanças: [moves.md](moves.md) (em inglês).

## Câmera livre (experimental)

Desligada por padrão. Ligue em *Câmera livre (experimental)* na tela Controls do launcher, na seção
*Controller* do menu do port (Insert) ou em Opções → Controles do jogo. Aí o analógico direito, ou o
mouse enquanto o jogo o prende, gira a câmera em volta do Kratos por completo e por cima dele, e a
câmera mantém esse ângulo enquanto ele anda; o mouse segue a *Sensibilidade da mira*, e com L2
pressionado ele mira como antes. O Kratos anda para onde a câmera olha, e L1 + analógico direito
faz a esquiva.

No jogo, G (configurável) ou um toque só no R3 desliga e liga a câmera livre: desligada, é a câmera
do próprio jogo, para as lutas de chefe e as cenas roteirizadas. O jogo retoma a câmera sozinho nos
cortes de câmera, nos eventos rápidos, na tela inicial e no menu de pausa. A câmera para antes de
paredes e rochas (a colisão da própria fase, não as paredes invisíveis na beira dos penhascos), sobe
por cima do Kratos quando ele está encostado numa parede e não desce abaixo dos pés dele. Muitos
lugares foram feitos só para a câmera do jogo, então outros ângulos podem mostrar o fundo inacabado
do cenário. Sem o launcher, defina `SPARTAN_FREE_CAMERA=1`; `SPARTAN_FREE_CAMERA_LIMITS=yaw,pitch`
(graus) limita quanto ela gira.

## Ícones de botões no jogo

Desligados por padrão. Escolha na tela Controls do launcher, em *Ícones dos botões no jogo*: *Igual
ao meu controle*, *Xbox*, *Nintendo* ou *Teclado*. Eles trocam os ícones PlayStation dos tutoriais,
da lista de golpes, dos eventos rápidos e da lira das Musas na Câmara da Chama; o desenho do
controle no menu de pausa continua sendo um DualShock 4. *Teclado* mostra a tecla que aperta cada
botão, também na lira, seguindo o seu mapeamento do teclado (uma tecla trocada aparece no próximo
início); *Igual ao meu controle* também mostra as teclas quando nenhum controle está conectado.

*Cores das teclas* (no mesmo lugar) pinta as teclas do X, Círculo, Quadrado e Triângulo, na lira e
nos outros ícones de teclado: *Cores do PlayStation*, ou *Personalizadas* com uma cor de uma paleta
curta para cada botão; os outros ícones e o *Cinza* padrão não mudam.

O launcher grava uma cópia com patch de até três arquivos do jogo (cerca de 410 MB) na pasta de
dados do port e nunca altera a pasta do jogo. Um controle novo é reconhecido após reiniciar. Sem o
launcher, defina `SPARTAN_GLYPH_STYLE` como `auto`, `xbox`, `nintendo` ou `keyboard` antes de
iniciar o `run.sh`.
