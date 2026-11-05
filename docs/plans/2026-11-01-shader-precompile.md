# Pré-compilação de shaders na primeira abertura: roadmap completo

> **Para agentes:** este documento é autossuficiente. Leia inteiro antes de tocar em qualquer arquivo.
> Ele diz o que fazer, em que ordem, onde está cada coisa no código, como testar, quando parar e
> pedir aprovação, e quando cada fase está pronta. Os pontos em que você para e espera o mantenedor
> estão marcados com **PARE**. Marque os checkboxes (`- [ ]` para `- [x]`) e atualize a tabela de
> estado no mesmo commit em que uma tarefa termina.

## Estado

| Fase | Assunto | Estado | Branch / PR |
|:--|:--|:--|:--|
| F0 | Preparação, linha de base e decisões | em andamento (falta medir o driver) | `feat/shader-precompile` |
| F1 | Captura das entradas no momento da tradução | código e testes prontos; falta sessão de jogo | `feat/shader-precompile` |
| F2 | API de tradução offline e verificador 100% | API e verificador prontos; falta provar 100% em cache novo | `feat/shader-precompile` |
| F3 | Leitura do código dos shaders nos WADs | leitor e testes prontos; falta `app0` (F5) | `feat/shader-precompile` |
| F4 | Manifesto: formato e ferramenta que o gera | formato, ferramenta e testes prontos; falta gerar de cache real | `feat/shader-precompile` |
| F5 | Materialização na primeira abertura (shaders) | não iniciada | `feat/shader-precompile` |
| F6 | Pré-compilação dos pipelines no driver | só o diário de crash pronto (`CrashJournal`) | `feat/shader-precompile` |
| F7 | Invalidação: GPU, driver e versão | decisão, marcador e testes prontos; falta ligar no jogo (F5) | `feat/shader-precompile` |
| F8 | Interface: tela de preparo, launcher e textos | não iniciada | `feat/shader-precompile` |
| F9 | Campanha de gravação e empacotamento | não iniciada | `feat/shader-precompile` |
| F10 | Validação final e release | não iniciada | `feat/shader-precompile` |

**Autorização do mantenedor (2026-11-05):** spec de F0 a F4 e F7 aprovada, com as decisões
recomendadas de 4.1. Nesta tarefa, commit e push nesta branch a cada passo, sem pedir aprovação de
cada mensagem. PR, merge, release e `main` continuam exigindo aprovação explícita.

---

## 1. Objetivo

**Objetivo:** na primeira abertura, o port prepara de uma vez todos os shaders e pipelines que o
jogo usa, com uma tela de progresso. Depois disso o jogo não engasga mais compilando shader. O
preparo só é refeito quando muda a GPU, o driver ou a versão do port, como nos jogos modernos de PC.

**Restrição que não se negocia:** o release **não leva nenhum código de shader do jogo**. O código
sai do dump do próprio usuário (arquivos `.WAD` e `eboot.bin`). O release só leva um manifesto com
o contexto de cada shader (estado de render, dados de usuário, chaves de pipeline) e onde achar o
código no dump.

**Por que isto é necessário:** os shaders do PS4 só ficam conhecidos quando o jogo usa cada um, e a
tradução depende do estado do draw. Por isso hoje o cache se forma enquanto a pessoa joga e a
primeira passada engasga em toda área nova.

### Definição de pronto (do roadmap inteiro)

- [ ] Numa instalação limpa com o manifesto, a primeira abertura prepara tudo com tela de
      progresso, e uma passada pelas áreas gravadas mostra **0 shaders e 0 pipelines compilados
      durante o jogo** (linha `Disk shaders: ... 0 compiled` no log e nenhuma nota de compilação na tela).
- [ ] O verificador (F2) reproduz **100%** dos shaders de uma sessão de gravação, byte a byte.
- [ ] A matriz de invalidação (F7) passa inteira: troca de GPU, troca de driver, troca de versão do
      port, manifesto novo e cache apagado.
- [ ] Sem manifesto, ou com a opção desligada, o comportamento é idêntico ao de hoje.
- [ ] Windows e Linux compilam e passam nos testes. macOS não é exigido.
- [ ] Nenhum byte de código de shader do jogo no repositório nem nos pacotes de release.

---

## 2. Regras do projeto (obrigatórias)

### Fluxo de trabalho

- **Spec antes de código.** Antes de implementar cada fase, apresente ao mantenedor uma spec curta
  e **PARE até ele aprovar**:
  - **Objetivo:** uma frase.
  - **Requisitos:** `QUANDO <situação>, O SISTEMA DEVE <comportamento>`, um por linha.
  - **Arquivos:** os que vão mudar.
  - **Validação:** os testes automáticos e o que o mantenedor testa no jogo.
- **TDD** em toda lógica com condição não trivial (formato de arquivo, remapeamento de índices,
  invalidação, leitura de WAD): escreva o teste antes, veja falhar, depois implemente.
- **Uma branch só:** `feat/shader-precompile` (já criada a partir de `release/3.0.0`). Nada de commit
  em `main`, push, merge, PR, release ou fechar issue sem autorização explícita.
- **Mensagem de commit:** antes de cada commit, mostre a mensagem ao mantenedor e espere ele
  aprovar. Formato: use a skill de mensagens do projeto em `.claude/skills/` (blocos EN-US, `---`, PT-BR; verbos no particípio;
  hífen nos itens; **nunca** `Co-Authored-By` nem qualquer menção a IA).
- **Commits pequenos:** um por unidade lógica (por exemplo F1 captura, F1 testes). Cada commit
  compila e passa nos testes sozinho.

### Código

- **Nome do projeto em código, logs, commits e branches:** sempre `SpartanPort`. Nunca escreva o nome
  do jogo em identificadores, logs ou mensagens novas. Prefixo de comentário das mudanças no
  shadPS4: `// spartanport:`.
- **Linux é obrigatório:** nada pode quebrar o build ou o comportamento no Linux. Código só de
  Windows fica isolado (`#ifdef _WIN32`) e opcional. macOS é opcional e sempre o último passo.
- **Toda opção nova do usuário vira controle no launcher Rust** (checkbox ou botão). Nunca peça ao
  usuário para digitar variável de ambiente. Variáveis `SPARTAN_*` são só o transporte entre
  launcher e jogo.
- **Launcher:** o launcher é Rust (`launcher-rust/`). Valide com `cargo test` lá. O launcher Python
  (`launcher/`) é legado: não mexa.
- **Textos de interface:** sem emojis e sem travessão para explicar ou juntar ideias. Palavras
  simples. Todo texto novo tem tradução em todas as línguas existentes.
- **A opção "Show shader notes in the corner" continua desligada por padrão.** Não mude isso.
- Reuse o que já existe (warm-up, tela de carregamento, pool de threads, formato do cache) antes de
  criar coisa nova. Sem abstração com um uso só.

### Jogo e testes no jogo

- **O mantenedor joga, o agente monitora.** Abra o jogo com o launcher (`--play`) e só observe o log.
  Nunca injete teclas e nunca use script que feche o jogo sozinho. Só diga que funcionou depois de
  confirmar quadros reais do jogo no log ou numa captura.
