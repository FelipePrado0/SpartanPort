# Plano: câmera livre (analógico direito / mouse) — viabilidade e etapas

> Estado: **implementado, experimental** (desligado por padrão), na branch `feat/free-camera` do fork
> paulorband/SpartanPort, no PR FelipePrado0/SpartanPort#1136 (31/10/2026). As seções 1 a 4 são a análise original; a 5 marca o
> que foi feito, e a 6 o que ficou em aberto. Uso: README, seção "Keyboard controls".

**Goal:** uma opção (desligada por padrão) que deixa o jogador girar a câmera em volta do Kratos com o
analógico direito ou com o mouse, durante o gameplay, devolvendo a câmera do jogo em cutscenes, QTEs,
menus e trechos roteirizados.

## 1. Ponto de partida

O God of War III não tem câmera controlável. A câmera é roteirizada por área (trilhos, enquadramentos
fixos, transições definidas pelos designers) e o **analógico direito é a esquiva**, não a câmera
(`src/runtime_pad.c`, `apply_dodge`: "SpartanPort evades toward where the right stick is pushed"). Com
L2 seguro, o analógico direito mira os itens (Cabeça de Hélios, Arco de Apolo; `apply_mouse_aim`), e ele
também resolve os QTEs de rotação (`src/qte_hook.h`). A linha "Arrows | right stick (camera)" do README
descreve o eixo, não uma câmera.

O que o port já tem e serve:

| O que | Onde | Uso aqui |
|---|---|---|
| Código do jogo roda nativo; hooks por assinatura com stubs `jmp` (cheats, dano, QTE, menu) | `src/probe.c`, `src/*_hook.h`, `src/signature.h` | Injetar a rotação no sistema de câmera, sem emulação |
| Sistema de câmera já localizado em parte: objeto de câmera com `tan(fov/2)` em `+0xd8`, "câmera sinalizada", "câmera padrão", dois construtores de vetores de culling (~`0x5cbd89`..`0x5ccaa9` na build 6e72) | `tools/make_ultrawide_patch.py`, `docs/ultrawide.md` | Ponto de entrada da engenharia reversa: a câmera final é montada nessa região |
| View-projection de cada frame lida dos constantes do depth pre-pass | `docs/upscaler.md`, `vk_camera_motion.cpp` | Validar/depurar (comparar a matriz antes e depois do offset); **não** é o lugar da mudança |
| Mouse relativo capturado na janela, sensibilidade e estilo configuráveis no menu | `runtime_pad.c` (`mouse_watch`, `mouse_motion`), `gpu/shim` | Entrada do mouse para a câmera |
| Esquiva como ação de teclado/botão que gera a inclinação do analógico direito | `apply_dodge` | Liberar o analógico direito físico para a câmera, sem perder a esquiva |
| Detecção de QTE (heartbeat) e de menus do jogo (`note_game_menu`) | `qte_hook.h`, `runtime_pad.c` | Desligar a câmera livre nesses momentos |
| Modo foto do Remastered: o jogo já gira a câmera com os analógicos (pausado, raio limitado) | código do jogo | Atalho de RE: a função que aplica a órbita do modo foto mostra onde e como a câmera aceita um offset |

## 2. Abordagens

1. **Offset de órbita no CPU (recomendada).** Hook no ponto em que o jogo termina a câmera do frame
   (posição/alvo/up ou matriz de view, antes dos vetores de culling e da projeção). O stub soma um
   yaw/pitch acumulado, girando a posição da câmera em volta do Kratos (ou do alvo da câmera), e o
   resto do jogo — culling, áudio 3D, movimento relativo à câmera — vê a câmera nova. É o mesmo tipo de
   hook já usado em `actor_hook.h`/`qte_hook.h`.
2. **Reaproveitar o modo foto.** Chamar/forçar o caminho de órbita do modo foto durante o gameplay.
   Menos matemática própria, mas o modo foto pausa a simulação e limita o raio; pode não ser separável.
   Avaliar na fase 0; se for separável, vira a forma mais barata de (1).
3. **Mexer só na GPU (matriz no constant buffer).** Rápido para um protótipo visual, mas inviável como
   recurso: o culling do CPU já descartou o que estava fora do enquadramento original (buracos), sombras
   e outros passes usam outras matrizes, e o controle do Kratos continua relativo à câmera antiga.
   Serve só como prova de conceito de 1–2 dias.

## 3. Riscos (o que decide a viabilidade)

