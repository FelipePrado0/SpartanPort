# Intel Windows: vertex input dinâmico desligado, port para a main 2.8.0

Data: 2026-10-25. Estado: design aprovado em conversa (seções 1, 2 e 3). Texto aguardando revisão.

## 1. Objetivo

Levar para a `main` atual (2.8.0, `84f2d39`) o fix do 3D preto/azul nas GPUs Intel Windows que o
branch `worktree-intel-hd630` (PR #214) corrigiu em 2026-10-15 e nunca chegou a uma release.
A worktree antiga é apagada e o trabalho recomeça do zero em um branch novo a partir da `main`,
trazendo só o que os testers confirmaram.

Resultado esperado: um test build `windows-v2.8.0-intel-test1` pela CI, anunciado nos issues
abertos, e um PR novo para a `main` que só merge depois de testers Intel aprovarem.

## 2. Contexto e evidência

### 2.1 O bug

No driver Intel proprietário Windows (`VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS`), com
`VK_EXT_vertex_input_dynamic_state` ligado, todo objeto 3D sai preto ou azul; menus, HUD e vídeos
ficam corretos. Sem a extensão (caminho estático de vertex input) o render fica certo. Confirmado
pelos testers em HD 630, UHD 620 e UHD 630 com o pre-release `windows-v2.3.0-intel-hd630-test2`
(commit `c0a8684`).

### 2.2 Triage de 2026-10-25

Issues abertos com o mesmo sintoma, todos na 2.7.0:

| Issue | GPU | Log anexado | Observação |
| --- | --- | --- | --- |
| #861 | UHD (i3-1215U) | inline | canônico; #928 e #948 fechados como duplicata dele |
| #1055 | UHD 630, driver 101.2145 | sim | `Enabling extension: VK_EXT_vertex_input_dynamic_state`; test2 funciona; o tester tentou `GOW3_VK_DISABLE_EXT` (nome da época) e a 2.7.0 não tem o knob com nenhum nome |
| #1029 | Iris Plus (Ice Lake) | sim | blocos azuis |
| #1041 | UHD 620 | não | |
| #1035 | Iris Plus | não | |
| #1043 | Iris Xe | não | |
| #1018 | Iris Xe | não | também instabilidade e `Device lost` |
| #1039 | — | — | pedido explícito de um test build Intel sobre a 2.7.0 |

O log do #928 com o test2 mostra `GPU: vertex input dynamic state off (Intel Windows driver;
GOW3_VERTEX_INPUT_DYNAMIC=1 turns it on)` (nome da época) e render correto, seguido de
`Device lost during submit` em outro momento: esse crash é o #334 e fica fora desta entrega.

### 2.3 Estado do branch antigo

`worktree-intel-hd630` tem 26 commits sobre `1594332`; a `main` está 450 commits à frente.
`git cherry main worktree-intel-hd630` não encontra nenhum deles na `main`. Dos 8 commits do fix,
7 não aplicam com `git apply --check` (offsets em `vk_instance.cpp`, `gpu/CMakeLists.txt`,
`profile.h`, `vk_pipeline_serialization.cpp`). O launcher Python que o branch alterava foi
substituído por `launcher-rust/`. PR #214 está `CONFLICTING`.

## 3. Decisões

- **Branch e PR**: branch novo `intel-vertex-input` em worktree nova; PR novo; #214 é fechado com
  comentário apontando o PR novo. O branch `worktree-intel-hd630` fica (local e remoto) como
  referência até o merge, depois é apagado.
- **Escopo**: só o fix e os knobs de ambiente. Sem UI no launcher Rust, sem coluna no
  `spartanport-gpu-capabilities`, sem os commits de diagnóstico de log.
- **GitHub**: nenhum comentário de dedupe agora. Só o anúncio do test build, depois que a CI
  publicar.
- **Abordagem**: port manual com TDD por unidade. Headers novos entram verbatim do branch antigo;
  o código que toca arquivos existentes é reescrito contra a `main` com o diff antigo como
  referência.
- **Nomes**: a `main` renomeou o prefixo depois do branch antigo. Variáveis de ambiente são
  `SPARTAN_*` (`SPARTAN_NO_MSAA`, `SPARTAN_PIPELINE_BINARIES`), arquivos e funções
  `spartanport_*` (`spartanport_msaa_default.h`, `spartanport_no_msaa_default`), comentários
  `// spartanport:`. Os knobs novos seguem isso: `SPARTAN_VK_DISABLE_EXT` e
  `SPARTAN_VERTEX_INPUT_DYNAMIC`. O launcher Rust encaminha `GOW3_*` definidos à mão para
  `SPARTAN_*` (`launcher-rust/src/core/legacy.rs`); o exe rodado direto só lê `SPARTAN_*`.

