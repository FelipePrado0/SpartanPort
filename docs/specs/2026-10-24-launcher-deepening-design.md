# Aprofundamento dos modules do launcher Rust (11 candidatos) e bloqueadores do PR #959

Data: 2026-10-24. Estado: design aprovado em conversa (seções A, B, C e D). As seções A–C foram
revisadas no texto; a Fase 3 (§5, seção D) aguarda revisão do texto.

## 1. Objetivo

Aplicar ao `launcher-rust/` os 11 candidatos da revisão de arquitetura de 2026-10-24 (branch
`rust-launcher`, `25ea999`). Cada um transforma modules shallow em um module deep: uma interface
pequena com muito comportamento atrás, num seam onde os testes entram. O ganho buscado é
locality (mudança, bug e verificação num lugar só) e testabilidade (testes pela interface, menos
binários que sobem o exe real).

O comportamento fica igual ao de hoje, que segue o Python, salvo as mudanças marcadas com
**⚠ muda** neste texto. Todas foram aprovadas em conversa.

Depois do candidato 11 vem a **Fase 3** (§5): os quatro bloqueadores da validação do PR #959
(relatório de um testador, 2026-10-24, sobre `25ea999`):
1. o efeito de fogo do launcher tira ~24% do FPS do jogo;
2. a branch está 9 commits atrás de `main` e perde 5 features;
3. o launcher rebaixa o `controls_layout`;
4. o clippy falha num stable mais velho.

Vocabulário: **module** (qualquer coisa com interface e implementação), **interface** (tudo que
o caller precisa saber: tipos, invariantes, ordem, erros), **depth** (comportamento por unidade de
interface), **seam** (onde a interface mora), **adapter** (o que satisfaz a interface no seam; um
adapter = seam hipotético, dois = seam real).

## 2. Decisões já tomadas

