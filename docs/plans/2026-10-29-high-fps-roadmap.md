# Alto FPS jogável do começo ao fim: roadmap

> **Para agentes:** este documento é autossuficiente. Leia inteiro antes de começar. Ele diz o que
> fazer, em que ordem, onde está cada coisa no código, como testar, quando parar e pedir aprovação,
> e quando uma fase está pronta. Marque os checkboxes (`- [ ]` → `- [x]`) e atualize a tabela de
> estado no mesmo commit em que uma tarefa termina.

## Estado

| Fase | Assunto | Estado | Branch / PR |
|:--|:--|:--|:--|
| F0 | Preparação e linha de base | concluída (modo nuvem; nenhuma falha pré-existente) | — |
| F1 | Catálogo e triagem dos bugs de FPS alto | catálogo pronto; falta a confirmação no jogo e publicar a issue | `feat/high-fps` |
| F2 | Rede de segurança A: motor a 60 durante QTEs | **validada no jogo** (30/10: sequências do cavalo do Poseidon e mash, espera de 5 s; F2.5 completa); faltam os snapshots e a decisão F2.6 | `feat/high-fps` |
| F3 | Rede de segurança B: motor a 60 por área | não iniciada | — |
| F4 | Correções de causa raiz (uma por mecânica) | F4.0 (ferramenta) pronta; falta o mantenedor rodá-la no eboot | `feat/high-fps` |
| F5 | Validação final e release | não iniciada | — |

---

## 1. Objetivo

**Objetivo:** com o motor em qualquer taxa de 100 a 240 FPS ("Alto FPS"), o jogo é jogável do
começo ao fim, com física, colisão, pulos, QTEs e áudio de cutscene iguais aos de 60 FPS.

**Estratégia, em ordem de preferência:**

1. **Corrigir a causa no motor** (F4): achar as poucas rotinas do jogo que assumem quadros de
   1/60 s e fazê-las se comportar como a 60 em qualquer taxa. Uma rotina corrigida conserta todos
   os lugares que dependem dela, inclusive os que ninguém relatou.
2. **Rede de segurança** (F2 e F3): enquanto uma causa não estiver corrigida, o motor cai sozinho
   para 60 só onde quebra (durante QTEs e em áreas listadas) e volta depois. É uma rede de
   segurança, não a solução final: cada área da lista sai dela quando a causa for corrigida.
3. **Atalho manual** (já existe): R3 + L1 + R1 ou F9 troca a taxa do motor ao vivo.

**Fora do escopo:** frame generation (branch `feat/frame-generation`, outro trabalho), mudanças no
launcher Python (legado, não usar) e qualquer otimização de desempenho.

### Definição de pronto (do roadmap inteiro)

- [ ] Todo bug do catálogo (F1) confirmado como "de FPS" está **corrigido na causa** (F4) ou
      **coberto pela rede de segurança** (F2/F3), com o motivo registrado no catálogo.
- [ ] Uma passada completa do jogo a 240 FPS (F5) sem travar, sem ficar preso e sem QTE impossível.
- [ ] As issues do catálogo foram respondidas e fechadas no GitHub (comentários bilíngues).
- [ ] Testes automáticos novos para cada stub ou patch, rodando no CI.

---

## 2. Regras do projeto (obrigatórias)

### Fluxo de trabalho

- **Spec antes de código.** Antes de implementar qualquer fase ou mudança que toque mais de um
  arquivo, apresente ao mantenedor uma spec curta neste formato e **pare até ele aprovar**:
  - **Objetivo:** uma frase.
  - **Requisitos:** `QUANDO <situação>, O SISTEMA DEVE <comportamento>`, um por linha.
  - **Arquivos:** os que vão mudar.
  - **Validação:** os testes automáticos e o que o mantenedor vai testar no jogo.
- **Nada de commit em `main`, push, merge, fechar issue ou PR sem autorização explícita.**
- **Uma branch só para todo o Alto FPS:** `feat/high-fps`, a partir da `main`. Todas as fases, o
  catálogo, as ferramentas e este roadmap vivem nela. **A PR só é aberta quando tudo estiver validado
  e corrigido** (decisão do mantenedor, 29/10/2026); até lá, nenhuma PR parcial.
- **Teste no jogo é sempre do mantenedor.** O agente não tem o jogo. Toda fase termina com uma build
  Windows e um roteiro de teste curto (ver §6).
- **Comentários no GitHub são bilíngues:** inglês primeiro, depois português do Brasil, com o mesmo
  conteúdo (`docs/agents/issue-tracker.md`). Sempre passe `-R FelipePrado0/SpartanPort` ao `gh`.

### Código

- **Launcher: só o Rust** (`launcher-rust/`). O launcher Python (`launcher/`) é legado: não usar,
  não compilar, não validar e não estender.
- **Opção nova = três lugares:** a configuração no jogo (`gpu/shim/spartanport_settings.*` e uma
  linha no overlay, `gpu/shim/spartanport_overlay.cpp`), o launcher Rust (catálogo, opções, página e
  `launcher-rust/assets/i18n.json`) e as traduções do menu do jogo
  (`gpu/shim/ui_language/spartanport_ui_*.h`; o `ui-language-test` exige os mesmos textos em todos
  os idiomas).
- **Hooks no executável do jogo** seguem o padrão de `src/qte_hook.h`, `src/graphics_hook.h` e
  `src/switch_hook.h`:
  - o site é achado por assinatura (`src/signature.h`), nunca por endereço fixo;
  - os bytes originais são conferidos antes de instalar;
  - o stub cabe na vaga reservada;
  - um teste em `tests/` roda o stub como o jogo chamaria;
  - `tests/test_signatures.cpp` acha o site no executável de referência.
- **Patches de bytes** (estilo do patch de FPS) vão em `patches/CUSA01623.xml`, com `Pattern`/`At`
  para achar o site em qualquer build.
