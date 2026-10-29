# F2: motor a 60 durante QTEs

Fase F2 do roadmap `docs/plans/2026-10-29-high-fps-roadmap.md` (branch `docs/high-fps-roadmap`).
Branch: `feat/high-fps-f2-qte`.

## Spec (SDD)

* **Objetivo:** como jogador com o motor acima de 60 FPS, quero que os QTEs rodem a 60 sozinhos,
  para passar por eles sem trocar a taxa à mão.
* **Contrato:**
  - Entrada: o `heartbeat` de `SpartanQteState` (`src/qte_hook.h`), lido pela thread do controle
    (`src/runtime_pad.c`); a opção `auto_safe_fps` (0/1, padrão 1) no `spartanport.ini`.
  - Saída: `SpartanSettings::EngineFpsNow()` = `min(engine_fps, applied_engine_fps, teto)`, com
    teto 0 = sem teto; `SyncEngineFps()` continua o único escritor dos globais do motor.
  - API C nova em `gpu/spartanportgpu.h`: `void spartanportgpu_engine_fps_cap(int fps)` (0 = sem
    teto). Só grava atômicos.
  - Logs novos: `Engine FPS: 60 during a QTE (was N)` e `Engine FPS: back to N after the QTE`.
  - Opção nova nos três lugares: overlay (Desempenho, logo abaixo da taxa do motor) com traduções,
    launcher Rust (Performance, logo abaixo da taxa do motor) e `spartanport.ini`.
  - `engine_fps` salvo nunca muda pela queda.
* **Critérios de aceite:**
  - QUANDO o heartbeat mudar e a taxa em vigor for > 60 O SISTEMA DEVE aplicar o teto de 60 em
    até 1 quadro.
  - QUANDO o heartbeat ficar parado por mais de `QTE_ACTIVE_US` + 500 ms O SISTEMA DEVE retirar o
    teto.
  - QUANDO um novo prompt começar dentro desses 500 ms O SISTEMA DEVE manter o teto.
  - QUANDO o teto estiver ativo O SISTEMA NÃO DEVE gravar 60 em `engine_fps`.
  - QUANDO o jogador trocar a taxa (menu ou R3+L1+R1) com o teto ativo O SISTEMA DEVE guardar a
    escolha e aplicá-la quando o teto sair.
  - QUANDO `auto_safe_fps=0` O SISTEMA DEVE se comportar como hoje.
  - QUANDO o teto entrar ou sair O SISTEMA DEVE escrever `Engine FPS: 60 during a QTE (was N)` /
    `Engine FPS: back to N after the QTE`.
  - QUANDO `applied_engine_fps <= 60` ou os globais estiverem ausentes O SISTEMA NÃO DEVE fazer
    nada.
* **Requisitos não funcionais:** sem lock nem alocação na thread do controle (só atômicos); o teto
  nunca é lido nem salvo no ini; no máximo 2 linhas de log por QTE.
* **Arquivos afetados:**
  - `gpu/shim/spartanport_settings.h`, `gpu/shim/spartanport_settings.cpp`
  - `gpu/spartanportgpu.h`, `gpu/shim/spartanportgpu.cpp`
  - `src/runtime_pad.c`
  - `gpu/shim/spartanport_overlay.cpp`, `gpu/shim/ui_language/spartanport_ui_*.h`
  - `launcher-rust/src/core/catalog.rs`, `launcher-rust/src/ui/pages/performance.rs`,
    `launcher-rust/assets/i18n.json`, `launcher-rust/fixtures/collected/spartanport.ini`,
    `launcher-rust/src/core/store.rs` e testes do launcher (`tests/ui_save.rs`)
  - `tests/test_graphics_settings.cpp`, `tests/test_pad.c`
* **Plano técnico:**
  - `std::atomic<int> engine_fps_cap{0}` em `SpartanSettings::Values`, fora do `Save()` e do `Set()`.
    `EngineFpsNow()` aplica o teto só com `auto_safe_fps` ligado.
  - `CycleEngineFps()` cicla a partir de `min(engine_fps, applied_engine_fps)` (a escolha do
    jogador), não de `EngineFpsNow()`, para que R3+L1+R1 durante o teto ande a partir da escolha.
  - `SyncEngineFps()` (thread de apresentação, a cada vblank) detecta a entrada e a saída do teto
    e escreve as duas linhas; a troca comum continua escrevendo `Engine FPS: N`.
  - `runtime_pad.c`: `qte_active(now)` extraída de `qte_rotation()` (sem resetar o minigame);
    `qte_fps_cap(now)`, chamada a cada amostra do controle, é a máquina de estados com histerese de
    500 ms (`QTE_FPS_HOLD_US`), e chama `spartanportgpu_engine_fps_cap()` só nas transições.
  - Na entrada, o tipo do prompt (`+0x10` do bloco, `SPARTAN_QTE_TYPE_OFFSET`) vai junto para o
    log, pela mesma API (ver decisão 2).
  - Risco conhecido: não está confirmado que os prompts `repeat`, `timed`, `charged` e `circle`
    movem o `heartbeat`; o log com o tipo serve para o mantenedor confirmar. Sem stub novo agora.
* **Validação:** testes automáticos abaixo (TDD); roteiro F2.5 no jogo pelo mantenedor.
* **Aprovação:** aprovada pelo mantenedor (texto da spec); detalhes de implementação aprovados pelo
  líder por delegação do mantenedor.

### Decisões tomadas na implementação

1. **O log sai da thread de apresentação, não da do controle.** O `printf` trava o `stdout`, e a
   thread do controle só pode usar atômicos. `spartanportgpu_engine_fps_cap()` só grava o teto;
   `SyncEngineFps()`, que já roda a cada vblank, aplica o teto nos globais e escreve o log. A
   queda chega ao jogo em até 1 vblank (menos de 1 quadro do motor).
2. **Tipo do prompt no log:** a API recebe só a taxa (`int fps`), como no contrato; o tipo vai por
   uma segunda função só de atômico, `spartanportgpu_engine_fps_cap_qte_type(int type)`, chamada
   pela thread do controle logo antes do teto. A linha fica
   `Engine FPS: 60 during a QTE (was N), prompt type T`.
3. **`auto_safe_fps` é lida no lado GPU** (em `EngineFpsNow()`): desligar a opção no menu com o teto
   ativo devolve a taxa na hora. A thread do controle não lê a opção.
4. **A queda roda mesmo com o menu do port aberto**: se o jogo pausa com um QTE na tela, o teto sai
   depois de 650 ms e volta quando o prompt anda de novo (2 linhas extras nesse caso raro).
5. **Linha no overlay e no launcher**: na seção da taxa do motor (Desempenho no overlay,
   Performance no launcher), logo abaixo da taxa do motor.
