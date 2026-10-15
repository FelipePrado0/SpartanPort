# Intel HD 630: vertex input dinâmico desligado na Intel Windows (#35)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** build de teste para a #35 em que `VK_EXT_vertex_input_dynamic_state` fica desligada por padrão em
`VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS`, com o caminho estático endurecido, o modo gravado na identidade do
cache de pipelines, um aviso por formato de vértice sem suporte e a escolha exposta no launcher.

**Architecture:** o default segue o padrão de `gow3_msaa_default.h` (cabeçalho C puro, decisão por `VkDriverId`,
impresso por `gow3-gpu-capabilities --list` e mostrado no launcher). O caminho estático (extensão desligada)
passa a tolerar um V# ainda não escrito do mesmo jeito que `vk_draw_prep.cpp` já tolera no caminho dinâmico
(commit efb428d). `Shader::Profile` ganha o modo de vertex input, então um cache gravado no outro modo é
reconstruído por `CheckCacheProfile`. A variável `GOW3_VK_DISABLE_EXT` (branch `worktree-gpu-m8`, já
integrada nesta branch) continua valendo e ganha de tudo quando nomeia a extensão.

**Tech Stack:** C++20 (shadPS4 video core, Vulkan-Hpp), C (cabeçalhos compartilhados com `tools/`), Python 3
(launcher Tk, `unittest`), CMake/Ninja via MSYS2 CLANG64.