## 4. Unidades

Cinco unidades, nesta ordem. Cada uma compila e passa sozinha antes da próxima começar.

### 4.1 `SPARTAN_VK_DISABLE_EXT`: política de extensões

- Arquivo novo `gpu/shadps4/video_core/renderer_vulkan/vk_extension_policy.h`, verbatim de
  `worktree-intel-hd630`.
- `ParseExtensionPolicy(const char*)` lê a variável: lista separada por vírgula, espaços e tabs
  em volta de cada nome ignorados, nomes sensíveis a maiúsculas, sem duplicatas, na ordem de
  entrada; a palavra `all` desliga toda extensão opcional. Unset, vazio ou só itens em branco
  significa "nada desligado" (`Empty()`).
- `IsDisabled(name)`, `IgnoredNames(policy, disabled)` (nomes listados que não casaram com
  nenhum pedido; vazio com `all`), `JoinNames(vector)` (`none` para vazio, `a, b` senão),
  `OffNamesForLog(disabled, extra_name, extra_reason)` (lista para a linha de resumo, com o
  motivo do default do vertex input quando houver).
- Teste: `tests/test_extension_policy.cpp` (verbatim), alvo Ninja `extension-policy-test`.

### 4.2 Default do vertex input por driver

- Arquivo novo `gpu/shadps4/video_core/renderer_vulkan/spartanport_vertex_input_default.h`, verbatim.
  C puro, incluível por `vk_instance.cpp`.
- `spartanport_vertex_input_dynamic_default(VkDriverId)`: 0 só para
  `VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS`, 1 para todos os outros (NVIDIA, AMD, RADV, ANV,
  MoltenVK, Arc incluído enquanto for o mesmo driver id).
- `spartanport_vertex_input_dynamic(env, driver)`: `SPARTAN_VERTEX_INPUT_DYNAMIC` unset ou vazio mantém o
  default; `1` liga; qualquer outro valor desliga.
- Teste: `tests/test_vertex_input_default.cpp` (verbatim), alvo `vertex-input-default-test`.

### 4.3 Formato de vértice tolerante no caminho estático

- Arquivo novo `gpu/shadps4/video_core/renderer_vulkan/vk_vertex_format.h`, do branch antigo
  sem o `struct VertexFormatSeen` (ele servia ao aviso único por formato, fora do escopo).
- `LiverpoolToVK::VertexAttributeFormat(data_format, num_format)` devolve `eUndefined` para
  `FormatInvalid` (buffer nulo), o formato Vulkan quando a tabela conhece o par, e `std::nullopt`
  quando não conhece, em vez do assert de `SurfaceFormat`. Motivo: os workers de preparação de
  draw leem um V# que o guest ainda pode estar escrevendo (`data_format=15` a 480p no #35).
