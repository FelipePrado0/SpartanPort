# Identidade da release compilada no launcher

Data: 2026-10-25. Estado: implementado (branch worktree-launcher-identity, 2026-10-25).

## 1. Objetivo

O launcher Rust vai receber builds de teste (`windows-v2.8.0-beta2`, uma por issue) e precisa
saber qual release ele é sem depender de nada fora do executável. Hoje a identidade vem do
carimbo `release.json` que um passo do CI grava e empacota; sem ele (os pacotes da 2.8.0 foram
montados localmente, sem carimbo) o launcher cai no `VERSION` fixo em `src/lib.rs`, duplicado
com o `Cargo.toml`. A lista do menu de versões exige um asset `release-windows.json` que só o
mesmo passo publica.

Esta spec compila a identidade no binário a partir da tag, apaga o carimbo inteiro, troca o
filtro da lista por um piso de versão e faz a próxima build da mesma série aparecer como
"disponível", para o testador ir da `issue1-test1` à `issue1-test2` pelo launcher.

Vale para `launcher-rust/`, os dois workflows de pacote e os scripts de `packaging/`. O launcher
Python não muda: ele atualiza por `releases/latest` e pelo zip, sem carimbo.

## 2. Decisões

| Tema | Decisão |
|---|---|
| Identidade | Compilada. O CI passa `SPARTAN_RELEASE_TAG=<tag>` ao `cargo build`; `build.rs` grava no binário; o código lê com `option_env!`. Sem a variável: versão do `Cargo.toml`, estável, sem build. |
| Canal | Pela tag: com build é pre-release, sem build é release. A lista continua seguindo o `prerelease` da API. |
| `VERSION` | `pub const VERSION: &str = env!("CARGO_PKG_VERSION")`. Só o `Cargo.toml` carrega a versão. |
| Carimbo | Some inteiro: `release.json`, assets `release-*.json`, step Stamp, input `stamp`, `packaging/release_stamp.py`, `tests/test_release_stamp.py`, as cópias em `package.sh` e `build.sh`, a leitura em `core/update.rs`. |
| Lista | Entram releases com versão ≥ `2.8.0` (`FIRST_RUST_VERSION`, o primeiro pacote Rust). Equivale a "publicadas de 25/10 em diante": tudo da era Python é ≤ 2.7.0. Versão em vez de data porque "2.8.0" explica a si mesma. |
| Novidade | Estável acima da instalada, como hoje. Senão, a próxima build da mesma série da instalada (`issue1-test1` → `issue1-test2`). Outra série nunca é oferecida. Quem roda estável vê pre-release só na lista. |
| Barra inferior | `v2.8.0 · Windows` ou `v2.8.0 · beta2 · Windows`: a identidade compilada. |
| `--version` | `SpartanPort 2.8.0` ou `SpartanPort 2.8.0-beta2` (a tag sem o prefixo de sistema, ASCII). |
| Fora | Corte por data, gerar arquivo em tempo de execução, canal persistido, launcher Python, macOS. |

## 3. Núcleo

### 3.1 Build (`launcher-rust/build.rs`)

```rust
println!("cargo:rerun-if-env-changed=SPARTAN_RELEASE_TAG");
if let Some(tag) = std::env::var("SPARTAN_RELEASE_TAG").ok().filter(|t| !t.is_empty()) {
    println!("cargo:rustc-env=SPARTAN_RELEASE_TAG={tag}");
}
```

Variável vazia conta como ausente (o workflow passa `inputs.tag`, que pode ser vazio). O
`build.rs` não valida a tag: a regra de parse mora em `core/release.rs`, e o smoke test do CI
(§5) é quem prende uma tag errada.

### 3.2 Identidade (`src/lib.rs`, `src/core/release.rs`)