**Spec:** não há spec separada. A investigação está em `docs/plans/2026-10-12-gpu-corruption-intel.md`
(branch `pr/105`, seção 7 item 3) e no histórico da issue #35: run B1 (só a extensão desligada) corrige o menu;
New Game cai em `igc64.dll+0x1b0501e` (1 run); as cinco da "primeira metade" desligadas jogam; 480p com elas
desligadas para em `liverpool_to_vk.cpp:792 SurfaceFormat: Unknown data_format=15 and num_format=5`
(data_format 15 é valor reservado da GCN: V# rasgado). ANGLE (`vk_renderer.cpp`, commit 6607a2b9) e yuzu
(69f9b97e) desligam a extensão em toda Intel Windows. Decisões do mantenedor em 2026-10-13: worktree isolado;
gate = toda Intel Windows.

## Global Constraints

- Branch `worktree-intel-hd630`, worktree `.claude/worktrees/intel-hd630`, alias `X:` (`subst`), build em
  `out/x` via `<scratchpad>/ms.sh /x '<caminho longo>' out/x <alvos>` sob MSYS2 CLANG64 (ver memória
  `worktree-build-msys2`). Testes C rodam com `PATH=/c/msys64/clang64/bin:$PATH out/x/<alvo>.exe`.
- Todo código de produção por TDD: teste falhando, ver falhar, código mínimo, ver passar, commit.
- Mensagens de commit pela skill `caveman-commit` (Conventional Commits em português no estilo do repo:
  `Fix(GPU): ...`, `Feat(Launcher): ...`), sem menção a IA. Nunca `git add` do submódulo `gpu/third_party/fsr-vulkan`
  (sempre sujo): stage por nome de arquivo.
- Variável nova: `GOW3_VERTEX_INPUT_DYNAMIC` (`1` liga, `0` ou outro valor não vazio desliga, vazia/ausente =
  default), mesma semântica de `GOW3_NO_MSAA`. Chave de settings do launcher: `vertex_input`.
- Nada muda em NVIDIA, AMD, RADV, ANV, MoltenVK: `gow3_vertex_input_dynamic_default` só devolve 0 para
  `VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS`.
- Linhas de log novas são `std::printf` em stdout (aparecem em `user\last_run.log` em qualquer nível), exceto o
  aviso de formato de vértice, que é `LOG_WARNING` (nível padrão `warning` já o grava).
- Python: `python -m unittest discover -s tests -p "test_launcher_vertex_input.py"` (sem pytest).

## Review Focus

1. Intel com `GOW3_VK_DISABLE_EXT=VK_EXT_vertex_input_dynamic_state` e o default já desligado: o log tem de
   dizer qual dos dois decidiu e o cache fica fechado pela variável (Task 3, strings fixas).
2. Cache de 2.1.8 gravado em modo dinâmico numa Intel que agora roda estático: `Shader::Profile::vertex_input`
   é `u32` com valores 1/2, nunca 0, para que um blob antigo (0 ou padding) nunca confira (Task 2, teste
   `Profile{}.vertex_input == 0`).
3. V# rasgado lido pelo worker em modo estático: `VertexAttributeFormat` devolve `nullopt` e o worker marca
   `failed` em vez de abortar (Task 1, teste com `DataFormat(15)`).
4. Buffer nulo (`FormatInvalid`) continua válido (`eUndefined`) nos dois caminhos (Task 1).
5. `GOW3_VERTEX_INPUT_DYNAMIC=1` numa GPU sem a extensão: linha `unavailable`, sem crash; `=0` na NVIDIA
   roda o caminho estático (Task 3 strings; verificação local na GTX 1650 na seção Entrega).

---

### Task 1: formato de vértice tolerante e endurecimento do caminho estático

**Files:**
- Create: `gpu/shadps4/video_core/renderer_vulkan/vk_vertex_format.h`
- Create: `tests/test_vertex_format.cpp`
- Modify: `gpu/CMakeLists.txt` (depois de `extension-policy-test`, linhas 332-334)
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_draw_prep.cpp:136-147`
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_cache.cpp:1110-1125`

**Interfaces:**
- Consumes: `Vulkan::LiverpoolToVK::TrySurfaceFormat` (`liverpool_to_vk.h:68`), `AmdGpu::DataFormat::FormatInvalid`
  (`video_core/amdgpu/pixel_format.h`), `PipelineSelection::worker` (`vk_pipeline_cache.h:105`),
  `PrepWorker::failed` (`vk_pipeline_cache.h:120`).
- Produces, em `namespace Vulkan::LiverpoolToVK`:
  - `std::optional<vk::Format> VertexAttributeFormat(AmdGpu::DataFormat data_format, AmdGpu::NumberFormat num_format)`:
    `eUndefined` para `FormatInvalid` (buffer nulo), o formato Vulkan quando a tabela conhece o par, `std::nullopt`
    quando não conhece (o V# pode estar sendo escrito pelo guest).
  - `struct VertexFormatSeen { std::vector<vk::Format> seen; bool Add(vk::Format format); }`: `Add` devolve `true`
    na primeira vez que vê um formato (ignora `eUndefined`, devolve `false` para ele). Usado na Task 4.

- [ ] **Step 1: escrever o teste que falha** em `tests/test_vertex_format.cpp` (modelo: `tests/test_no_msaa.cpp`
  com os stubs `assert_fail_impl`, `unreachable_impl`, `Common::Log::ShouldLog/Write`; `_set_error_mode` no Win32):

```cpp
#include "video_core/renderer_vulkan/vk_vertex_format.h"
using namespace Vulkan::LiverpoolToVK;
using AmdGpu::DataFormat; using AmdGpu::NumberFormat;
int main() {
    assert(VertexAttributeFormat(DataFormat::FormatInvalid, NumberFormat::Unorm) == vk::Format::eUndefined);
    assert(VertexAttributeFormat(DataFormat::Format8_8_8_8, NumberFormat::Unorm) == vk::Format::eR8G8B8A8Unorm);
    assert(VertexAttributeFormat(DataFormat::Format32_32_32, NumberFormat::Float) == vk::Format::eR32G32B32Sfloat);
    assert(!VertexAttributeFormat(static_cast<DataFormat>(15), NumberFormat::Sint).has_value()); // #35, 480p
    VertexFormatSeen seen;
    assert(seen.Add(vk::Format::eB10G11R11UfloatPack32));
    assert(!seen.Add(vk::Format::eB10G11R11UfloatPack32));
    assert(seen.Add(vk::Format::eR8G8B8A8Unorm));
    assert(!seen.Add(vk::Format::eUndefined));
    assert(seen.seen.size() == 2);
    std::puts("vertex-format-test: ok");
}
```

Registro em `gpu/CMakeLists.txt`, logo após o bloco de `extension-policy-test`:

```cmake
add_executable(vertex-format-test EXCLUDE_FROM_ALL ../tests/test_vertex_format.cpp
    shadps4/video_core/renderer_vulkan/liverpool_to_vk.cpp shadps4/video_core/amdgpu/pixel_format.cpp)
target_compile_options(vertex-format-test PRIVATE -UNDEBUG)
target_include_directories(vertex-format-test PRIVATE shim shadps4 third_party/gcn/include)
target_link_libraries(vertex-format-test PRIVATE Vulkan::Headers fmt::fmt Boost::headers magic_enum::magic_enum)
```

Se o link reclamar do dispatcher da Vulkan-Hpp, acrescentar no teste a mesma definição de storage que
`tests/test_no_msaa.cpp` ou `tests/test_async_pipeline_cache.cpp` usam (procurar `VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE`).

- [ ] **Step 2: ver falhar.** Run: `ms.sh /x '<caminho longo>' out/x vertex-format-test`.
  Expected: erro de compilação `vk_vertex_format.h: No such file or directory`.

- [ ] **Step 3: implementar `vk_vertex_format.h`** (header-only, `#pragma once`, inclui `<optional>`, `<vector>`,
  `liverpool_to_vk.h`, `video_core/amdgpu/pixel_format.h`). Comentário de cabeçalho em inglês, estilo `// gow3: ...`,
  citando a #35 e o commit efb428d (mesma proteção no caminho dinâmico).

- [ ] **Step 4: ver passar.** Run: build do alvo e `PATH=/c/msys64/clang64/bin:$PATH out/x/vertex-format-test.exe`.
  Expected: `vertex-format-test: ok`.

- [ ] **Step 5: usar o helper no caminho dinâmico** em `vk_draw_prep.cpp:136-147`: substituir o bloco
  `TrySurfaceFormat` + comparação por `const auto format = LiverpoolToVK::VertexAttributeFormat(...); if (!format) { out.valid = false; return; }`
  e usar `*format` no `attributes.push_back`. Comportamento idêntico ao atual.

- [ ] **Step 6: endurecer o caminho estático** em `vk_pipeline_cache.cpp:1112-1125` (ramo
  `!instance.IsVertexInputDynamicState()`): para cada atributo,
  `const auto format = LiverpoolToVK::VertexAttributeFormat(buffer.GetDataFmt(), buffer.GetNumberFmt());`
  `if (!format) { if (sel.worker) { sel.worker->failed = true; return false; } }` e, sem worker (GPU thread, V#
  final), manter `LiverpoolToVK::SurfaceFormat(...)` para preservar a mensagem do assert; senão
  `key.vertex_buffer_formats[vertex_binding++] = *format;`. Comentário: o worker lê user data que o guest ainda
  pode estar escrevendo (ver `vk_draw_prep.cpp`); a GPU thread refaz a chave na submissão.

- [ ] **Step 7: compilar `gow3gpu`** (`ms.sh ... out/x gow3-probe`) e rodar de novo `vertex-format-test`,
  `no-msaa-test`, `extension-policy-test`. Expected: três `ok`, build sem warnings novos.

- [ ] **Step 8: commit** (mensagem pela `caveman-commit`; intenção: formato de vértice ilegível não aborta mais o
  worker de preparação no caminho estático). Stage: `tests/test_vertex_format.cpp`, `gpu/CMakeLists.txt`,
  `gpu/shadps4/video_core/renderer_vulkan/vk_vertex_format.h`, `vk_draw_prep.cpp`, `vk_pipeline_cache.cpp`.

### Task 2: modo de vertex input na identidade do cache de pipelines

**Files:**
- Create: `tests/test_shader_profile.cpp`
- Modify: `gpu/CMakeLists.txt` (após `vertex-format-test`)
- Modify: `gpu/shadps4/shader_recompiler/profile.h:56-59`
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_cache.cpp:330-391` (construção de `profile`)
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_serialization.cpp:879-882` (comentário)

**Interfaces:**
- Produces: `Shader::Profile::vertex_input` (`u32`, `1` = dinâmico, `2` = estático; `0` só em um profile vazio ou
  num blob antigo). `CheckCacheProfile` já compara `sizeof` e `operator==`, nada mais muda lá.

- [ ] **Step 1: teste que falha** em `tests/test_shader_profile.cpp` (header-only, modelo `tests/test_extension_policy.cpp`):

```cpp
#include "shader_recompiler/profile.h"
int main() {
    Shader::Profile dynamic{.vertex_input = 1};
    Shader::Profile fixed{.vertex_input = 2};
    assert(!(dynamic == fixed));
    assert(Shader::Profile{}.vertex_input == 0);   // a 2.1.8 cache blob never matches 1 or 2
    assert(!(Shader::Profile{} == dynamic) && !(Shader::Profile{} == fixed));
    std::puts("shader-profile-test: ok");
}
```

CMake: `add_executable(shader-profile-test EXCLUDE_FROM_ALL ../tests/test_shader_profile.cpp)`,
`target_compile_options(... -UNDEBUG -Wall -Wextra -Werror)`, `target_include_directories(... PRIVATE shadps4)`.

- [ ] **Step 2: ver falhar.** Expected: `no member named 'vertex_input' in 'Shader::Profile'`.

- [ ] **Step 3: implementar.** Em `profile.h`, depois de `supports_shader_stencil_export`:
  `/// gow3: 1 with VK_EXT_vertex_input_dynamic_state, 2 with the static vertex input path (#35); keys and`
  `/// pipelines of one mode are invalid in the other, so the disk cache is rebuilt when it flips.`
  `u32 vertex_input{};`. Em `vk_pipeline_cache.cpp` na inicialização de `profile`:
  `.vertex_input = instance_.IsVertexInputDynamicState() ? 1u : 2u,`. Em `vk_pipeline_serialization.cpp:879-882`
  tirar "vertex input dynamic state" da lista de extensões fora do Profile.

- [ ] **Step 4: ver passar** (`shader-profile-test` + rebuild de `gow3-probe`). Expected: `shader-profile-test: ok`.

- [ ] **Step 5: commit** (intenção: cache de pipelines reconstruído quando o modo de vertex input muda).

### Task 3: default Intel Windows, override e linha de log

**Files:**
- Create: `gpu/shadps4/video_core/renderer_vulkan/gow3_vertex_input_default.h` (C puro, só `<vulkan/vulkan_core.h>`)
- Create: `tests/test_vertex_input_default.cpp`
- Modify: `gpu/CMakeLists.txt`
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_instance.cpp:378` e o bloco de `printf` em `:810-822`

**Interfaces:**
- Consumes: `Instance::driver_id` (preenchido em `CollectDeviceParameters`, chamado em `:190` antes de
  `CreateDevice` em `:196`), `add_extension` (lambda de `CreateDevice`), `disabled_extensions`
  (preenchido pelo knob), `GetDriverVersionName()`.
- Produces:
  - `static inline int gow3_vertex_input_dynamic_default(VkDriverId driver)`: `0` para
    `VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS`, `1` para qualquer outro (inclusive `0`, driver desconhecido).
  - `static inline int gow3_vertex_input_dynamic(const char* env, VkDriverId driver)`: `env && *env ? *env == '1' : default`.
  - Linha de stdout, uma das seis, impressa logo após a decisão da extensão:
    `GPU: vertex input dynamic state off (Intel Windows driver; GOW3_VERTEX_INPUT_DYNAMIC=1 turns it on)`,
    `GPU: vertex input dynamic state off (GOW3_VERTEX_INPUT_DYNAMIC=0)`,
    `GPU: vertex input dynamic state off (GOW3_VK_DISABLE_EXT)`,
    `GPU: vertex input dynamic state on (GOW3_VERTEX_INPUT_DYNAMIC=1)`,
    `GPU: vertex input dynamic state on`,
    `GPU: vertex input dynamic state unavailable (VK_EXT_vertex_input_dynamic_state missing)`.

- [ ] **Step 1: teste que falha** em `tests/test_vertex_input_default.cpp` (modelo `test_extension_policy.cpp`,
  mais `#include <vulkan/vulkan_core.h>`):

```cpp
assert(gow3_vertex_input_dynamic_default(VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) == 0);
for (VkDriverId d : {VK_DRIVER_ID_NVIDIA_PROPRIETARY, VK_DRIVER_ID_AMD_PROPRIETARY, VK_DRIVER_ID_MESA_RADV,
                     VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, VK_DRIVER_ID_MOLTENVK, (VkDriverId)0})
    assert(gow3_vertex_input_dynamic_default(d) == 1);
assert(gow3_vertex_input_dynamic(nullptr, VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) == 0);
assert(gow3_vertex_input_dynamic("", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 1);
assert(gow3_vertex_input_dynamic("1", VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) == 1);
assert(gow3_vertex_input_dynamic("0", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 0);
assert(gow3_vertex_input_dynamic("2", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 0);
```

CMake: `vertex-input-default-test` como `extension-policy-test` mais `target_link_libraries(... PRIVATE Vulkan::Headers)`.

- [ ] **Step 2: ver falhar.** Expected: `gow3_vertex_input_default.h: No such file or directory`.

- [ ] **Step 3: implementar o cabeçalho** (comentário de topo como em `gow3_msaa_default.h`: o que o driver Intel
  faz com a extensão na HD 630 (#35), que ANGLE e yuzu desligam a extensão em toda Intel Windows, que Arc segue
  o mesmo default até alguém medir o contrário, e que `tools/gpu_capabilities.c` imprime o mesmo resultado).

- [ ] **Step 4: ver passar.** Expected: `vertex-input-default-test: ok`.

- [ ] **Step 5: aplicar em `vk_instance.cpp:378`:**
  `const char* vertex_input_env = std::getenv("GOW3_VERTEX_INPUT_DYNAMIC");`
  `const bool vertex_input_wanted = gow3_vertex_input_dynamic(vertex_input_env, static_cast<VkDriverId>(driver_id)) != 0;`
  `vertex_input_dynamic_state = (vertex_input_wanted || ext_policy.IsDisabled(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME)) && add_extension(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);`
  (o knob precisa ver o pedido para registrar a extensão em `disabled_extensions` e fechar o cache; com
  `vertex_input_wanted && add_extension(...)` ele era ignorado quando o default já desligava a extensão).
  Depois decidir a linha: se `vertex_input_dynamic_state` → `on` (com sufixo `(GOW3_VERTEX_INPUT_DYNAMIC=1)` quando
  `vertex_input_env` for `"1"`); senão, se o nome está em `disabled_extensions` → `off (GOW3_VK_DISABLE_EXT)`; senão
  se `!vertex_input_wanted` → `off (GOW3_VERTEX_INPUT_DYNAMIC=0)` quando `vertex_input_env` não vazio, ou
  `off (Intel Windows driver; ...)`; senão → `unavailable (...)`. Incluir o cabeçalho novo ao lado de
  `gow3_msaa_default.h` em `vk_instance.cpp`.

- [ ] **Step 6: build `gow3-probe`** e os quatro testes leves (`no-msaa`, `extension-policy`, `vertex-format`,
  `shader-profile`, `vertex-input-default`). Expected: todos `ok`.

- [ ] **Step 7: commit** (intenção: vertex input dinâmico desligado por padrão na Intel Windows, #35).

### Task 4: aviso único por formato de vértice sem o feature VertexBuffer

**Files:**
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.h` (membro `LiverpoolToVK::VertexFormatSeen vertex_formats_seen;`)
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp:1800-1806` (fim de `ResolveVertexBuffers`, após
  `attributes` preenchido pelos dois caminhos)

**Interfaces:**
- Consumes: `VertexFormatSeen` (Task 1), `Instance::IsFormatSupported(vk::Format, vk::FormatFeatureFlags2)` (`vk_instance.h:515`).
- Produces: `LOG_WARNING(Render_Vulkan, "Vertex attribute format {} lacks VertexBuffer support on this GPU (#35)", vk::to_string(format))`
  uma vez por formato, só quando `!instance.IsFormatSupported(format, vk::FormatFeatureFlagBits2::eVertexBuffer)`.

- [ ] **Step 1:** o teste de `VertexFormatSeen` da Task 1 já cobre a lógica pura; não há teste novo. Implementar o
  laço: `for (const auto& attribute : attributes) if (vertex_formats_seen.Add(attribute.format) && !instance.IsFormatSupported(attribute.format, vk::FormatFeatureFlagBits2::eVertexBuffer)) LOG_WARNING(...)`.
  Só a GPU thread chama `ResolveVertexBuffers`, então o `std::vector` não precisa de lock.

- [ ] **Step 2: build `gow3-probe`.** Expected: compila sem warnings.

- [ ] **Step 3: commit** (intenção: log diz quais formatos de vértice a GPU não aceita, para separar driver de port na #35).

### Task 5: coluna do `gow3-gpu-capabilities --list`, launcher e README

**Files:**
- Modify: `tools/gpu_capabilities.c:8` (include) e `:98-123` (comentário e `printf`)
- Create: `tests/test_launcher_vertex_input.py`
- Modify: `launcher/gow3_launcher_win.py:119-141` (docstring de `parse_gpu_list`, nova `vertex_input_options`),
  `:277` (`APP_DEFAULTS['vertex_input'] = ''`), `:356-362` (`VERTEX_INPUT`, `VERTEX_INPUT_HINT`), `:708` (env),
  `:1305-1312` (linha na página Performance e `relabel`)
- Modify: `launcher/gow3_lang.py:746-752` (`EXTRA['pt']`)
- Modify: `README.md:214-224` (parágrafo após "Game multisampling")

**Interfaces:**
- Produces: sexta coluna de `--list`: `on`/`off` = `gow3_vertex_input_dynamic_default(driver.driverID)`, anexada
  ao fim (as colunas 0-4 não mudam; `chosen_gpu` lê `row[4]`).
  `VERTEX_INPUT = [('', ('Automatic',)), ('1', ('On',)), ('0', ('Off',))]`;
  `VERTEX_INPUT_HINT = "Vulkan's dynamic vertex input. Intel's Windows driver draws the 3D scene black or blue with it (HD 630, #35), so Automatic turns it off on Intel cards and keeps it on elsewhere. On and Off force the choice on any card; a wrong choice only changes how vertex formats reach the driver."`;
  `vertex_input_options(gpus, gpu_id)` igual a `no_msaa_options` lendo `row[5]` e os mesmos rótulos
  `'Automatic ({}: off)'`/`'Automatic ({}: on)'`; env `GOW3_VERTEX_INPUT_DYNAMIC` só quando `s.get('vertex_input')`.
  Texto pt em `EXTRA`: `'Dynamic vertex input': 'Entrada de vértices dinâmica'` e a tradução da dica.

- [ ] **Step 1: teste Python que falha** em `tests/test_launcher_vertex_input.py` (espelho de
  `tests/test_launcher_no_msaa.py`): linhas de seis colunas
  `HD630_PC = [('0', 'Intel(R) HD Graphics 630', 'integrated', 'off', '0', 'off'), ('1', 'NVIDIA GeForce GTX 1050 Ti', 'discrete', 'on', '1', 'on')]`;
  testes: default `''` não adiciona `GOW3_VERTEX_INPUT_DYNAMIC` e preserva um valor herdado do ambiente; `'1'`/`'0'`
  setam a variável; `VERTEX_INPUT` tem valores `['', '1', '0']`; settings antigos sem a chave não quebram;
  `vertex_input_options(HD630_PC, '0')[0][1] == ('Automatic (Intel(R) HD Graphics 630: off)',)`, `''` escolhe a
  1050 Ti (`on`), lista de cinco colunas → `('Automatic',)`; pt: `'Automático (Intel(R) HD Graphics 630: desligado)'`;
  `gow3_lang.table('pt')` tem `'Dynamic vertex input'` e `VERTEX_INPUT_HINT`; e o teste Tk da página Performance
  com a nova linha, copiado de `NoMsaaLauncherTest` (pula sem display).

- [ ] **Step 2: ver falhar.** Run: `python -m unittest discover -s tests -p "test_launcher_vertex_input.py"`.
  Expected: `AttributeError: module 'gow3_launcher_win' has no attribute 'VERTEX_INPUT'`.

- [ ] **Step 3: implementar** launcher + `gow3_lang.py`; `relabel` passa a atualizar as duas caixas (multisampling e
  vertex input) quando a placa muda.

- [ ] **Step 4: ver passar** o teste novo e `test_launcher_no_msaa.py` e `test_launcher_controls.py`.

- [ ] **Step 5: `tools/gpu_capabilities.c`:** incluir `video_core/renderer_vulkan/gow3_vertex_input_default.h`,
  acrescentar `\t%s` com `gow3_vertex_input_dynamic_default(driver.driverID) ? "on" : "off"` e atualizar o comentário.
  Build `gow3-gpu-capabilities` e rodar `out/x/gow3-gpu-capabilities.exe --list`. Expected na GTX 1650: linha com
  seis colunas terminando em `on`.

- [ ] **Step 6: README** (inglês): parágrafo "**Performance → GPU data → Dynamic vertex input**" após o de Game
  multisampling, dizendo o que Automatic faz na Intel, a variável `GOW3_VERTEX_INPUT_DYNAMIC` e a linha de log
  `GPU: vertex input dynamic state ...`.

- [ ] **Step 7: commit** (intenção: opção de entrada de vértices dinâmica no launcher com a decisão por placa).

### Entrega (sessão principal, depois das tasks)

1. Verificação local na GTX 1650 (memória `local-game-runs`; ~8,5 GB de commit livres, uma instância por vez):
   - `GOW3_VERTEX_INPUT_DYNAMIC=0 GOW3_GPU_LOG=info` até o menu: caminho estático com o endurecimento; esperar
     `GPU: vertex input dynamic state off (GOW3_VERTEX_INPUT_DYNAMIC=0)`, `Pipeline cache isn't compatible ... rebuilding it`
     na primeira vez, menu com Kratos, nenhum `SurfaceFormat: Assertion Failed`.
   - default até o menu: `GPU: vertex input dynamic state on`, nada muda na NVIDIA.
   - `gow3-gpu-capabilities --list` com seis colunas.
2. Pacote: `packaging/windows/package.sh` a partir do alias (memória `worktree-build-msys2`), sha256 `-b`, push
   da branch, pré-release `windows-v2.1.8-intel-hd630-test1` com notas em inglês: o que muda, os `.bat` para
   (a) default, (b) `GOW3_VERTEX_INPUT_DYNAMIC=1` (controle), (c) bissecção das outras quatro extensões com
   `GOW3_VK_DISABLE_EXT` (metades `VK_EXT_provoking_vertex,VK_EXT_shader_stencil_export` e
   `VK_KHR_workgroup_memory_explicit_layout,VK_EXT_image_2d_view_of_3d`, depois nomes únicos), todos com
   `GOW3_GPU_LOG=info`, New Game até o primeiro combate, e o pedido da linha `Compiling graphics pipeline`
   anterior a qualquer `Fault:`.
3. Comentário curto na #35 (memória `issue-comments-short`): link, rodar com cache limpo, funcionou?, `user\last_run.log`.
4. Integração (merge/PR) é decisão do mantenedor: `superpowers:finishing-a-development-branch`.