| Risco | Gravidade | Mitigação |
|---|---|---|
| **Cenário feito para câmera fixa.** Olhando para trás aparecem fundos sem acabamento, faces sem backface, buracos, skybox cortado, LODs errados | Alta — é intrínseco ao jogo | Limitar o ângulo (ex.: ±60° de yaw, ±25° de pitch em volta da câmera do jogo), com volta suave ao enquadramento original ao soltar o analógico |
| **Visibilidade por zona/portal** calculada a partir da câmera; objetos somem ao girar | Alta | O hook tem de ficar antes do culling (os dois construtores já conhecidos); validar em várias áreas |
| **Conflito do analógico direito com a esquiva** | Média (design) | Modo da opção: (a) esquiva num botão (a ação `dodge` já existe; no controle, ex.: R3 ou L1+analógico) e analógico livre para a câmera; (b) segurar um modificador para olhar. Mouse não tem conflito fora do L2 |
| **Movimento relativo à câmera** muda de direção enquanto a câmera gira | Média | Decidir: ou o controle segue a câmera nova (natural com mouse), ou o stub entrega ao código de movimento a câmera original. Depende de onde o movimento lê a câmera |
| **Colisão da câmera** com paredes (o jogo não precisa disso hoje) | Média | Raio de órbita curto e limites de ângulo; raycast do jogo só se a RE achar um |
| **Contextos que não podem ter câmera livre:** cutscenes em tempo real, QTEs, puzzles, escalada, quedas/voo, chefes (Poseidon, Cronos, Gaia), Arco/Hélios com L2, menus | Média | Desligar por sinal: QTE heartbeat, menus, L2, troca de câmera "sinalizada", FOV de cinemática (a projeção estreita, `docs/upscaler.md`) |
| Builds diferentes | Baixa | Assinaturas, como os outros hooks; sítio não encontrado deixa a opção indisponível com linha no log |

**Veredito:** viável como **câmera de órbita com limites** (girar e inclinar em volta da câmera do jogo
durante o gameplay). Uma câmera totalmente livre "estilo jogo moderno", 360° em qualquer lugar, **não**
é realista sem refazer cenário e visibilidade, e não vale o esforço. O risco técnico está quase todo na
engenharia reversa da fase 0; o resto (entrada, menu, launcher) é trabalho conhecido neste código.

**Na prática:** girar o próprio nó da câmera (fase 0) fez o culling e o cenário acompanharem, e a volta
completa em torno do Kratos não quebrou o cenário nas áreas testadas; por isso ela é o padrão, e os
limites ficaram opcionais. Fundos sem acabamento ainda podem aparecer em outras áreas, como previsto.

## 4. Esforço estimado

Para uma pessoa que já conhece os hooks do port e faz RE em x86-64 (IDA/Ghidra + capstone):

| Fase | Esforço | Incerteza |
|---|---|---|
| 0. Engenharia reversa do sistema de câmera | 1–3 semanas | Alta |
| 1. Protótipo do hook (offset fixo, variável de ambiente) | 3–5 dias | Média |
| 2. Entrada (analógico, mouse, remapeamento da esquiva) | 3–5 dias | Baixa |
| 3. Contextos e retorno suave | 1–2 semanas | Média |
| 4. Opção no menu/launcher, docs, testes | 3–5 dias | Baixa |
| 5. Teste do jogo inteiro e ajuste dos limites | 1–2 semanas | Média |
| **Total** | **~5–9 semanas** | |

## 5. Fases

### Fase 0 — RE do sistema de câmera (spike, decide tudo)

- [x] Partindo dos sítios de `CAMERA_SITES` (`tools/make_ultrawide_patch.py`), mapear a função que monta
      a câmera do frame: onde ficam posição, alvo, up/view, quem chama os construtores de culling.
- [x] Achar a posição do Kratos (estrutura do ator já parcialmente conhecida: `actor_hook.h`, `+0x298`
      = jogador) para ser o pivô da órbita.
- [ ] ~~Achar a função do modo foto que gira a câmera com os analógicos; ver se é separável da pausa.~~
      Não foi preciso: a órbita própria bastou.
- [x] Descobrir de onde o código de movimento lê a direção da câmera.
- [x] Prova de conceito: escrever um yaw fixo de 20° na câmera (patch manual/depurador) e andar por
      3–4 áreas. Anotar buracos de cenário e culling.
- [x] **Decisão:** se o culling não acompanha a câmera ou o ângulo útil for < ~30°, parar e registrar
      o resultado neste arquivo.

#### Resultados da fase 0 (build 6e72; endereços = imagem + 0x400000)