```rust
// lib.rs
pub const VERSION: &str = env!("CARGO_PKG_VERSION");

// release.rs
pub const FIRST_RUST_VERSION: &str = "2.8.0";

impl ReleaseId {
    /// `Release · VERSION`, sem build: o que um `cargo build` sem tag é.
    pub fn fallback() -> ReleaseId;
    /// A identidade de `tag`; None, vazia ou inválida viram `fallback()`.
    pub fn from_tag(tag: Option<&str>) -> ReleaseId;
    /// A release que este binário é: `from_tag(option_env!("SPARTAN_RELEASE_TAG"))`.
    pub fn this() -> ReleaseId;
    /// `2.8.0`, `2.8.0-beta2`: a tag sem o prefixo de sistema.
    pub fn plain(&self) -> String;
    /// A série e o número de uma build: `issue1-test2` → `("issue1-test", 2)`,
    /// `hotfix` → `("hotfix", 0)`. None numa estável.
    pub fn series(&self) -> Option<(&str, u64)>;
}
```

`from_tag` usa `ReleaseId::parse(tag, prerelease)` com `prerelease = build.is_some()`. Tag
sem `-v<dígito>` (o `parse` devolve None) vira `fallback()`.

`series`: o build sem a sequência final de dígitos ASCII, e essa sequência como número (sem
dígitos, 0). `same_build`, `label` e `short` não mudam.

### 3.3 Release instalada (`src/core/update.rs`)

`ReleaseHost::installed` em `GitHubReleases` devolve `ReleaseId::this()`. Saem `stamp_path`,
`installed_release`, `read_stamp`, `STAMP_FILE`, `stamp_asset`, o campo `appdir` de
`GitHubReleases` (só o carimbo o lia) e os testes do carimbo. `Updater::installed` passa a
documentar "a release que este binário é".

### 3.4 Lista (`src/core/release.rs`, `parse_entry`)

Sai a exigência do asset `release-*.json`. Entra: `version_tuple(&id.version) >=
version_tuple(FIRST_RUST_VERSION)`. O resto fica: draft é pulado, Windows exige `.zip` e Linux
`.AppImage`, ordem por `published_at`.

Efeito nas releases de hoje: o draft `windows-v2.8.0` (sem carimbo) passa a ser listado quando
publicado. `windows-v2.7.0-rust-launcher-test1` já não está no GitHub. As pré-releases vivas
`windows-v2.8.0-intel-test1` (c04f8d8) e `windows-v2.8.0-issue1075-test1` (6093789) foram
compiladas com o código antigo, que só lista releases com um asset `release-*.json`: quem as
usa não vê builds nem releases sem esse asset.

### 3.5 Novidade (`src/core/release.rs`)

```rust
/// A estável acima da instalada; senão, a próxima build da série da instalada.
pub fn newer<'a>(releases: &'a [Release], installed: &ReleaseId) -> Option<&'a Release>;
```

1. `newer_stable(releases, installed)` (regra de hoje). Com resultado, é ele.
2. Senão, se `installed.series()` é `Some((série, n))`: a primeira release listada cuja
   `series()` tem a mesma série e cujo par `(version_tuple(version), número)` é maior que o da
   instalada. O canal da candidata não importa (como em `same_build`).
3. Senão, None.

`Updater::newer` chama `newer` em vez de `newer_stable`.

## 4. Interface

### 4.1 Botão (`src/ui/version_menu.rs`, `button_text`)

Oferta de estável: `· 2.9.0 disponível`, como hoje. Oferta de build da série: `· issue1-test2
disponível`, só o build (chave `{} available`, já traduzida), para o rótulo não repetir a versão:
`Pre-release · 2.8.0 · issue1-test1 · issue1-test2 disponível ▾`.

### 4.2 Menu (`menu`)

A marca "novo" vai para a linha de `newer()`, esteja ela entre as estáveis ou entre as
pre-releases. Hoje as linhas de pre-release passam `false`.

### 4.3 Barra inferior (`src/ui/app.rs`)

`version_label(installed: &ReleaseId, windows: bool) -> String` dá `v{installed.short()} ·
{sistema}`: `v2.8.0 · Windows`, `v2.8.0 · beta2 · Linux`. O `bar` passa
`state.updater.installed()`.