- Usos, portados do diff de `d9aae15`:
  - `vk_draw_prep.cpp`, `PrepareVertexInputs`: `nullopt` marca `out.valid = false` e devolve.
  - `vk_pipeline_cache.cpp`, `RefreshGraphicsStages`, ramo estático
    (`!instance.IsVertexInputDynamicState()`): `nullopt` em worker marca `sel.worker->failed` e
    devolve `false`; na thread da GPU (V# final) cai no `SurfaceFormat` assertivo, que mantém a
    mensagem para um par que a tabela realmente não tem.
- Teste: `tests/test_vertex_format.cpp` sem os asserts de `VertexFormatSeen`, alvo
  `vertex-format-test` (linka `Vulkan::Headers`, `fmt`, `Boost::headers`, `magic_enum`, como no
  branch antigo).

### 4.4 Identidade do cache de pipelines

- `gpu/shadps4/shader_recompiler/profile.h`: campo `u32 vertex_input{}` em `Shader::Profile`:
  1 com `VK_EXT_vertex_input_dynamic_state`, 2 no caminho estático. Chaves e pipelines de um modo
  são inválidos no outro, então o cache de disco reconstrói quando o modo muda. `operator==`
  default já cobre o campo.
- `vk_pipeline_cache.cpp`, construtor: `.vertex_input = instance.IsVertexInputDynamicState() ? 1u : 2u`.
- `vk_pipeline_serialization.cpp`, `CheckCacheProfile`: devolve `false` (cache fechado na
  sessão, nada é escrito) quando `instance.GetDisabledExtensions()` não está vazio, com
  `LOG_WARNING(Render, "Pipeline cache skipped: SPARTAN_VK_DISABLE_EXT is active")`. Esse
  trecho usa o acessor de 4.5, então no plano ele entra depois de 4.5.
- Teste: `tests/test_shader_profile.cpp` (verbatim): dois perfis iguais exceto `vertex_input`
  são diferentes; o default é 0 e difere de ambos. Alvo `shader-profile-test`.

### 4.5 Fiação em `vk_instance.cpp` e `vk_instance.h`

Sem teste unitário (precisa de device Vulkan); verificado pelas linhas de log na seção 9.

- `vk_instance.h`: `std::vector<std::string> disabled_extensions`, `std::string disable_ext_env`,
  `GetDisabledExtensions()`.
- `CreateDevice`, no começo: lê `SPARTAN_VK_DISABLE_EXT`, guarda `ParseExtensionPolicy`, lista
  `optional_queried`.
- `add_extension(extension, required = false)`: opcional entra em `optional_queried`;
  indisponível devolve `false` como hoje; opcional disponível mas desligada pela política loga
  `Extension {} disabled by SPARTAN_VK_DISABLE_EXT`, entra em `disabled_extensions` e devolve
  `false` (mesmo fallback de indisponível); senão habilita como hoje. Os quatro `ASSERT_MSG` de
  extensões obrigatórias passam `required = true`.
- Vertex input: `vertex_input_wanted = spartanport_vertex_input_dynamic(getenv("SPARTAN_VERTEX_INPUT_DYNAMIC"), driver_id)`.
  `vertex_input_dynamic_state = (wanted || policy.IsDisabled(ext)) && add_extension(ext)`:
  a política ainda precisa ver o pedido para registrar em `disabled_extensions`.
- Linha `GPU: vertex input dynamic state %s`, por `std::printf` em qualquer nível de log, com um
  destes textos: `on`, `on (SPARTAN_VERTEX_INPUT_DYNAMIC=1)`, `off (SPARTAN_VK_DISABLE_EXT)`,
  `off (SPARTAN_VERTEX_INPUT_DYNAMIC=0)`,
  `off (Intel Windows driver; SPARTAN_VERTEX_INPUT_DYNAMIC=1 turns it on)`,
  `unavailable (VK_EXT_vertex_input_dynamic_state missing)`.
- Lista de extensões do DLSS passa pelo mesmo filtro da política (desligada: entra em
  `disabled_extensions` uma vez e não é habilitada).
- `pipeline_binaries = pipeline_binaries && disabled_extensions.empty()`, avaliado depois do
  último pedido opcional, DLSS incluído.
- Duas linhas de resumo por `std::printf` no fim de `CreateDevice`:
  `GPU: device <nome>, driver <id> <versão>, Vulkan <api>; extensions off: <lista> (SPARTAN_VK_DISABLE_EXT=<valor|unset>)`
  e `GPU: optional extensions unavailable: <lista>; ignored names: <lista>`. A lista de
  `extensions off` inclui `VK_EXT_vertex_input_dynamic_state (Intel Windows driver)` ou
  `(SPARTAN_VERTEX_INPUT_DYNAMIC=0)` quando o default a manteve fora e a extensão existia;
  `disabled_extensions` em si fica só com o que `SPARTAN_VK_DISABLE_EXT` desligou, porque é ela que
  fecha os caches.
- Referência: `git diff 1594332 worktree-intel-hd630 -- gpu/shadps4/video_core/renderer_vulkan/vk_instance.cpp gpu/shadps4/video_core/renderer_vulkan/vk_instance.h`.

## 5. Fluxo e precedência

```
SPARTAN_VK_DISABLE_EXT ──► 4.1 política ─┐
SPARTAN_VERTEX_INPUT_DYNAMIC ──► 4.2 ────┼─► 4.5 CreateDevice ─► IsVertexInputDynamicState()
driver id ─────────────────────────────┘             │                    │
                                                     │          ┌─────────┴──────────┐
                                                     │     dinâmico (hoje)     estático (hoje) + 4.3
                                                     │                    │
                                                     └─► 4.4 Profile::vertex_input ─► chave e cache de disco
```

Precedência: `SPARTAN_VK_DISABLE_EXT` > `SPARTAN_VERTEX_INPUT_DYNAMIC` > default por driver.
Quem não define nada e não usa Intel Windows não muda de comportamento.

## 6. Erros

- Nome desconhecido em `SPARTAN_VK_DISABLE_EXT`: ignorado, aparece em `ignored names`.
- Extensão obrigatória listada: ignorada; `required = true` nunca passa pela política.
- `VK_EXT_vertex_input_dynamic_state` indisponível no device: linha `unavailable`, caminho
  estático como hoje.
- Formato de vértice desconhecido lido por worker no caminho estático: o worker falha a chave e a
  thread da GPU refaz; o processo não para.
- `SPARTAN_VK_DISABLE_EXT` ativo: caches de pipeline (disco e binários) ficam fechados na sessão.

## 7. Fora do escopo

- Toggle "entrada de vértices dinâmica" no launcher Rust e em `spartanport.ini`.
- Coluna de vertex input no `spartanport-gpu-capabilities --list` e `tests/test_probe.py`.
- Commits de diagnóstico do branch antigo: nomes de thread nas linhas de fault (a `main` já tem
  o seu em `src/runtime_thread.c`), log sem interleave, hashes de compile assíncrono, estado de
  pipeline sem fragment shader, aviso único por formato sem `VertexBuffer` (`8227287`).
- README (PT-BR e EN): as linhas antigas documentavam a opção do launcher Python.
- #334 (`Device lost during submit` em Gen12) e #974 (viewport em Iris Plus).
- ADR: `docs/adr/` não existe no repositório.

## 8. Entrega

1. `git worktree remove .claude/worktrees/intel-hd630`.
   `git worktree add .claude/worktrees/intel-vertex-input -b intel-vertex-input main`.
2. Plano em `docs/plans/2026-10-25-intel-vertex-input.md`; execução por
   `superpowers:subagent-driven-development`. Um commit por unidade (teste e código juntos),
   mensagens via `caveman-commit`.
3. Push do branch e
   `gh workflow run windows-zip.yml -R FelipePrado0/SpartanPort --ref intel-vertex-input -f tag=windows-v2.8.0-intel-test1`.
   Base 2.8.0 (`launcher-rust/Cargo.toml`); a release 2.8.0 ainda é Draft. Se a CI deixar o
   pre-release como Draft, o usuário publica.
4. Anúncio, só com o build publicado: comentário bilíngue (`**EN:**`, `---`, `**PT-BR:**`) em
   #861, #1055, #1029, #1041, #1035, #1043, #1018 e #1039: link do pre-release; a linha
   esperada em `user/last_run.log` (`GPU: vertex input dynamic state off (Intel Windows driver; SPARTAN_VERTEX_INPUT_DYNAMIC=1 turns it on)`);
   o cache de pipelines reconstrói sozinho na primeira execução; pedido de `user/last_run.log`
   e de "funcionou?". Label `awaiting-test` nos oito.
5. PR `intel-vertex-input` → `main`, corpo bilíngue, `Closes #861`, refs #1055, #1029, #1041,
   #1035, #1043, #1018, #1039. Comentário bilíngue no #214 apontando o PR novo e fechando-o.
6. Merge só depois dos testers aprovarem; `superpowers:finishing-a-development-branch` apresenta
   as opções e o usuário decide. Depois do merge, apagar `worktree-intel-hd630` local e remoto.

## 9. Verificação

- **Unitários**, na worktree nova, MSYS2 CLANG64:
  `source ./msys2-env.sh`, `cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSPARTAN_PGO=off -DSPARTAN_LTO=OFF -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`
  (a linha do `build.sh`), depois
  `ninja -C out/gpu extension-policy-test vertex-input-default-test vertex-format-test shader-profile-test`
  e os quatro `.exe`. Só alvos de teste; a lib GPU inteira não é compilada localmente.
  TDD: alvo registrado e falhando (não compila ou assert) antes do código.
- **Compilação completa e smoke headless**: CI `windows-zip.yml` (~12 min de build, ~1 min de
  `packaging/windows/smoke-test.ps1`). CI vermelha devolve o trabalho para a worktree.
- **Smoke local na GTX 1650**, com o zip da CI, uma execução por vez, perguntando antes de abrir
  janelas (~2 min cada), `GOW3_GPU_LOG=info`:
  1. default até o menu: `GPU: vertex input dynamic state on`; `extensions off: none`.
  2. `SPARTAN_VERTEX_INPUT_DYNAMIC=0` até o menu: `off (SPARTAN_VERTEX_INPUT_DYNAMIC=0)`, cache de
     pipelines reconstruindo na primeira vez, Kratos visível no menu, nenhum
     `SurfaceFormat: Assertion Failed`.
  3. `SPARTAN_VK_DISABLE_EXT=VK_EXT_vertex_input_dynamic_state`: `off (SPARTAN_VK_DISABLE_EXT)` e
     `Pipeline cache skipped: SPARTAN_VK_DISABLE_EXT is active`.
- **Intel**: só os testers. Aprovado quando `last_run.log` traz a linha
  `off (Intel Windows driver; ...)` e o relato diz 3D correto em UHD 620/630 e Iris Plus/Xe.
  Freeze ou `Device lost` depois disso é #334 e não bloqueia.
- **Pronto para merge**: unitários verdes, CI verde, smoke local nos três cenários, testers
  aprovam.

## 10. Referências

- Branch antigo `worktree-intel-hd630` (`c0a8684`), PR #214, plano antigo
  `docs/plans/2026-10-13-intel-hd630-vertex-input.md` (só nesse branch).
- Commits do fix no branch antigo: `d3c1030`, `dc43e2d`, `417044f`, `d9aae15`, `c2ef675`,
  `ce8fb29`, `59a17a8`, `4265b09`.
- Pre-releases antigos: `windows-v2.3.0-intel-hd630-test1` (`34fac9c`),
  `windows-v2.3.0-intel-hd630-test2` (`c0a8684`).
- Logs lidos em 2026-10-25: #1055 (`33916012`), #1029 (`33899862`), #928 (`33804422` 2.6.0 e
  `33807341` test2), #974 (`33848027`), #986/#991 (`33861192`, `33868175`), todos em
  `https://github.com/user-attachments/files/<id>/...`.