- Teste sempre no monitor principal 1440p (`monitor=0`, saída 2560x1440).
- **Nunca apague uma pasta `mod-game-*`** com `rm -rf`: ela tem junctions que apontam para o dump real.
- Nunca rode nada que escreva dentro do dump do jogo (`game_dir`). Leitura só.
- Termine cada dia de trabalho com um pedido concreto de sessão de jogo ao mantenedor (o que abrir,
  onde ir, o que observar).

---

## 3. O que já foi descoberto (não refaça)

Tudo abaixo foi medido em 2026-11-01 no dump `CUSA01623` do mantenedor
(`E:\Games\ps4\Jogos\CUSA01623`, 48 GB) e no cache local dele (`user/cache/CUSA01623`).

### 3.1 Onde estão os shaders

- Ficam sem compressão dentro dos `.WAD` em `EXEC/WAD/PS4_LE/` e `EXEC/DLC/*/WAD/PS4_LE/`, mais
  16 no `eboot.bin`. São 413 WADs e 23 GB.
- Cada shader termina num rodapé `OrbShdr` (`AmdGpu::BinaryInfo` em
  [regs_shader.h](../../gpu/shadps4/video_core/amdgpu/regs_shader.h)), com 28 bytes:
  assinatura (7), versão (1), um `u32` com bits (`length` nos 24 bits altos), 4 bytes, `shader_hash`
  (`u64`) e `crc32` (`u32`).
- **O início do código não é `rodapé - length`.** O código começa com o token `s_mov_b32 vcc_hi, imm`
  (`0xBEEB03FF`), e o rodapé fica em `início + (imm + 1) * 8` bytes (ver `SearchBinaryInfo`). O
  tamanho do código passado ao tradutor é `length` bytes a partir do início. Todos os 97.567
  shaders do dump têm esse token.
- Total: **97.567 shaders únicos** (cerca de 68 mil VS e 68 mil PS entre repetidos, 6 compute).

### 3.2 Como o cache identifica cada shader

- `pgm_hash` (a chave do programa no cache) é exatamente o `shader_hash` do rodapé
  (`AmdGpu::GetParams`). Os 4.550 programas do cache local do mantenedor estavam todos nos WADs (100%).
- O mesmo hash aparece em vários WADs com bytes idênticos (conferido). O primeiro achado serve.
- Arquivos do cache em `user/cache/<serial>/`:
  - `0x<pgm_hash>_<perm>.spv`: SPIR-V de uma permutação.
  - `0x<perm_hash>.meta`: especialização + `Info` (ver `RegisterShaderMeta`).
    `perm_hash = HashCombine(pgm_hash, perm_idx)`.
  - `g_0x<hash>.key` e `c_0x<hash>.key`: chaves de pipeline gráfico e de compute.
  - `profile.bin`: o `Shader::Profile` da GPU. Se a GPU muda, o cache é recomeçado
    (`CheckCacheProfile`).
  - `disk_shaders.bin`: marcador do modo sob demanda.
  - `pipeline-binaries/`: binários do driver (`VK_KHR_pipeline_binary`), por chave global do driver.

### 3.3 A prova de conceito (POC)

Arquivos commitados nesta branch:

- [tests/test_shader_replay.cpp](../../tests/test_shader_replay.cpp): para cada `.meta` do cache, lê
  o código no WAD, monta um `Shader::Info` novo, instala `Info::UdSnapshot` (dados de usuário) e
  `Gcn::FetchShaderSnapshot` (fetch shader), chama `TranslateProgram` + `EmitSPIRV` e compara com o
  `.spv` salvo.
- Alvo `shader-replay-test` em [gpu/CMakeLists.txt](../../gpu/CMakeLists.txt).
- Gancho provisório `Info::replay_skip_srt_walk` em
  [info.h](../../gpu/shadps4/shader_recompiler/info.h) (pula o `walker_func` do SRT, que lê memória do jogo).
- Índice gerado por script Python (`hash início tamanho caminho`, um por linha). O script não ficou
  no repositório; a lógica está em 3.1 e é refeita em C++ na F3.

Resultado nos 4.837 shaders do cache: **0 travamentos, 3.934 idênticos (81%)**. Todos os VS (com e
sem fetch) e compute bateram. A tradução leva **1,1 ms por shader** num núcleo.

**Por que 903 diferiram (todos fragment):** em `PipelineCache::PublishShaders`
([vk_pipeline_cache.cpp](../../gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_cache.cpp)), uma
permutação nova grava o `.meta` com `program->info`, o `Info` compartilhado do programa, cujo
`flattened_ud_buf` é atualizado a cada draw. O `.meta` acaba com os dados de usuário de um draw
**posterior**, e não os da tradução. Nos casos que diferiram, esse draw posterior tinha uma
textura MSAA (o replay gerou `OpImageFetch ... Sample` e `SampleId`; o original, uma amostragem
comum). A especialização (`ImageSpecialization::type`) separa MSAA de não MSAA. O defeito está só
em quais dados vão para o arquivo. A F1 corrige isso.

### 3.4 Leituras de memória do jogo durante a tradução

A tradução só lê memória do jogo nestes pontos. A tradução offline precisa substituir todos:

| Ponto | Onde | Substituição offline |
|:--|:--|:--|
| Código do shader | `ShaderParams::code` | bytes do WAD |
| `pgm_base` (cbuffer embutido no código) | `Info::ProgramBase()`, `resource.h` | ponteiro do buffer lido do WAD |
| Dados de usuário (16 SGPRs) | `Info::UserData()` | gravados (F1) via `UdSnapshot` |
| Buffer achatado (sharps T#, V#, S#) | `Info::FlatUserData()` | gravado no momento da tradução (F1) via `UdSnapshot` |
| Walker do SRT | `Info::RefreshFlatBuf` (`walker_func`) | não roda; o buffer gravado já é o resultado |
| Fetch shader (parse) | `Gcn::ParseFetchShader` | `FetchShaderData` gravado via `FetchShaderSnapshot` |
| V# dos atributos de vértice | `VertexAttribute::GetSharp` | V# gravados (F1) via `FetchShaderSnapshot::buffers` |
| Constantes de tesselação | `Info::ReadTessConstantBuffer` | conferir na F2 (o jogo não usou tesselação no cache medido) |

A POC reconstruía os V# a partir de `vs_attribs` (classe numérica e swizzle) e mesmo assim bateu em
todos os VS. A gravação definitiva grava os V# reais (`FetchShaderSnapshot::buffers`), sem chute.

---

## 4. Arquitetura escolhida

A ideia central: **o preparo gera o cache local do usuário no formato que já existe**. Depois que
os arquivos estão lá, o caminho atual (leitura sob demanda, prefetch, binários do driver) faz o
resto sem mudança. Menos código novo e o jogo continua funcionando igual quando o preparo não roda.

```
Equipe joga com "Gravar shaders" ligado
        |  (.meta com entradas da tradução + .key)
        v
shader-manifest build  --->  shaders/<serial>.spm  (vai no release; sem código do jogo)
        |
Usuário abre o jogo pela 1a vez
        v
F5 Materializar: lê o manifesto, lê o código nos WADs dele, traduz em paralelo,
                 grava .meta/.spv/.key no user/cache/<serial>/ (formato atual)
        v
F6 Pipelines: cria todos os pipelines das chaves, enche pipeline-binaries/ do driver
        v
Jogo roda com o cache completo. Próximas aberturas: nada a fazer.
Mudou GPU, driver ou versão? F7 decide o que refazer.
```

Partes:

1. **Gravação (F1):** o `.meta` passa a levar as entradas exatas da tradução. Vale para todo mundo,
   não só para a equipe, e conserta o defeito de 3.3.
2. **Tradução offline (F2):** uma API no `shader_recompiler` traduz a partir de código + entradas
   gravadas, sem memória do jogo. Um verificador prova 100%.
3. **Leitor de WAD (F3):** acha o código pelo `pgm_hash`, primeiro pela dica de local do manifesto
   (rápido) e, se não bater, varrendo o dump.
4. **Manifesto (F4):** arquivo único, comprimido, determinístico, juntando gravações de várias pessoas.
5. **Materializador (F5) e pipelines (F6):** rodam na abertura, com tela de progresso e opção de pular.
6. **Invalidação (F7):** um marcador diz o que já foi feito, para quê e com o quê.

### 4.1 Decisões recomendadas (confirmar na spec da F0)

Apresente estas decisões na spec da F0 e **PARE** até o mantenedor aprovar ou mudar:

1. **Ligado por padrão**, com checkbox no launcher (Desempenho): "Prepare all shaders on first start".
2. **Pode pular:** Esc ou B na tela de preparo pula. O jogo segue sob demanda (como hoje) e o
   preparo tenta de novo na próxima abertura.
3. **Gravação** é um checkbox em Avançado: "Record shaders for the shared list", desligado por padrão.
4. **Manifesto no release** em `shaders/<serial>.spm` ao lado do executável (Windows) e dentro do
   AppImage (Linux).
5. **Formato do manifesto** próprio, binário, versionado e comprimido com miniz (já está em
   `gpu/third_party/miniz`). Sem dependência nova.
6. **O manifesto pode conter** `user_data`, buffer achatado, `RuntimeInfo`, `Bindings`,
   `FetchShaderData`, V# e chaves de pipeline. São descritores de recurso e estado de render, não
   código do jogo. O código e os cbuffers embutidos vêm sempre do WAD do usuário.

---

## 5. Ferramentas e comandos (copie e use)

Windows (MSYS2 CLANG64), na raiz do repositório:

```bash
source ./msys2-env.sh
ninja -C out/gpu spartanport-probe                  # jogo
ninja -C out/gpu shader-replay-test                 # POC / verificador
out/gpu/shader-replay-test.exe user/cache/CUSA01623 <pasta do jogo ou indice> [primeiro] [quantos]
ninja -C out/gpu perf-overlay-test warmup-inbox-test graphics-settings-test
out/gpu/perf-overlay-test.exe && out/gpu/warmup-inbox-test.exe && out/gpu/graphics-settings-test.exe
```

Linux:

```bash
bash build.sh --test
ninja -C out/gpu perf-overlay-test warmup-inbox-test graphics-settings-test
```

Launcher:

```bash
cd launcher-rust && cargo fmt --check && cargo clippy --all-targets -- -D warnings && cargo test
```

Ferramentas úteis: `spirv-dis` e `spirv-val` (em `/clang64/bin`). Para comparar dois SPIR-V sem ruído
de IDs: `diff <(sed -E 's/%[0-9]+/%/g' a.txt) <(sed -E 's/%[0-9]+/%/g' b.txt)`.

Caminhos do mantenedor: dump em `settings.json` (`game_dir`), cache em `user/cache/CUSA01623`, log em
`user/last_run.log`.

**Cuidado:** mudar `info.h` ou outro cabeçalho do `shader_recompiler` recompila a biblioteca
`spartanportgpu` inteira. Rode builds longos em segundo plano.

---

## 6. Mapa do código

| O quê | Onde |
|:--|:--|
| Entrada da tradução | `Shader::TranslateProgram` em `gpu/shadps4/shader_recompiler/recompiler.cpp` |
| Emissão SPIR-V | `Shader::Backend::SPIRV::EmitSPIRV` em `shader_recompiler/backend/spirv/emit_spirv.h` |
| `Info`, `UdSnapshot`, `RefreshFlatBuf` | `shader_recompiler/info.h` |
| Especialização (chave da permutação) | `shader_recompiler/specialization.h` |
| Snapshot do fetch shader | `shader_recompiler/frontend/fetch_shader.{h,cpp}` |
| `Profile` | `shader_recompiler/profile.h`; preenchido no construtor `PipelineCache::PipelineCache` |
| Tradução síncrona | `PipelineCache::GetProgram` e `CompileModule` em `video_core/renderer_vulkan/vk_pipeline_cache.cpp` |
| Tradução assíncrona | `PipelineCache::QueueShader` (captura) e `PublishShaders` (grava `.meta`/`.spv`) |
| Gravação/leitura do `.meta`, `.key` | `vk_pipeline_serialization.cpp`: `RegisterShaderMeta`, `RegisterShaderBinary`, `RegisterPipelineData`, `LoadShaderMeta`, `Info::Serialize`, `StageSpecialization::Serialize` |
| Versões do formato | topo de `vk_pipeline_serialization.cpp`: `ShaderBinaryVersion`, `ShaderMetaVersion`, `PipelineKeyVersion` |
| Perfil e recomeço do cache | `PipelineCache::CheckCacheProfile` |
| Início do cache na abertura | `vk_presenter.cpp` perto da linha 325: `UseOnDemandPipelines` decide entre `StartOnDemand` e warm-up |
| Warm-up em massa com tela | `PipelineCache::WarmUp(progress)`; tela via `SpartanOverlay::SetLoading` + `Presenter::PresentLoadingFrame` |
| Modo sob demanda | `StartOnDemand`, `IndexDiskShaders`, `StartDiskPrefetch`, `StartBinaryConversion`, `LoadDiskProgram` |
| Pool de threads do warm-up | `WarmupPool`, `WarmupThreadCount` em `vk_warmup_inbox.h` |
| Armazenamento | `video_core/cache_storage.{h,cpp}` (`Storage::DataBase`, `BlobType`) |
| Configuração vinda do loader | `SpartanGpuConfig` em `gpu/spartanportgpu.h`; preenchida em `src/probe.c` perto da linha 1311 (`app0` é a pasta do jogo) |
| Textos do overlay | `gpu/shim/spartanport_overlay.cpp` + `gpu/shim/spartanport_ui_language.h` + `gpu/shim/ui_language/*.h` |
| Launcher: opções de shader | `launcher-rust/src/ui/pages/performance.rs` (perto da linha 360), `src/core/settings.rs`, `src/core/env.rs`, `assets/i18n.json` |
| Testes do launcher | `launcher-rust/tests/env_contract.rs`, `ui_pages.rs`, `ui_snapshots.rs` |
| Empacotamento | `packaging/windows/package.sh`, `packaging/linux/build.sh`, `packaging/linux/AppRun` |
| CI | `.github/workflows/windows-zip.yml`, `linux-appimage.yml`, `rust-launcher.yml` |

---

## 7. Fase 0: preparação, linha de base e decisões

- [ ] Leia este documento inteiro, depois `CONTEXT.md`, `docs/adr/` e o README (seções de build e testes).
- [ ] Confirme a branch: `git branch --show-current` mostra `feat/shader-precompile`.
- [x] Rode a linha de base: build do jogo, os três testes C++ e `cargo test`. Anote qualquer falha
      que já existia antes de mexer (não é sua para consertar; registre).
      Resultado em 2026-11-05: build e os três testes C++ passaram. `cargo test` teve 776 aprovados e
      1 falha que já existia: `core::patches::compile_tests::built_in_file_is_not_external`.
- [ ] Rode a POC (seção 5) e confirme os números de 3.3 na sua máquina. Para o índice, use o
      verificador da F2 quando existir; até lá, refaça o índice com a regra de 3.1.
- [ ] Meça o tempo de compilação no driver, que ainda é desconhecido: abra o jogo com cache vazio
      (pasta do cache renomeada, **nunca apagada**, para poder voltar) e leia no log os contadores
      de pipeline (`g_spartanport_pipeline_driver_ns`, linhas `Disk shaders`). Isso dá a estimativa da
      tela de preparo da F6.
- [x] Escreva a spec da F0 com as decisões de 4.1 e o plano das fases. **PARE.** Aprovada em 2026-11-05.
- [x] POC commitada como base da F2 (`tests/test_shader_replay.cpp`, alvo no CMake e o gancho
      provisório `replay_skip_srt_walk` em `info.h`, que não faz nada no jogo). Na F2 o gancho é
      trocado pela API `ReplayTranslate`.

**Pronto quando:** linha de base registrada, decisões aprovadas, tempo de driver medido.

---

## 8. Fase 1: captura das entradas no momento da tradução

**Objetivo:** cada `.meta` passa a guardar exatamente o que a tradução usou, para que qualquer
permutação possa ser reproduzida fora do jogo.

### 8.1 O que gravar (`ReplayInputs`)

Crie uma struct (por exemplo em `shader_recompiler/replay.h`) com:

- `std::array<u32, 16> user_data`: os 16 SGPRs vistos pela tradução.
- `std::vector<u32> flat_ud`: o `flattened_ud_buf` **logo após** a tradução daquela permutação
  (resultado do `RefreshFlatBuf` dentro de `FlattenExtendedUserdataPass`).
- `std::vector<AmdGpu::Buffer> fetch_buffers`: os V# dos atributos (`FetchShaderSnapshot::buffers`).
- `u32 code_size` e `u64 code_xxh3`: tamanho e hash XXH3 do código (o XXH3 já é usado no projeto).
  Servem para conferir que o código lido do WAD é o mesmo.
- `std::vector<u32> gs_copy` não: o VS de cópia do geometry shader é outro programa; registre só o
  `vs_copy_hash` que já está em `RuntimeInfo` e confira na F2 se o jogo usa GS.

### 8.2 Onde capturar

- **Caminho assíncrono:** em `QueueShader`, `build->info` é o `Info` da tradução e `build->fetch`
  o snapshot. Depois de `TranslateProgram` (dentro do `capture_job`), copie
  `build->info.flattened_ud_buf`, `build->user_data` e `build->fetch.buffers` para a build.
  Em `PublishShaders`, passe esses dados para `RegisterShaderMeta`. **Não** use `program->info`
  para os dados de usuário.
- **Caminho síncrono:** em `GetProgram`, use `new_info` (o `Info` passado a `CompileModule`) depois
  da tradução, não `info` (o do programa).
- **Permutação inicial:** a primeira tradução do programa também passa pelos dois caminhos acima.
  Confira que ela grava as entradas dela.

### 8.3 Formato

- Acrescente `ReplayInputs` ao fim do `.meta` (`RegisterShaderMeta` e `LoadShaderMeta`).
- **Suba `ShaderMetaVersion`** (com comentário `// spartanport:` dizendo o motivo). Isso invalida os
  `.meta` antigos uma vez, o que é aceitável: o preparo da F5 reconstrói tudo.
- `LoadShaderMeta` continua devolvendo o mesmo para o jogo. As entradas só são lidas por quem precisa
  (verificador, gerador do manifesto).

### 8.4 Testes (antes do código)

- [x] Teste de ida e volta: serializar e ler um `.meta` com `ReplayInputs` preenchido devolve os
      mesmos bytes. Coloque num teste C++ novo (alvo `EXCLUDE_FROM_ALL`, mesmo padrão de
      `scene-resolution-test` no CMake).
- [x] Teste que um `.meta` da versão antiga é recusado sem travar.
- [x] Teste (com stubs, sem GPU) que, no caminho assíncrono, as entradas gravadas são as da build e
      não as de `program->info`. Se for difícil isolar, documente na spec e cubra pelo verificador da F2.
      O teste cobre `CaptureReplayInputs`. A escolha da build no caminho assíncrono só é provada pelo
      verificador da F2, numa sessão gravada.

### 8.5 Validação

- [x] Build Windows e Linux. Linux em 2026-11-05: WSL Ubuntu 24.04 com GCC, código já com a main;
      `replay-inputs`, `shader-manifest`, `precompile-state` e `dump-index` passaram. O jogo não rodou no Linux.
- [ ] Sessão curta com o mantenedor: cache novo (pasta antiga renomeada), jogo até o primeiro
      combate. Confira no log que os `.meta` foram gravados e que nada mudou visualmente.

**Pronto quando:** testes passando e sessão sem regressão. Commit.

---

## 9. Fase 2: API de tradução offline e verificador 100%

**Objetivo:** traduzir qualquer permutação gravada sem memória do jogo, e provar que o resultado é
idêntico ao do jogo.

### 9.1 API

Em `shader_recompiler/replay.{h,cpp}` (adicione ao `${RECOMPILER}` no CMake):

```cpp
namespace Shader {
// spartanport: translates a recorded permutation without guest memory (shader precompile).
std::vector<u32> ReplayTranslate(std::span<const u32> code, HwStage hw, SwStage sw, u64 pgm_hash,
                                 const ReplayInputs& in, const StageSpecialization& spec,
                                 const Profile& profile, Pools& pools);
}
```

Dentro dela, como a POC faz, mas sem gancho solto:

1. `Info info(hw, sw, ShaderParams{in.user_data, code, pgm_hash})`.
2. Instale `Info::UdSnapshot` com `user_data`, `flat_ud` e `pgm_base = code.data()`. Remova o
   snapshot no fim (RAII).
3. `FetchShaderSnapshot` com `spec.fetch_shader_data` e `in.fetch_buffers`, dentro de
   `ScopedFetchShaderSnapshot`.
4. Impeça o `walker_func` de rodar: troque o gancho provisório por um mecanismo com escopo (por
   exemplo um `thread_local` privado de `replay.cpp` consultado em `RefreshFlatBuf`, ligado e
   desligado por RAII). Ele nunca pode ficar ligado fora de `ReplayTranslate`.
5. `runtime = spec.runtime_info`, `binding = spec.start`, `TranslateProgram`, `EmitSPIRV`.

A função precisa ser segura em várias threads ao mesmo tempo (os snapshots já são `thread_local`).

### 9.2 Verificador

Transforme a POC em `shader-replay-test` definitivo:

- Entrada: pasta de cache e pasta do jogo (o índice vem da F3; até a F3 existir, aceite o arquivo de
  índice).
- Para cada `.meta`, chama `ReplayTranslate` e compara com o `.spv`.
- Saída: uma linha por estágio (iguais, diferentes, pulados), tempo médio, e a lista dos diferentes.
  Código de saída diferente de zero se houver qualquer diferença.
- Opção para gravar os SPIR-V diferentes numa pasta (como `REPLAY_DUMP` na POC) para comparar com
  `spirv-dis`.
- Pula as permutações com `motion_vectors` no VS (elas embutem endereços da sessão; `LoadShaderMeta`
  já as recusa) e conta separado.

### 9.3 Validação

Resultado em 2026-11-05 no cache antigo (`.meta` versão 7, sem entradas gravadas, sessões anteriores à
F1): 5.049 traduzidos, 0 travamentos, 4.131 idênticos (81,8%), 1,13 ms cada. As 918 diferenças são
todas fragment, o defeito de 3.3. Isso não prova a F1: a prova é a sessão abaixo.

- [ ] Sessão de gravação com o mantenedor num cache novo (pasta antiga renomeada). Pelo menos 30
      minutos, com várias áreas.
- [ ] Verificador nessa pasta: **100% idênticos** em todos os estágios. Qualquer diferença bloqueia
      a fase: ache a entrada que faltou (tabela 3.4), grave na F1 e repita.
- [ ] Rode o verificador no Linux contra uma cópia da mesma pasta (o mesmo SPIR-V deve sair,
      porque o perfil é o mesmo; se a GPU do Linux for outra, grave uma sessão lá).

**Pronto quando:** 100% numa sessão real. Commit.

---

## 10. Fase 3: leitura do código dos shaders nos WADs

**Objetivo:** dado um `pgm_hash`, devolver o código exato do dump do usuário.

### 10.1 Regras de busca (3.1)

- Procure `OrbShdr` em qualquer byte: nos WADs reais os shaders ficam em posições ímpares do arquivo.
  Leia `length` e `shader_hash`.
- Ache o início: procure para trás (no máximo 64 KB) o token `0xBEEB03FF` tal que
  `início + (imm + 1) * 8 == posição do rodapé`. Sem token, descarte (não chute).
- Código = `length` bytes a partir do início.

### 10.2 Dois modos

1. **Por dica (rápido):** o manifesto traz, para cada `pgm_hash`, caminho relativo + deslocamento
   no dump de quem gravou. Leia só aqueles bytes, valide o rodapé e o `code_xxh3`. Se bater, pronto.
2. **Varredura (fallback):** para os que não bateram (dump com outra versão ou outra organização),
   varra os WADs e o `eboot.bin` uma vez, montando o índice `pgm_hash -> (arquivo, início, tamanho)`.
   Use leitura sequencial em blocos grandes. A varredura completa leva cerca de 90 s com Python num
   SSD; em C++ deve ser bem menos, mas num HD pode passar de minutos. Por isso ela só roda para o
   que faltou e nunca na thread do jogo.

### 10.3 De onde vem a pasta do jogo

- Acrescente `const char *app0;` ao fim de `SpartanGpuConfig` (`gpu/spartanportgpu.h`) e preencha
  em `src/probe.c` (a variável `app0` já existe lá). Mantenha a ordem dos campos e confira todos os
  lugares que montam a struct (`rtk proxy grep -rn SpartanGpuConfig`).
- Leia a pasta como foi passada. Ela pode ser uma pasta `mod-game-*` com junctions. Não resolva nem
  mexa nas junctions.
- Considere também os WADs de DLC (`EXEC/DLC/*/WAD/PS4_LE/`) e, se o launcher passar pasta de
  atualização separada, procure nela antes.

### 10.4 Testes (antes do código)

- [x] WAD sintético gerado no teste (bytes inventados, nunca código real do jogo) com 3 shaders:
      token certo, rodapé certo, e um com rodapé sem token. O leitor acha os 2 válidos e ignora o terceiro.
- [x] Dica certa, dica com deslocamento errado (cai na varredura), arquivo da dica inexistente.
      A queda para a varredura fica no materializador (F5); o leitor recusa e devolve nada.
- [x] Hash XXH3 diferente: o shader é recusado.
- [x] Local que sai da pasta do jogo (`..` ou caminho absoluto): recusado.

**Pronto quando:** testes passando; no dump real, o leitor acha os 4.550+ programas do cache do
mantenedor e o índice completo bate com os 97.567. Commit.

Resultado em 2026-11-05: `dump-index-test` achou 97.567 shaders em 81 s, e os 4.734 programas do
cache local estão no índice.

---

## 11. Fase 4: manifesto (formato e ferramenta que o gera)

### 11.1 Formato `shaders/<serial>.spm`

Binário, little-endian, comprimido com miniz. Implementado em `vk_shader_manifest.{h,cpp}`: o cabeçalho
guarda só magic, formato, `manifest_id` e tamanho; serial e versões ficam no conteúdo, protegidos pelo
hash. A prioridade vem do horário dos `.meta`: os dois primeiros minutos da gravação contam como
menus e título, o resto como jogo.


- Cabeçalho: magic `SPSM`, versão do formato, serial (`CUSA01623`), `ShaderMetaVersion`,
  `ShaderBinaryVersion` e `PipelineKeyVersion` com que foi gerado, número de entradas, hash de todo o
  conteúdo (`manifest_id`, XXH3 64 bits).
- Tabela de programas: `pgm_hash`, estágio, dica de local (caminho relativo dentro do dump,
  deslocamento), `code_size`, `code_xxh3`.
- Tabela de permutações: índice do programa, `perm_idx` final (0, 1, 2... em ordem determinística),
  `StageSpecialization` serializada, `ReplayInputs`, prioridade (`u8`: 0 menus e título, 1 usadas em
  jogo, 2 o resto). O preparo processa por prioridade, para que pular no meio já deixe pronto o mais útil.
- Tabela de pipelines: chaves gráficas e de compute (`GraphicsPipelineKey` + `SerializationSupport`,
  como em `RegisterPipelineData`) com os `stage_hashes` **remapeados**: em vez de `perm_hash` da
  sessão de quem gravou, guarde (índice do programa, índice da permutação). O materializador
  recalcula `perm_hash` com o `perm_idx` final.

### 11.2 Ferramenta `shader-manifest`

Alvo novo no CMake (executável de linha de comando, roda em Windows e Linux):

```
shader-manifest build --out shaders/CUSA01623.spm --game <app0> <cache1> [<cache2> ...]
shader-manifest info shaders/CUSA01623.spm
```

- Lê vários caches gravados (de pessoas diferentes), junta e tira repetidos: duas permutações do
  mesmo programa são a mesma se a `StageSpecialization` for igual (`operator==`). Se forem iguais na
  especialização mas o SPIR-V traduzido sair diferente, **é erro**: pare e reporte (sinal de entrada
  faltando na F1).
- Ordem determinística: programas por `pgm_hash`; permutações por um hash estável da especialização
  serializada. Rodar duas vezes com as mesmas entradas gera o mesmo arquivo, byte a byte.
- Gera as dicas de local a partir do `--game` usando o leitor da F3.
- Recusa `.meta` sem `ReplayInputs` (versão antiga).
- `info` mostra contagens por estágio, tamanho e `manifest_id`.

### 11.3 Testes (antes do código)

- [x] Ida e volta do formato.
- [x] Junção de dois caches com sobreposição: repetidos somem, a contagem fecha.
- [x] Remapeamento: uma chave de pipeline que apontava para `perm_hash` de sessão aponta para a
      permutação certa no manifesto.
- [x] Determinismo: duas execuções, mesmo arquivo.
- [x] Nenhum byte de código no arquivo: o teste usa um WAD sintético com um padrão de bytes
      marcador no código e confere que o padrão não aparece no `.spm`.

**Pronto quando:** testes passando; manifesto gerado do cache da F2 com tamanho anotado (meta: menos
de 20 MB comprimido; se passar muito disso, discuta na spec). Commit.

---

## 12. Fase 5: materialização na primeira abertura (shaders)

**Objetivo:** com o manifesto, gerar no `user/cache/<serial>/` os `.meta`, `.spv` e `.key` no formato
atual, como se o usuário já tivesse jogado tudo.

### 12.1 Onde entra

Em `vk_presenter.cpp`, antes da decisão `UseOnDemandPipelines` (perto da linha 325):

1. `CheckCacheProfile()` (já recomeça o cache se a GPU mudou).
2. Se o preparo está ligado, existe `shaders/<serial>.spm` e o marcador (F7) diz que falta, rode
   o materializador com a tela de carregamento (`SpartanOverlay::SetLoading` +
   `PresentLoadingFrame`, como o `WarmUp` já faz).
3. Depois siga o caminho de hoje (`StartOnDemand` ou warm-up). Ele encontra o cache cheio.

### 12.2 O que o materializador faz

- Lê o manifesto e confere versões (se o manifesto foi gerado com outra `ShaderMetaVersion` ou
  `ShaderBinaryVersion`, ainda serve: as entradas são as mesmas; só o SPIR-V é refeito. Se o formato
  das próprias entradas mudou, o manifesto é recusado e o jogo segue sob demanda, com aviso no log).
- Lê o código de cada programa (F3), em paralelo, com `WarmupPool`/`WarmupThreadCount`.
- Para cada permutação: `ReplayTranslate`, grava `0x<pgm>_<perm>.spv` (`RegisterShaderBinary`) e o
  `.meta` (`RegisterShaderMeta` com o `Info` resultante da tradução e as `ReplayInputs`).
  A permutação `perm_idx` vem do manifesto, então `.meta`, `.spv` e `.key` sempre combinam.
- Grava as chaves de pipeline com os `perm_hash` recalculados (`RegisterPipelineData`).
- Grava o marcador `disk_shaders` = 1 e o `profile.bin`, como `StartOnDemand` espera.
- Programa cujo código não foi achado no dump: pula, conta e registra no log (nunca trava).
- Progresso: `done/total` para a tela, atualizado no máximo a cada 250 ms.
- Pular (Esc/B): para as threads, deixa o que já foi gravado (é cache válido) e não grava o marcador
  de pronto. Na próxima abertura, continua de onde parou (pule permutações cujo `.spv` e `.meta` já
  existem e conferem).

### 12.3 Convivência com o cache que o usuário já tem

- Se o usuário já tem cache de sessões anteriores, os números de permutação dele podem não bater com
  os do manifesto. Regra simples: se o marcador do preparo não existe, **recomece o cache** (como
  `StartOnDemand` já faz com `db.Clear()` quando o marcador `disk_shaders` muda) e materialize.
  O manifesto cobre mais do que o cache pessoal, e permutações novas que o usuário encontrar depois
  continuam sendo criadas sob demanda com números a partir do último do manifesto.
- Confira que `NewPermutationIndex` numera depois das permutações já carregadas, para não
  sobrescrever arquivos do manifesto.

### 12.4 Testes

- [ ] Materializar de um manifesto pequeno (gerado no teste a partir de shaders sintéticos traduzíveis,
      ou do fixture do verificador) numa pasta vazia gera os arquivos esperados com nomes e
      `perm_hash` corretos.
- [ ] Retomada: interromper no meio e rodar de novo termina sem refazer o que já estava pronto.
- [ ] Código ausente: contado e pulado.

### 12.5 Validação

- [ ] Pasta de cache nova (a antiga renomeada), manifesto da F4, jogo aberto pelo launcher. A tela de
      preparo aparece, o progresso anda, o jogo começa.
- [ ] Verificador da F2 rodado na pasta materializada contra a pasta gravada original: os `.spv` de
      cada permutação são idênticos (mesma GPU).
- [ ] Tempo do preparo anotado.

**Pronto quando:** tudo acima. Commit.

---

## 13. Fase 6: pré-compilação dos pipelines no driver

**Objetivo:** o engasgo maior vem da criação de pipelines no driver. Depois da F5, criar todos os
pipelines das chaves uma vez, para que o driver guarde os binários (`pipeline-binaries/`).

### 13.1 Investigação (antes da spec)

- Leia `StartOnDemand`, `StartBinaryConversion`, `LoadGraphicsPipeline`, `LoadComputePipeline` e o
  caminho que grava `pipeline-binaries/` (`PipelineBinaryCache::Store`/`StoreShared`).
- Responda na spec: no modo sob demanda, criar um pipeline a partir de uma chave do disco grava o
  binário do driver? Se sim, a F6 é "percorrer todas as chaves criando os pipelines com a tela de
  progresso". Se não, descreva o que falta.
- Confira o que o `WarmUp(progress)` em massa já faz (ele cria pipelines das chaves com várias
  threads). O ideal é reusar esse caminho, chamado uma vez depois da F5, e depois seguir sob demanda.

Resultado da leitura em 2026-11-05 (só código, sem jogo):

- **Gráficos: sim.** `LoadGraphicsPipeline` cria `GraphicsPipeline`, que chama
  `CreateCachedGraphicsPipeline`. Sem binário salvo, ele compila e grava em `pipeline-binaries/`.
  O caminho é o mesmo do sob demanda: `preloading` só reaproveita `sdata`, que veio do disco.
- **`WarmUp(progress)` serve de base:** percorre todas as chaves com `WarmupPool`, informa progresso,
  e um pipeline que falha não para o resto. O diário entra nas tarefas que ele empurra para o pool.
- **Compute não grava binário:** `vk_compute_pipeline.cpp` chama `createComputePipelineUnique` direto,
  só com o `VkPipelineCache`. São poucos shaders compute (6 rodapés no dump), então a F6 pode deixá-los
  no `VkPipelineCache`.
- **Cuidado 1:** `WarmUp` deixa todos os pipelines em memória nos mapas. Para a F6 eles devem ser
  destruídos depois de gravar o binário, ou o preparo guarda tudo na RAM.
- **Cuidado 2:** `StartOnDemand` faz `db.Clear()` quando falta o marcador `disk_shaders`. O preparo
  grava esse marcador (F5) antes de chamar `StartOnDemand`, ou apaga o que acabou de gerar.

### 13.2 Requisitos

- Usa as mesmas threads e a mesma tela da F5 (uma barra só, em duas etapas: "shaders" e "pipelines").
- Pode pular, como a F5.
- Pipeline que falha ao criar: registra e segue (o jogo cria sob demanda depois).
- **Diário de crash do driver:** erro retornado não cobre o caso em que o compilador do driver derruba
  o processo (visto no NVIDIA 615.78.08 em outro projeto, sempre na mesma variante). Sem isso, a pessoa
  trava a cada abertura. Antes de entregar cada pipeline ao driver, a thread grava a chave num slot
  próprio de um arquivo de diário, direto no sistema operacional, e zera o slot ao voltar. Terminar
  bem apaga o diário. Na abertura seguinte, um diário que sobrou move essas chaves para uma lista de
  pulo marcada com GPU e versão do driver, e o preparo não as compila. Elas ficam para o primeiro uso.
  Driver novo tenta de novo. Ideia vinda de `valentderah/totk-pc-recomp` (`runtime/vk_pipeline.c`,
  `skip_init`), projeto MIT; o código é escrito do zero aqui.
- Ao terminar, grava no marcador a chave global do driver usada
  (`instance.GetPipelineBinaryGlobalKey()`).

### 13.3 Validação

- [ ] Cache limpo, manifesto, preparo completo. Depois o mantenedor joga pelas áreas gravadas.
      No log: `Disk shaders: ... 0 compiled; pipeline keys 0 written` (ou equivalente para pipelines)
      e nenhuma nota de compilação na tela (o mantenedor liga "Show shader notes in the corner" só
      para esse teste e desliga depois).
- [ ] Tempo total do preparo anotado (shaders + pipelines) na GPU do mantenedor.

**Pronto quando:** zero compilações nas áreas gravadas. Commit.

---

## 14. Fase 7: invalidação (GPU, driver e versão)

### 14.1 Marcador

Um blob novo (por exemplo `BlobType::ShaderProfile`, nome `precompile`) com:

- `manifest_id` do manifesto usado.
- `ShaderMetaVersion`, `ShaderBinaryVersion`, `PipelineKeyVersion` do build.
- Hash do `Profile` (o mesmo que `profile.bin`).
- Hash da chave global do driver (`GetPipelineBinaryGlobalKey`).
- Estado: `shaders_done`, `pipelines_done`.

### 14.2 Regras

| Mudou | Refaz |
|:--|:--|
| GPU (perfil) | tudo (o `CheckCacheProfile` já recomeça o cache) |
| Driver (chave global) | só pipelines (F6). Os `.spv` continuam valendo |
| `ShaderBinaryVersion` ou `ShaderMetaVersion` (port novo) | shaders e pipelines |
| `PipelineKeyVersion` | pipelines e chaves |
| `manifest_id` (manifesto novo no release) | só o que for novo: materialize as entradas que faltam; refaça pipelines das chaves novas |
| Lista de pulo de outro driver | ignora a lista antiga; tenta aquelas chaves de novo |
| Nada | nada; o jogo abre direto |

### 14.3 Testes (antes do código)

- [x] Teste de tabela para cada linha acima, com marcador falso em memória (sem GPU).
      Implementado em `vk_precompile_state.{h,cpp}`. Mudança de versão de `.meta` ou `.spv` recomeça o
      cache, que é o caminho mais simples para "shaders e pipelines".
- [x] Marcador corrompido ou ausente: tratado como "falta tudo", sem travar.

### 14.4 Validação no jogo

- [ ] Abrir duas vezes seguidas: na segunda, nenhum preparo (log confirma).
- [ ] Simular troca de driver: edite só o hash do driver no marcador (numa cópia do cache) e abra:
      só a etapa de pipelines roda.
- [ ] Simular troca de GPU: edite um campo de `profile.bin` numa cópia: tudo refaz.
- [ ] Build com `ShaderBinaryVersion` + 1 (só local, não commitar): shaders refazem.

**Pronto quando:** matriz inteira passou. Commit.

---

## 15. Fase 8: interface (tela de preparo, launcher e textos)

### 15.1 Tela de preparo (overlay)

- Reuse a tela de carregamento (`SetLoading`) com texto novo:
  - "Preparing shaders %u%%" na etapa 1 e "Preparing pipelines %u%%" na etapa 2.
  - Uma linha menor: "This happens once. Press Esc or B to skip." (texto final na spec).
- Traduza em todas as línguas de `gpu/shim/ui_language/*.h` e `spartanport_ui_language.h`.
  Rode `ui-language-test` depois.
- Esc ou B pula (F5 e F6).

### 15.2 Launcher Rust

- Desempenho: checkbox "Prepare all shaders on first start" (padrão ligado), com dica curta em
  palavras simples. Variável de transporte: `SPARTAN_SHADER_PRECOMPILE`.
- Avançado: checkbox "Record shaders for the shared list" (padrão desligado). Variável:
  `SPARTAN_SHADER_RECORD`. Com ela ligada, o jogo grava como hoje (a F1 já grava as entradas sempre);
  a opção serve para avisar no log e para a F9 saber que a sessão é de gravação. Se na spec a gravação
  ficar sempre ligada para todos, esta opção some.
- Siga o padrão dos checkboxes vizinhos (`disk_shaders`, `shader_prefetch`): `settings.rs`
  (padrão e migração), `env.rs` (transporte e ordem das variáveis, ver o teste em `env_contract.rs`),
  `performance.rs`/página Avançado, `assets/i18n.json` (todas as línguas).
- Testes: atualize `env_contract.rs`, `ui_pages.rs` e os snapshots (`ui_snapshots.rs`).

### 15.3 Validação

- [ ] `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings`, `cargo test`.
- [ ] Abra o launcher, abra as páginas, confira em capturas de tela **em português e em inglês** que
      os checkboxes e textos aparecem certos e cabem. Só então reporte.
- [ ] Captura da tela de preparo no jogo (1440p).

**Pronto quando:** tudo acima. Commit.

---

## 16. Fase 9: campanha de gravação e empacotamento

### 16.1 Gravação (com o mantenedor e a equipe)

Peça ao mantenedor que organize, e acompanhe pelos logs:

- [ ] Uma passada completa da campanha, do início ao fim, com "Record shaders" ligado.
- [ ] Os modos extras e a DLC (Desafio do Olimpo, Arena de Combate e o que o dump tiver), figurinos.
- [ ] Menus, cutscenes, morte e recarregar save, tela de mapa e itens.
- [ ] Investigue antes quais opções do port mudam `RuntimeInfo` ou `GraphicsPipelineKey` (por
      exemplo HDR, formato de saída, patches de resolução, MSAA se existir). Liste na spec e grave ao
      menos uma sessão com cada combinação que muda chave.
- [ ] Juntar os caches com `shader-manifest build`. Anotar contagens e tamanho.
- [ ] Verificador da F2 em cada cache gravado: 100%.

Permutações que ninguém gravou continuam funcionando sob demanda, como hoje. O manifesto pode ser
melhorado em releases futuros.

### 16.2 Empacotamento

- [ ] `packaging/windows/package.sh`: copiar `shaders/<serial>.spm` para o zip, ao lado do exe.
- [ ] `packaging/linux/build.sh` e `AppRun`: incluir no AppImage e achar o caminho em tempo de
      execução (pasta do executável).
- [ ] Onde o jogo procura o manifesto: pasta do executável primeiro, depois `user/`. Documente.
- [ ] O manifesto entra no repositório? Decida na spec (tamanho, frequência de mudança). Se não
      entrar, defina onde ele é guardado e como o CI o pega.
- [ ] Smoke tests de pacote (`packaging/windows/smoke-test.ps1`, `packaging/linux/smoke-test.sh`)
      conferem que o arquivo está no pacote.

### 16.3 Documentação

- [ ] README em inglês, português e espanhol: o que é a tela de preparo, que ela roda uma vez, quando
      roda de novo e como pular. Palavras simples.

**Pronto quando:** manifesto gerado da campanha completa e presente nos dois pacotes. Commit.

---

## 17. Fase 10: validação final e release

### 17.1 Matriz

| Teste | Plataforma | Esperado |
|:--|:--|:--|
| Verificador em todos os caches gravados | Windows | 100% idênticos |
| Verificador numa sessão gravada no Linux | Linux | 100% idênticos |
| Instalação limpa, primeira abertura | Windows (AMD RX 6700 XT do mantenedor) | tela de preparo, tempo anotado, jogo abre |
| Passada pelas áreas gravadas | Windows | 0 shaders e 0 pipelines compilados no jogo |
| Instalação limpa, primeira abertura | Linux | idem |
| GPU NVIDIA e Intel (testadores) | Windows ou Linux | idem; perfil diferente refaz tudo |
| Segunda abertura | ambas | sem preparo |
| Troca de driver (simulada) | Windows | só pipelines |
| Port com versão nova (build local) | Windows | shaders e pipelines |
| Sem manifesto | ambas | igual ao de hoje |
| Opção desligada no launcher | ambas | igual ao de hoje |
| Pular no meio e reabrir | Windows | retoma de onde parou |
| Dump numa pasta `mod-game-*` | Windows | acha o código pelas junctions, sem mexer nelas |
| HD lento (se houver) | Windows | preparo termina; varredura só para faltantes |

### 17.2 Antes de pedir o merge

- [ ] Todos os testes: C++ (Windows e Linux), `cargo test`, smoke de pacote.
- [ ] `git grep` confirma: nenhum nome do jogo em código ou log novo; nenhum código de shader do jogo
      versionado.
- [ ] Tabela de estado e checkboxes deste documento atualizados.
- [ ] Rascunho do PR (formato da skill de mensagens do projeto) mostrado ao mantenedor. **PARE.** Push, PR e
      merge só com autorização explícita. O merge é feito pelo mantenedor, nunca em `main` direto.
- [ ] Na entrega, liste a ação operacional depois do deploy (por exemplo: "o cache de shaders de
      todos é refeito uma vez na primeira abertura, por causa da `ShaderMetaVersion` nova").

---

## 18. Requisitos (EARS), para conferir na revisão final

1. QUANDO o jogo abre com o preparo ligado, um manifesto válido e o marcador incompleto, O SISTEMA
   DEVE preparar shaders e pipelines com uma tela de progresso antes do primeiro quadro do jogo.
2. QUANDO o marcador confere com o build, a GPU, o driver e o manifesto, O SISTEMA DEVE abrir o jogo
   sem preparo.
3. QUANDO só a chave global do driver mudou, O SISTEMA DEVE refazer só os pipelines.
4. QUANDO o perfil da GPU mudou, O SISTEMA DEVE recomeçar o cache e refazer tudo.
5. QUANDO o usuário pula o preparo, O SISTEMA DEVE seguir sob demanda e retomar o preparo na próxima
   abertura sem refazer o que já ficou pronto.
6. SE o código de um shader do manifesto não for achado ou não conferir no dump, O SISTEMA DEVE pular
   esse shader, registrar no log e continuar.
7. SE o manifesto não existir, estiver corrompido ou for de formato incompatível, O SISTEMA DEVE se
   comportar exatamente como sem a feature.
8. O SISTEMA DEVE sempre gerar, para cada permutação do manifesto, o mesmo SPIR-V que o jogo geraria
   ao vivo na mesma GPU.
9. O SISTEMA DEVE sempre manter `.meta`, `.spv` e `.key` de uma permutação consistentes entre si.
10. O SISTEMA NUNCA DEVE incluir código de shader do jogo no repositório ou nos pacotes.
11. O SISTEMA NUNCA DEVE escrever na pasta do jogo.
12. SE o driver derrubar o processo durante o preparo, O SISTEMA DEVE pular as chaves do diário na
    próxima execução, só para aquela GPU e versão do driver.

---

## 19. Problemas prováveis e o que fazer

| Sintoma | Causa provável | O que fazer |
|:--|:--|:--|
| Verificador abaixo de 100% em fragment com MSAA | dados de usuário de outro draw no `.meta` | conferir a F1 nos dois caminhos (síncrono e assíncrono) |
| Diferença só em VS com fetch | V# não gravados | conferir `fetch_buffers` na F1 |
| Assert em `MarkReadConstBufferSharpSources` | código lido do lugar errado | conferir a regra do token (3.1); na POC isso aconteceu com `rodapé - length` |
| `First instruction is not s_mov_b32 vcc_hi` no log | idem | idem |
| Travamento ao traduzir offline | `walker_func` rodou ou leitura de memória não coberta | revisar a tabela 3.4; rodar com `ReplayTranslate` sob depurador |
| Pipelines compilam no jogo mesmo após o preparo | chaves não remapeadas ou binários do driver não gravados | revisar o remapeamento (F4) e a investigação da F6 |
| Arquivos do manifesto sobrescritos depois | `NewPermutationIndex` reusando números | numerar a partir do último do manifesto (F5) |
| Primeira abertura muito lenta num HD | varredura completa dos WADs | conferir se as dicas de local estão batendo; a varredura deve ser exceção |
| Manifesto grande demais | `flat_ud` ou `RuntimeInfo` repetidos | deduplicar blocos iguais no formato (F4) |

---

## 20. Checklist final (marque tudo antes de declarar pronto)

- [ ] F0 a F10 concluídas, tabela de estado atualizada.
- [ ] Verificador 100% em todas as gravações (Windows e Linux).
- [ ] Zero compilações durante o jogo nas áreas gravadas, depois do preparo.
- [ ] Matriz de invalidação e matriz final completas.
- [ ] Sem manifesto ou com a opção desligada: idêntico ao comportamento anterior.
- [ ] Launcher com os controles novos, testes e capturas em PT e EN.
- [ ] Textos traduzidos em todas as línguas (launcher e overlay).
- [ ] README nas três línguas.
- [ ] Nenhum código do jogo versionado ou empacotado; nenhum nome do jogo em código novo.
- [ ] Mensagens de commit e PR aprovadas pelo mantenedor; nenhuma menção a IA.