- **Estilo:** comentários em inglês, curtos, no tom do arquivo vizinho. Mensagens de commit no estilo
  do repo: `Feat(Engine): …`, `Fix(QTE): …`, `Test(Engine): …`, `Docs(Engine): …`.
- **Nunca faça commit de arquivos do jogo:** `eboot.bin`/`eboot.elf`, WADs, saves ou logs com dados
  do jogador. Logs anexados a issues só com autorização.

### Máquina

- No container Linux, compile com no máximo 2 jobs por agente: `-j2`, `CMAKE_BUILD_PARALLEL_LEVEL=2`,
  `cargo -j 2`. Nunca dois builds pesados de agentes diferentes ao mesmo tempo.
- O `build.sh` aplica patches no submódulo `gpu/third_party/fsr-vulkan`. Depois de compilar,
  desfaça-os com `git apply --reverse` de cada `gpu/patches/fsr-vulkan/*.patch`, em ordem reversa.
  Nunca use `checkout -f` nem `clean` no submódulo.
- Nunca use `git stash` sem nome: a pilha de stash é compartilhada entre worktrees.

### Onde o agente roda

O roadmap vale nos dois ambientes abaixo. Descubra em qual você está antes da F0 (`uname -s`,
`$OS`, o caminho do repo) e siga as regras do modo certo.

| | **Modo nuvem** (Claude Code na web, container Linux) | **Modo local** (Claude Code no PC Windows do mantenedor) |
|:--|:--|:--|
| Repositório | clone em `/home/user/...` | `C:\Projetos\Projects\GOW3PC`, com worktrees ao lado |
| Executável do jogo | **não tem** | existe no dump do mantenedor: pergunte o caminho na F0 |
| Build do jogo | Linux (`build.sh`), só para compilar e testar | Windows, MSYS2 CLANG64 (`build.sh`), como o CI |
| Testes só do Windows | não rodam (`test_qte_hook`, snapshots do launcher) | rodam |
| `test_signatures` com o executável | não roda | roda, com o `eboot.elf` preparado |
| Push e PR | em geral recusado (403): entrega por bundle (§3.5) | direto, **com autorização** do mantenedor |
| Build Windows para teste | workflow Windows zip, disparado pelo mantenedor | workflow Windows zip, ou o pacote local (`packaging/windows/package.sh`) |
| Ler o `last_run.log` | o mantenedor envia o arquivo | ler direto da pasta do port (`<port>\user\last_run.log`), depois que o mantenedor jogar |
| Jogar | **nunca** | **nunca**: o teste no jogo é sempre do mantenedor |

**Regras do modo local:**
- **O executável do jogo fica onde está.** Leia direto do dump, sem copiar para dentro do repo nem
  para worktrees, e sem anexar a issues ou PRs. Saídas de análise (endereços e trechos curtos de
  disassembly) podem ir para documentos da pesquisa; o arquivo inteiro, nunca.