| Tema | Decisão |
|---|---|
| Onde | Direto na branch `rust-launcher` (PR #959). |
| Ordem | 01 → 11, um candidato por vez, e depois a Fase 3 (F1 → F10). Cada candidato e cada bloco F é um bloco de tasks no plano. O bug de FPS fica no ar durante o refactor (escolha do usuário). |
| `main` | Entra por `git merge origin/main` (F3), sem rebase e sem force push no PR #959. |
| Execução | `superpowers:subagent-driven-development`, uma task por subagent, com revisão. |
| Testes | TDD: teste falhando na nova interface antes do código. Testes de helpers shallow são substituídos pelos da interface, não acumulados. |
| Paridade de bytes | Nos candidatos 01–11, `tests/runner_fixtures.rs`, `tests/dump.rs` e `tests/fixtures.rs` não mudam os bytes esperados. Na Fase 3, os fixtures são regerados a partir do Python de `main` (`tools/launcher_fixtures.py`, `tools/runner_fixtures.py`), nos blocos que portam a feature correspondente. |
| Mapas de paridade | `tests/parity_doc.py` cita testes Rust pelo nome. Toda task que renomeia ou move teste atualiza as linhas de `docs/specs/2026-10-2*-parity.md` no mesmo commit. |
| `CONTEXT.md` | Não é criado (descartado pelo usuário em 2026-10-24). |
| Python | Fica congelado. Nada em `launcher/`, `run.py` ou `scripts/` é editado; esses arquivos só mudam pelo merge de `main` (F3), como referência para o port. |
| Spec §3.2 do runner | Os arquivos de estágio continuam um por script Python. Arquivos compartilhados novos entram na tabela como "compartilhado por …". |
| Fronteira | `src/core` continua sem iced e sem `crate::runner` (`tests/core_boundary.rs`). |

## 3. Gate por task

Cada task termina verde e com um commit (mensagem via `caveman-commit`, sem menção a IA):

1. `cargo fmt --check`
2. `cargo clippy --all-targets -- -D warnings` (uma vez por task)
3. `cargo test` completo (uma vez por task; reruns focados com `cargo test <nome>`)
4. `py -3 -m pytest tests/test_launcher_parity_doc.py tests/test_runner_parity_doc.py tests/test_cutover_parity_doc.py`

O `~/.cargo/config.toml` do usuário limita `jobs = 8` e `RUST_TEST_THREADS = 4`. Builds longos
(release, pacote) ficam fora deste trabalho. O dump real (`SPARTAN_GAME_DIR`) roda ao fim de 02,
04 e 07, que tocam bytes de saída.

## 4. Candidatos

### 01 Catálogo de settings

**Problema.** O que um setting é (arquivo, tipo, default, escolhas, nome `SPARTAN_*`,
codificação) está espalhado em `core/settings.rs`, `core/ini.rs`, `core/env.rs`,
`core/options.rs`, `ui/app.rs` (`apply_set`), `ui/persist.rs` e quatro pages. São 126 chamadas por
string em 20 arquivos. Há três regras de "verdadeiro": `Settings::get_bool`, `truthy` de
`env.rs` e `effects_enabled`. Hoje `"hdr": "0"` aparece desligado na UI e chega ligado ao jogo
(`SPARTAN_HDR=1`).

**Interface.**

```rust
// core/catalog.rs: só a tabela, sem I/O
pub enum File { App, Ini }
pub enum Kind { Flag, Text, Choice(&'static [Choice]), Int, Json }
pub enum Default { Bool(bool), Text(&'static str), Int(i64), Object }
pub struct Env { pub name: &'static str, pub absent: &'static str } // absent = o default do run.py
pub struct Setting {
    pub key: &'static str, pub file: File, pub kind: Kind, pub default: Default,
    pub written: bool,          // App: entra num settings.json novo; Ini: um controle grava
    pub env: Option<Env>,
}
pub static SETTINGS: &[Setting];   // App na ordem de APP_DEFAULTS, Ini na ordem do collect
pub fn setting(key: &str) -> Option<&'static Setting>;
pub fn run_default(name: &str) -> Option<&'static str>;   // o "ausente" de um --run direto

// core/settings.rs (só App; game_environment continua com &Settings)
impl Settings { pub fn flag(&self, key: &str) -> bool; pub fn text(&self, key: &str) -> String; }

// core/store.rs: os dois arquivos num valor
pub struct Store { app: Settings, ini: Ini }
impl Store {
    pub fn load(paths: &Paths) -> (Store, Vec<PathBuf>);   // + arquivos ilegíveis
    pub fn flag(&self, key: &str) -> bool;
    pub fn text(&self, key: &str) -> String;
    pub fn json(&self, key: &str) -> Option<&Value>;
    pub fn set(&mut self, key: &str, v: Value) -> bool;    // mudou?
    pub fn replace(&mut self, app: Settings, ini: Ini) -> Vec<String>;   // chaves mudadas
    pub fn app(&self) -> &Settings;
    pub fn ini(&self) -> &Ini;
    pub fn ini_to_write(&self) -> IndexMap<String, String>;
}
```

Invariantes:
- Chave ausente lê o default da tabela.
- Valor App usa o `bool()` do Python; valor Ini é ligado quando `== "1"`.
- Gravar chave Ini: Bool vira `"1"`/`"0"`, String fica como está, o resto vira texto.
- Chave desconhecida vai para settings.json sem mudança.

Ficam privados: os campos, o roteamento por `File` e os encoders.

**O que muda.**
- `app_defaults`, `INI_DEFAULTS`, `INI_FLAGS` e `SHOWN_INI_KEYS` passam a ser derivados da
  tabela, com os mesmos valores e a mesma ordem.
- Saem `Settings::get_bool`, `truthy`/`flag` de `env.rs`, `advanced::effects_enabled`,
  `State.effects`, `checkbox_ini`, os quatro `ini_on`, `home::ini_value`, o roteamento de
  `apply_set` e a normalização de flags em `persist::gather`.
- `State.settings` e `State.ini` viram `State.store`.
- `env.rs` mantém a ordem do Python e lê nome, default e escolhas da tabela. Uma função pura
  `environment(...)` sai de `game_environment`, que fica só com `write_trophy_texts`.
- No runner, só `SPARTAN_MODS_ENABLED`, `SPARTAN_STREAM_MEMORY` e `DEFAULT_ENGINE_FPS` passam a
  ler o catálogo.
- `launcher_effects` e `controls_device` entram como linhas `written: false`, então os bytes do
  settings.json não mudam.

**Comportamento.**
- **⚠ muda:** a truthiness das flags na UI segue o Python. Um valor gravado como `"0"`, `"no"`,
  `2` ou `"true"` aparece marcado, igual ao que o jogo recebe. `launcher_effects` segue a mesma
  regra.
- Continuam sem port: o default do Steam Deck e a normalização de valores ao salvar (desvios 3 e
  4 do mapa do launcher).

**Testes.**
- Novos (core, puros):
  - `every_key_is_described_once`: `app_defaults` igual às linhas App escritas, em ordem.
  - `flag_follows_python_truth_and_defaults`
  - `an_ini_flag_is_stored_as_one_or_zero`
  - `an_unknown_key_goes_to_settings_json`
  - `replace_names_the_changed_keys`
  - `ini_to_write_normalizes_only_shown_flags`
- Contrato: `tests/env_contract.rs::every_env_setting_round_trips`. Para cada linha com env,
  `environment` grava a codificação de cada valor. `runner_defaults_are_run_pys` fixa todo
  `absent` no literal do run.py.
- Substituídos: `ui_widgets.rs::checkbox_ini_emits_one_and_zero` vira
  `a_check_box_sends_a_bool`, e `persist.rs::shown_ini_keys_are_the_18_controls_of_the_window`
  vai para o teste do catálogo.
- Intocados: `fixtures.rs`, `runner_fixtures.rs`, `dump.rs` e os testes de `ini.rs` e
  `settings.rs`.

**Tasks.**
1. Catálogo, `Settings::{flag,text}` e `Store`, com os testes.
2. Derivar `app_defaults`, `INI_*` e `SHOWN_INI_KEYS` da tabela. `fixtures.rs` continua verde.
3. `State.store`; `apply_set` vira `store.set`; um widget de checkbox só; apagar as cópias;
   migrar as leituras em src e tests.
4. `env.rs` sobre o catálogo, a função pura `environment`, os testes de contrato e as
   constantes do runner.

### 02 Plano e aplicação de patches

**Problema.** `Runner::patches` (`runner/mod.rs:789-865`) encadeia cerca de 8 chamadas de
`core::patches`:
- usa `SPARTAN_PATCHES` como variável interna;
- relê o `patch_status.json` que acabou de escrever;
- completa três campos que o arquivo não tem.

`write_game_patches` refaz o profile, o merge e a seleção. São 45 itens `pub`; 22 não têm caller
fora do arquivo.

**Interface (`core/patches.rs`).**

```rust
pub struct PlanInput<'a> {
    pub game: &'a Path,              // lido por um facts() privado; o 05 troca por Installation
    pub builtin_dir: &'a Path, pub out: &'a Path,
    pub mod_patches: &'a [PathBuf],  // ordem de carga
    pub selection: Selection<'a>,
    pub external: Option<External<'a>>,
}
pub enum Selection<'a> {
    Launch { chosen: &'a [String], only: bool, graphics: &'a Options }, // run.py:280-288
    Script { extra: &'a [String], only: bool },                         // patches.py avulso
}
pub struct External<'a> { pub dir: &'a Path, pub config: &'a Path }
pub struct PatchPlan { /* privado */ }
impl PatchPlan { pub fn launcher_list(&self) -> Option<&[String]>; }   // SPARTAN_PATCHES
pub struct Applied {
    pub status: Option<PatchStatus>, pub engine_fps: u32,
    pub dmem_mb: Option<u64>, pub vblank_hz: Option<u32>,
}
impl Applied { pub fn environment(&self) -> Vec<(&'static str, String)>; } // ordem do run.py:300-311
pub fn plan(input: &PlanInput, log: &mut dyn io::Write) -> Result<PatchPlan, PatchError>;
pub fn apply(plan: &PatchPlan, eboot_elf: &Path, log: &mut dyn io::Write) -> Result<Applied, PatchError>;
```

Invariantes:
- `plan` escreve só o `patches-merged.xml`, com um merge.
- `apply` apaga o `patch_status.json`, escreve o `patches.bin` e, quando há profile, o status.
- Requisitos e fps saem da lista aplicada (run.py:297-299).
- Erros de busca por assinatura viram `skipped`.

Continuam públicos (UI e runner): `PatchMeta`, `PatchStatus` (só as 3 chaves do arquivo),
`patch_status`, `game_patch_metas`, `selectable`, `resolution_names`, `external_patches`,
`external_enabled`, `builtin_xml`, `game_title_id` e `PatchError`.

`tests/dump.rs` compila em modo "reference Address", que o `apply` não alcança. Por isso existe
`#[doc(hidden)] pub mod compiler { compile_patches, eboot_image, eboot_segments, Write }`.

**O que muda.**
- Novo `core/python.rs` com `py_repr`, `py_list`, `py_hex`, `py_int`, `ensure_ascii` e
  `python_json`, vindos de `patches.rs` (7 importadores).
- `runner/graphics_options.rs` vai para `core/graphics_options.rs` (`git mv`). As constantes de
  nome repetidas vão para `core/options.rs`. A spec §3.2 do runner e as linhas de paridade passam
  a apontar o caminho novo.
- Saem `PatchJob`, `write_game_patches`, `game_patchable` (de patches.rs), os campos
  `PatchStatus.{engine_fps, dmem_mb, vblank_hz}` e `engine_fps`/`split_patches` do runner, além da
  releitura do env e do status em `runner/mod.rs`.
- `mods::enabled_patch_files` passa a devolver `Result` e substitui a cópia do runner. A de
  `performance.rs` fica, porque lê a página Mods ainda não salva.

**Comportamento.**
- **⚠ muda (só log):** cada linha "from the mod file" sai uma vez em vez de duas. Os bytes do
  `patches-merged.xml` não mudam.
- A seleção ainda passa pelo join/split com `;`, como o env fazia (paridade).
- O caso do status corrompido (`tests/runner.rs:512-523`) deixa de existir, porque nada mais relê
  o arquivo. Ele é substituído por `applied_environment_in_run_py_order`.

**Testes.**
- Novos em `patches.rs`:
  - `script_selection_is_patches_py`
  - `launch_selection_adds_ini_resolution_and_top_frame_rate`
  - `launch_selection_keeps_a_mods_default_patch`
  - `a_missing_frame_rate_patch_falls_back_with_two_log_lines`
  - `mod_lines_are_logged_once`
  - `apply_without_profile_writes_empty_patches_bin_and_no_status`
  - `a_skipped_frame_rate_patch_leaves_engine_at_60`
  - `applied_notes_give_dmem_and_vblank`
  - `apply_removes_a_stale_status_first`
  - `applied_environment_in_run_py_order`
- Cerca de 15 testes de compilação são reescritos sobre `plan`/`apply` com o mesmo nome:
  seleção, profile, merge, requisitos, `write_*` e terceiros.
- Intocados: cerca de 20 testes do compilador (via `compiler`), os testes de
  `graphics_options` (só o caminho muda) e os bytes de `runner_fixtures::patches_match_python`.

**Tasks.**
1. `core/python.rs` (move puro).
2. `graphics_options` para core, constantes em `options.rs`, caminhos nos mapas e linha na §3.2.
3. `plan` e `PatchPlan`, teste primeiro.
4. `apply` e `Applied`, teste primeiro. Runner, `runner_fixtures` e `dump.rs` passam a usá-los;
   saem `PatchJob`, `write_game_patches`, `engine_fps` e os campos extras.
5. Interface pública reduzida, `compiler` oculto, os ~15 testes reescritos, sai
   `game_patchable`.
6. `enabled_patch_files` devolve `Result` e o runner o usa.

### 03 Runner testável em processo: o Host

**Problema.** `Runner::start` (274 linhas, 16 passos) chama o SO direto: `set_current_dir`,
`catch_interrupt`, `current_exe`/`process::id`, `physical_memory_mb` e `Command`. O único seam é
`interrupted: fn() -> bool`. Por isso, 16 de 18 testes de `tests/runner.rs` sobem o exe real com
um Port completo. Um deles leva 15 s porque `GAMEPAD_PROBE_LIMIT` é constante.

**Interface.**

```rust
// core/platform/tool.rs (task 1 do 03; o 11 generaliza)
pub struct ToolCall<'a> {
    pub program: OsString, pub args: Vec<OsString>,
    pub env: Option<&'a IndexMap<String, String>>, pub cwd: Option<&'a Path>,
    pub limit: Option<Duration>, pub stderr: Stderr /* Inherit | Null */,
    pub window: Window /* Hidden | LikeConsole */,
}
pub struct ToolOutput { pub code: Option<i32>, pub stdout: String }   // UTF-8 lossy
pub enum ToolError {
    Spawn(io::Error), TimedOut { command: Vec<String>, limit: Duration }, Wait(io::Error),
}
// Display de TimedOut = "Command '[..]' timed out after N seconds" (run.py:132)
pub fn run_tool(call: &ToolCall) -> Result<ToolOutput, ToolError>;

// runner/host.rs
pub trait Host {
    fn tool(&self, call: &ToolCall) -> Result<ToolOutput, ToolError>;  // --cpu-features, --gamepad
    fn play(&self, call: &ToolCall, log: &mut dyn FnMut(String)) -> io::Result<Option<i32>>; // None = Ctrl+C
    fn wait_for_pid(&self, pid: u32, limit: Duration) -> bool;
    fn interrupted(&self) -> bool;
    fn this_process(&self) -> (PathBuf, u32);
    fn physical_memory_mb(&self) -> Option<u64>;
}
pub struct SystemHost;
pub fn run_with(ctx: RunContext<'_>, host: &dyn Host) -> i32;   // run() = catch_interrupt + run_with(SystemHost)
```

Toda `ToolCall` do runner leva `cwd = port`. Ficam privados: `Runner`, `run_within`,
`wait_or_interrupt`, `this_exe`, `which`, `is_executable`, `parse_cpu_features`,
`isa_rewrite_wanted`, `launch_prefix` e `find_executable`. Continuam públicos (para
`tests/dump.rs`): `glyph_mounts`, `pause_menu_args` e `restart_command`.

O `play_with` não entra no Host: o 07 o leva para o `core::launch`.

**O que muda.**
- `run_within` e a chamada de cpu-features passam a usar `run_tool`.
- As checagens de `interrupted`, `current_exe`/`process::id`, a memória, o `wait_for_pid` do
  `--after` e o spawn com espera vão para o `SystemHost`.
- `set_current_dir` sai.

**Comportamento.**
- **⚠ muda:** os caminhos `SPARTAN_*` relativos (`path_or`, `SPARTAN_CONFIG`,
  `SPARTAN_PATCH_DIR`, `SPARTAN_PROBE`) passam a ser juntados à pasta do port. No Python o run.py
  fazia `chdir` (run.py:168). Argv e log mostram o caminho absoluto; o efeito é o mesmo, porque o
  probe continua rodando no port.
- O env entregue ao jogo continua como o jogador escreveu.

**Testes.**
- `FakeHost`:
  - registra cada `ToolCall`;
  - roteiriza CPU, gamepad, código de saída e `Interrupt::{Never, Always, DuringGame}`;
  - tem um gancho `on_play` que lê arquivos sob `--app0`.
- O fixture de Port grava um `bin/spartanport-probe` vazio em vez de copiar o fake-probe.
- 14 dos 16 testes que sobem o exe passam a rodar em processo. O de 15 s vira "o host recebeu
  `limit == GAMEPAD_PROBE_LIMIT`, e a linha de log saiu".
- Ficam ponta a ponta:
  - `a_restart_from_the_game_starts_the_exe_again`;
  - `ctrl_c_cleans_up_and_ends_with_130` (unix);
  - um novo `run_starts_the_probe_in_the_port`.
- No nível da ferramenta: `a_hanging_tool_is_stopped_at_its_limit` (fake-probe `hang`, 1 s).
- Unit tests de helpers em `runner/mod.rs` saem. No lugar entram, em processo:
  - `a_missing_movbe_gets_the_isa_table`
  - `isa_rewrite_0_skips_it`
  - `spartan_probe_is_the_program_started`
  - `a_ctrl_c_stops_before_the_game`
  - `a_relative_data_dir_is_from_the_port`
  - `a_pool_above_ram_warns`
- `tests/runner_wait.rs` vai para dentro de `tests/runner.rs` (um binário de teste a menos).

**Tasks.**
1. `core/platform/tool.rs`. Vermelho: `a_hanging_tool_is_stopped_at_its_limit`.
2. `Host`, `SystemHost` e `run_with`. Vermelho: `a_broken_mods_json_stops_with_the_mods_message`
   em processo.
3. Sai `set_current_dir`. Vermelho: `a_relative_data_dir_is_from_the_port`, com os arquivos em
   `port/data/out` e toda `call.cwd == port`.
4. Mover os demais casos, apagar os unit tests de helpers, privatizar.
5. Juntar `runner_wait.rs`.

### 04 Leitor de módulo SCE e codec G3PROBE

**Problema.** Os estágios seguem os scripts Python, não os formatos:
- a leitura da tabela dinâmica aparece 3×;
- o interpretador de relocações, 3×;
- há 3 writers e 2 readers de G3PROBE;
- há um ciclo de imports `link_libc → link_modules`;
- `prepare.rs` tem 37 itens `pub(super)` e `link_libc.rs` tem 43;
- `eboot.bin` e `libc.prx` são lidos 3× cada.

**Interface.**

```rust
// runner/sce.rs
pub fn parse_self(data: &[u8]) -> Result<SelfInfo, SceError>;     // movido; prepare.rs reexporta
pub fn read_self(path: &Path) -> Result<SelfInfo, SceError>;
pub struct Symbol { pub name: String, pub kind: u8, pub binding: u8, pub section: u16, pub value: u64, pub size: u64 }
pub struct SceModule { pub file: String, pub sha256: [u8; 32], pub bytes: u64, pub info: SelfInfo, pub symbols: Vec<Symbol> }
impl SceModule {
    pub fn read(path: &Path) -> Result<Self, SceError>;            // dinâmica até DT_NULL, strtab, symtab múltipla de 24
    pub fn header(&self, kind: u32) -> Option<&ProgramHeader>;
    pub fn tag(&self, tag: u64) -> Option<u64>;
    pub fn needed(&self) -> Result<Vec<String>, SceError>;
    pub fn relocations(&self) -> Result<Vec<Relocation>, SceError>; // sob demanda, múltipla de 24
    pub fn ids(&self, plain: PlainNames) -> Result<Ids, SceError>;  // sob demanda
    pub fn mapped(&self, address: u64, size: u64) -> bool;
    pub fn memory_image(&self) -> Result<Vec<u8>, SceError>;
    pub fn code(&self, s: &Symbol) -> Result<&[u8], SceError>;
    pub fn resolve(&self, r: &Relocation) -> Result<Target<'_>, SceError>;
}
pub enum Target<'a> { Absolute(i128), Defined(i128), Undefined(&'a Symbol) }
pub enum ImportKind { Function = 1, Object = 2 }   // ::of(&Symbol), .allows(addend)

// runner/probe_image.rs
pub enum Version { Prepared, Libc, Linked }        // G3PROBE2 / 4 / 5
pub struct ProbeImage {
    pub entry: u64, pub flags: u64, pub segments: Vec<[u64; 3]>, pub names: Vec<String>,
    pub relocations: Vec<ImageRelocation>, pub image: Vec<u8>, pub extension: Extension,
}
pub enum Extension { None, Libc(LibcHeader), Linked(LinkedHeader) }
impl ProbeImage {
    pub fn decode(bytes: &[u8], expect: Version) -> Result<Self, ImageError>;
    pub fn encode(&self) -> Result<Vec<u8>, ImageError>;
    pub fn relocate(&mut self, target: u64, value: i128);
    pub fn map_module(&mut self, m: &SceModule, base: u64, size_error: impl Fn() -> LinkError) -> Result<u64, LinkError>;
    pub fn patch_fs_loads(&mut self, loads: &[Segment], base: u64) -> usize;
}
```

A mensagem de `ImageError` depende da `Version`, então cada estágio mantém o texto do seu script
Python:
- magic errado: "linker expects freshly prepared G3PROBE2" ou "not a G3PROBE5 image";
- nome longo: "import name too long" no G3PROBE2, "name too long" nos demais.

Ficam privados: os leitores de bytes, os tamanhos de registro e `IMPORT_NAME_SIZE`.

**O que muda.**
- Os quatro arquivos de estágio viram callers finos com os nomes do Python:
  - `prepare.rs` reexporta `parse_self`, `read_self` e os tipos de cabeçalho;
  - `link_libc::module` e `link_modules::module` viram wrappers de uma linha;
  - `isa_rewrite::read_boot_linked` fica com ~4 linhas sobre `decode`.
- `patch_fs_loads` sai de `link_modules`, o que desfaz o ciclo de imports.
- Saem `Boot`, `read_boot`, `MODULE_SIZE` e `boot_image`.
- A §3.2 do runner ganha `sce.rs` e `probe_image.rs` como "compartilhados por prepare,
  link_libc, link_modules e isa_rewrite".

**Comportamento.**
- **⚠ muda:** `inspect_libc` passa a parar no `DT_NULL`, como os demais estágios. Só afeta uma
  libc.prx com tags não-zero depois dele.
- **⚠ muda (só a mensagem):** em link_libc, link_modules e inspect_libc, "out of bounds: …" vira
  "bad symbol/relocation table size". Os mesmos inputs continuam rejeitados.
- **⚠ muda:** o decode do `isa_rewrite` fica estrito (trailer e nomes UTF-8). Só afeta um
  `boot-linked.bin` corrompido, e a falha continua não fatal (`runner/mod.rs:559`).
- Relocações e ids são lidos sob demanda. O `inspect_libc` nunca os lê, então nenhuma entrada
  aceita hoje passa a ser recusada.
- Os bytes de saída de todos os estágios não mudam.

**Testes.**
- Novos em `sce.rs`:
  - `stops_at_dt_null_for_every_stage`
  - `a_partial_symbol_table_is_named_by_every_stage`
  - `a_partial_relocation_table_is_named`
  - `conflicting_ids_are_refused`
  - `plain_names_refused_or_unlinkable`
  - `resolve_gives_absolute_defined_or_undefined`
  - `import_kind_and_addend_rule`
- Novos em `probe_image.rs`:
  - `round_trips_every_version`
  - `decode_checks_the_magic_before_anything`
  - `decode_refuses_a_trailer_and_non_utf8_names`
  - `name_of_128_bytes_is_refused_with_each_versions_text`
  - `value_past_i64_is_refused`
  - `decodes_the_python_package_fixture`
  - `linked_module_records_are_56_bytes`
  - `patch_fs_loads_only_in_executable_loads`
- Novo em `isa_rewrite.rs`: `read_boot_linked_refuses_a_trailer`.
- Intocados: todos os testes de prepare, link_libc, link_modules e isa_rewrite (passam pelos
  wrappers), `runner_fixtures`, `dump.rs` e os builders de `tests/common/packages.rs`.

**Tasks.**
1. Mover a camada de contêiner para `sce.rs`, com reexports (move puro).
2. `SceModule::read`, `relocations()` e `ids()`, teste primeiro. `link_libc::load` vira wrapper.
3. `prepare` e `inspect_libc` sobre `SceModule`; os três interpretadores usam `resolve` e
   `ImportKind`.
4. Codec `probe_image` e builders, teste primeiro. Os quatro estágios passam a usá-lo;
   `patch_fs_loads` muda de lugar; saem `Boot`, `read_boot` e `MODULE_SIZE`.
5. Varredura de visibilidade e linhas da §3.2.

### 05 Installation do jogo

**Problema.** `game_dir` e `update_dir` viajam como duas strings, em 7 lugares:
- há APIs duplicadas entre `game.rs` e `patches.rs` (`game_title_id`, profile, `game_patchable`);
- a regra de pasta está implementada 3× (`ready_to_play`, `play::checks`,
  `SetupState::check_folders`);
- `state.game_ready` está morto;
- `refresh_facts` lê o param.sfo 3×.

**Interface.**

```rust
// core/game.rs: uma pasta, param.sfo lido uma vez
pub struct GameFolder { values: IndexMap<String, SfoValue>, has_eboot: bool }
impl GameFolder {
    pub fn read(p: &Path) -> Self;
    pub fn title_id(&self) -> Option<&str>;
    pub fn app_version(&self) -> Option<&str>;
    pub fn profile(&self) -> Option<&'static GameProfile>;
    pub fn info(&self) -> Option<GameInfo>;
}

// core/installation.rs
#[derive(Clone, Debug)]
pub struct Installation { game: String, update: String, meta: GameFolder, verdict: Verdict }
#[derive(Clone, Debug, PartialEq)]
pub struct Verdict {
    pub game_missing: Vec<(String, FileState)>, pub same_folder: bool,
    pub update_missing: Vec<(String, FileState)>,
}
impl Installation {
    pub fn read(game_dir: &str, update_dir: &str) -> Self;
    pub fn of(store: &Store) -> Self;
    pub fn meta_folder(&self) -> &Path;
    pub fn info(&self) -> Option<&GameInfo>;
    pub fn title_id(&self) -> Option<&str>;
    pub fn profile(&self) -> Option<&'static GameProfile>;
    pub fn patchable(&self) -> bool;
    pub fn has_game(&self) -> bool;
    pub fn verdict(&self) -> &Verdict;
    pub fn system_file(&self, rel: &str) -> Option<PathBuf>;
    pub fn trophy_package(&self) -> Option<PathBuf>;
}
impl Verdict { pub fn passes(&self) -> bool; }
```

`Installation` é uma fotografia: fica velha depois de mudanças no disco até o 06 relê-la. Ela é
`Clone + Send`. Ficam fora da interface: `eboot_sha256` (só o runner usa, sobre a vista do update),
a oferta de arquivos do Sc0 e `folder_or_here`.

**O que muda.**
- Saem de `game.rs`: `ready_to_play`, `folder_or_here`, `meta_folder`, `game_title_id`,
  `game_profile_of`, `game_patchable` e `game_sfo`.
- Saem de `patches.rs`: `game_app_version`, `game_title_id` e `game_profile_for`. O `facts()`
  privado do 02 passa a usar `GameFolder`, e o fn injetado em `game_update.rs:115` também.
- Saem `trophies::trophy_package`, `play::folder_or_here`, os campos e `check_folders` do setup,
  `HomeFacts.{info, title_id, patchable}`, `State.game_ready` e `Check::GameFolder`.
- Call sites (11 em 10 arquivos): `main.rs`, `splash.rs`, `home.rs`, as pages game, mods e
  trophies, `play.rs`, `setup.rs`, `env.rs` e `advanced.rs`.
- O texto "Choose two separate folders…" vira uma constante só.

**Comportamento.**
- As duas regras "update vence" ficam, porque o Python tem as duas:
  - a pasta inteira do update vale quando ela tem `eboot.bin` (py:884);
  - troféus e arte são escolhidos arquivo a arquivo (py:2184, py:1408).
- **⚠ muda (caso de borda):** pasta do jogo vazia vale `"."` em todo lugar (py:955, 969, 3281).
  O setup passa a acusar "mesma pasta" também com o campo do jogo vazio.
- O PLAY mantém a ordem de checagens do Python (py:3266-3291). `passes()` é o `ready_to_play`.

**Testes.**
- Novos em `core/installation.rs` (tempdir):
  - `meta_folder_is_the_update_only_with_its_eboot`
  - `facts_come_from_the_meta_folder`
  - `system_files_prefer_the_update_file_by_file`
  - `verdict_lists_missing_files_per_folder`
  - `same_folder_counts_an_empty_game_folder_as_here`
  - `passes_is_ready_to_play`
- Substituídos:
  - os testes de `ready_to_play`/`meta_folder`/profile em `game.rs`;
  - `trophies.rs:348-353`;
  - os asserts de `patches.rs:2354-2359`;
  - o assert de `ui_splash.rs:58-62` sai.
- Intocados: os testes de file_state, sfo, Sc0 e adopt, `runner_fixtures` e os fluxos de
  `ui_setup` e `ui_play`.

**Tasks.**
1. `GameFolder`, com o `facts()` de patches e o runner sobre ele. `runner_fixtures` verde.
2. `Installation` e `Verdict` com os testes. Saem as duplicatas do core; `main.rs` e `env.rs`
   migram.
3. `State.installation`, depois home, setup, play, mods, trophies e splash. Sai `game_ready`.

### 06 Views derivadas seguem os settings

**Problema.** O refresh das views lidas do disco é escolhido à mão em 9 lugares: `State::new`,
`apply_set`, `folder_chosen`/`game_changed`, Skip, `collect`, `rebuild`, `exited`, `Restored` e o
foco da janela. `Message::Set` de uma pasta só atualiza a checagem do setup. Isso não chega ao
usuário, porque o campo de pasta manda `Browsed` (`widgets/folder.rs:38`), mas os testes tropeçam
nisso.

**Interface (`ui/follow.rs`).**

```rust
pub enum View { Installation, Home, Patches, Backups, ModResolutions, Aspect, Mods, Trophies }
pub enum Cause<'a> { Keys(&'a [&'a str]), Start, Rebuild }
pub fn follow(state: &mut State, cause: Cause<'_>) -> Task<Message>;
pub fn inputs(view: View, store: &Store) -> Vec<String>;   // guarda contra leitura velha
```

Tabela, avaliada nesta ordem:

| View | Chaves | Start | Rebuild | Leitura |
|---|---|---|---|---|
| Installation | game_dir, update_dir | sim | sim | síncrona |
| Home | game_dir, update_dir, user_dir | sim | sim | síncrona |
| Patches | game_dir, update_dir | sim | sim | síncrona |
| Backups | user_dir | sim | não | síncrona |
| ModResolutions | mods_enabled, mods_dir, game_dir | sim | não | síncrona |
| Aspect | render_resolution | sim | não | síncrona |
| Mods | mods_dir, game_dir, update_dir, patches_dir | sim | sim | síncrona em Start/Rebuild, assíncrona no resto |
| Trophies | game_dir, update_dir, user_dir | não | não | assíncrona |

Invariante: uma view é relida se e somente se uma chave dela mudou ou a causa a nomeia.

Callers:
- `Set` faz `if store.set(k, v) { follow(Keys(&[k])) }`; `Browsed(k, Some(p))` é o mesmo que
  `Set(k, p)`.
- Skip e collect: `follow(Keys(&store.replace(..)))`.
- `State::new`: `follow(Start)`. `rebuild`: `follow(Rebuild)`.
- Saída do jogo e Restored: `Keys(&["user_dir"])`.
- AdoptDone e foco da janela com o setup aberto: `Keys(&["game_dir", "update_dir"])`.

Ficam fora da tabela:
- `loaded`/`edited` da página Mods continuam atualizando ModResolutions direto, porque essa
  dependência é da view Mods, não de uma chave;
- os refreshes ao mostrar uma página (o `PAGE_REFRESH` do Python).

**O que muda.**
- Saem `game::folder_chosen`, `game::game_changed`, `setup::refresh_folders`, os `FOLDERS` e
  `folders()` de mods e trophies, e os ifs por chave de `apply_set`.
- Saem também os refreshes à mão dos 9 lugares.
- As funções `refresh_*` ficam privadas.

**Comportamento.**
- **⚠ muda:** trocar a pasta do jogo não relista mais os backups, como o `game_changed` do
  Python (py:2798-2803). Digitar a pasta não reseta o backup escolhido.
- **⚠ muda (novo):** quando o jogo sai, os backups são relistados, e o backup feito pelo runner
  antes do start aparece.
- Depois de um restore, os troféus também são relidos (assíncrono).

**Testes.**
- Novos em `tests/ui_follow.rs`, via `app::update` com tempdir:
  - `each_view_follows_exactly_its_keys` (por tabela)
  - `typing_and_browsing_a_folder_give_the_same_views`
  - `skip_follows_the_restored_folders`
  - `collect_follows_a_resolution_changed_on_disk`
- Movido: `a_read_for_a_folder_typed_over_is_dropped` passa a usar `inputs`.
- Os refreshes à mão nos testes viram `Message::Set` ou `follow(Keys(..))`.
- `load_mods`/`load_trophies` de `tests/common` ficam: eles entregam as leituras em segundo
  plano, porque os testes não rodam Tasks do iced.

**Tasks.**
1. Tabela, `follow(Keys)` e `inputs`. `Set` e `Browsed` passam por ela. Saem `folder_chosen`,
   `game_changed` e `refresh_folders`.
2. Start, Rebuild, Skip, collect, saída do jogo, AdoptDone, Restored e foco sobre `follow`.
   Refreshes privados e testes migrados.

### 07 Um spawner do jogo

**Problema.** `runner::play_with` reimplementa o `core::launch::spawn_game` e já diverge dele:
- o sinal vira `-signal` num e `-1` no outro;
- não há corte em `MAX_CHUNK`;
- não há grupo de processos;
- o `--run` vem inline.

**Interface (`core/launch.rs`).**

```rust
pub struct Launch<'a> {
    pub command: &'a [String],   // run_command(exe, args) ou fake_game
    pub cwd: &'a Path, pub env: &'a IndexMap<String, String>,
    pub run_log: RunLog, pub lang: &'a str, pub group: Group,
}
pub enum Group { Own /* PLAY: grupo/job próprio; STOP encerra */, Caller /* --play: sem job; Ctrl+C chega ao jogo */ }
pub struct Exit { pub code: i32, pub hint: Option<ExitHint> }   // code = returncode do Python
pub enum GameEvent { Output(String), Exited(Exit) }
pub fn spawn_game(launch: Launch<'_>) -> Result<GameHandle, LaunchError>;
pub fn returncode(status: ExitStatus) -> i32;                   // -N para o sinal N
// runner
pub fn play_with(ctx, settings, paths, command: &[String], notify: &mut dyn FnMut(&str)) -> i32;
```

A thread leitora trabalha em blocos de até 64 KiB:
- para cada bloco, grava o log, faz `hint = exit_hint(bloco).or(hint)` e envia `Output`;
- no EOF, se `code != 0` e há dica, grava `"\n{conselho}\n"` no log e envia `Exited`.

`play` chama `play_with` com `run_command(&this_exe(), &[])`.

**O que muda.**
- Saem o loop de pipe/Command/leitura e `exit_code` de `runner/mod.rs`. O `returncode` também
  serve ao `wait_or_interrupt`.
- Em `ui/play.rs`:
  - `GameState` perde os campos `hint`;
  - `output()` para de chamar `exit_hint`;
  - `exited()` recebe um `Exit`;
  - um leitor sumido vira `Exit { code: -1, hint: None }`.
- `fake_game.rs` ganha `--say TEXT` e `--env NAME`.

**Comportamento.**
- **⚠ muda:** o código de um jogo morto por sinal é `-N`, o `process.wait()` do Python nos dois
  caminhos (py:3339, 3752, 3760). No Linux, STOP mostra "code -15" na UI, como no Python. O
  Windows não muda (o job encerra com 1).
- O grupo de processos segue o Python: PLAY abre sessão própria (py:68, 3301); `--play` não
  (py:3741-3743).
- Uma regra de dica: vale a última linha que casar. A UI mostra a dica em qualquer saída;
  `--play` só com código diferente de 0.
- **⚠ muda:** o conselho de saída passa a ir para o `last_run.log` também no PLAY. O Python só
  gravava no `--play` (py:3755), embora o comentário do PLAY peça o mesmo arquivo (py:3296).
- O corte em 64 KiB passa a valer no `--play`. Os bytes de saída não mudam.

**Testes.**
- Em `tests/launch.rs`:
  - os 5 testes existentes passam a usar `Launch`; o esperado de `:106` vira 1 no Windows e -15
    nos demais;
  - novos: `a_signal_exit_is_minus_the_signal`, `the_last_hint_line_reaches_the_exit`,
    `a_failed_exit_writes_the_advice_to_the_run_log` e `a_clean_exit_writes_no_advice`;
  - novos no unix: `caller_group_keeps_the_process_group` e `own_group_has_its_own`.
- `tests/runner.rs`: `play_writes_last_run_log_with_the_advice` e
  `play_starts_the_game_with_the_saved_stream_memory` passam a usar o `fake_game`, com os mesmos
  nomes e sem Port.
- `tests/ui_play.rs`: os envios de `Exited` passam a usar `Exit { .. }`.

**Tasks.**
1. `returncode` (-N) no `spawn_game` e na espera do runner.
2. `fake_game --say` e `--env`.
3. `Launch`, `Group`, `Exit` e o conselho no log. A UI passa a usá-los.
4. `play_with` sobre `spawn_game(Group::Caller)`. Os testes do runner passam para o `fake_game`;
   o loop de leitura sai.

### 08 `GameState::is_live()` e uma recusa única

**Problema.** Quatro predicados idênticos respondem "o jogo está vivo?":
- `play::game_live`;
- `game::restore_blocked`;
- `trophies::game_running`;
- `advanced::clear_cache_blocked`.

O `home.rs` casa as variantes de novo. São 11 diálogos montados à mão, com 6 helpers locais.

**Interface.**

```rust
impl GameState {
    pub fn is_live(&self) -> bool;   // Preparing | Running
    pub fn is_idle(&self) -> bool;
    pub fn is_copying(&self) -> bool;
}
#[derive(Clone, Copy)]
pub enum LiveAction { RestoreSaves, ResetTrophies, ClearShaderCache, Update }
/// Com o jogo vivo, abre o Info "Feche o jogo antes de …" e devolve true para o caller parar.
pub fn refuse_while_live(state: &mut State, action: LiveAction) -> bool;   // ui/play.rs
impl Modal {
    pub fn info(t: String) -> Self;
    pub fn error(t: String) -> Self;
    pub fn ask(t: String, yes: Message, no: Message) -> Self;
    pub fn confirm(t: String, yes: Message) -> Self;   // no = Dismiss
}
```

**O que muda.** Saem os 4 predicados e os 6 helpers de diálogo. Os 11 literais
`Modal { title: APP_NAME }` passam a usar os construtores.

**Comportamento.** Sem mudança. Copiar arquivos de sistema continua sem contar como "vivo"
(py:2165, 2282, 3459, 3537), e os textos ficam iguais.

**Testes.**
- Novos:
  - `ui_play.rs::every_live_action_is_refused_while_the_game_runs`, por tabela;
  - `modal.rs::constructors_title_the_dialog_with_the_app_name`.
- Substituídos: `ui_play.rs:705-738` e os asserts de `ui_pages.rs:686` e `:1828`.

**Tasks.**
1. `is_live`/`is_idle` e `refuse_while_live` com o teste. Saem os 4 predicados.
2. Construtores de `Modal`. Saem os 6 helpers.

### 09 Contexto da execução derivado uma vez

**Problema.**
- `main.rs` monta `Paths::from_env` 4×, uma por rota.
- O env é encaminhado de três jeitos:
  - uma cópia no `--run`;
  - `set_var` no `--play` e na janela;
  - nada no `--install-update`.
- A `Route` não carrega argumentos (`args[1]`, `args[2]`).
- O runner refaz `Paths` com env vazio e reimplementa os defaults dele.

Isso causa um bug real de paridade: o `--install-update` não encaminha os `GOW3_*` (o Python
fazia isso no import) e pode ler o `settings.json` errado para escolher o idioma.

**Interface.**

```rust
// startup.rs
pub enum Route {
    Version, Run { args: Vec<OsString> }, Play { migrate: bool },
    InstallUpdate { target: PathBuf, pid: u32 /* ilegível → 0 */ },
    VoiceSwap { args: Vec<OsString> }, Launcher { migrate: bool },
}
pub struct Context { pub paths: Paths, pub env: IndexMap<String, String> }
pub fn start(args: &[OsString], exe_dir: &Path,
             vars: impl IntoIterator<Item = (OsString, OsString)>, windows: bool) -> (Route, Context);
pub fn apply_to_process(context: &Context);   // set_var da diferença; só Play e Launcher

// runner/plan.rs
pub struct RunPlan {
    pub port: PathBuf, pub data: PathBuf, pub out: PathBuf, pub config: PathBuf,
    pub game: PathBuf, pub update: Option<PathBuf>,
    pub mods_dir: PathBuf, pub mods_config: PathBuf, pub mods_switch: String,
    pub patches_dir: PathBuf, pub patches_config: PathBuf,
    pub chosen_patches: Vec<String>, pub patches_only: bool,
    pub user: PathBuf, pub probe: Option<PathBuf>, pub isa_rewrite: Option<bool>,
    pub content_sku: String, pub timeout: String, pub stream_memory: String,
    pub glyph_style: String, pub language: Option<String>,
    // e um campo para cada SPARTAN_* restante que runner/mod.rs lê hoje com default próprio
}
impl RunPlan { pub fn read(paths: &Paths, env: &IndexMap<String, String>) -> RunPlan; } // sem erros
```

`start` monta o contexto nesta ordem:
1. `environment_of(vars)`;
2. no Windows, os nomes em maiúsculas (vem de `Runner::new`);
3. `start_up_environment`;
4. `Paths::from_env`, no `--run` com raiz no port resolvido.

`RunContext` passa a receber `&Context`. O nome `Context` evita choque com `core::launch::Launch`
do 07.

Origem dos defaults do `RunPlan`:
- `SPARTAN_MODS_ENABLED`, `SPARTAN_STREAM_MEMORY` e `SPARTAN_GLYPH_STYLE` vêm de
  `catalog::run_default` (01);
- os caminhos vêm de `Paths`;
- os defaults só do runner viram consts em `plan.rs`, com o literal do run.py: TIMEOUT `"0"`,
  CONTENT_SKU `"full"`, MSYS2_ROOT e o jogo em `port.parent/CUSA01623`.

O `mods_switch` mantém as duas leituras do run.py: `!= "0"` no passo 4, e outro valor dá saída 2
no passo 9.

**Comportamento.**
- Encaminhar primeiro em toda rota. Isso corrige o `--install-update` e deixa o `GOW3_PORT_DIR`
  consistente (nenhuma release o leu).
- Nomes em maiúsculas no Windows em toda rota, como o `os.environ` do Python.
- **⚠ muda:** uma variável de caminho vazia conta como ausente. No Python, `""` valia a pasta do
  port; o launcher nunca envia vazio (`env.rs:167-182`).

**Testes.**
- Os testes de rota de `tests/startup.rs` passam a checar os payloads.
- Novos:
  - `every_route_forwards_old_names_before_paths`
  - `run_paths_start_at_the_resolved_port`
  - `windows_names_are_upper_case`
  - `install_update_carries_target_and_pid`
- Novos em `plan.rs`:
  - `defaults_are_the_paths_ones`
  - `variables_override_each_default`
  - `relative_values_are_from_the_port`
  - `empty_values_count_as_unset`
  - `plan_reads_the_catalog_run_defaults`

**Tasks.**
1. Payloads da `Route`. Vermelho: os testes de payload.
2. `Context` e `start`; sai `forward_environment`. Vermelho: o teste de encaminhar antes do
   `Paths`.
3. `RunPlan::read`, e o runner lê o plano. Vermelho: os testes do plano.
4. Ligar o catálogo. Vermelho: `plan_reads_the_catalog_run_defaults`.

### 10 Updater atrás de um port ReleaseHost

**Problema.**
- O estado do update está em 4 campos de `State`: `update`, `installed`, `update_progress` e
  `self_update`.
- Nada no tipo garante que o progresso só existe durante o download.
- GitHub, `APPIMAGE`, `is_packaged()` e a sondagem de pasta gravável são lidos inline, e
  `list_releases` é chamado em 2 lugares.
- Os testes testam helpers puros em vez do fluxo.
- Os testes de UI chamam o GitHub de verdade (`ui_home.rs:738-747`) e, no Windows, abrem o
  navegador (`ui_home.rs:942-945`).

**Interface.**

```rust
// core/update.rs (sem iced)
pub enum InstallTarget { WindowsPackage, WindowsSource, AppImage { folder: PathBuf, writable: bool }, Elsewhere }
pub trait ReleaseHost: Send + Sync {
    fn installed(&self) -> ReleaseId;
    fn target(&self) -> InstallTarget;   // perguntado a cada Install
    fn list(&self) -> Result<Vec<Release>, String>;
    fn install(&self, r: &Release, progress: &mut dyn FnMut(u8)) -> Result<(), String>;
    fn open_page(&self, url: &str);
}
pub struct GitHubReleases { /* windows, port_dir, appimage, appdir, packaged */ }

// ui/update.rs
pub struct Updater { host: Arc<dyn ReleaseHost>, installed: ReleaseId, self_update: bool, phase: Phase }
enum Phase {
    Idle, Checking, Ready(Vec<Release>), Failed(String),
    Downloading { releases: Vec<Release>, release: Release, percent: u8, progress: Receiver<u8> },
}
impl Updater {
    pub fn new(host: Arc<dyn ReleaseHost>) -> Self;
    pub fn releases(&self) -> &[Release];
    pub fn newer(&self) -> Option<&Release>;
    pub fn installed(&self) -> &ReleaseId;
    pub fn downloading(&self) -> Option<(&Release, u8)>;
    pub fn listing(&self) -> Listing;   // Fetching | Failed | Listed
    pub fn self_update(&self) -> bool;
}
pub fn start_up_check(state: &State) -> Task<Message>;   // substitui splash.rs:194-201
```

O host é um trait object (e não uma tabela de fn pointers) porque o fake carrega canais e
respostas roteirizadas. O `Arc` é clonado para dentro dos closures `blocking`/`blocking_or`.

`State::with_host(paths, host)` injeta o host:
- `State::new` usa `GitHubReleases`;
- `tests/common::state` usa `FakeReleases`.

Ficam privados: `Phase`, `Decision`, `InstallPlan`, `offer_decision` e `install_decision`.
O `version_menu` fica fora (guarda só timers de hover).

**O que muda.**
- Os 4 campos viram `updater`.
- `install_windows`, `install_appimage`, `find_launcher` e `is_packaged` vão para o core.
- `version_menu.rs`, `advanced.rs` e `splash.rs` passam a ler do `Updater`.

**Comportamento.**
- Nada visível muda. `APPIMAGE` e "empacotado" passam a ser lidos uma vez; o env não muda com o
  programa rodando. A gravabilidade continua sondada a cada Install.
- Os testes deixam de usar a rede e de abrir o navegador.

**Testes.**
- O `blocking` dispara a thread na hora, então o fake reporta cada chamada num canal. Os testes
  usam `recv_timeout` e alimentam `Checked`/`Done` como hoje.
- Na task 1, checar se o iced 0.14 expõe `into_stream`. Se expuser, os testes rodam a Task de
  verdade por um `common::run_task`; se não, as mensagens continuam alimentadas à mão.
- Novos via `app::update`:
  - `a_silent_check_asks_the_host`
  - `install_on_a_read_only_appimage_offers_the_page`
  - `install_from_a_source_tree_opens_the_page`
  - `install_while_the_game_runs_says_close_it`
  - `a_confirmed_install_shows_progress`
  - `the_start_up_check_needs_self_update`
- Saem `ui_pages.rs:2421, 2469, 2482, 2491` (helpers puros).
- As escritas diretas em `state.update` (14×), `state.installed` (12×) e `self_update` passam a
  usar mensagens ou o fake.

**Tasks.**
1. O port, `GitHubReleases`, `FakeReleases` e `with_host`. Vermelho: o teste do AppImage
   somente leitura.
2. O sub-state `Updater`, com o receiver dentro de `Downloading`. Vermelho: o teste de progresso.
3. Migrar os testes e privatizar os helpers.

### 11 Seam de plataforma completo

**Problema.** `platform/` é um seam real (Windows e unix), mas a interface é assimétrica:
`has_console`, `set_pipe_nowait` e `pipe_bytes_available` só existem no Windows. O `discord.rs`
tem 10 gates `cfg` em produção. Há código de SO duplicado fora do seam:
- `no_window`/`CREATE_NO_WINDOW`, 4×;
- `make_executable`, 2× com modos diferentes;
- a pasta home, achada de 4 jeitos;
- 3 loops de processo com timeout;
- `which`/`is_executable` duplicados;
- `physical_memory_mb` como par de `cfg`;
- leituras diretas do env do processo.

Os wrappers que chamam ferramentas (`list_gpus`, `steam_running`, `connected_gamepad`) só são
testados no caminho "ferramenta ausente".

**Interface (`core/platform/`, todo item nos dois sistemas).**

```rust
pub fn has_console() -> bool;                       // unix: true
pub fn hide_window(cmd: &mut Command);              // CREATE_NO_WINDOW sempre
pub fn hide_console_window(cmd: &mut Command);      // só sem console (run.py:51-55)
pub enum Exec { Exactly /* 0o755 */, Adding /* mode | 0o755 */ }
pub fn make_executable(p: &Path, how: Exec) -> io::Result<()>;
pub fn which(name: &str, path: &OsStr) -> Option<PathBuf>;
pub fn is_executable(p: &Path) -> bool;
pub fn home(env: Lookup) -> Option<PathBuf>;        // USERPROFILE no Windows, HOME nos demais; vazio = None
pub fn physical_memory_mb() -> Option<u64>;
pub mod ipc { pub trait Transport; pub fn connect(p: &Path) -> io::Result<Box<dyn Transport>>; }
pub mod tool {
    // os tipos do 03, mais:
    pub trait Tools: Send + Sync { fn run(&self, c: &ToolCall) -> Result<ToolOutput, ToolError>; }
    pub struct SystemTools;
}
// core/env.rs
pub type Lookup<'a> = &'a dyn Fn(&str) -> Option<String>;
```

O 03 entrega os tipos e `run_tool` com um adapter só. O 11 acrescenta `Tools`, `SystemTools` e
`FakeTools`; com o segundo adapter o seam fica real. Depois disso `trait Host: Tools`, e
`Host::tool` sai. `set_pipe_nowait` e `pipe_bytes_available` ficam privados em `ipc`.

**O que muda.**
- `gpu.rs`, `legacy.rs` e `steam.rs` passam a usar `Tools`. `list_gpus`, `list_displays` e
  `connected_gamepad` recebem `&dyn Tools`.
- As duplicatas de janela, `make_executable`, `which`, memória e home vão para o seam.
- As leituras de env em `gpu.rs`, `desktop.rs` e `pause_menu.rs` passam a receber um `Lookup`.

**Comportamento.**
- `home` segue o `Path.home()` do Python. A ordem HOME/USERPROFILE do `steam.rs` não muda nada
  visível, porque o Windows ignora home nesse ponto. Com HOME vazio, `paths.rs` passa a cair em
  `"."` em vez de um caminho vazio.
- `make_executable` mantém os dois modos do Python (py:1163 e py:3818).
- **⚠ muda (só tempo):** o polling fica em 20 ms; o `gpu.rs` usava 50.
- A maquinaria de links do `mods.rs` fica fora do 11.

**Testes.**
- Novos com `FakeTools`:
  - `gpus_come_from_the_tool_rows`
  - `a_failing_gpu_tool_lists_nothing`
  - `displays_run_under_the_chosen_backend`
  - `a_gamepad_line_means_connected`
  - `steam_running_reads_the_task_list`
  - `retarget_gives_up_on_timeout` (sem espera real)
- Novos, outros: `home_is_userprofile_or_home_never_empty` e, no unix,
  `adding_keeps_other_mode_bits` (0o640 vira 0o775).
- Ficam: `gpu.rs:550`, `desktop.rs:914` e os testes de junction. Os testes de transporte do
  Discord vão junto com o código.

**Tasks.**
1. `Tools` e `FakeTools`, `Host: Tools`, e depois gpu, steam e legacy. Vermelho:
   `gpus_come_from_the_tool_rows`.
2. Funções simétricas e as duplicatas removidas. Vermelho: `adding_keeps_other_mode_bits`.
3. `home(Lookup)`. Vermelho: o teste de home.
4. O módulo `ipc`. Vermelho: `connect` num caminho inexistente dá NotFound nos dois sistemas.
5. As leituras restantes recebem `Lookup`. Vermelho: o teste do rótulo do `pause_menu`.

## 5. Fase 3: bloqueadores da validação do PR #959

Vem depois do candidato 11. A fonte é o relatório de validação de um testador
(24/10/2026, `25ea999`, Windows 11, RX 6700 XT, dump CUSA01623 v01.02). A Fase 3 se apoia nos
modules dos candidatos: o catálogo (01), o `Store` (01), `core/graphics_options.rs` (02),
`LiveAction` (08) e o seam de plataforma (11).

### F1 Toolchain e lints (bloqueador 4)

- O `rust-toolchain.toml` passa a fixar `channel = "1.99.1"` (a versão local hoje) com
  `components = ["rustfmt", "clippy"]`. Assim o CI e qualquer validador usam o mesmo clippy.
- Os 4 lints que o clippy 1.94.1 acusa são corrigidos mesmo assim, para o código passar nas duas
  versões:
  - `nonminimal_bool`: `core/desktop.rs:194`, 2 ocorrências;
  - `manual_range_contains`: `core/draw.rs:2309`;
  - `nonminimal_bool`: `startup.rs:67`.

  As linhas podem ter mudado nos candidatos; ache-as pelo padrão.
- O `Cargo.toml` ganha `[profile.dev] debug = "line-tables-only"`. O `target/debug` passa de
  12 GB na máquina do testador e do usuário. Os backtraces continuam com arquivo e linha.

### F2 FPS do jogo (bloqueador 1)

**Causa confirmada** (`ui/app.rs:820`): a `subscription` liga o relógio de 33 ms quando
`effects && visible`, sem olhar se há um jogo vivo. `close_on_play` vem desligado, então o fogo
(wgpu, na mesma GPU do jogo) redesenha a janela inteira durante a partida.

**Correção:**
- Uma função privada `fn clock(state: &State) -> Clock` com `enum Clock { Fast, Idle, Off }`.
  `Fast` exige `effects && visible && !state.game.is_live()`, ou splash não terminado, ou um timer
  de menu.
- Com o jogo vivo, o relógio vira `Idle` (`IDLE_CLOCK`, que já lê a saída do jogo).
- O fogo também para de avançar e de pedir redraw com o jogo vivo: o `Tick` não muda o estado do
  fogo.

**Testes:**
- `the_fire_clock_slows_while_the_game_runs`
- `the_fire_does_not_advance_while_the_game_runs`
- `the_fire_clock_comes_back_after_the_game_exits`

**Aceite manual** (do relatório): com o jogo rodando e o launcher aberto atrás, o launcher mede
perto de 0% de GPU e o FPS fica no nível do launcher Python no mesmo trecho. Vai como pedido ao
testador no relatório final.

### F3 Merge de `main` (bloqueador 2, parte 1)

- `git merge origin/main` em `rust-launcher` (merge commit, sem rebase). O `merge-tree` de
  2026-10-24 não tem conflito de texto. Os arquivos mudados dos dois lados (`.gitignore`,
  `packaging/windows/package.sh` e os 3 READMEs) se juntam sozinhos.
- No mesmo commit, os mapas de paridade ganham uma linha para cada teste Python novo ou mudado de
  `main`.
  - Os ainda sem port levam o status `dropped: pending port (F4)` (ou F5…F9).
  - Cada bloco que porta o teste troca o status para `ported`.
  - O relatório final confere que nenhuma linha `pending port` sobrou.
- `tests/test_controller_mappings.py` e `tests/test_controller_mapper_*.py` entram nos checadores
  (`test_runner_parity_doc.py` tem uma lista `FILES` fixa).
- Depois do merge, o `cargo test` continua verde: nenhum teste Rust roda o runtime C nem o Python
  de `main`. Ficam vermelhos, até F4 e F6, só os testes do dump real `real_menu_*` e
  `real_glyph_*`, porque os carimbos de versão mudam.

### F4 #614 + #983: roda do mouse e sticks no teclado (bloqueadores 2 e 3)

`main` está no layout **4** (o #614 subiu para 3 e o #983 para 4). Os dois entram como uma
unidade.

**O que muda em `core/controls.rs`:**
- `WEAPON_ROWS`: `weapon_next` = Wheel Up, `weapon_prev` = Wheel Down, ícone `label:D-PAD`.
- `STICK_ROWS`: `move_up/down/left/right` = W/S/A/D e `look_*` = Up/Down/Left/Right, ícone
  `leftstick` para `move_*`.
- `KEY_ACTIONS = [STICK_ROWS…, rage, grab, dodge, WEAPON_ROWS…]`.
- Rage vai para **F**, com `RAGE_LEGACY_INPUT = "Mouse Middle"`.
- `CONTROLS_LAYOUT = 4`.
- `migrate_controls` com os helpers `inputs_in_use`, `claim_new_actions` e `keep_saved_rage`.
  Reproduz o Python de `main` para os layouts 2 e 3 salvos, para o caminho de tradução nativa e
  para o caminho de teclado antigo. Uma linha nova cujo input já está em uso fica com `None`.
- `bind_key`: `None` nunca troca.
- `keyboard_map`: as linhas de stick no default não são emitidas.

**Bloqueador 3** (decisão Rust, além do Python):
- Um `controls_layout` **maior** que `CONTROLS_LAYOUT` não é tocado: a migração devolve o mapa
  como está.
- Voltar a uma versão anterior nunca reescreve as teclas do jogador.
- Fica registrado como desvio no mapa de paridade do launcher.

**UI (`ui/pages/controls.rs`):**
- 10 nomes de ação novos.
- As chaves i18n novas da nota KEY e da dica de stick.
- Ícones de `game_icon` para `weapon_*` e `move_*`.
- Um botão **"Clear input"** por linha, que manda um novo `ControlsMessage` e grava `None`.

**Runner (`runner/glyphs.rs`):**
- `VERSION` 11 → 12.
- `STICK_BINDINGS`.
- `key_buttons` troca os rótulos de `lstick`/`rstick` por `T/F/G/H` quando uma direção muda.

**Regerados:**
- `assets/i18n.json` (`tools/launcher_i18n.py`);
- os fixtures do launcher (`tools/launcher_fixtures.py`);
- os snapshots `snapshots/controls-*.png`;
- um caso novo de glyph de teclado com movimento remapeado.

**Testes** (os 7 novos de `test_launcher_controls.py` mais os mudados):
- `the_wheel_cycles_the_weapons_by_default`
- `layout_two_keeps_its_table_and_gets_the_weapon_cycle`
- `rage_moves_to_f_but_a_saved_table_keeps_the_wheel_click`
- `stick_directions_are_saved_and_sent_like_other_keyboard_actions`
- `new_directions_do_not_take_keys_from_saved_actions`
- `unbinding_several_inputs_does_not_swap_their_old_keys`
- `direction_conflicts_swap_and_invalid_entries_are_not_forwarded`
- `a_newer_layout_is_left_alone` (bloqueador 3)
- `glyphs::key_map_moves_the_key` atualizado

### F5 #980, parte 1: opções de gráficos e o ini

**`core/graphics_options.rs`** (movido no 02):
- `Options` e `startup_options` ganham:
  - `monitor`: só dígitos ASCII, até `i32::MAX`, senão `""`;
  - `hdr`;
  - `glyph_style`: `off`, `auto`, `xbox`, `nintendo` ou `keyboard`;
  - `anisotropy`: `0`, `2`, `4`, `8` ou `16`.
- `prepare(…, environment)` grava `SPARTAN_MONITOR`, `SPARTAN_HDR`, `SPARTAN_GLYPH_STYLE` e
  `SPARTAN_ANISOTROPY` no ini uma vez, só para as chaves que o ini não tem. A partir daí essas
  chaves seguem a reversão de restart.
- `environment()` exporta as 4. Sem isso, as linhas Monitor, HDR e Controller Type do menu nativo
  não têm efeito: o shim só lê o env.

**Launcher, no catálogo do 01:**
- `monitor`, `hdr` e `glyph_style` ganham linhas Ini **semeadas de App**. O catálogo ganha um
  campo `seed: Option<&'static str>`, a chave App que semeia a linha Ini na leitura. O ini do
  arquivo vence. As linhas App continuam, como em `main`.
- `INI_FLAGS` ganha `hdr`; `SHOWN_INI_KEYS` vai de 18 para 21.
- `SPARTAN_HDR` passa a ser sempre `"1"`/`"0"`.
- As pages que leem essas chaves já passam pelo `Store` (01), então só a tabela muda.

**Regerados:** os fixtures de `graphics/expected/{good,tried,reverted}` (com um caso novo de
`environment=` para a migração) e os fixtures do launcher.

**Testes:**
- os 5 novos de `test_graphics_options.py` e os mudados;
- `glyph_style_ini_overrides_legacy_launcher_choice`;
- `ini_migrates_legacy_display_preferences_and_then_takes_precedence`;
- os casos de Quick Settings do monitor.

### F6 #980, parte 2: menu de pausa

`runner/pause_menu.rs`:
- `VERSION` 14 → 22.
- `native_cheats(data, spans, language)` com:
  - a página Options › Graphics Settings: 5 controles, Restore Defaults com Yes/No, e Apply and
    Restart;
  - 8 linhas de Display mais Apply;
  - 6 linhas de Controls mais Reset/Apply.

  Os rótulos ficam em inglês ou pt, como no Python.
- `native_scrollbar` clona `gosliderbar3` em `gosliderbar10/11/12`.
- `block_end` trata o último bloco como terminando na tabela de símbolos.
- As funções GBL vão de `0xf70` a `0xff7`; `0xfcb` reinicia pelo caminho de restart do runtime.

**Testes** (o novo de `test_pause_menu.py`, os 3 mudados, os 13 novos de `test_native_cheats.py`
e o renomeado):
- unit tests em `pause_menu.rs`;
- `real_menu_*` em `tests/dump.rs`.

O dump real precisa voltar ao verde neste bloco.

### F7 #694: mapeamento de controle USB genérico, port completo

**`core/controller_mappings.rs`** (port de `scripts/controller_mappings.py`):
- `FILENAME = "gamecontrollerdb.txt"`;
- `STEPS`, `State` e a máquina `Capture`: 3 frames estáveis, liberação antes do próximo,
  limiares 7000/18000, inversão `~` e meio eixo `+a`/`-a`;
- `mapping_line`, `mapping_key`, `load_mapping` (a linha da plataforma primeiro),
  `mapping_bindings`, `edit_mapping` (um campo, mantém os desconhecidos) e `save_mapping`
  (atômico, troca só a linha deste GUID e plataforma);
- `configure_environment(env, data_dir)`: as linhas da pasta de dados, depois
  `SDL_GAMECONTROLLERCONFIG_FILE`, depois `SDL_GAMECONTROLLERCONFIG`, sem duplicar (vence a
  última), gravadas em `SDL_GAMECONTROLLERCONFIG`.

**`core/joysticks.rs`**, um seam com dois adapters:

```rust
pub struct Device { pub id: u32, pub guid: String, pub name: String,
                    pub buttons: usize, pub axes: usize, pub hats: usize, pub mapped: bool }
pub trait Joysticks {
    fn platform(&self) -> &str;
    fn devices(&mut self) -> Result<Vec<Device>, JoyError>;
    fn select(&mut self, device: &Device) -> Result<(), JoyError>;
    fn state(&mut self) -> Result<State, JoyError>;          // "Controller disconnected…" ao perder o pad
    fn current_mapping(&mut self) -> Option<String>;          // GUID do aparelho + o resto da linha do SDL
    fn begin_preview(&mut self, line: &str) -> Result<(), JoyError>;
    fn preview_state(&mut self) -> Option<(Vec<&'static str>, [i16; 6])>;
    fn end_preview(&mut self);
    fn close_device(&mut self);
}
pub struct Sdl3 { /* libloading::Library + as funções */ }
impl Sdl3 { pub fn open(port: &Path, data_dir: &Path, library: Option<&Path>) -> Result<Sdl3, JoyError>; }
```

- O `Sdl3` carrega o SDL3 em runtime com `libloading`, que já está no `Cargo.lock` como
  dependência transitiva (0.8.9) e passa a ser dependência direta.
- Ordem de busca, como o Python:
  1. `port/bin/SDL3.dll`
  2. `port/out/SDL3.dll`
  3. `MSYS2_ROOT/clang64/bin/SDL3.dll`
  4. no unix, `libSDL3.so.0` e depois `libSDL3.so`.
- Usa as ~30 funções de `controller_mapper.py`, os hints `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS` e
  `SDL_GAMECONTROLLERCONFIG` (do `configure_environment`) e restaura os hints antigos no `Drop`.
- O `FakeJoysticks` dos testes é o segundo adapter.

**UI (`ui/pages/mapper.rs`):**
- Uma página nova, aberta pelo botão **"Set up a controller"** no topo da página Controls.
  - O botão é recusado com o jogo vivo (`LiveAction::SetUpController`, ligado ao 08).
  - Um SDL3 ausente mostra o erro do Python ("SDL3 was not found…").
- O conteúdo segue a janela Tk:
  - escolha do aparelho, Refresh e os detalhes (botões, eixos, hats);
  - a tabela de `STEPS` com Configure e Clear;
  - o prompt, o status e o preview ("Pressed: …", sticks e gatilhos em %);
  - Cancel capture e Close.
- Comportamento:
  - Esc cancela a captura ou fecha;
  - um poll a cada 20 ms numa subscription, só com a página aberta;
  - cada mudança é validada pelo preview do SDL antes de salvar e salva na hora.
- As ~50 strings novas entram no `i18n.json`.

**Runner:** `configure_environment(&mut env, data)` logo depois da limpeza de `SPARTAN_DERIVED`,
para que a checagem de CPU, o `--gamepad` e o jogo vejam o mesmo mapeamento.

**Core:** `gamecontrollerdb.txt` entra em `USER_FILES` (10 → 11).

**Testes:**
- os 15 de `test_controller_mappings.py`, como unit tests;
- os 3 de `test_controller_mapper_launch.py`: o runner exporta, a atualização preserva o arquivo,
  a UI recusa com o jogo vivo;
- os fluxos da página com `FakeJoysticks`: fora de ordem, cancelar, limpar uma linha, falha ao
  salvar e preservar um mapeamento existente do SDL;
- os 9 de `test_controller_mapper_sdl.py` em `tests/sdl_mapper.rs`, sobre o SDL3 de verdade com
  joysticks virtuais. Eles só rodam com `SPARTAN_TEST_SDL3=<caminho do SDL3>`, como o
  `GOW3_TEST_SDL3` do Python. Localmente: `C:/msys64/clang64/bin/SDL3.dll`.

### F8 #956: barra de título escura

- Primeiro, um screenshot da janela: o iced/winit já pede o tema escuro, então a barra
  provavelmente já está escura e só faltam as cores exatas.
- Depois, `platform::dark_title_bar(hwnd)` no seam do 11:
  - `DwmSetWindowAttribute` 20 (com fallback 19) = escuro;
  - 35 = cor da barra `0x000b0b0d` (`#0d0b0b`);
  - 36 = texto branco;
  - no unix, não faz nada.
- O handler `WindowOpened` obtém o handle por `window::run`.
- A feature `Win32_Graphics_Dwm` entra no `Cargo.toml`.
- `dark_title_bar` chama um `fn dark_title_bar_with(set: &mut dyn FnMut(u32, u32) -> bool)`
  interno (atributo e valor; devolve se o DWM aceitou), que recebe a função do DWM.
- **Testes:** o COLORREF e a ordem de fallback, com uma closure que registra as chamadas. Os 10 testes
  de `test_launcher_titlebar.py` são mecânica do Tk e ficam `dropped`, com o motivo, exceto o das
  cores.

### F9 #619: PKGBUILD do Arch sem Python

Em `main`, o `packaging/linux/PKGBUILD` depende de `python`/`tk`, instala `run.py` e
`launcher/*.py` e lê a versão do arquivo Python. Isso quebra a promessa do cutover de não ter
Python no produto.

- O PKGBUILD passa a compilar o exe Rust (`cargo build --release --locked` no `build()`) e o probe
  (`build.sh`), sem as dependências Python.
- A versão vem de `launcher-rust/Cargo.toml`.
- `tests/test_arch_pkgbuild.py` é ajustado: é um teste estático do arquivo, não do launcher
  congelado.
- Não há build de Arch local; o teste estático é o gate.

### F10 Relatório final

Ver §8. Além do conteúdo de lá, o relatório traz:
- o pedido ao testador para refazer o A/B de FPS (mesmo save e mesmo trecho) e a validação
  completa sobre o novo HEAD;
- a nota para o 2.8.0: a primeira sessão em cada área compila shaders;
- a versão continua 2.8.0. Se `main` lançar um 2.7.x antes do merge, o número é revisto.

## 6. Dependências entre candidatos

| De | Para | O quê |
|---|---|---|
| 01 | 05, 06, 09 | `Store` (05 `Installation::of`, 06 `replace` → chaves) e `catalog::run_default` (09). |
| 02 | 05 | O `facts()` privado de patches passa a usar `GameFolder` na task 1 do 05. |
| 03 | 07 | O 03 não toca o `play_with`; o 07 o leva para o `spawn_game`. |
| 03 | 11 | `core/platform/tool.rs` nasce no 03; o 11 acrescenta `Tools` e `Host: Tools`. |
| 05 | 06 | `Installation` é a primeira view da tabela do `follow`. |
| 09 | 11 | `Context.env` é a fonte do `Lookup` nas leituras de env. |
| 01 | F4, F5 | `controls_layout` e as linhas Ini semeadas de App são linhas do catálogo. |
| 02 | F5 | `prepare(environment)` entra em `core/graphics_options.rs`. |
| 08 | F2, F7 | `is_live()` no relógio; `LiveAction::SetUpController`. |
| 11 | F8 | `dark_title_bar` é uma função simétrica de `platform`. |
| F3 | F4–F9 | O Python de `main` é a referência dos ports. |
| F4 | F6 | Os glyphs (VERSION 12) e o menu (VERSION 22) mudam o dump juntos; o dump volta ao verde no fim de F6. |

## 7. Erros

Nenhum texto de erro visível ao jogador muda, salvo:
- as mensagens de tabela do 04 ("bad symbol/relocation table size");
- o código `-N` do 07 na linha "The game exited (code …)".

Os erros novos de interface (`ToolError`, `ImageError`, `SceError`, `JoyError`) carregam o texto do
script Python correspondente. Os textos novos da Fase 3 são os do Python de `main`.

## 8. Relatório final

Ao fim da Fase 3, um relatório HTML local (como o da revisão) com:
- para cada candidato e bloco F, o que entrou (commits, interfaces, linhas), as mudanças de
  comportamento confirmadas e o que ficou fora ou foi adiado, com o motivo;
- a contagem de testes antes e depois, e o tempo do `cargo test`;
- quantos testes ainda sobem o exe real;
- o estado dos mapas de paridade, incluindo zero linhas `pending port`;
- os itens de F10.

## 9. Escopo

**Entra:** §4 e §5 inteiras, os mapas de paridade, as linhas novas da spec §3.2 do runner e o
relatório final.

**Fica fora:**
- dividir `runner/mod.rs` além do que o 03 e o 09 movem;
- os links de `mods.rs`;
- o default do Steam Deck;
- normalizar valores ao salvar;
- o workspace Cargo;
- release e pacote (o testador refaz a validação do pacote);
- as traduções zh/id do mapper, que o Python de `main` também não tem.

## 10. Riscos

| Risco | Mitigação |
|---|---|
| 11 candidatos e a Fase 3 num PR já em revisão | Um commit por task, gate verde por task, relatório final para o revisor. |
| Paridade de bytes quebrada sem perceber | `runner_fixtures` em todo gate; o dump real ao fim de 02, 04, 07, F6 e F7. |
| Teste renomeado quebra os mapas de paridade | O pytest dos mapas entra no gate de toda task. |
| PC trava no cargo | `jobs = 8`, um `cargo test` e um clippy por task, reruns focados. |
| `State.store` (01) toca ~150 sites | Task 3 do 01 dedicada só à migração, sem mudança de comportamento além da truthiness. |
| iced sem `into_stream` | Os testes do 10 alimentam as mensagens à mão, como hoje. |
| O bug de FPS fica no ar durante o refactor | Escolha do usuário. O contorno do relatório vale até F2: desligar Effects ou ligar `close_on_play`. |
| SDL3 indisponível no CI | Os testes de SDL real só rodam com `SPARTAN_TEST_SDL3`; os fluxos da página rodam sempre, sobre `FakeJoysticks`. |
| Menu de pausa (F6) é o maior port | Os testes do dump `real_menu_*` contra o Python de `main` são o aceite. |

## 11. Referências

- Revisão de arquitetura de 2026-10-24 (relatório HTML local, 11 candidatos).
- Relatório de validação do PR #959 (testador, 2026-10-24).
- `docs/specs/2026-10-21-rust-launcher-design.md`, `docs/specs/2026-10-22-rust-runner-design.md`,
  `docs/specs/2026-10-23-rust-cutover-design.md` e os três mapas de paridade.
- `launcher/spartanport_launcher_win.py` (citado como `py:linha`), `run.py` e `scripts/*.py` em
  `25ea999`. Para a Fase 3: os mesmos arquivos e `launcher/controller_mapper.py`,
  `scripts/controller_mappings.py` e `packaging/linux/PKGBUILD` em `origin/main` (`9dd2d12`).
