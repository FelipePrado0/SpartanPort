# Plano: câmera livre (analógico direito / mouse) — viabilidade e etapas

> Estado: **análise**, nenhum código escrito. Cada fase termina num ponto de decisão (seguir ou parar).

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

- [ ] Partindo dos sítios de `CAMERA_SITES` (`tools/make_ultrawide_patch.py`), mapear a função que monta
      a câmera do frame: onde ficam posição, alvo, up/view, quem chama os construtores de culling.
- [ ] Achar a posição do Kratos (estrutura do ator já parcialmente conhecida: `actor_hook.h`, `+0x298`
      = jogador) para ser o pivô da órbita.
- [ ] Achar a função do modo foto que gira a câmera com os analógicos; ver se é separável da pausa.
- [ ] Descobrir de onde o código de movimento lê a direção da câmera.
- [ ] Prova de conceito: escrever um yaw fixo de 20° na câmera (patch manual/depurador) e andar por
      3–4 áreas. Anotar buracos de cenário e culling.
- [ ] **Decisão:** se o culling não acompanha a câmera ou o ângulo útil for < ~30°, parar e registrar
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
- **Movimento:** com o nó girado, o movimento do Kratos segue a câmera nova (como numa câmera livre).
- **Pivô = Kratos (testado no jogo, "relativamente bom").** `[gerente+0xa4]` é só o ponto de foco
  (a meio caminho, ~6 unidades). A posição do Kratos está em `[0x84bf68] + 0x70` (objeto do jogador,
  matriz em `+0x40..+0x70`, 47 referências no código; achado por varredura da memória com
  `SPARTAN_FREE_CAMERA_FIND=1`, o que fica à frente da câmera em movimento). A câmera gira em volta
  dele (até 60 unidades; senão, do foco). Distâncias vistas: 5 a 56 unidades conforme a área.
- **Cortes:** o giro volta a 0° quando a câmera do jogo salta (> 4 unidades ou > 30° numa
  atualização) ou troca de nó: cada cena começa no enquadramento do jogo.
- **QTE:** com qualquer prompt na tela a câmera volta à do jogo e o analógico vai para o jogo.
- **Liga/desliga (testado no jogo):** G no teclado (ação `camera` do `SPARTAN_KEY_MAP`; F já é o
  Rage of Sparta) ou um toque no R3 do controle (sem L1/R1/L2/L3 junto, para não disparar com o Rage
  L3+R3, o menu R3+L2 ou o combo de FPS). Desligada, a câmera volta à do jogo e o analógico direito
  volta a ser a esquiva. É a saída para as lutas de chefe até haver uma detecção automática.
- **Problemas vistos no teste:**
  - a câmera entra no cenário quando há parede ao lado do Kratos e se gira para esse lado
    (falta colisão: encurtar a distância até o Kratos quando algo fica entre ele e a câmera);
  - em lutas de chefe a câmera do jogo é melhor: falta um sinal de "luta de chefe";
  - em quedas/sequências roteirizadas o giro continua valendo (o foco do jogo vai a 0 nelas).
- **Ainda em aberto:** modo foto, mouse, opção no menu/launcher.

### Fase 1 — Hook da câmera

- [ ] `src/camera_hook.h`: sítio por assinatura, stub que lê um bloco compartilhado
      (`yaw`, `pitch`, `enabled`, `heartbeat`) e aplica a rotação em volta do pivô, no estilo de
      `SpartanQteState`.
- [ ] `install_camera_hook` em `src/probe.c`, com as linhas de log no padrão
      (`Free camera: hook ready` / `Free camera: unavailable (...)`).
- [ ] Assinatura gerada com `tools/make_signatures.py`; teste de assinatura como os de cheats/QTE.
- [ ] `SPARTAN_FREE_CAMERA_DEBUG=yaw,pitch` para testar um offset fixo sem entrada.

### Fase 2 — Entrada

- [ ] Em `runtime_pad.c`: com a opção ligada, o analógico direito físico alimenta yaw/pitch (zona morta,
      curva, velocidade) e é zerado antes de chegar ao jogo — exceto com L2 (mira), em QTE de rotação e
      durante a esquiva sintetizada.
- [ ] Esquiva: no controle, novo mapeamento padrão (a definir na fase 0 com quem joga; candidatos R3 ou
      um botão traseiro) reutilizando `apply_dodge`; no teclado já existe (`Mouse Back`).
- [ ] Mouse: movimento relativo (`mouse_motion`) vira yaw/pitch fora do L2; sensibilidade e inversão de
      Y próprias da câmera.
- [ ] Testes em `tests/test_pad.c`/`test_pad_map.c`: o jogo não vê o analógico direito com a opção
      ligada, vê com L2, vê a esquiva.

### Fase 3 — Contextos e retorno

- [ ] Desligar e voltar suavemente (ex.: 0,3 s) quando: QTE ativo, menu do jogo, L2, cinemática (FOV
      estreito ou câmera sinalizada), troca de câmera do jogo (corte de cena).
- [ ] Limites de yaw/pitch configuráveis; retorno automático ao enquadramento original após N segundos
      sem entrada (opcional).
- [ ] Decidir o movimento relativo (câmera nova ou original) com teste prático.

### Fase 4 — Opção e documentação

- [ ] Menu do jogo (`gpu/shim/spartanport_overlay.cpp`, `spartanport_settings.cpp`) e página Controls
      do launcher (`launcher-rust/src/ui/pages/controls.rs`, `launcher-rust/src/core/controls.rs`):
      ligar/desligar, sensibilidade analógico/mouse, inverter Y, limites, mapeamento da esquiva.
- [ ] README (3 idiomas): opção, limitações conhecidas, que é experimental.

### Fase 5 — Validação

- [ ] Jogar o jogo inteiro (ou um save por capítulo) com a opção ligada; lista de áreas com problemas.
- [ ] Ajustar limites por padrão; se preciso, lista de áreas onde a opção fica desligada.
- [ ] `tools/soak.sh` e replay de pad (`SPARTAN_PAD_REPLAY`) para regressão de entrada.

## 6. Perguntas em aberto

- Esquiva no controle com a opção ligada: qual botão? (decisão de quem joga)
- Movimento do Kratos relativo à câmera nova ou à original?
- Aceitar uma opção "experimental" com limites de ângulo, ou só seguir se o ângulo útil for grande?