- **Não mexa na instalação do jogo do mantenedor.** Builds de teste vão para uma pasta separada (por
  exemplo `C:\Projetos\Projects\GOW3PC-test\<fase>`). Faça backup de `user\` (saves e configurações)
  antes de qualquer teste que possa escrever nela.
- **Não abra o jogo.** Prepare a build e o roteiro e avise o mantenedor. Ele joga, e depois você lê
  o log.
- **A carga é do PC do mantenedor:** no máximo 2 builds pesados ao mesmo tempo, e pergunte antes de
  rodar o empacotamento completo.
- **Não toque nas worktrees de outros trabalhos** (por exemplo as do Codex). Crie as suas com nomes
  da fase.
- **Comandos:** use Git Bash ou o shell do MSYS2 para `build.sh`, e PowerShell ou Git Bash para
  `cargo` e `gh`.

---

## 3. Como o motor e o "Alto FPS" funcionam hoje

Leia isto antes de qualquer fase. Os caminhos são da `main` em `f593377`.

### 3.1 A taxa do motor

- O jogo foi feito para **59,94 FPS**. O **patch de FPS** (`patches/CUSA01623.xml`, entradas
  "Frame Rate Patch - 100/120/144/240 FPS") troca a constante 59,94 (`0x75dbe4` no v01.02) pela taxa
  alta. Com isso, o "frametime clamp" do jogo produz um delta de jogo de 1/N s, e **todo o motor
  passa a andar em passos menores**.
- **Precedente de correção de causa raiz:** o mesmo patch corrige a "swing energy". A instrução em
  `0x48ff1a` lia a taxa do motor e passou a ler a constante 60,0 (`0x6fc0d8`), para que esse cálculo
  continue igual ao de 60 Hz. **Esse é o modelo das correções da F4.**
- **Troca ao vivo:** `src/probe.c` acha o setter da taxa (`engine_fps_setter`, `find_engine_fps`) e
  guarda os dois globais que o jogo lê a cada quadro: um float e um inteiro arredondado. A troca
  passa por estas funções:
  - `SpartanSettings::SyncEngineFps()` (`gpu/shim/spartanport_settings.cpp`) escreve a taxa nesses
    globais;
  - `EngineFpsNow()` devolve a taxa em vigor: `min(engine_fps, applied_engine_fps)`;
  - `applied_engine_fps` é o teto desta execução, vindo de `SPARTAN_ENGINE_FPS` do runner;
  - `CycleEngineFps()` é o atalho R3 + L1 + R1 / F9 (`src/runtime_pad.c`, `engine_fps_combo` → `spartanportgpu_engine_fps_cycle()`).
- **Taxas válidas:** `SpartanGraphics::EngineFps = {60, 100, 120, 144, 180, 240}`
  (`gpu/shim/spartanport_graphics.h`).
- **Persistência:** `engine_fps` é salvo em `spartanport.ini`. **Uma queda automática para 60 nunca
  pode ser salva** (ver F2).
- **No log:** `Engine frame rate: X FPS, live up to Y` na partida, `Engine FPS: N` a cada troca (`SyncEngineFps`), e `Engine FPS: live frame rate unavailable`
  quando a assinatura não bate.

### 3.2 QTEs

`src/qte_hook.h` documenta o sistema de prompts:

- `0x12c670` inicia um prompt e grava o tipo em `+0x10` do bloco de estado (0–3 botões, 4–9
  rotações).
- `0x12c4a0` é a atualização do prompt e roda **a cada quadro enquanto ele está na tela**. O hook de
  atualização incrementa `SpartanQteState.heartbeat`.
- **"QTE ativo"** = o `heartbeat` mudou nos últimos `QTE_ACTIVE_US`. `src/runtime_pad.c`,
  `qte_rotation()`, já usa essa regra para os prompts de analógico.
- **Outros hooks:** `timed` (aperto no tempo certo), `repeat` (mash, `SCR_RepeatPress`), `charged`
  (segurar) e `circle`. O Automatic QTE (`spartanport_auto_qte_flag`) responde por eles.
- **A #1092** (aberta) corrige o dreno do mash em taxa alta dentro do stub `repeat`. Ela é uma
  correção de causa para o mash e passa a competir com a F2: ver F2.6.
- **Atenção:** não está confirmado que **todos** os tipos de prompt (mash, timed, charged) passam
  pela atualização `0x12c4a0`. A F2 precisa verificar isso antes de confiar só no `heartbeat`.

### 3.3 Arquivos e áreas do jogo

- Toda abertura de arquivo do jogo passa por `do_open()` em `src/runtime_file.c` (exposto como
  `runtime_file_open`). Os níveis do jogo são WADs em `/app0/EXEC/WAD/PS4_LE/<NOME>.WAD`.
- O próprio jogo escreve no log linhas `WAD_STATS: WAD <NOME> ...` (exemplo de uma sessão real:
  `R_PosHorse00`).
- O formato dos WADs está descrito em `scripts/glyphs.py`.
- Mudar o WAD de nível como sinal de "área atual" é a hipótese da F3. A granularidade precisa ser
  confirmada lá.

### 3.4 Como pesquisar o executável

- **Método que funcionou** (`docs/moves.md`, seção "How this was found"): um **build só de
  registro**. São hooks que copiam o que o jogo faz para um buffer circular, e a thread do controle
  escreve esse buffer no `last_run.log`. Depois, uma sessão curta no trecho em questão. Registre
  só mudanças, porque muitas funções rodam a cada quadro.
- **O executável de referência** é o `eboot.bin` do CUSA01623 v01.02 (sha256
  `d85c8135d330c3b601bf5dc3f1dd86bd6119fb8a9fce372bed2c9785f9a79299`). O
  `tests/test_signatures.cpp` recebe o `eboot.elf` preparado como argumento.
- **Decisão do mantenedor (2026-10-29): o executável não sai do PC dele.** A pesquisa segue assim:
  - **Modo nuvem:** o agente escreve a ferramenta de análise (F4.0). O mantenedor a roda no próprio
    dump e cola a saída na conversa. Depois, cada hipótese é confirmada por um build de registro que
    ele joga.
  - **Modo local:** o agente roda a mesma ferramenta direto no dump do mantenedor, sem copiar o
    arquivo (§2, "Onde o agente roda"), e lê o `last_run.log` depois das sessões dele.
- **Ferramentas úteis no Linux:** `objdump -D -b binary -mi386:x86-64 --start-address=…`, ou
  `rizin`/`radare2` se instaláveis, e um script Python que procura referências RIP-relative a um
  endereço (`lea`/`mov` com `[rip+disp32]`).

### 3.5 Build, testes e entrega

- **Linux (container):**
  - `bash build.sh` e `bash build.sh --test` (testes C);
  - `cmake --build out/gpu --target <alvo>` para testes de GPU e settings (`graphics-settings-test`,
    `ui-language-test`, `menu-confirm-swap-test` e outros);
  - `cargo test -j 2`, `cargo fmt --check` e `cargo clippy --all-targets -- -D warnings` em
    `launcher-rust/`.
- **Testes que só rodam no Windows:**
  - `tests/test_qte_hook.cpp` e os outros testes de stub que incluem `<windows.h>`;
  - as comparações de imagem do launcher (`launcher-rust/tests/ui_snapshots.rs`).

  Se uma página do launcher mudar, as imagens de referência precisam ser regeradas no Windows
  (apague as PNGs afetadas e rode `cargo test --test ui_snapshots` duas vezes).
- **Build Windows de teste:** workflow **Windows zip** (`.github/workflows/windows-zip.yml`), rodado à
  mão na branch, sem tag. O zip fica como artefato, e o build leva de 12 a 20 minutos.
- **Se a sessão não puder dar push** (o GitHub recusa com 403):
  1. Gere `git bundle create <arquivo>.bundle origin/main..<branch>`.
  2. Entregue o bundle ao mantenedor, junto com um script único para o Git Bash dele. O script faz
     `git fetch` do bundle, `git push`, `gh workflow run windows-zip.yml --ref <branch>`,
     `gh run watch` e `gh run download`.
  3. O repositório local do mantenedor fica em `C:\Projetos\Projects\GOW3PC`.
- **Modo local:**
  - faça o push da branch da fase (com autorização) e dispare o Windows zip com
    `gh workflow run windows-zip.yml -R FelipePrado0/SpartanPort --ref <branch>`;
  - ou, para iterar mais rápido em builds de registro, compile no MSYS2 CLANG64 (`bash build.sh`) e
    monte o pacote numa pasta de teste separada.

  Em qualquer caso, rode os testes só do Windows antes de entregar.
- **Ambiente de teste do mantenedor:** Windows, RX 6700 XT, monitor de 240 Hz, CUSA01623 v01.02,
  controle de PS4 e headset Logitech G733.

---

## 4. Fases

### Progresso (29/10/2026)

**F0, concluída (modo nuvem):**
- [x] `build.sh --test`, `graphics-settings-test`, `ui-language-test`, `menu-confirm-swap-test` e, em `launcher-rust`, `cargo test` (1163 passed), `fmt` e `clippy`: tudo verde na `main` `f593377`. Nenhuma falha pré-existente.
- [x] Modo nuvem registrado: a análise do executável será feita pelo mantenedor com a ferramenta da F4.0.
- [ ] `last_run.log` de referência a 240 FPS: pedir ao mantenedor.

**F1, catálogo pronto:**
- [x] 846 issues lidas; `docs/high-fps/catalog.md` com grupos A (travessia/pulo), B (QTE/mash), C (áudio) e D (progressão), descartadas e roteiro de confirmação. A balista é a #812.
- [x] Saem do ponto de partida, por acontecerem a 60: #620 e #1001, diálogo duplicado do Hefesto (#652, #931, #1002), #988 e lógica acelerada (#1083, #1027, #1074). O chute da #1085 é do Automatic QTE; só o empurrar é de FPS.
- [x] Rascunho bilíngue da issue de acompanhamento em `docs/high-fps/tracking-issue-draft.md` (não publicado).
- [ ] Mantenedor roda o roteiro de confirmação (240 e 60) e devolve os resultados.
- [ ] Publicar a issue de acompanhamento (com autorização).

**F2, implementada:**
- [x] Teto do motor nunca salvo, `qte_active` com histerese de 500 ms (depois 1,5 s, abaixo), opção `auto_safe_fps` (overlay em todos os idiomas e launcher Rust, página Performance). TDD registrado em `docs/specs/2026-10-29-high-fps-f2-qte.md`.
- [x] Linux: `build.sh --test` rc=0, `ui-language-test` (220 textos), `cargo test` 1166 passed, `fmt`, `clippy`.
- Decisões: o log sai da thread de apresentação (a do controle só usa atômicos); a linha de entrada inclui o tipo do prompt (`prompt type T`); a apresentação também cai para 60 durante o QTE; `SPARTAN_AUTO_SAFE_FPS` também liga/desliga.
- [ ] Testes só do Windows (`test_qte_hook`) e regerar `launcher-rust/snapshots/performance-{1920x1080,1280x800}-{en,pt,ar}.png`.
- [x] Primeiro teste no jogo (30/10, Windows, RX 6700 XT, motor a 240, teclado, QTEs do cavalo do
      Poseidon no início do jogo): só 3 quedas, todas `prompt type 3`, de 1 a 4 s cada, e os QTEs
      **não** ficaram a 60. Confirmou o risco: só a atualização `0x12c4a0` movia o `heartbeat`; os
      prompts timed, repeat, charged e circle não moviam nada, e a queda saía com o prompt na tela.
- [x] Correção (spec `docs/specs/2026-10-30-high-fps-f2-qte-all-kinds.md`): os 4 stubs do Automatic QTE
      contam os quadros do seu tipo em `SpartanQteState.kinds[4]`, sempre (Auto QTE ligado ou não),
      com um prefixo de 30 bytes que preserva registradores, flags e red zone; "QTE ativo" = qualquer
      um dos 5 contadores; histerese de 500 ms para 1,5 s e depois 5 s (sequências de apertos únicos); log com os tipos
      (`kinds: update,repeat`) e `QTE cap held 20 s, kinds: ...` uma vez por queda longa (denuncia um
      site que roda fora de QTE). O giro do analógico pelo teclado/mouse continua só no `update`.
- [x] Linux: `test_qte_hook` rodado com um `windows.h` de mmap (scratch, não versionado), com um
      mutante que mexe nas flags pego pelo teste; `pad-test`, `graphics-settings-test` e
      `build.sh --test` verdes.
- [x] Reteste no jogo (30/10/2026, build do Windows zip `40980000336`, head `f9903c5`): o mantenedor confirmou que os QTEs do cavalo do Poseidon ficam a 60 FPS.
- [x] Espera depois do QTE aumentada para 5 s (30/10): a 1,5 s o motor voltava a 240 entre os botões de uma sequência.
- [x] Reteste com 5 s (30/10, build `40984214345`, head `a899c61`), validado pelo mantenedor: as 2 sequências do cavalo do Poseidon ficaram a 60 do começo ao fim, uma queda por sequência (`kinds: update,timed`), com janelas de `Frame stats: 60.0 FPS` durante a queda; 2 mashes também caíram (`kinds: repeat`). Nenhum `QTE cap held 20 s`.
- [x] F2.5 itens 2 a 5: troca de taxa durante o QTE, reabrir o jogo (`engine_fps` ≠ 60), opção desligada, minutos sem QTE sem ficar preso a 60. Validados pelo mantenedor; os logs de 31/10 mostram `engine_fps=240` no ini depois de várias sessões com QTE e a volta a 240 depois de cada QTE.
- [ ] `test_qte_hook` no Windows.
- [ ] Decisão F2.6 sobre a #1092 (atenção: o stub repeat dela tem 122 bytes; com o prefixo de
      contagem passa da vaga de 128, que precisaria crescer).

**F4.0, pronta:**
- [x] `tools/find_rate_readers.py` com 4 testes sintéticos (TDD registrado na spec `docs/specs/2026-10-29-high-fps-f40-rate-readers.md`). Usa o GNU objdump, se houver.
- [ ] Mantenedor roda no `eboot.bin` e cola a saída; a última linha deve confirmar o caso 0x48ff1a.

### F0. Preparação e linha de base

**Objetivo:** saber o estado atual antes de mudar qualquer coisa.

- [ ] Criar a branch da fase a partir da `main` atual.
- [ ] Compilar e rodar os testes no Linux. Anotar qualquer teste que já falhe na `main`: falha
      pré-existente não é culpa da fase.
- [ ] Ler `src/qte_hook.h`, `src/graphics_hook.h`, `src/runtime_pad.c` (seções QTE e
      `engine_fps_combo`), `gpu/shim/spartanport_settings.cpp` (`SyncEngineFps`, `EngineFpsNow`,
      `CycleEngineFps`) e `docs/moves.md`.
- [ ] Identificar o modo (nuvem ou local) e registrar aqui.
- [ ] **Modo local:**
  - perguntar ao mantenedor o caminho do dump do jogo (a pasta com o `eboot.bin`) e o da instalação
    do port usada nos testes (a do log de referência era `E:\Games\ps4\Jogos\spartanport-windows`);
  - registrar os dois aqui;
  - conferir o sha256 do `eboot.bin` (§3.4);
  - **nunca fazer commit nem copiar esse arquivo.**
- [ ] **Modo nuvem:** registrar que a análise do executável será feita pelo mantenedor com a
      ferramenta da F4.0.
- [ ] Pedir ao mantenedor um `last_run.log` de uma sessão normal a 240 FPS, como referência. No modo
      local, ler direto da pasta do port.

**Pronto quando:** os testes passam (ou as falhas pré-existentes estão anotadas), e o modo e os
caminhos estão registrados.

### F1. Catálogo e triagem dos bugs de FPS alto

**Objetivo:** uma lista única de cada sintoma, agrupada pela **mecânica** que provavelmente o causa,
com a confirmação de que o bug **some a 60 FPS**. Sintoma que também acontece a 60 não é bug de FPS:
é do jogo ou de outra parte do port, e sai do catálogo.

**Entregáveis:**
- `docs/high-fps/catalog.md`, uma linha por sintoma, com as colunas:
  - ID e issues ligadas;
  - capítulo ou área e local exato;
  - sintoma;
  - taxa em que quebra e se some a 60 (sim / não / não testado);
  - mecânica suspeita;
  - save disponível (sim/não);
  - estado: aberto, coberto pela F2/F3 ou corrigido pela F4.
- Uma issue de acompanhamento no GitHub, bilíngue, que linka o catálogo e pede saves e relatos à
  comunidade.

**Ponto de partida** (issues achadas numa busca em 2026-10-29; **todas a confirmar**):

| Grupo provável | Issues | Observação |
|:--|:--|:--|
| Pulo / travessia | #620 e #1001 (salto no Realm of Hades), #910 (pulo na Gaia), #965 (descida pelas vinhas, luta com Poseidon) | candidatos fortes a uma rotina de pulo ou impulso |
| Colisão / parede | #839 (parede invisível em Chains of Balance), #1011 (personagem preso) | a #1011 tem relato de que o bug existe no jogo original (PS3): confirmar a 60 |
| QTE / mash | #44 (levantar a tora), #589 (empurrar a árvore), #911 (QTE), #811 (crash no finisher do centauro) | #44 e #589 fechadas sem reprodução; ver a #1092 |
| Áudio de cutscene | #652, #931 e #1002 (diálogo duplicado do Hefesto, relatado a 120 FPS), #885 (áudio faltando em FPS alto), #988 (dessincronia a 60) | pode ser temporização de áudio ligada à taxa |
| Lógica acelerada | #1083 (VSync 60 acelera a lógica desde a 2.5.0) | inverso: a taxa real e a do motor divergem |
| Progressão (a classificar) | #640, #736, #768, #1019 | ler cada uma; muitas podem não ser de FPS |

**Passos:**
- [ ] Listar todas as issues (abertas e fechadas) com
      `gh api "repos/FelipePrado0/SpartanPort/issues?state=all&per_page=100&page=N"`, ignorando
      PRs. Filtrar por: preso, parede, pulo, salto, escalar, cair, QTE, mash, botão, "can't
      pass/progress", FPS, 120, 144, 240, áudio de cutscene e diálogo duplicado.
- [ ] Ler cada candidata com os comentários. Anotar a versão do port, a taxa do motor (pela versão
      ou pelo log anexado) e o local exato.
- [ ] Preencher o catálogo e agrupar por mecânica.
- [ ] Montar para o mantenedor um **roteiro de confirmação**: para cada linha sem confirmação, o
      local, como chegar lá e o teste a 240 e depois a 60 com R3 + L1 + R1. Ele roda o roteiro e
      devolve os resultados.
- [ ] Atualizar o catálogo com os resultados. Remover o que não é de FPS e comentar na issue
      correspondente (bilíngue), com autorização.
- [ ] **Apresentar a spec da F1** (o catálogo e a issue de acompanhamento) e esperar a aprovação
      antes de abrir a issue no GitHub.

**Pronto quando:** cada linha do catálogo tem mecânica suspeita e estado de confirmação, e a issue de
acompanhamento foi aberta.

### F2. Rede de segurança A: motor a 60 durante QTEs

**Objetivo:** enquanto um QTE (qualquer tipo, inclusive o mash) estiver na tela e o motor estiver
acima de 60, o motor roda a 60. Quando o prompt termina, ele volta à taxa que o jogador escolheu.

#### F2.1 Spec a apresentar (texto base)

- **Objetivo:** QTEs a 60 FPS em qualquer taxa do motor, sem o jogador precisar fazer nada.
- QUANDO um QTE ficar ativo e a taxa em vigor for maior que 60, O SISTEMA DEVE escrever 60 nos
  globais do motor em até 1 quadro.
- QUANDO o QTE terminar (sem quadro novo por `QTE_ACTIVE_US`), O SISTEMA DEVE voltar à taxa que
  estaria em vigor sem a queda.
- QUANDO a queda estiver ativa, O SISTEMA NÃO DEVE salvar 60 em `engine_fps` no `spartanport.ini`.
- QUANDO o jogador trocar a taxa (menu ou R3 + L1 + R1) durante a queda, O SISTEMA DEVE guardar a
  escolha e aplicá-la quando a queda acabar.
- QUANDO a opção "Queda automática para 60" estiver desligada, O SISTEMA DEVE se comportar como hoje.
- QUANDO a queda começar ou terminar, O SISTEMA DEVE escrever uma linha no `last_run.log`:
  `Engine FPS: 60 during a QTE (was N)` e `Engine FPS: back to N after the QTE`.
- **Opção:** `auto_safe_fps=1` (ligada por padrão) no `spartanport.ini`; uma linha no overlay
  (Insert > Imagem ou Desempenho, junto da taxa do motor) e no launcher Rust (página Performance ou
  Graphics, onde estiver a taxa do motor), com traduções.

#### F2.2 Desenho

- **Camada de override, não de configuração.** Acrescentar a `SpartanSettings::Values` um teto
  temporário, por exemplo `std::atomic<int> engine_fps_cap{0}` (0 = sem teto), que **não** é lido
  nem salvo pelo ini. `EngineFpsNow()` passa a ser
  `min(engine_fps, applied_engine_fps, cap ? cap : ∞)`. `SyncEngineFps()` continua sendo o único
  lugar que escreve nos globais do motor.
- **Quem detecta o QTE:** a thread do controle (`src/runtime_pad.c`), que já lê o `heartbeat` em
  `qte_rotation()`.
  - Extrair a regra "QTE ativo" para uma função própria (`qte_active(now)`).
  - Chamar uma API nova do lado GPU, por exemplo `spartanportgpu_engine_fps_cap(int fps)`, que grava
    o teto e chama `SyncEngineFps()`.
  - Seguir o padrão das outras chamadas `spartanportgpu_*` entre `src/` e `gpu/`
    (`gpu/spartanportgpu.h`).
- **Histerese:** entrar na queda no primeiro quadro novo do prompt e sair só depois de
  `QTE_ACTIVE_US` sem quadro. Não oscilar entre prompts seguidos: se outro prompt começar dentro
  de, por exemplo, 500 ms, a queda continua.
- **Cobertura dos tipos de prompt:** antes de confiar só no `heartbeat`, confirmar com o mantenedor,
  por um build de registro ou pelo log, que o mash (`repeat`), o `timed`, o `charged` e o `circle`
  também movem o `heartbeat`. Se algum não mover, fazer o stub dele incrementar o mesmo contador.
  Isso é um stub novo em `src/qte_hook.h`, com teste em `tests/test_qte_hook.cpp`.
- **Transição:** a troca de taxa muda o delta do jogo no quadro seguinte. Registrar no log o quadro
  da troca e pedir ao mantenedor que observe se há tranco visual ou de som.

#### F2.3 Arquivos prováveis

- `gpu/shim/spartanport_settings.h/.cpp`: o teto, `EngineFpsNow`, `auto_safe_fps` (Set/Save).
- `gpu/spartanportgpu.h` e `gpu/shim/spartanportgpu.cpp`: a API do teto.
- `src/runtime_pad.c`: `qte_active()` e a chamada da queda.
- `gpu/shim/spartanport_overlay.cpp` e `gpu/shim/ui_language/*.h`: a linha da opção e as traduções.
- `launcher-rust/src/core/catalog.rs`, `options.rs`, a página (`ui/pages/…`),
  `assets/i18n.json`, `fixtures/collected/spartanport.ini` e os testes. Siga como a #1007 adicionou
  `menu_confirm_swap`.
- `src/qte_hook.h` e `tests/test_qte_hook.cpp`: só se algum tipo de prompt precisar de heartbeat.

#### F2.4 Testes automáticos

- [ ] `tests/test_graphics_settings.cpp`, ou um teste novo: `EngineFpsNow()` com e sem teto; o teto
      nunca salvo no ini; a troca manual durante o teto é guardada e aplicada depois.
- [ ] Um teste C da regra `qte_active()` com relógio simulado, incluindo a histerese entre prompts
      seguidos.
- [ ] Launcher Rust: valor padrão e persistência de `auto_safe_fps`, e a linha na página. Regerar as
      imagens de referência da página no Windows (§3.5).
- [ ] `ui-language-test` passa com os textos novos em todos os idiomas.

#### F2.5 Validação no jogo (roteiro para o mantenedor)

1. Motor a 240, opção ligada. Em três QTEs diferentes (botão, rotação de analógico e mash, por
   exemplo a tora da #44), o log deve mostrar a queda e a volta, e o QTE deve se comportar como a 60.
2. Trocar para 144 com R3 + L1 + R1 durante um QTE. Depois do QTE, o motor deve estar a 144.
3. Fechar e reabrir o jogo. O `engine_fps` no `spartanport.ini` não pode ser 60.
4. Opção desligada: nenhuma queda no log.
5. Anotar qualquer tranco na entrada ou na saída do QTE.

#### F2.6 Decisão sobre a #1092

Com a F2 pronta, o mash roda a 60 e a #1092 deixa de ser necessária para jogar. Apresente ao
mantenedor as duas opções:
- **(a) Fechar a #1092** com agradecimento: a F2 cobre o caso.
- **(b) Manter a #1092** como correção de causa: ela entra na F4 e permite tirar o mash da queda.

**Pronto quando:** os testes passam e o mantenedor validou os itens 1 a 4 no jogo.

### F3. Rede de segurança B: motor a 60 por área

**Objetivo:** áreas do catálogo que ainda não têm correção de causa rodam a 60 enquanto o jogador
estiver nelas.

#### F3.1 Pesquisa primeiro (sem mudar comportamento)

- [ ] Adicionar um log só de registro em `do_open()`: cada abertura de
      `/app0/EXEC/WAD/PS4_LE/*.WAD`, com o horário relativo. Registrar só mudanças.
- [ ] Pedir ao mantenedor uma sessão que passe por duas ou três áreas do catálogo, anotando o horário
      em que chegou em cada trecho problemático.
- [ ] Com o log, decidir o **sinal de área**:
  - o WAD de nível aberto por último (`<NOME>.WAD`);
  - um conjunto de WADs carregados;
  - ou, se a granularidade for grossa demais (uma fase inteira a 60 por causa de um pulo), um
    sinal mais fino, achado por build de registro, como um id de checkpoint ou de zona do jogo.
- [ ] Registrar a decisão neste documento antes da spec.

#### F3.2 Spec a apresentar (texto base)

- **Objetivo:** motor a 60 nas áreas listadas, só enquanto o jogador estiver nelas.
- QUANDO o sinal de área corresponder a uma entrada da lista, O SISTEMA DEVE aplicar o teto de 60 da
  F2.
- QUANDO o jogador sair da área, O SISTEMA DEVE retirar o teto, salvo se um QTE estiver ativo.
- QUANDO uma área entrar ou sair, O SISTEMA DEVE registrar no log `Engine FPS: 60 in area <NOME>`
  e `Engine FPS: area <NOME> left`.
- A lista fica em um arquivo de dados versionado, por exemplo `patches/safe_fps_areas.txt`, uma área
  por linha com o ID do catálogo em comentário, para que tirar uma área seja uma linha a menos.
- A opção `auto_safe_fps` da F2 liga e desliga as duas redes.

#### F3.3 Desenho e testes

- O teto da F2 vira a combinação de dois motivos: `qte` e `area`. O teto vale enquanto houver pelo
  menos um motivo ativo. Testar as combinações.
- Teste de parsing da lista e de correspondência de nomes (maiúsculas e minúsculas, extensão).
- O runner (launcher Rust) copia ou aponta a lista, se for preciso: siga como os patches são
  passados hoje.

#### F3.4 Validação no jogo

- Para cada área da lista: entrar a 240, o log mostra a queda, o trecho passa, e ao sair o log mostra
  a volta.
- Uma área fora da lista não pode cair para 60.

**Pronto quando:** as áreas confirmadas no catálogo que ainda não têm correção de causa estão na lista
e foram validadas.

### F4. Correções de causa raiz (uma por mecânica)

**Objetivo:** para cada grupo do catálogo, achar a rotina do jogo que assume quadros de 1/60 s e
corrigi-la, para que funcione igual a 60 em qualquer taxa. Ao final, a área ou o tipo de prompt sai
da rede de segurança.

**Pré-requisito:** a ferramenta da F4.0 e acesso à saída dela: colada pelo mantenedor no modo
nuvem, ou rodada direto no dump no modo local (§3.4).

#### F4.0 Ferramenta de análise (fazer uma vez, antes da primeira mecânica)

**Objetivo:** listar, no executável do jogo, **todo código que lê a taxa do motor**, sem que o arquivo
saia do PC do mantenedor.

- **Arquivo:** `tools/find_rate_readers.py`. É Python 3 puro, sem dependências, e roda com
  `python -I`. Ele usa o `llvm-objdump`/`objdump` só se estiver no PATH.
- **Entrada:** o caminho do `eboot.elf` (o preparado pelo port) ou do `eboot.bin`.
- **O que faz:**
  1. Acha o setter da taxa com a mesma assinatura de `engine_fps_setter` (`src/probe.c`) e calcula os
     endereços dos dois globais da taxa (o float e o inteiro arredondado). Implemente a busca de
     assinatura com `??` como coringa, igual a `src/signature.h`.
  2. Acha a constante 59,94 (`0x426fc28f`), a 60,0 (`0x42700000`) e a 1/60 (`0x3c888889`) nos dados.
  3. Percorre o código procurando instruções com `[rip+disp32]` que apontam para esses endereços. No
     mínimo: `lea`, `mov`, `movss`/`vmovss`, `cvtsi2ss` e as formas VEX de `mulss`/`divss`/`addss`.
     Na dúvida, aceite qualquer instrução cujo disp32 resolva para o alvo, e marque como "a
     confirmar".
  4. Para cada leitor, imprime o offset no executável e o endereço virtual, qual alvo ele lê, 48
     bytes em hex ao redor e, se houver `objdump`, a disassembly de ±16 instruções. Agrupa os
     leitores por função (do `push rbp` anterior até o `ret` seguinte) e diz quantos leitores cada
     função tem.
  5. Confere os casos conhecidos: o leitor da "swing energy" em `0x48ff1a` (v01.02) **precisa
     aparecer**. Se não aparecer, a ferramenta está errada.
- **Saída:** texto, para colar na conversa (modo nuvem) ou salvar em
  `docs/high-fps/readers-<data>.txt` (modo local). Só endereços e trechos curtos, nunca o arquivo.
- **Teste:** `tests/test_find_rate_readers.py`, com um "executável" sintético montado no teste (um
  setter falso com a assinatura, um global e três leitores) e a contagem esperada de leitores. Não
  usa arquivos do jogo.
- **Spec curta** antes de implementar, como sempre.

#### F4.1 Método (repetir por mecânica)

1. **Hipótese:** qual cálculo depende do delta. Exemplos:
   - impulso de pulo aplicado por quadro;
   - gravidade integrada com passo fixo;
   - contador de quadros usado como timer;
   - colisão testada só no fim do passo.
2. **Achar quem lê a taxa:** use a saída da F4.0. Cada leitor é um candidato. O caso da "swing
   energy" (`0x48ff1a` → 60,0) é exatamente um leitor que não devia escalar com a taxa.
   - Priorize as funções com leitores que ficam perto do sintoma: uma função de pulo, de gravidade,
     de colisão, de áudio.
   - Se nenhum leitor explicar o sintoma, a rotina provavelmente conta quadros sem ler a taxa. Nesse
     caso, um build de registro mais largo (passo 3) mostra quais valores mudam entre 60 e 240.
3. **Build de registro** no trecho do catálogo: registre os valores do candidato, como velocidade
   vertical, altura máxima ou contadores, a 60 e a 240, na mesma ação. A diferença confirma a causa.
   - **Modo nuvem:** entregue a build por bundle e Windows zip (§3.5), mais um roteiro de 3 a 5
     linhas: onde ir, o que fazer a 60 e a 240 com R3 + L1 + R1, e quais linhas do log enviar.
   - **Modo local:** compile e monte a build na pasta de teste. Peça ao mantenedor que jogue o
     roteiro e leia o `last_run.log` direto.
   - Em ambos: uma hipótese por rodada, registrando só mudanças, com um prefixo fácil de filtrar no
     log (por exemplo `HighFPS: jump ...`). Retire o build de registro antes da PR de correção.
4. **Correção, uma de três formas, em ordem de preferência:**
   - **(a)** apontar a leitura para a constante 60,0, como na "swing energy": uma linha no
     `patches/CUSA01623.xml`, com `Pattern`/`At`;
   - **(b)** um stub que escala o valor pelo `60 / taxa` ou compensa o passo, como a #1092 faz no
     mash. Segue o padrão de hooks de §2, com teste;
   - **(c)** substepping: chamar a rotina em passos fixos de 1/60 s acumulando o delta. Só quando
     (a) e (b) não servem, porque é mais arriscado.
5. **Teste automático** do patch ou stub e da assinatura no executável de referência.
6. **Validação no jogo** de todos os trechos do grupo a 240, com a rede de segurança **desligada**
   para aquele grupo.
7. **Tirar da rede:** remover as áreas do grupo de `patches/safe_fps_areas.txt` e atualizar o
   catálogo.

#### F4.2 Ordem sugerida

1. **Pulo / travessia:** mais relatos (#620, #1001, #910, #965) e provavelmente uma rotina só.
2. **QTE / mash:** a #1092 já é a correção do mash. Revalidar com o método acima e decidir junto
   com a F2.6.
3. **Colisão / parede:** #839 e outras confirmadas.
4. **Áudio de cutscene:** #652, #931, #1002 e #885.
5. **O resto do catálogo**, por número de relatos.

Cada mecânica é uma spec aprovada antes e commits próprios na `feat/high-fps`.

**Pronto quando:** cada grupo confirmado tem correção validada no jogo, ou está explicitamente
mantido na rede de segurança com o motivo no catálogo.

### F5. Validação final e release

- [ ] **Passada completa** pelo mantenedor, a 240 FPS e com a rede de segurança **ligada**, do
      começo ao fim. Para cada capítulo, guardar o `last_run.log` e anotar quedas para 60 inesperadas,
      travamentos e QTEs.
- [ ] **Passada focada**, com a rede **desligada**, nos trechos do catálogo: confirma que as
      correções da F4 seguram sozinhas.
- [ ] Revisar os logs: quantas quedas por sessão, quanto tempo a 60, e se alguma área não listada
      caiu.
- [ ] Atualizar o README (a seção da taxa do motor) e as notas de release: o que é corrigido na
      causa, o que ainda cai para 60 e como desligar.
- [ ] Responder e fechar as issues do catálogo com comentário bilíngue (com autorização).
- [ ] Marcar este roadmap como concluído na tabela de estado.

---

## 5. Entregáveis por fase

| Fase | Código | Docs | GitHub |
|:--|:--|:--|:--|
| F0 | — | estado e resposta sobre o executável neste arquivo | — |
| F1 | — | `docs/high-fps/catalog.md` | issue de acompanhamento |
| F2 | teto do motor, `qte_active`, opção `auto_safe_fps` | este arquivo e o README (opção) | — |
| F3 | sinal de área, lista de áreas | este arquivo e o catálogo | — |
| F4 | `tools/find_rate_readers.py` (F4.0), patches e stubs por mecânica | o catálogo e `docs/moves.md`, se houver pesquisa nova | — |
| F5 | — | README e notas de release | PR única de `feat/high-fps` e issues fechadas |

## 6. Formato de cada entrega ao mantenedor

Ao fim de cada fase, nesta ordem:

1. **Resumo curto:**
   - o que mudou;
   - quais testes rodaram e o resultado;
   - o que não foi testado.
2. **Build Windows:** um link da execução do Windows zip, ou um bundle mais um script único (§3.5).
3. **Roteiro de teste** em português, com no máximo 2 linhas por item: o que fazer, o que esperar e
   que linha do log confirma.
4. **Pedido do `last_run.log`** de cada teste.
5. Depois da validação: a mensagem da PR no formato do repo (EN e PT-BR, `Closes #…`) e as
   mensagens para as issues afetadas.

## 7. Riscos e como lidar

| Risco | Efeito | Mitigação |
|:--|:--|:--|
| Tranco ao trocar de taxa no meio do jogo | engasgo de 1 quadro, som estalando | registrar e medir na F2; se incomodar, suavizar ou trocar só na borda do QTE |
| Um tipo de QTE não move o `heartbeat` | a queda não acontece nele | F2.2: confirmar todos os tipos e adicionar heartbeat aos stubs que faltarem |
| Sinal de área grosso demais | uma fase inteira a 60 | F3.1: escolher sinal mais fino; priorizar a correção daquela mecânica na F4 |
| Executável só no PC do mantenedor | rodadas a mais na F4 no modo nuvem | ferramenta da F4.0 rodada por ele e builds de registro focados; no modo local, análise direta no dump |
| Correção de causa muda a dificuldade | o jogo fica diferente de 60 | validar contra 60 no mesmo trecho; a referência é sempre 60 |
| Bug do jogo original confundido com bug de FPS | trabalho perdido | F1: confirmar a 60 antes de qualquer código |
| Imagens de referência do launcher desatualizadas | CI do Windows falha | regerar no Windows (§3.5) em toda PR que muda uma página |

## 8. Glossário

- **Taxa do motor:** os FPS em que a lógica do jogo avança (60 a 240), diferente da taxa de exibição.
- **Delta:** o tempo de jogo de um quadro (1/taxa).
- **Rede de segurança:** a queda automática para 60 (F2/F3).
- **Causa raiz:** a rotina do jogo que assume quadros de 1/60 s.
- **Build de registro:** build com hooks que só registram valores no log, sem mudar o jogo.
- **Teto:** o limite temporário da taxa do motor que a rede de segurança aplica. Nunca é salvo.