- **Render view.** `0x5cc470` atualiza uma render view (`rdi`/`rbx`) a partir do nó de câmera no topo
  da lista dela (`+0x300`, ordenada por prioridade; `0x5cbc10` insere, `0x5cbce0` remove). Copia a view
  em cache do nó (`nó+0xf0`, inversa rígida do mundo `nó+0x40`/`+0x90`) para `+0x00..+0x30` e o mundo
  para `+0x180..+0x1b0`; chama `0x5cbf10` (projeção em `+0xc0`, inversa em `+0x1c0`) e deriva
  view×proj (`+0x40`), as matrizes de culling (`+0x80`, `+0x100`) e o frustum (`+0x240`, `+0x250`).
  Matrizes row-major com vetor-linha (`p' = p·M`), posição na linha 3; eixo vertical do mundo = Y.
  A view é recalculada sob demanda, uma vez por frame (`+0x328` contra o contador global; a view×proj
  anterior vai para `+0x200`, para o motion blur).
- **A global `0x84ea50` não é a câmera 3D.** No jogo ela guarda uma view 2D (câmera identidade,
  projeção ortográfica). A view 3D do gameplay é a de projeção em perspectiva cuja câmera ativa tem
  prioridade `0x200` (a pilha `0x84ec10`); o port a reconhece em tempo de execução.
- **Girar só a render view não serve.** Teste no jogo: os personagens giraram e o cenário não, com
  borrão forte (motion blur/TAA com matrizes que não batem). Breakpoints de hardware no nó
  (`SPARTAN_FREE_CAMERA_WATCH=1`) mostraram que só o controlador escreve nele (`0x43f070` →
  `0x43f490`, matriz local `+0x40..+0x70`; a câmera do gameplay não tem pai, `+0x80 = -1`) e que muitos
  sistemas o leem direto, sem passar pela render view (`0x59a1c2`, `0x59f908`, `0x4db4d5`, `0x5439dd`,
  `0x5c37e3`, `0x5836b0`…: cenário, culling, distâncias).
- **Solução (testada no jogo, sem quebra gráfica):** girar o próprio nó. Hook no fim do controlador
  (`0x43f848`, depois das quatro escritas): guarda a matriz limpa e escreve a girada, marcando o nó
  alterado. Hook na render view (`0x5cc764`): refaz o giro a partir da matriz limpa (frames em que o
  controlador não roda) e escreve a mesma matriz na view. Não acumula; com o analógico no centro a
  câmera volta exatamente à do jogo.
- **Movimento (testado no jogo):** o código de movimento não lê o nó girado (usa a câmera do
  próprio controlador), então "para frente" ficava preso à câmera original. O pad gira o analógico
  esquerdo (e a esquiva L1 + analógico direito) pelo yaw da câmera antes de entregá-los ao jogo;
  a mira com L2 fica como está.
- **Volta completa (testada no jogo):** o yaw não tem limite por padrão (dá a volta em torno do
  Kratos); com a câmera atrás dele o cenário não quebrou nas áreas testadas.
  `SPARTAN_FREE_CAMERA_LIMITS=yaw,pitch` volta a limitar.
- **Pivô = Kratos (testado no jogo).** `[gerente+0xa4]` é só o ponto de foco (a meio caminho, ~6
  unidades). O pivô é `[0x84bf68] + 0x70` (matriz em `+0x40..+0x70`, 47 referências no código; achado
  por varredura da memória com `SPARTAN_FREE_CAMERA_FIND=1`, o que fica à frente da câmera em
  movimento). **Não é o ator do Kratos:** `SPARTAN_FREE_CAMERA_WATCH=player` mostrou que só a
  atualização da cena escreve nele (`0x43f070` → `0x43e500` → `0x6aebb0`; `0x444c48`/`0x445112`
  interpolam as matrizes entre os passos da simulação). É o nó que a câmera segue, nos pés dele; serve
  de pivô e de "chão". A câmera gira em volta dele (até 60 unidades; senão, do foco). Distâncias
  vistas: 5 a 56 unidades conforme a área.
- **O ator do Kratos** (o `r13` do gancho de dano, `+0x298 == 0` para o jogador, `src/actor_hook.h`):
  a posição dele é a linha `+0xa0` da matriz em `+0x70..+0xa0`, de onde `0x44fa60` monta o nó da cena.
  `0x49ac00` despacha o movimento por modo (`0x486840`, `0x495a40`, `0x498a30`, `0x4ad780`, `0x494380`,
  `0x48a340`, `0x482f50`) com a posição nova por ponteiro e devolve a antiga se o modo falha.
  Achado com `SPARTAN_FREE_CAMERA_WATCH=actor` (busca no ator depois de um golpe) e `actor:a0`.
