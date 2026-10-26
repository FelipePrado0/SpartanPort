# Sem compatibilidade com as instalações anteriores à 2.8.0

Data: 2026-10-25. Estado: implementado (branch worktree-launcher-identity, 2026-10-26).

## 1. Objetivo

A 2.8.0 é o primeiro pacote com o launcher Rust, e o launcher só conhece releases a partir dela
(`FIRST_RUST_VERSION`, `docs/specs/2026-10-25-launcher-identity-design.md`). O launcher ainda
carrega a camada de compatibilidade com o nome antigo (gow3_pc, até a 2.5.0) e com o pacote Python
(2.6.0 e 2.7.0): o módulo `core::legacy`, migrações de pastas e de ajustes, e duas cópias do
launcher com os nomes antigos no pacote do Windows (`God of War III.exe`, `Play God of War III.exe`).

Esta spec apaga essa camada. O pacote do Windows passa a ter só `SpartanPort.exe` e
`Play SpartanPort.exe`. Quem está numa versão anterior à 2.8.0 instala a 2.8.0 baixando o pacote da
página de releases; a atualização pelo launcher vale para quem já está na 2.8.0 ou numa
pre-release 2.8.0.

Vale para `launcher-rust/`, `packaging/windows/package.sh` e os três READMEs. O launcher Python
(`launcher/`, `run.py`, `scripts/*.py`) não muda; se o Rust precisar de alguma função dele, ela é
portada quando for preciso.

## 2. Decisões

| Tema | Decisão |
|---|---|
| Nome antigo | Sai tudo: dados de `<data>/../gow3`, `gow3.ini`, ajustes de `gow3-launcher/`, entrada `gow3.desktop`, atalho `God of War III.lnk`, variáveis `GOW3_*`, binários `gow3-*`, patch `God_of_War_III_Remastered.xml`, atalho não-Steam chamado "God of War III". |
| Pacote Python | Sai a limpeza de `_internal/`, `run.py`, `scripts/*.py` e `tools/xvag_voice_swap.py`. |
| Cópias no pacote | `package.sh` deixa de gravar `God of War III.exe` e `Play God of War III.exe`. |
| `--install-update` | Não muda: o updater Rust usa a rota. Um launcher Python 2.6/2.7 que a chamar cai no mesmo código, sem tratamento especial e sem suporte. |
| Marcadores | `legacy-migrated` e `shortcut-migrated` já gravados nas pastas dos jogadores ficam lá, ignorados. |
| Fora | Launcher Python, `PKGBUILD` (`conflicts` com `gow3-pc` é metadado do pacman), migrações de formato de ajustes (ver §6). |

## 3. Launcher (`launcher-rust/`)

### 3.1 Apagado

- `src/core/legacy.rs` inteiro e `pub mod legacy;` em `src/core/mod.rs`.

### 3.2 Chamadores

- `src/startup.rs`: sai `move_from_legacy_name`. Sai o campo `migrate` de `Route::Play` e
  `Route::Launcher`, a função `migrates` e o parâmetro `migrate` de `prepare`, que só existiam para
  pular a migração na cópia que instala uma atualização. `prepare(paths)` mantém o resto na ordem
  de hoje: ajustes, integração com o menu no Linux, limpeza da pasta de atualização.
- `src/main.rs`: chama `startup::prepare(paths)` nas rotas `Play` e `Launcher`.
- `src/core/env.rs`: sai a chamada a `forward_environment` em `start_up_environment` e, no doc
  dela, "the old `GOW3_*` names are forwarded and".
- `src/core/steam.rs`: o atalho não-Steam existente é achado por `appid` ou por
  `AppName == APP_NAME`; sai a comparação com "God of War III".
- `src/runner/mod.rs`: sai `legacy::copy_game_settings(data)` e o `use crate::core::legacy`.
- `src/core/patches.rs`: sai `LEGACY_PATCH_FILE` e a exclusão dele na varredura de patches; a
  exclusão de `CUSA01623.xml` fica.
- `src/core/paths.rs`: saem `LEGACY_CONFIG_DIR`, o campo `legacy_config_dir` e
  `legacy_config_file`.
- `src/core/settings.rs`: saem `migrate_portable_settings`, a chamada dela em
  `read_app_settings` e `copy_if_missing` (só ela o usa). O doc do módulo perde "and the portable
  migration"; o de `SettingsFile` perde "after the portable migration".
- `src/ui/app.rs`: o comentário "after its portable migration" (perto da linha 172) perde essa
  parte.

### 3.3 Testes que saem ou mudam

- Os testes de `legacy.rs` vão junto com o arquivo.
- `src/core/env.rs`: o teste com `GOW3_DATA_DIR`.
- `src/core/paths.rs`: os dois testes de `legacy_config_file`.
- `src/core/patches.rs`: o teste que grava `LEGACY_PATCH_FILE`.
- `src/core/settings.rs`: os testes da migração (os que chamam `migrate_portable_settings` e o
  que grava um `settings.json` só em `legacy_config_file`).
