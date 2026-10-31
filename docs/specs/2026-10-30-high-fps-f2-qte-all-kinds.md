# F2, correção: a queda para 60 em todos os tipos de QTE

Continuação de `docs/specs/2026-10-29-high-fps-f2-qte.md`. Branch: `feat/high-fps`.

**Teste do mantenedor que motivou a correção** (build da F2, Windows, RX 6700 XT, motor a 240,
teclado, QTEs do cavalo do Poseidon no início do jogo): o log mostrou só 3 quedas, todas
`prompt type 3`, cada uma de 1 a 4 s, e os QTEs não ficaram a 60. O risco registrado na F2 se
confirmou: só a atualização `0x12c4a0` move o `heartbeat`; os prompts que passam pelos stubs do
Automatic QTE (timed, repeat, charged, circle) não movem nada, e a queda sai cedo.

## Spec (SDD)

* **Objetivo:** como jogador com o motor acima de 60 FPS, quero que todo tipo de QTE (botão,
  mash, segurar, Círculo, giro) rode a 60 do começo ao fim, para passar por eles sem trocar a taxa.
* **Contrato:**
  - `SpartanQteState` (`src/qte_hook.h`) ganha `uint32_t kinds[4]` em `+0x10`: um contador por stub
    do Automatic QTE (timed, repeat, charged, circle). `heartbeat` (+0), `pass` (+4) e `block` (+8)
    ficam onde estão.
  - Os 4 stubs incrementam o seu contador **sempre** (Automatic QTE ligado ou não), antes do
    corpo atual, preservando todos os registradores, as flags e a red zone do jogo.
  - Tipos de QTE no lado do controle: bit 0 `update` (o `heartbeat`), 1 `timed`, 2 `repeat`,
    3 `charged`, 4 `circle` (`SPARTAN_QTE_KIND_*`).
  - "QTE ativo" = algum contador mudou nos últimos `QTE_ACTIVE_US`. O giro de analógico do teclado
    e do mouse continua olhando só o `update` (o tipo em `+0x10` do bloco é dele).
  - API C (substitui `spartanportgpu_engine_fps_cap_qte_type`):
    `spartanportgpu_engine_fps_cap_qte(unsigned kinds, int type)` (tipos ativos na entrada, tipo do
    prompt ou -1) e `spartanportgpu_engine_fps_cap_held(unsigned kinds)`. Só gravam atômicos.
  - Logs: `Engine FPS: 60 during a QTE (was N), kinds: update,repeat[, prompt type T]` na entrada;
    `Engine FPS: QTE cap held 20 s, kinds: ...` uma vez por queda que passe de 20 s seguidos;
    `Engine FPS: back to N after the QTE` na saída (igual).
* **Critérios de aceite:**
  - QUANDO qualquer um dos 5 contadores mudar e a taxa em vigor for > 60 O SISTEMA DEVE aplicar o
    teto de 60 em até 1 quadro, com os tipos ativos no log.
  - QUANDO nenhum contador mudar por `QTE_ACTIVE_US` + `QTE_FPS_HOLD_US` O SISTEMA DEVE retirar o teto.
  - QUANDO um prompt parar de contar por menos que a histerese (ex.: 1 s entre rajadas de mash)
    O SISTEMA DEVE manter o teto.
  - QUANDO o teto durar mais de 20 s seguidos O SISTEMA DEVE escrever uma linha
    `QTE cap held 20 s` com os tipos que ainda contam, uma vez por queda.
  - QUANDO o Automatic QTE estiver desligado O SISTEMA DEVE contar igual (o stub conta e segue o
    código do jogo).
  - QUANDO só os hooks do Automatic QTE estiverem instalados (sem os de giro) O SISTEMA DEVE
    cair para 60 mesmo assim.
* **Requisitos não funcionais:** cada stub cabe na vaga de 128 bytes; o prefixo de contagem não
  muda registradores nem flags (pula a red zone, `pushfq`/`push rax`, `lock inc`, `pop`/`popfq`);
  na thread do controle, só atômicos; no máximo 3 linhas por queda.
* **Arquivos afetados:** `src/qte_hook.h`, `src/probe.c`, `src/runtime_pad.c`,
  `gpu/spartanportgpu.h`, `gpu/shim/spartanport_settings.{h,cpp}`, `tests/test_qte_hook.cpp`,
  `tests/test_pad.c`, `tests/test_graphics_settings.cpp`, o roadmap (seção da F2).
* **Plano técnico:** prefixo de 30 bytes `lea rsp,[rsp-0x80]; pushfq; push rax; movabs rax,cnt;
  lock inc dword [rax]; pop rax; popfq; lea rsp,[rsp+0x80]` (`lea` não mexe nas flags) antes de
  cada corpo; os corpos e as vagas não mudam. A histerese sobe de 500 ms para 1,5 s (ver TDD).
  Riscos: (1) se um desses sites rodar fora de QTE, o jogo fica a 60: a linha de 20 s denuncia
  qual tipo; (2) a #1092 aumenta o stub repeat para 122 bytes: com o prefixo passa de 128, e a
  vaga precisa crescer se ela entrar.
* **Validação:** `tests/test_qte_hook.cpp` (Windows; no Linux rodado com um `windows.h` de mmap no
  scratchpad), `tests/test_pad.c`, `tests/test_graphics_settings.cpp`, `bash build.sh --test`;
  reteste do mantenedor nos QTEs do cavalo do Poseidon.