- **Cortes:** o giro volta a 0° quando a câmera do jogo salta (> 4 unidades ou > 30° numa
  atualização) ou troca de nó: cada cena começa no enquadramento do jogo.
- **QTE:** com qualquer prompt na tela a câmera volta à do jogo e o analógico vai para o jogo.
- **Liga/desliga (testado no jogo):** G no teclado (ação `camera` do `SPARTAN_KEY_MAP`; F já é o
  Rage of Sparta) ou um toque no R3 do controle (sem L1/R1/L2/L3 junto, para não disparar com o Rage
  L3+R3, o menu R3+L2 ou o combo de FPS). Desligada, a câmera volta à do jogo e o analógico direito
  volta a ser a esquiva. É a saída para as lutas de chefe até haver uma detecção automática.
- **Mouse (testado no jogo):** com a janela segurando o mouse (`spartanportgpu_mouse_locked`), o
  movimento relativo gira a câmera, 0,12°/px × a *Right stick sensitivity* da mira; com L2 seguro o
  mouse continua sendo a mira.
- **Menus e tela inicial (testado no jogo):** aberto o menu de pausa (Options, touchpad, tecla de
  pausa), a câmera livre solta o pad até o controlador da câmera voltar a escrever (ele para com o jogo
  pausado), para o WASD e o analógico navegarem sem o giro. A tela inicial é a fase Shell: do
  `SHELL.WAD` até abrir uma fase de jogo (três letras e três dígitos, ou `CHALLENGE*`;
  `runtime_file.c` avisa `runtime_camera.c` a cada arquivo aberto) a câmera e os analógicos são do jogo.
- **Chão (testado no jogo):** o pitch para onde a câmera chegaria a 0,5 unidade acima do pivô
  (`spartanport_camera_floor_pitch`, `SPARTAN_FREE_CAMERA_FLOOR`), e o pitch do analógico para ali
  também. O limite do pitch passou de 25° para 70° (o chão segura por baixo).
- **Raycast do cenário (testado no jogo):** `0x556d10`, `int ray(xmm0 início, xmm1 fim)` System V,
  usado pelos modos de movimento e por ~140 outros pontos. Monta uma esfera em volta do segmento, pede
  ao índice espacial (`0x556c30`, que testa a máscara `+0` contra o tipo de cada objeto, `0x5e0590`)
  os objetos dentro dela e chama o teste de raio de cada um (virtual `+0x50`). O filtro é a global das
  `lea r13` dele (`0x8662f0`; 0x38 bytes: `+0` tipos que contam, `+4` superfícies excluídas, `+8`
  flags — 2 é o acerto mais próximo, sem ela o primeiro —, `+0x10` objeto ignorado, `+0x30` callback);
  o acerto, a da `lea r12` (`0x866330`; `+0` ponto, `+0x10` normal, `+0x20` fração 0..1, `+0x28`
  objeto). O movimento usa `{0xfff4c00c, 0, 2}`, que pega também as paredes invisíveis na beira dos
  penhascos; os raycasts de linha de visão excluem `0x4d3220c2` (`0x5361f9` e outros), e a câmera faz o
  mesmo (`SPARTAN_FREE_CAMERA_RAY=tipos,excluídos` troca).
- **Paredes (testado no jogo):** só onde o controlador roda (dentro do update do jogo, não no update da
  render view), um raio do peito do Kratos (pivô + 1,5) até a câmera; batendo, a câmera vem na hora
  para 0,4 antes do acerto, na mesma linha, e volta 15% do caminho por raio quando abre. Com menos de 4
  unidades livres ela sobe por cima dele (10° por passo, até o limite do pitch, 25% por raio) em vez de
  ficar nas costas. O filtro e o acerto do jogo são guardados e devolvidos em volta de cada raio.
- **Rastro do motion blur (testado no jogo):** girando rápido, o blur de câmera do jogo borrava o
  fundo sobre o Kratos. Nos quadros em que yaw/pitch mudam, a view×proj anterior (`+0x200`) vira a do
  quadro (`spartanport_camera_hold_blur`, a mesma conta de `0x5cc784`), sem blur de câmera. A opção de
  desligar o motion blur inteiro é a do `main` (`src/graphics_hook.h`, no mesmo update
  `MotionBlurFX`, `0x16ae80`).
- **Opção (testada no jogo):** `free_camera` no `spartanport.ini` (`SPARTAN_FREE_CAMERA`); caixa na
  página Controls do launcher (Rust e Python), linha na seção *Controller* do menu do port e sétima
  linha de Opções → Controles do jogo (builtins `0xf6d` lê, `0xf6e` lê com setas, `0xf6f` alterna; os
  `0xf61..0xf6c` são do Display do `main`; patch do `R_UI.WAD` versão 25). "Restaurar padrões dos
  controles" desliga.