- `tests/startup.rs`: o teste que encaminha `GOW3_DATA_DIR` em todas as rotas.
- `tests/runner.rs`: o filtro de ambiente perde o prefixo `GOW3_`.
- `tests/packaging.rs`: ver §5.

## 4. Pacote e docs

- `packaging/windows/package.sh`: saem as duas linhas `cp` das cópias e o comentário acima delas
  (que cita `core::legacy::LEGACY_EXE`). O pacote tem `SpartanPort.exe` e `Play SpartanPort.exe`.
- `README.md`, `README.pt-BR.md`, `README.es.md`: a nota "Formerly gow3_pc" perde a promessa de
  migração. Fica: o projeto se chamava gow3_pc; quem vem de uma versão anterior à 2.8.0 baixa a
  2.8.0 na página de releases. No Windows, para manter saves e ajustes, copia `user/`, `mods/`,
  `spartanport.ini` e `settings.json` da pasta antiga para a nova. No Linux, troca o AppImage; os
  dados de antes da renomeação (`~/.local/share/gow3`) não são movidos.
- `docs/specs/2026-10-23-rust-cutover-design.md`: uma linha na parte do empacotamento (§4.1)
  apontando para esta spec, porque ela descreve as cópias com os nomes antigos.

## 5. Testes

- Primeiro o teste que falha: em `tests/packaging.rs`, `the_windows_package_ships_the_rust_launcher`
  perde `legacy::LEGACY_EXE`, `legacy::LEGACY_PLAY_EXE`, `LEGACY_EXE` e `LEGACY_PLAY_EXE` da lista
  `needed` e ganha `God of War III` na lista `banned`. Ele falha com o `package.sh` de hoje.
- Depois as remoções de §3 e §4, cada uma com `cargo test` verde.
- Gate final em `launcher-rust/`: `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings`,
  `cargo test`. No fim, `grep -rn "legacy::\|GOW3_\|gow3-launcher\|God of War III\.exe" launcher-rust/src launcher-rust/tests packaging/windows`
  não imprime nada.

## 6. Fora

Ficam, por não serem do nome antigo nem do pacote Python, e sim do formato dos dados do jogador:

- `src/core/mods.rs`: a camada `<pasta do jogo>-mods`.
- `src/core/controls.rs`: `RAGE_LEGACY_INPUT`, a tecla da Fúria de Esparta numa tabela salva.
- `src/core/graphics_options.rs`: a migração única das escolhas de lançamento antigas.

## 7. Riscos

- Um jogador que vem da 2.5.0 ou anterior e instala a 2.8.0 perde, sem aviso, os ajustes e saves
  da pasta antiga se não seguir o README.
- Um launcher Python 2.6/2.7 que achar a 2.8.0 como `latest` ainda a instala por
  `--install-update`; `_internal/`, `run.py` e `scripts/*.py` ficam na pasta, inertes.
- Instalações de teste ≥ 2.8.0 que já têm as cópias com os nomes antigos (`windows-v2.8.0-intel-test1`)
  ficam com elas: a atualização copia por cima e não apaga nada. Esta mudança entra antes de
  publicar o DRAFT `windows-v2.8.0`, para que nenhuma 2.8.0 estável as tenha.
- Um atalho não-Steam antigo chamado "God of War III" não é mais atualizado: o launcher cria um
  "SpartanPort" ao lado.
- Um `God_of_War_III_Remastered.xml` esquecido em `patches/` passa a ser lido como patch externo.

## 8. Referências

- `docs/specs/2026-10-25-launcher-identity-design.md`: identidade compilada e piso 2.8.0.
- `docs/specs/2026-10-23-rust-cutover-design.md` §4.1: as cópias com os nomes antigos.
- `launcher-rust/src/core/legacy.rs`: o módulo apagado ("Delete this module (and its callers)
  three releases after the rename").

## 9. Desvios

- O grep final de §5 imprime as linhas de dois testes que fixam que os nomes antigos são ignorados
  (`env.rs` `the_old_gow3_names_are_not_forwarded`, `settings.rs`
  `a_settings_file_at_the_old_location_is_ignored`); o resto do código não cita os nomes antigos.
- Comentários e docs fora da lista de §3.2 também foram corrigidos por descreverem o que saiu: os
  docs de `Context` e `start` em `startup.rs`, o comentário da rota `Play` em `main.rs`, o de
  `state_in` em `tests/ui_navigation.rs` e o da exclusão em `patches.rs`.
- `patches.rs`: o teste `external_patches_skip_builtin_and_other_games` perdeu a linha que gravava
  `God_of_War_III_Remastered.xml` (o nome deixou de ser especial; o teste
  `a_file_named_like_an_older_builds_patch_is_external` cobre o caso).