### 4.4 `--version` (`src/main.rs`)

`println!("{APP_NAME} {}", ReleaseId::this().plain())`. Nenhum arquivo é lido.

### 4.5 Textos

Nenhuma chave nova.

## 5. Workflows e packaging

- `.github/workflows/windows-zip.yml`: saem o input `stamp`, o step Stamp, o `setup-python` (só
  servia ao Stamp) e `dist/release-windows.json` do artifact e do `gh release upload`. O step
  "Build the launcher" ganha `env: SPARTAN_RELEASE_TAG: ${{ inputs.tag }}`. O smoke recebe
  `SPARTAN_RELEASE_TAG: ${{ inputs.tag }}`.
- `.github/workflows/linux-appimage.yml`: o mesmo; a variável vai no step "AppImage", onde
  `packaging/linux/build.sh` roda o `cargo build`.
- `packaging/windows/package.sh` e `packaging/linux/build.sh`: sai a cópia de `release.json`.
- `packaging/release_stamp.py` e `tests/test_release_stamp.py`: apagados.
- `packaging/windows/smoke-test.ps1` e `packaging/linux/smoke-test.sh`: sai a conferência de
  `release.json`. Com `SPARTAN_RELEASE_TAG`, `--version` tem de imprimir exatamente
  `SpartanPort <tag sem o prefixo de sistema>` (`windows-v2.8.0-beta2` → `SpartanPort
  2.8.0-beta2`; PowerShell `-replace '^[a-z]+-v', ''`, bash `${SPARTAN_RELEASE_TAG#*-v}`). Sem
  a variável, a conferência de hoje (`SpartanPort <x.y.z>`).
- `launcher-rust/README.md`, seção Environment: `SPARTAN_RELEASE_TAG` é lida em tempo de
  compilação; build local de teste: `SPARTAN_RELEASE_TAG=windows-v2.8.0-beta2 cargo build
  --release --locked` (PowerShell: `$env:SPARTAN_RELEASE_TAG = 'windows-v2.8.0-beta2'`).
- `.github/workflows/rust-launcher.yml` (fmt, clippy, test) não muda: compila sem tag, e os
  testes comparam contra `ReleaseId::this()`, não contra texto fixo.
- `docs/specs/2026-10-22-version-menu-design.md` (§3.3, §3.4, §5) e
  `docs/specs/2026-10-23-rust-cutover-design.md` (§3.4, frase do carimbo) ganham uma linha
  apontando para esta spec.

## 6. Erros

| Situação | Comportamento |
|---|---|
| `SPARTAN_RELEASE_TAG` inválida no build | Identidade `fallback()`, sem aviso em tempo de execução. O smoke test do CI falha: `--version` não bate com a tag. |
| Variável vazia | Igual a ausente. |
| Nenhuma release ≥ 2.8.0 na lista | Nenhuma estável e "No test builds", como hoje com lista vazia. |
| Instalada é build de teste e a série não está na lista | Só a regra da estável. |
| Lista indisponível | Como hoje: "Could not fetch the versions"; a identidade não depende da lista. |

## 7. Testes

### 7.1 Núcleo (`cargo test`)

- `release.rs`: `from_tag` com None, vazia, `windows-v2.8.0` (Release, sem build),
  `windows-v2.8.0-beta2` (PreRelease, `beta2`), `linux-v2.8.1-issue1-test2`, tag inválida
  (`fallback()`); `plain` de estável e de build; `series` de `issue1-test2`, `hotfix`, `teste3`
  e de uma estável (None).
- `newer`: estável acima vence mesmo com build da série listada; mesma série com número maior;
  mesma série com número menor ou igual (nada); outra série (nada); mesma série em versão maior
  (`2.9.0-issue1-test2` sobre `2.8.0-issue1-test1`); instalada estável com pre-releases listadas
  (nada); lista vazia.