* **Aprovação:** aprovada pelo líder (delegação do mantenedor).

## Ciclo TDD (registro)

### 1. Stubs contam o seu tipo

* **Comportamento:** cada um dos 4 stubs incrementa o seu contador a cada chamada, com Automatic
  QTE ligado ou não; o prefixo devolve `rax`, as flags e a red zone intactos; cada stub cabe em 128 bytes.
* **Teste:** `tests/test_qte_hook.cpp` (contadores depois de cada bloco de chamadas;
  `check_count_prefix()` roda só o prefixo com `rax`, flags CF/PF/AF/ZF/SF/OF ligadas e a red zone
  marcada). Só Windows no CI; aqui rodado no Linux com um `windows.h` de mmap no scratchpad
  (`g++ -I<shim> tests/test_qte_hook.cpp`), não versionado.
* **Red:** `error: 'struct SpartanQteState' has no member named 'kinds'`,
  `'SPARTAN_QTE_COUNT_LENGTH' was not declared`.
* **Green:** `kinds[4]` em `+0x10` (static_assert), `spartanport_qte_count()` e o contador como
  primeiro argumento dos builders; `probe.c` passa `&state->kinds[i]`. `PASS: QTE hooks` a -O1 e -O2.
  Tamanhos: timed 91, repeat 75, charged 71, circle 91 bytes.
* **Mutantes (o teste pega):** `popfq` trocado por `nop` (pilha desbalanceada: o teste cai) e o
  `lea rsp,[rsp+0x80]` final trocado por `add rsp,0x80` (mexe nas flags):
  `Assertion 'check_count_prefix(stub)' failed`.

### 2. Controle: qualquer tipo liga a queda, histerese e linha de 20 s

* **Teste:** `tests/test_pad.c`, `test_qte_fps_cap_kinds()` (relógio simulado): só o contador repeat
  liga a queda sem os hooks de giro; rajadas de mash de 3 s com pausas de 1 s por 24 s mantêm o
  teto; uma chamada `held` aos 20 s com `repeat,timed`; saída depois da janela; a próxima queda
  longa reporta de novo; o giro do analógico ignora os outros contadores.
* **Red 1:** compilação (`SPARTAN_QTE_KIND_UPDATE`, `kinds` inexistentes).
* **Red 2 (comportamento):** com os contadores feitos e a histerese ainda em 500 ms:
  `tests/test_pad.c:231: test_qte_fps_cap_kinds: Assertion 'engine_fps_cap_calls==calls+1 && engine_fps_cap==60' failed`
  (a pausa de 1 s derrubava o teto).
* **Green:** `QTE_FPS_HOLD_US` de 500 ms para 1,5 s. Justificativa: no teste do mantenedor as
  quedas de 1 a 4 s saíram com o prompt ainda na tela; mash e segurar podem ficar ~1 s sem quadro
  de nenhum tipo contado; ficar a 60 um pouco depois do QTE custa pouco, sair no meio dele quebra o
  QTE. `out/pad-test` → `PASS: ... QTE frame rate cap (all prompt kinds) ...`.

### 3. Lado GPU: log dos tipos e da queda longa

* **Teste:** `tests/test_graphics_settings.cpp` (`TestEngineFpsCap`), linhas comparadas:
  `Engine FPS: 60 during a QTE (was 144), kinds: update,repeat, prompt type 2`,
  `Engine FPS: QTE cap held 20 s, kinds: timed,circle` (uma vez), `held` depois da saída ignorado.
* **Red:** `collect2: error: ld returned 1 exit status` (API nova inexistente).
* **Green:** `spartanportgpu_engine_fps_cap_qte(kinds, type)` e `spartanportgpu_engine_fps_cap_held(kinds)`
  (substituem `..._qte_type`), `QteKinds()` e a linha de 20 s em `SyncEngineFps()`.
  `PASS: graphics settings`.

### Validação final

* `bash build.sh --test` (via `heavy.sh`) → `build_rc=0`, todos os testes C, `PASS: graphics settings`;
  patches do fsr-vulkan revertidos. (Uma primeira rodada falhou com `-Werror=enum-compare` no
  `_Static_assert` novo do `probe.c`; corrigido com os casts.)
* **Não testado:** `test_qte_hook` no Windows de verdade (rodado no Linux com o shim), o launcher
  (sem mudança), e o jogo: reteste nos QTEs do cavalo do Poseidon.

## Ajuste de 30/10/2026: espera de 5 s

* **Comportamento:** QUANDO uma sequência de apertos únicos tiver segundos entre os prompts (cavalo do Poseidon), O SISTEMA DEVE manter o motor a 60 do primeiro ao último prompt e voltar uma vez só no fim.
* **Teste:** `tests/test_pad.c`, `test_qte_fps_cap_sequence` (5 prompts `timed` de 0,5 s com 4 s entre eles).
* **Red:** `bash build.sh --test` → `pad-test: tests/test_pad.c:221: test_qte_fps_cap_sequence: Assertion 'engine_fps_cap_calls==calls+1 && engine_fps_cap==60' failed.` (com 1,5 s o motor voltava a 240 entre os prompts, como o mantenedor viu no jogo).
* **Green:** `QTE_FPS_HOLD_US` de 1,5 s para 5 s; o teste antigo que abria um prompt novo 5 s depois passou a usar 10 s. `bash build.sh --test` rc=0, todos os PASS.
* **Aprovação:** aprovada pelo mantenedor (30/10/2026).