- **Problemas que ficaram:**
  - em lutas de chefe a câmera do jogo é melhor: falta um sinal de "luta de chefe" (hoje, G/R3);
  - em quedas/sequências roteirizadas o giro continua valendo (o foco do jogo vai a 0 nelas);
  - não há "seguir por trás" automático: o ângulo fica o que o jogador deixou, relativo à câmera do
    jogo;
  - o modo foto não foi usado (a órbita própria bastou).

### Fase 1 — Hook da câmera

- [x] `src/camera_hook.h`: sítio por assinatura, stub que lê um bloco compartilhado
      (`yaw`, `pitch`, `enabled`, `heartbeat`) e aplica a rotação em volta do pivô, no estilo de
      `SpartanQteState`.
- [x] `install_camera_hook` em `src/probe.c`, com as linhas de log no padrão
      (`Free camera: hook ready` / `Free camera: unavailable (...)`).
- [x] Assinatura gerada com `tools/make_signatures.py`; teste de assinatura como os de cheats/QTE.
- [x] `SPARTAN_FREE_CAMERA_DEBUG=yaw,pitch` para testar um offset fixo sem entrada.

### Fase 2 — Entrada

- [x] Em `runtime_pad.c`: com a opção ligada, o analógico direito físico alimenta yaw/pitch (zona morta,
      curva, velocidade) e é zerado antes de chegar ao jogo — exceto com L2 (mira), em QTE de rotação e
      durante a esquiva sintetizada.
- [x] Esquiva (decidido: L1 + analógico direito, girado pela câmera; G/toque no R3 desligam a câmera livre): no controle, novo mapeamento padrão (a definir na fase 0 com quem joga; candidatos R3 ou
      um botão traseiro) reutilizando `apply_dodge`; no teclado já existe (`Mouse Back`).
- [x] Mouse: movimento relativo (`mouse_motion`) vira yaw/pitch fora do L2; sensibilidade e inversão de
      Y próprias da câmera. (Feito com a sensibilidade da mira; sem inversão de Y própria.)
- [x] Testes em `tests/test_pad.c`/`test_pad_map.c`: o jogo não vê o analógico direito com a opção
      ligada, vê com L2, vê a esquiva.

### Fase 3 — Contextos e retorno

- [x] Desligar e voltar suavemente (ex.: 0,3 s) quando: QTE ativo, menu do jogo, L2, cinemática (FOV
      estreito ou câmera sinalizada), troca de câmera do jogo (corte de cena). (QTE, menu de pausa,
      tela inicial, L2 e cortes; a cinemática pelo FOV não foi preciso.)
- [x] Limites de yaw/pitch configuráveis (`SPARTAN_FREE_CAMERA_LIMITS`). O retorno automático ao
      enquadramento original após N segundos sem entrada ficou de fora; o jogador pediu, para
      depois, um "seguir por trás" (seção 6).
- [x] Decidir o movimento relativo (câmera nova ou original) com teste prático.

### Fase 4 — Opção e documentação

- [x] Menu do jogo (`gpu/shim/spartanport_overlay.cpp`, `spartanport_settings.cpp`) e página Controls
      do launcher (`launcher-rust/src/ui/pages/controls.rs`, `launcher-rust/src/core/controls.rs`):
      ligar/desligar, sensibilidade analógico/mouse, inverter Y, limites, mapeamento da esquiva.
      (Feito: ligar/desligar no launcher, no menu do port e em Opções → Controles; a tecla G remapeável.)
- [x] README (3 idiomas): opção, limitações conhecidas, que é experimental.

### Fase 5 — Validação

- [ ] Jogar o jogo inteiro (ou um save por capítulo) com a opção ligada; lista de áreas com problemas.
- [ ] Ajustar limites por padrão; se preciso, lista de áreas onde a opção fica desligada.
- [ ] `tools/soak.sh` e replay de pad (`SPARTAN_PAD_REPLAY`) para regressão de entrada.

## 6. Perguntas em aberto

Respondidas: a esquiva no controle é L1 + analógico direito, girada pela câmera; o Kratos anda relativo
à câmera nova; a opção é experimental, com volta completa por padrão e limites opcionais.

Em aberto:

- Detectar luta de chefe (e quedas/sequências roteirizadas) para voltar à câmera do jogo sozinho.
- "Seguir por trás": a câmera voltar devagar para trás do Kratos quando ele anda e o jogador solta o
  analógico.
- Validação do jogo inteiro (fase 5) e áreas onde a opção deveria ficar desligada.