- `parse_releases`: 2.8.0 sem asset de carimbo entra; 2.7.0 com carimbo não; draft continua
  fora; a fixture `mixed()` perde os assets `release-*.json`.
- `update.rs`: saem os testes do carimbo (`the_stamp_names_the_installed_release`,
  `a_missing_or_broken_stamp_is_this_version`, `the_stamp_sits_beside_the_launcher`).
- `tests/core_boundary.rs:38` (pin `CARGO_PKG_VERSION == VERSION`) sai: ficou tautológico.

### 7.2 Interface (`iced_test`)

- `tests/ui_home.rs`: o botão nomeia `ReleaseId::this()`; instalada `2.8.0 · issue1-test1` com
  `issue1-test2` listada mostra `· issue1-test2 disponível` no botão e "novo" na linha dela;
  instalada estável com a mesma lista não mostra nada.
- `tests/ui_navigation.rs`: `version_label` com e sem build, Windows e Linux.
- `tests/startup.rs`: `--version` imprime `SpartanPort {ReleaseId::this().plain()}`.

### 7.3 Pacote

Os dois smoke tests conferem `--version` contra a tag (rodam no CI). Conferência local:
`SPARTAN_RELEASE_TAG=windows-v2.8.0-beta2 cargo run -- --version` imprime
`SpartanPort 2.8.0-beta2`.

## 8. Riscos

- Um desenvolvedor que deixa `SPARTAN_RELEASE_TAG` no shell compila um binário rotulado como
  build de teste. Visível na barra inferior e no `--version`; `rerun-if-env-changed` garante que
  tirar a variável recompila.
- O piso `2.8.0` é uma constante no código. Uma correção na linha 2.7 (Python) nunca entra na
  lista, de propósito.
- Releases antigas carimbadas não ganham nada com o carimbo; o arquivo `release.json` que já
  está num pacote instalado é ignorado.
- Sequência de publicação: o zip e o AppImage do draft `windows-v2.8.0` foram compilados a
  partir de 6d733ed, cujo launcher lista só releases carimbadas. Publicado assim, quem tem a 2.8.0
  nunca veria uma release posterior. Antes de publicar: recompilar o draft a partir da main com os
  dois workflows (`tag: windows-v2.8.0`; o job de publicação sobe com `--clobber`), conferir que a
  saída do smoke é `SpartanPort 2.8.0` e só então publicar. Quem está nas duas builds test1
  carimbadas acima não vê a test2 nem a 2.8.0 no menu: baixa a test2 à mão (ou se anexa às
  releases de destino um asset de preenchimento chamado `release-windows.json`).

## 9. Referências

- `docs/specs/2026-10-22-version-menu-design.md`: menu de versões, carimbo (§3.3, §5) e regra
  de novidade (§3.4), que esta spec substitui.
- `docs/specs/2026-10-23-rust-cutover-design.md` §3.4: "só lista releases carimbadas", que esta
  spec substitui pelo piso de versão.
- `.github/workflows/windows-zip.yml`, `.github/workflows/linux-appimage.yml`,
  `packaging/windows/smoke-test.ps1`, `packaging/linux/smoke-test.sh`.

## 10. Desvios

Nenhum desvio de comportamento em relação a esta spec. Dois acréscimos fora do que ela lista:

- `same_build` (`core/release.rs`) teve a doc reescrita (o canal compilado no binário, não o do
  carimbo, que não existe mais).
- O teste `releases_below_the_first_rust_one_are_not_listed` mantém de propósito um asset
  `release-windows.json`: prova que o asset já não decide a lista, só o piso de versão.
- Convenção de tags de teste: `<sistema>-v<versão>-<issue>-test<N>` (por exemplo
  `windows-v2.8.0-issue1-test2`). `series()` descarta os dígitos finais da build, então `beta2` e
  `beta3`, ou `test1` e `test2` soltos de issues diferentes, contariam como uma série só; o
  launcher oferece a próxima build da mesma série. Documentada na descrição do input `tag` dos
  dois workflows e no README do launcher.
