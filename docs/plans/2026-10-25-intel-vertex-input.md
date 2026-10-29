# Intel Windows vertex input: plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** levar para a `main` 2.8.0, em um branch novo, o fix do 3D preto/azul nas GPUs Intel Windows (vertex input dinâmico desligado nesse driver) e os dois knobs de ambiente que o acompanham, e sair com um test build pela CI.

**Architecture:** três headers novos sem dependência de device (política de `SPARTAN_VK_DISABLE_EXT`, default do vertex input por `VkDriverId`, formato de vértice tolerante), um campo novo em `Shader::Profile` para a identidade do cache, e a fiação em `Instance::CreateDevice` que lê os knobs, filtra `add_extension`, imprime as linhas de log e fecha os caches quando algo foi desligado. O caminho estático de vertex input já existe na `main`; o port só decide quando ele é usado e o endurece contra um V# meio escrito.

**Tech Stack:** C++23 (shadPS4 em `gpu/shadps4`), CMake + Ninja no MSYS2 CLANG64, testes como executáveis `EXCLUDE_FROM_ALL` com `assert`, CI `windows-zip.yml`, `gh`.

**Spec:** `docs/specs/2026-10-25-intel-vertex-input-design.md`

## Global Constraints

- Branch `intel-vertex-input` em `.claude/worktrees/intel-vertex-input`, criado a partir de `main` (`a3e9e71` ou mais novo). O branch antigo `worktree-intel-hd630` (`c0a8684`) é só referência: nada é cherry-picked.
- Nomes: variáveis `SPARTAN_VK_DISABLE_EXT` e `SPARTAN_VERTEX_INPUT_DYNAMIC`; header `spartanport_vertex_input_default.h` com `spartanport_vertex_input_dynamic_default` e `spartanport_vertex_input_dynamic`; comentários `// spartanport:`. Nenhum `GOW3_` ou `gow3_` novo no código.
- Strings de log exatas da spec §4.5: `GPU: vertex input dynamic state %s` com um de `on`, `on (SPARTAN_VERTEX_INPUT_DYNAMIC=1)`, `off (SPARTAN_VK_DISABLE_EXT)`, `off (SPARTAN_VERTEX_INPUT_DYNAMIC=0)`, `off (Intel Windows driver; SPARTAN_VERTEX_INPUT_DYNAMIC=1 turns it on)`, `unavailable (VK_EXT_vertex_input_dynamic_state missing)`; `Extension {} disabled by SPARTAN_VK_DISABLE_EXT`; `Pipeline cache skipped: SPARTAN_VK_DISABLE_EXT is active`.
- Precedência: `SPARTAN_VK_DISABLE_EXT` > `SPARTAN_VERTEX_INPUT_DYNAMIC` > default por driver. Quem não define nada fora da Intel Windows não muda de comportamento.
- Alvos de teste: `add_executable(<nome>-test EXCLUDE_FROM_ALL ../tests/test_<nome>.cpp)` em `gpu/CMakeLists.txt`, logo depois do bloco `no-msaa-test` (linhas 340-344), com `target_compile_options(... PRIVATE -UNDEBUG -Wall -Wextra -Werror)` quando o alvo não puxa fontes do shadPS4. Cada teste imprime `<nome>-test: ok` e devolve 0.
- Fora do escopo (spec §7): launcher Rust, `spartanport-gpu-capabilities`, commits de diagnóstico, README, #334.
- Commits: um por task, mensagem gerada pela skill `caveman-commit` (Conventional Commits, tipo capitalizado como no repo: `Feat(GPU):`, `Fix(GPU):`), sem menção a IA, sem `--author`.
- Arquivos novos: escrever com a ferramenta Write ou por `git show <ref>:<path> | sed ... > <path>`. Nunca por heredoc do Bash: ele corrompe `\t` e outras barras invertidas que os testes contêm.
- Toolchain: `source ./msys2-env.sh` antes de `cmake`/`ninja` no Bash. Nunca `build.sh` inteiro nem `ninja -C out/gpu` sem alvo: a lib GPU completa fica para a CI.

## Review Focus

1. `SPARTAN_VERTEX_INPUT_DYNAMIC=" "` (só espaço): o valor é "definido e diferente de `1`", logo desliga a extensão em qualquer driver e a linha diz `off (SPARTAN_VERTEX_INPUT_DYNAMIC=0)`. Teste na Task 2.
2. Os dois knobs em conflito (`SPARTAN_VK_DISABLE_EXT=VK_EXT_vertex_input_dynamic_state` e `SPARTAN_VERTEX_INPUT_DYNAMIC=1`): a política vence, a linha diz `off (SPARTAN_VK_DISABLE_EXT)`, a extensão aparece uma vez só em `extensions off`. Teste de `OffNamesForLog` na Task 1; cenário 3 do smoke na Task 7.
3. Tester com `GOW3_VK_DISABLE_EXT` ou `GOW3_VERTEX_INPUT_DYNAMIC` definido à mão (#1055): o launcher Rust encaminha para `SPARTAN_*` (`launcher-rust/src/core/legacy.rs`, testes existentes nesse arquivo); o exe rodado direto ignora. O anúncio da Task 7 só cita os nomes `SPARTAN_*`.
4. Blob de perfil escrito pela 2.7.0 (sem o campo `vertex_input`, `sizeof` diferente): `CheckCacheProfile` avisa o tamanho, o perfil não casa, o cache reconstrói uma vez. Teste na Task 4 (`Shader::Profile{}.vertex_input == 0` e desigualdade com 1 e 2); primeiro run do cenário 2 do smoke mostra a reconstrução.
5. `SPARTAN_VK_DISABLE_EXT=all`: toda extensão opcional cai, inclusive `VK_EXT_memory_budget` e as do DLSS, caches fechados, `ignored names: none`. Teste de `IgnoredNames(all)` na Task 1. Nunca recomendar `all` a testers.

---

### Task 0: worktree nova e toolchain (sessão principal)

**Files:**
- Remove: `.claude/worktrees/intel-hd630/` (worktree; o branch `worktree-intel-hd630` fica)
- Create: `.claude/worktrees/intel-vertex-input/` (worktree do branch novo `intel-vertex-input`)

**Interfaces:**
- Produces: worktree configurada com `out/gpu/build.ninja`; `ninja -C out/gpu <alvo>` funciona para alvos de teste.

- [ ] **Step 1: remover a worktree antiga**

Run (no checkout principal): `git worktree remove --force .claude/worktrees/intel-hd630`
`--force` porque o submodule `gpu/third_party/fsr-vulkan` está marcado ` m` (patches do `build.sh` aplicados); nenhum commit se perde.
Verificar: `git worktree list` mostra só o checkout principal; `git branch --list worktree-intel-hd630` ainda lista o branch.

- [ ] **Step 2: criar a worktree nova**

Run: `git worktree add .claude/worktrees/intel-vertex-input -b intel-vertex-input main`
Verificar: `git -C .claude/worktrees/intel-vertex-input log --oneline -1` mostra o HEAD da `main`.

- [ ] **Step 3: submodules e patches, como o `build.sh` faz**

Run (dentro da worktree, Bash):
```bash
git submodule update --init --recursive
for patch in gpu/patches/fsr-vulkan/*.patch; do
    git -C gpu/third_party/fsr-vulkan apply --reverse --check "$PWD/$patch" 2>/dev/null ||
        git -C gpu/third_party/fsr-vulkan apply "$PWD/$patch"
done
```
Verificar: `ls gpu/third_party/fsr-vulkan/CMakeLists.txt gpu/third_party/imgui/imgui.h gpu/third_party/xbyak/xbyak/xbyak.h` existem.

- [ ] **Step 4: configurar o Ninja**

Run (dentro da worktree, Bash):
```bash
source ./msys2-env.sh
cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSPARTAN_PGO=off -DSPARTAN_LTO=OFF -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
```
Verificar: `out/gpu/build.ninja` existe.

- [ ] **Step 5: provar o toolchain com um teste que já existe**

Run: `source ./msys2-env.sh && ninja -C out/gpu no-msaa-test && out/gpu/no-msaa-test.exe`
Expected: última linha `no-msaa-test: ok`, exit 0.

- [ ] **Step 6: descobrir os nomes dos objetos que as tasks 3 a 6 compilam**

Run: `ninja -C out/gpu -t targets all | grep -E "vk_instance\.cpp\.obj|vk_pipeline_cache\.cpp\.obj|vk_pipeline_serialization\.cpp\.obj|vk_draw_prep\.cpp\.obj"`
Anotar os quatro caminhos (formato esperado `CMakeFiles/spartanportgpu.dir/shadps4/video_core/renderer_vulkan/<arquivo>.cpp.obj`) e passá-los nos prompts das tasks 3 a 6 como `OBJ_INSTANCE`, `OBJ_CACHE`, `OBJ_SERIAL`, `OBJ_PREP`.

Nada a commitar.

---

### Task 1: política de `SPARTAN_VK_DISABLE_EXT`

**Files:**
- Create: `gpu/shadps4/video_core/renderer_vulkan/vk_extension_policy.h`
- Create: `tests/test_extension_policy.cpp`
- Modify: `gpu/CMakeLists.txt` (depois da linha 344, fim do bloco `no-msaa-test`)

**Interfaces:**
- Produces, em `namespace Vulkan`:
  - `struct ExtensionPolicy { bool all; std::vector<std::string> names; bool IsDisabled(std::string_view) const; bool Empty() const; }`
  - `ExtensionPolicy ParseExtensionPolicy(const char* env)`
  - `std::vector<std::string> IgnoredNames(const ExtensionPolicy&, const std::vector<std::string>& applied)`
  - `std::string JoinNames(const std::vector<std::string>&)` (`none` para vazio, senão `a, b`)
  - `std::vector<std::string> OffNamesForLog(const std::vector<std::string>& disabled, std::string_view default_off_name, std::string_view reason)`

- [ ] **Step 1: trazer o teste do branch antigo com os nomes novos**

Run:
```bash
git show worktree-intel-hd630:tests/test_extension_policy.cpp | sed 's/GOW3_/SPARTAN_/g' > tests/test_extension_policy.cpp
```
Verificar: `grep -c SPARTAN_ tests/test_extension_policy.cpp` dá 3 e `grep -c GOW3_ tests/test_extension_policy.cpp` dá 0. O teste cobre: unset/vazio/só brancos → `Empty()`; um nome; lista; espaços e tabs; sensível a maiúsculas; prefixo não casa; `all` em qualquer posição, `ALL`/`All` não; duplicatas uma vez em ordem; `IgnoredNames("VK_A,bogus", {"VK_A"}) == {"bogus"}` e `IgnoredNames(all, ...)` vazio; `JoinNames`; `OffNamesForLog` com nome vazio, nome novo com motivo, nome já desligado sem duplicar, e a string `"VK_B (SPARTAN_VERTEX_INPUT_DYNAMIC=0)"`.

- [ ] **Step 2: registrar o alvo**

Em `gpu/CMakeLists.txt`, depois da linha `target_link_libraries(no-msaa-test ...)`:
```cmake
add_executable(extension-policy-test EXCLUDE_FROM_ALL ../tests/test_extension_policy.cpp)
target_compile_options(extension-policy-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(extension-policy-test PRIVATE shadps4)
```

- [ ] **Step 3: ver falhar**

Run: `source ./msys2-env.sh && ninja -C out/gpu extension-policy-test`
Expected: FAIL, `fatal error: 'video_core/renderer_vulkan/vk_extension_policy.h' file not found`.

- [ ] **Step 4: trazer o header do branch antigo com os nomes novos**

Run:
```bash
git show worktree-intel-hd630:gpu/shadps4/video_core/renderer_vulkan/vk_extension_policy.h | sed -e 's/GOW3_/SPARTAN_/g' -e 's#// gow3:#// spartanport:#g' > gpu/shadps4/video_core/renderer_vulkan/vk_extension_policy.h
```
Verificar: `grep -c "GOW3_\|gow3" gpu/shadps4/video_core/renderer_vulkan/vk_extension_policy.h` dá 0; o header define exatamente as cinco entidades do bloco Interfaces.

- [ ] **Step 5: ver passar**

Run: `ninja -C out/gpu extension-policy-test && out/gpu/extension-policy-test.exe`
Expected: `extension-policy-test: ok`, exit 0.

- [ ] **Step 6: commit**

```bash
git add gpu/shadps4/video_core/renderer_vulkan/vk_extension_policy.h tests/test_extension_policy.cpp gpu/CMakeLists.txt
git commit -F <mensagem da skill caveman-commit; intenção: "Feat(GPU): parse SPARTAN_VK_DISABLE_EXT", corpo: knob de teste que desliga extensões opcionais para bissecção em hardware que os mantenedores não têm; Refs #861>
```

---

### Task 2: default do vertex input por driver

**Files:**
- Create: `gpu/shadps4/video_core/renderer_vulkan/spartanport_vertex_input_default.h`
- Create: `tests/test_vertex_input_default.cpp`
- Modify: `gpu/CMakeLists.txt` (depois do bloco `extension-policy-test`)

**Interfaces:**
- Produces (C puro, `static inline`, inclui só `<vulkan/vulkan_core.h>`):
  - `int spartanport_vertex_input_dynamic_default(VkDriverId driver)`: 0 para `VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS`, 1 para o resto.
  - `int spartanport_vertex_input_dynamic(const char* env, VkDriverId driver)`: `env` nulo ou vazio → default; `"1"` → 1; qualquer outro → 0.

- [ ] **Step 1: escrever o teste (Write)**

`tests/test_vertex_input_default.cpp`:
```cpp
// SPDX-License-Identifier: GPL-2.0-or-later
// Default and SPARTAN_VERTEX_INPUT_DYNAMIC override for VK_EXT_vertex_input_dynamic_state
// (spartanport_vertex_input_default.h). No game, Vulkan device or window required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#include <vulkan/vulkan_core.h>

#include "video_core/renderer_vulkan/spartanport_vertex_input_default.h"

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    // Intel's Windows driver draws 3D objects black or blue with the extension (#861): off.
    assert(spartanport_vertex_input_dynamic_default(VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) == 0);
    // Every other driver, including an unknown one, keeps it on.
    for (VkDriverId d : {VK_DRIVER_ID_NVIDIA_PROPRIETARY, VK_DRIVER_ID_AMD_PROPRIETARY, VK_DRIVER_ID_MESA_RADV,
                         VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, VK_DRIVER_ID_MOLTENVK, (VkDriverId)0})
        assert(spartanport_vertex_input_dynamic_default(d) == 1);

    // Unset or empty variable: the driver default.
    assert(spartanport_vertex_input_dynamic(nullptr, VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) == 0);
    assert(spartanport_vertex_input_dynamic("", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 1);
    // "1" turns it on everywhere; any other value, a blank included, turns it off.
    assert(spartanport_vertex_input_dynamic("1", VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) == 1);
    assert(spartanport_vertex_input_dynamic("0", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 0);
    assert(spartanport_vertex_input_dynamic("2", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 0);
    assert(spartanport_vertex_input_dynamic(" ", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 0);

    std::puts("vertex-input-default-test: ok");
    return 0;
}
```

- [ ] **Step 2: registrar o alvo**

```cmake
add_executable(vertex-input-default-test EXCLUDE_FROM_ALL ../tests/test_vertex_input_default.cpp)
target_compile_options(vertex-input-default-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(vertex-input-default-test PRIVATE shadps4)
```

- [ ] **Step 3: ver falhar**

Run: `source ./msys2-env.sh && ninja -C out/gpu vertex-input-default-test`
Expected: FAIL, `'video_core/renderer_vulkan/spartanport_vertex_input_default.h' file not found`.

- [ ] **Step 4: escrever o header (Write)**

Modelo: `gpu/shadps4/video_core/amdgpu/spartanport_msaa_default.h` (mesmo cabeçalho `// spartanport:`, `#pragma once`, `#include <vulkan/vulkan_core.h>`, `static inline`). Conteúdo de referência: `git show worktree-intel-hd630:gpu/shadps4/video_core/renderer_vulkan/gow3_vertex_input_default.h`, com os dois nomes de função e o nome da variável trocados pelos do bloco Interfaces, o comentário citando #861 em vez de #35 e a frase sobre `tools/gpu_capabilities.c` removida (fora do escopo).

- [ ] **Step 5: ver passar**

Run: `ninja -C out/gpu vertex-input-default-test && out/gpu/vertex-input-default-test.exe`
Expected: `vertex-input-default-test: ok`, exit 0.

- [ ] **Step 6: commit**

```bash
git add gpu/shadps4/video_core/renderer_vulkan/spartanport_vertex_input_default.h tests/test_vertex_input_default.cpp gpu/CMakeLists.txt
git commit -F <mensagem da skill caveman-commit; intenção: "Feat(GPU): default vertex input off on Intel Windows", corpo: o driver desenha o 3D preto ou azul com VK_EXT_vertex_input_dynamic_state; SPARTAN_VERTEX_INPUT_DYNAMIC=1 religa; Refs #861>
```

---

### Task 3: formato de vértice tolerante no caminho estático

**Files:**
- Create: `gpu/shadps4/video_core/renderer_vulkan/vk_vertex_format.h`
- Create: `tests/test_vertex_format.cpp`
- Modify: `gpu/CMakeLists.txt` (depois do bloco `vertex-input-default-test`)
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_draw_prep.cpp:138-147` (`PrepareVertexInputs`)
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_cache.cpp:1139-1150` (`RefreshGraphicsStages`, ramo `!instance.IsVertexInputDynamicState()`)

**Interfaces:**
- Consumes: `vk::Format LiverpoolToVK::TrySurfaceFormat(AmdGpu::DataFormat, AmdGpu::NumberFormat)` (`liverpool_to_vk.h:68`, devolve `eUndefined` para par desconhecido); `SurfaceFormat` (linha 64, assertivo); `sel.worker` (`PipelineSelection::worker`, `vk_pipeline_cache.h:108`, ponteiro para `PrepWorker` com campo `failed`).
- Produces: `std::optional<vk::Format> Vulkan::LiverpoolToVK::VertexAttributeFormat(AmdGpu::DataFormat, AmdGpu::NumberFormat)` em `vk_vertex_format.h`.

- [ ] **Step 1: escrever o teste (Write)**

`tests/test_vertex_format.cpp`, o do branch antigo sem `VertexFormatSeen`:
```cpp
// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: vertex attribute formats read from a V# that the guest may still be writing (#861):
// VertexAttributeFormat tolerates unknown pairs instead of asserting. No game or Vulkan device
// required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "common/logging/log.h"
#include "video_core/renderer_vulkan/vk_vertex_format.h"

// The format tables assert and log through the port's logging; no logger is linked here.
void assert_fail_impl() {
    std::abort();
}
void unreachable_impl() {
    std::abort();
}
namespace Common::Log {
bool ShouldLog(Level) {
    return false;
}
void Write(std::string_view, Level, const char*, int, const char*, std::string&&) {}
} // namespace Common::Log

using namespace Vulkan::LiverpoolToVK;
using AmdGpu::DataFormat;
using AmdGpu::NumberFormat;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    assert(VertexAttributeFormat(DataFormat::FormatInvalid, NumberFormat::Unorm) ==
           vk::Format::eUndefined);
    assert(VertexAttributeFormat(DataFormat::Format8_8_8_8, NumberFormat::Unorm) ==
           vk::Format::eR8G8B8A8Unorm);
    assert(VertexAttributeFormat(DataFormat::Format32_32_32, NumberFormat::Float) ==
           vk::Format::eR32G32B32Sfloat);
    // #35, 480p: "SurfaceFormat: Unknown data_format=15 and num_format=5".
    assert(!VertexAttributeFormat(static_cast<DataFormat>(15), NumberFormat::Sint).has_value());
    std::puts("vertex-format-test: ok");
}
```
Se os stubs de `Common::Log` não casarem com `common/logging/log.h` da `main`, copiar as assinaturas de `tests/test_no_msaa.cpp:21-30`, que compila hoje.

- [ ] **Step 2: registrar o alvo (mesmas fontes e libs do `no-msaa-test`, mais `liverpool_to_vk.cpp`, `number_utils.cpp` e `half`)**

```cmake
add_executable(vertex-format-test EXCLUDE_FROM_ALL ../tests/test_vertex_format.cpp
    shadps4/video_core/renderer_vulkan/liverpool_to_vk.cpp shadps4/video_core/amdgpu/pixel_format.cpp
    shadps4/common/number_utils.cpp)
target_compile_options(vertex-format-test PRIVATE -UNDEBUG)
target_include_directories(vertex-format-test PRIVATE shim shadps4 third_party/gcn/include
    third_party/half/include)
target_link_libraries(vertex-format-test PRIVATE Vulkan::Headers fmt::fmt Boost::headers magic_enum::magic_enum)
```

- [ ] **Step 3: ver falhar**

Run: `source ./msys2-env.sh && ninja -C out/gpu vertex-format-test`
Expected: FAIL, `'video_core/renderer_vulkan/vk_vertex_format.h' file not found`.

- [ ] **Step 4: escrever o header (Write)**

`gpu/shadps4/video_core/renderer_vulkan/vk_vertex_format.h`: `git show worktree-intel-hd630:gpu/shadps4/video_core/renderer_vulkan/vk_vertex_format.h` sem o `struct VertexFormatSeen` (e sem `<algorithm>`/`<vector>` se só ele os usava), comentário de topo com `// spartanport:` e #861, mantendo a referência a `efb428d`. Fica só:
```cpp
inline std::optional<vk::Format> VertexAttributeFormat(AmdGpu::DataFormat data_format,
                                                       AmdGpu::NumberFormat num_format);
```
no `namespace Vulkan::LiverpoolToVK`: `eUndefined` para `FormatInvalid`, `std::nullopt` quando `TrySurfaceFormat` devolve `eUndefined` para outro `data_format`, senão o formato.

- [ ] **Step 5: ver passar**

Run: `ninja -C out/gpu vertex-format-test && out/gpu/vertex-format-test.exe`
Expected: `vertex-format-test: ok`, exit 0.

- [ ] **Step 6: usar o header em `vk_draw_prep.cpp`**

Trocar o include `video_core/renderer_vulkan/liverpool_to_vk.h` por `video_core/renderer_vulkan/vk_vertex_format.h` (que inclui o primeiro) e, em `PrepareVertexInputs` (linhas 141-147), substituir o par `TrySurfaceFormat` + comparação com `eUndefined`/`FormatInvalid` por:
```cpp
const auto format =
    LiverpoolToVK::VertexAttributeFormat(buffer.GetDataFmt(), buffer.GetNumberFmt());
if (!format) {
    out.valid = false;
    return;
}
```
e `.format = *format` no `attributes.push_back`. O comentário das linhas 138-140 fica.

- [ ] **Step 7: endurecer o ramo estático em `vk_pipeline_cache.cpp`**

Incluir `video_core/renderer_vulkan/vk_vertex_format.h` junto dos outros includes de `renderer_vulkan`. Em `RefreshGraphicsStages`, no loop das linhas 1143-1150, trocar a atribuição direta de `SurfaceFormat` por:
```cpp
// spartanport: a worker reads user data the guest may still be writing (see
// vk_draw_prep.cpp): an unknown format fails the worker's key, and the GPU thread builds it
// again once the draw is submitted. There the V# is final, so the asserting SurfaceFormat
// keeps its message for a format the table really lacks.
const auto format =
    LiverpoolToVK::VertexAttributeFormat(buffer.GetDataFmt(), buffer.GetNumberFmt());
if (!format) {
    if (sel.worker) {
        sel.worker->failed = true;
        return false;
    }
    key.vertex_buffer_formats[vertex_binding++] =
        LiverpoolToVK::SurfaceFormat(buffer.GetDataFmt(), buffer.GetNumberFmt());
    continue;
}
key.vertex_buffer_formats[vertex_binding++] = *format;
```

- [ ] **Step 8: compilar os dois objetos tocados**

Run: `ninja -C out/gpu OBJ_PREP OBJ_CACHE` (caminhos da Task 0, Step 6)
Expected: os dois `.obj` compilam sem erro nem warning novo.

- [ ] **Step 9: rodar os três testes até aqui**

Run: `ninja -C out/gpu extension-policy-test vertex-input-default-test vertex-format-test && out/gpu/extension-policy-test.exe && out/gpu/vertex-input-default-test.exe && out/gpu/vertex-format-test.exe`
Expected: três linhas `...-test: ok`.

- [ ] **Step 10: commit**

```bash
git add gpu/shadps4/video_core/renderer_vulkan/vk_vertex_format.h tests/test_vertex_format.cpp gpu/CMakeLists.txt gpu/shadps4/video_core/renderer_vulkan/vk_draw_prep.cpp gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_cache.cpp
git commit -F <mensagem da skill caveman-commit; intenção: "Fix(GPU): tolerate unknown vertex formats on the static path", corpo: workers de preparação leem um V# meio escrito; no caminho estático o SurfaceFormat assertivo derrubava o processo (data_format=15 a 480p); Refs #861>
```

---

### Task 4: identidade do cache de pipelines

**Files:**
- Modify: `gpu/shadps4/shader_recompiler/profile.h:56-61` (struct `Shader::Profile`)
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_cache.cpp:391-392` (inicialização de `profile` no construtor)
- Create: `tests/test_shader_profile.cpp`
- Modify: `gpu/CMakeLists.txt` (depois do bloco `vertex-format-test`)

**Interfaces:**
- Consumes: `bool Instance::IsVertexInputDynamicState() const` (`vk_instance.h:191`); `Shader::Profile::operator==` default (`profile.h:61`).
- Produces: `u32 Shader::Profile::vertex_input` (0 default, 1 dinâmico, 2 estático).

- [ ] **Step 1: escrever o teste (Write)**

`tests/test_shader_profile.cpp`:
```cpp
// SPDX-License-Identifier: GPL-2.0-or-later
// Shader::Profile identity of the vertex input mode (#861): the disk pipeline cache compares the
// profile blob, so the dynamic and static vertex input paths must never compare equal. No game,
// Vulkan device or window required.
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "shader_recompiler/profile.h"

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    const Shader::Profile dynamic{.vertex_input = 1};
    const Shader::Profile fixed{.vertex_input = 2};
    assert(!(dynamic == fixed));
    // A 2.7.0 cache blob has no vertex input field, so it never matches 1 or 2.
    assert(Shader::Profile{}.vertex_input == 0);
    assert(!(Shader::Profile{} == dynamic) && !(Shader::Profile{} == fixed));
    std::puts("shader-profile-test: ok");
    return EXIT_SUCCESS;
}
```

- [ ] **Step 2: registrar o alvo**

```cmake
add_executable(shader-profile-test EXCLUDE_FROM_ALL ../tests/test_shader_profile.cpp)
target_compile_options(shader-profile-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(shader-profile-test PRIVATE shadps4)
```

- [ ] **Step 3: ver falhar**

Run: `source ./msys2-env.sh && ninja -C out/gpu shader-profile-test`
Expected: FAIL, `no member named 'vertex_input' in 'Shader::Profile'`.

- [ ] **Step 4: o campo**

Em `profile.h`, entre `supports_shader_stencil_export{}` e o comentário de `dref_broadcast`:
```cpp
/// spartanport: 1 with VK_EXT_vertex_input_dynamic_state, 2 with the static vertex input path
/// (#861); keys and pipelines of one mode are invalid in the other, so the disk cache is rebuilt
/// when it flips.
u32 vertex_input{};
```

- [ ] **Step 5: ver passar**

Run: `ninja -C out/gpu shader-profile-test && out/gpu/shader-profile-test.exe`
Expected: `shader-profile-test: ok`, exit 0.

- [ ] **Step 6: preencher no construtor de `PipelineCache`**

Em `vk_pipeline_cache.cpp`, na lista de inicialização de `profile` (depois de `.supports_shader_stencil_export = ...`):
```cpp
.vertex_input = instance_.IsVertexInputDynamicState() ? 1u : 2u,
```

- [ ] **Step 7: compilar o objeto**

Run: `ninja -C out/gpu OBJ_CACHE`
Expected: compila sem erro.

- [ ] **Step 8: commit**

```bash
git add gpu/shadps4/shader_recompiler/profile.h gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_cache.cpp tests/test_shader_profile.cpp gpu/CMakeLists.txt
git commit -F <mensagem da skill caveman-commit; intenção: "Feat(GPU): key the pipeline cache on the vertex input mode", corpo: chaves do modo dinâmico são inválidas no estático e vice-versa; o perfil muda e o cache de disco reconstrói; Refs #861>
```

---

### Task 5: fiação em `Instance::CreateDevice`

**Files:**
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_instance.h:352-354` (acessor público) e `:562-563` (membros privados)
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_instance.cpp:15-21` (includes), `:264` (início de `CreateDevice`), `:297-311` (`add_extension`), `:313-321` (asserts de obrigatórias), `:358` (vertex input), `:505-513` (bloco DLSS), `:788` (antes de `CreateAllocator()`)

**Interfaces:**
- Consumes: Task 1 (`ParseExtensionPolicy`, `ExtensionPolicy::IsDisabled`, `OffNamesForLog`, `JoinNames`, `IgnoredNames`); Task 2 (`spartanport_vertex_input_dynamic`); `driver_id` (preenchido em `CollectDeviceParameters`, que roda antes de `CreateDevice`: `vk_instance.cpp:189,195`); `GetDriverVersionName()`, `GetReadableVersion(properties.apiVersion)`, `properties.deviceName`.
- Produces: `const std::vector<std::string>& Instance::GetDisabledExtensions() const` (só o que `SPARTAN_VK_DISABLE_EXT` desligou, em ordem de pedido; vazio quando nada).

Sem teste unitário: precisa de device Vulkan. Verificação: o objeto compila (Step 8) e os cenários do smoke da Task 7. Referência completa do diff antigo: `git diff 1594332 worktree-intel-hd630 -- gpu/shadps4/video_core/renderer_vulkan/vk_instance.cpp gpu/shadps4/video_core/renderer_vulkan/vk_instance.h` (trocar `GOW3_`→`SPARTAN_`, `gow3_`→`spartanport_`, `// gow3:`→`// spartanport:`, `#35`→`#861`).

- [ ] **Step 1: `vk_instance.h`**

Depois de `GetAvailableExtensions()`:
```cpp
/// spartanport: optional extensions turned off by SPARTAN_VK_DISABLE_EXT, in request order; empty
/// when the variable disabled nothing.
const std::vector<std::string>& GetDisabledExtensions() const {
    return disabled_extensions;
}
```
Depois de `std::vector<std::string> available_extensions;`:
```cpp
std::vector<std::string> disabled_extensions; // spartanport: applied by SPARTAN_VK_DISABLE_EXT
std::string disable_ext_env;                  // spartanport: raw SPARTAN_VK_DISABLE_EXT, or empty
```

- [ ] **Step 2: includes em `vk_instance.cpp`**

Em ordem alfabética entre os `video_core/renderer_vulkan/...`: `spartanport_vertex_input_default.h` e `vk_extension_policy.h`.

- [ ] **Step 3: ler o knob no início de `CreateDevice`**

Primeiras linhas do corpo:
```cpp
// spartanport: SPARTAN_VK_DISABLE_EXT turns optional extensions off as if the device lacked them
// (vk_extension_policy.h).
const char* disable_ext_value = std::getenv("SPARTAN_VK_DISABLE_EXT");
const bool disable_ext_set = disable_ext_value != nullptr;
disable_ext_env = disable_ext_set ? disable_ext_value : "";
const auto ext_policy = ParseExtensionPolicy(disable_ext_value);
std::vector<std::string> optional_queried;
```

- [ ] **Step 4: `add_extension(extension, required = false)`**

Assinatura `[&](std::string_view extension, bool required = false) -> bool`. Ordem: (1) se `!required`, `optional_queried.emplace_back(extension)`; (2) indisponível → `LOG_WARNING(... "Extension {} unavailable.")`, `false` (como hoje); (3) se `!required && ext_policy.IsDisabled(extension)` → `LOG_WARNING(Render_Vulkan, "Extension {} disabled by SPARTAN_VK_DISABLE_EXT", extension)`, `disabled_extensions.emplace_back(extension)`, `false`; (4) senão habilita como hoje. Os quatro `ASSERT_MSG(add_extension(...))` das linhas 313-321 passam `true` como segundo argumento.

- [ ] **Step 5: vertex input (substitui a linha 358)**

```cpp
// spartanport: off by default on Intel's Windows driver (#861), SPARTAN_VERTEX_INPUT_DYNAMIC
// overrides it (spartanport_vertex_input_default.h); SPARTAN_VK_DISABLE_EXT still wins inside
// add_extension.
const char* vertex_input_env = std::getenv("SPARTAN_VERTEX_INPUT_DYNAMIC");
const bool vertex_input_wanted =
    spartanport_vertex_input_dynamic(vertex_input_env, static_cast<VkDriverId>(driver_id)) != 0;
// spartanport: the knob must still see the request so it can record it in disabled_extensions.
vertex_input_dynamic_state =
    (vertex_input_wanted ||
     ext_policy.IsDisabled(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME)) &&
    add_extension(VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);
const char* vertex_input_line;
// spartanport: why the default kept the extension off, for the "extensions off" summary.
std::string_view vertex_input_off_reason;
if (vertex_input_dynamic_state) {
    vertex_input_line = vertex_input_env && std::string_view{vertex_input_env} == "1"
                            ? "on (SPARTAN_VERTEX_INPUT_DYNAMIC=1)"
                            : "on";
} else if (std::find(disabled_extensions.begin(), disabled_extensions.end(),
                     VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME) !=
           disabled_extensions.end()) {
    vertex_input_line = "off (SPARTAN_VK_DISABLE_EXT)";
} else if (!vertex_input_wanted) {
    vertex_input_line =
        vertex_input_env && *vertex_input_env
            ? "off (SPARTAN_VERTEX_INPUT_DYNAMIC=0)"
            : "off (Intel Windows driver; SPARTAN_VERTEX_INPUT_DYNAMIC=1 turns it on)";
    // spartanport: add_extension never ran here, so look the extension up before calling it off.
    if (std::find(available_extensions.begin(), available_extensions.end(),
                  VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME) !=
        available_extensions.end()) {
        vertex_input_off_reason = vertex_input_env && *vertex_input_env
                                      ? "SPARTAN_VERTEX_INPUT_DYNAMIC=0"
                                      : "Intel Windows driver";
    }
} else {
    vertex_input_line = "unavailable (VK_EXT_vertex_input_dynamic_state missing)";
}
std::printf("GPU: vertex input dynamic state %s\n", vertex_input_line);
```
`vertex_input_off_reason` precisa viver até o Step 7: declarar no escopo de `CreateDevice`, não dentro de um bloco.

- [ ] **Step 6: DLSS e binários (bloco das linhas 505-513)**

No `for (const char* name : dlss_extensions)`: já habilitada → `continue`; `ext_policy.IsDisabled(name)` → se ainda não estiver em `disabled_extensions`, `LOG_WARNING(Render_Vulkan, "Extension {} disabled by SPARTAN_VK_DISABLE_EXT", name)` e `emplace_back`; `continue`; senão `push_back` se couber (como hoje). Logo depois do bloco DLSS, antes de `device_chain`:
```cpp
// spartanport: binary keys on disk were written under another extension set; while
// SPARTAN_VK_DISABLE_EXT has turned something off they may not match, so leave binaries out.
// Checked after the last optional request, the DLSS ones included, so that every disabled
// extension counts.
pipeline_binaries = pipeline_binaries && disabled_extensions.empty();
```

- [ ] **Step 7: resumo antes de `CreateAllocator()`**

```cpp
// spartanport: printed at any log level so that a user's console output shows what the device got.
std::vector<std::string> unavailable;
for (const std::string& name : optional_queried) {
    if (std::none_of(enabled_extensions.begin(), enabled_extensions.end(),
                     [&](const char* e) { return std::string_view{e} == name; })) {
        unavailable.push_back(name);
    }
}
// spartanport: the list printed also names the extension the vertex input default kept off
// (#861); disabled_extensions itself stays SPARTAN_VK_DISABLE_EXT only, as it gates the caches.
const std::vector<std::string> off_names = OffNamesForLog(
    disabled_extensions,
    vertex_input_off_reason.empty() ? std::string_view{}
                                    : VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
    vertex_input_off_reason);
std::printf("GPU: device %s, driver %s %s, Vulkan %s; extensions off: %s "
            "(SPARTAN_VK_DISABLE_EXT=%s)\n",
            properties.deviceName.data(), vk::to_string(driver_id).c_str(),
            GetDriverVersionName().c_str(), GetReadableVersion(properties.apiVersion).c_str(),
            JoinNames(off_names).c_str(), disable_ext_set ? disable_ext_env.c_str() : "unset");
std::printf("GPU: optional extensions unavailable: %s; ignored names: %s\n",
            JoinNames(unavailable).c_str(),
            JoinNames(IgnoredNames(ext_policy, disabled_extensions)).c_str());
```

- [ ] **Step 8: compilar o objeto**

Run: `source ./msys2-env.sh && ninja -C out/gpu OBJ_INSTANCE`
Expected: compila sem erro. Se `<cstdlib>`/`<algorithm>`/`<string_view>` faltarem, incluir.

- [ ] **Step 9: conferir contra a spec §4.5**

`grep -n "SPARTAN_VK_DISABLE_EXT\|SPARTAN_VERTEX_INPUT_DYNAMIC\|vertex input dynamic state\|extensions off\|ignored names" gpu/shadps4/video_core/renderer_vulkan/vk_instance.cpp` mostra as seis variantes da linha de vertex input, as duas de resumo e a de `disabled by`, todas com os textos exatos de Global Constraints. `grep -c "GOW3\|gow3" gpu/shadps4/video_core/renderer_vulkan/vk_instance.cpp gpu/shadps4/video_core/renderer_vulkan/vk_instance.h` dá 0.

- [ ] **Step 10: commit**

```bash
git add gpu/shadps4/video_core/renderer_vulkan/vk_instance.cpp gpu/shadps4/video_core/renderer_vulkan/vk_instance.h
git commit -F <mensagem da skill caveman-commit; intenção: "Fix(GPU): turn vertex input dynamic state off on Intel Windows", corpo: lê SPARTAN_VK_DISABLE_EXT e SPARTAN_VERTEX_INPUT_DYNAMIC em CreateDevice, imprime o modo e as extensões desligadas em qualquer nível de log, desliga os binários de pipeline quando algo foi desligado; Refs #861>
```

---

### Task 6: cache de disco fechado com `SPARTAN_VK_DISABLE_EXT`

**Files:**
- Modify: `gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_serialization.cpp:1014-1018` (`PipelineCache::CheckCacheProfile`)

**Interfaces:**
- Consumes: `Instance::GetDisabledExtensions()` (Task 5).

- [ ] **Step 1: o guard**

Logo depois do `if (!EmulatorSettings.IsPipelineCacheEnabled()) { return false; }`:
```cpp
// spartanport: extensions outside Shader::Profile (maintenance8, feedback loop, mixed samples, ...)
// shape pipeline keys too, so keys written under another extension set may be invalid here.
// Keep the disk cache closed for the session; Save checks IsOpened(), so nothing is written
// either.
if (!instance.GetDisabledExtensions().empty()) {
    LOG_WARNING(Render, "Pipeline cache skipped: SPARTAN_VK_DISABLE_EXT is active");
    return false;
}
```

- [ ] **Step 2: compilar o objeto**

Run: `source ./msys2-env.sh && ninja -C out/gpu OBJ_SERIAL`
Expected: compila sem erro.

- [ ] **Step 3: todos os testes**

Run: `ninja -C out/gpu extension-policy-test vertex-input-default-test vertex-format-test shader-profile-test no-msaa-test graphics-settings-test && for t in extension-policy vertex-input-default vertex-format shader-profile no-msaa graphics-settings; do out/gpu/$t-test.exe || exit 1; done`
Expected: seis linhas `...: ok` (o `graphics-settings-test` imprime o seu próprio texto), exit 0.

- [ ] **Step 4: commit**

```bash
git add gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_serialization.cpp
git commit -F <mensagem da skill caveman-commit; intenção: "Fix(GPU): skip the disk pipeline cache under SPARTAN_VK_DISABLE_EXT", corpo: chaves escritas com outro conjunto de extensões podem ser inválidas; Refs #861>
```

---

### Task 7: test build, smoke local e anúncio (sessão principal)

**Files:**
- Nenhum no repositório. GitHub: pre-release `windows-v2.8.0-intel-test1`, comentários, PR.

**Interfaces:**
- Consumes: branch `intel-vertex-input` com as tasks 1-6 commitadas; memórias `windows-zip-ci-workflow`, `local-game-smoke-runs`, `github-comments-bilingual`.

- [ ] **Step 1: push e CI**

Run (na worktree):
```bash
git push -u origin intel-vertex-input
gh workflow run windows-zip.yml -R FelipePrado0/SpartanPort --ref intel-vertex-input -f tag=windows-v2.8.0-intel-test1
gh run list -R FelipePrado0/SpartanPort --workflow windows-zip.yml --branch intel-vertex-input --limit 1
```
Esperar (~13 min; `gh run watch <id> -R FelipePrado0/SpartanPort`). Expected: run verde; `gh release view windows-v2.8.0-intel-test1 -R FelipePrado0/SpartanPort` lista o zip. CI vermelha: ler o log do job, corrigir na worktree (TDD onde couber), commit, push, rodar o workflow de novo com a mesma tag.

- [ ] **Step 2: zip local**

Run: `gh release download windows-v2.8.0-intel-test1 -R FelipePrado0/SpartanPort -D dist/spartanport-windows-2.8.0-intel-test1 --clobber` e extrair em `dist/spartanport-windows-2.8.0-intel-test1/spartanport-windows` (`dist/` é gitignored).

- [ ] **Step 3: smoke na GTX 1650, perguntando antes de cada run (abre janela, ~2 min)**

Rodar o jogo a partir do pacote extraído com `SPARTAN_GPU_LOG=info` e os três cenários da spec §9, lendo `user/last_run.log` de cada um:
1. default: `GPU: vertex input dynamic state on`; `extensions off: none`.
2. `SPARTAN_VERTEX_INPUT_DYNAMIC=0`: `off (SPARTAN_VERTEX_INPUT_DYNAMIC=0)`; `extensions off: VK_EXT_vertex_input_dynamic_state (SPARTAN_VERTEX_INPUT_DYNAMIC=0)`; na primeira vez a mensagem de cache de pipelines incompatível/reconstruindo; Kratos visível no menu; nenhum `SurfaceFormat: Assertion Failed`.
3. `SPARTAN_VK_DISABLE_EXT=VK_EXT_vertex_input_dynamic_state`: `off (SPARTAN_VK_DISABLE_EXT)`; `Pipeline cache skipped: SPARTAN_VK_DISABLE_EXT is active`; `ignored names: none`.
Variante do Review Focus 2, se der tempo: os dois knobs juntos (`SPARTAN_VK_DISABLE_EXT=VK_EXT_vertex_input_dynamic_state SPARTAN_VERTEX_INPUT_DYNAMIC=1`): `off (SPARTAN_VK_DISABLE_EXT)` e a extensão uma vez só em `extensions off`.
Qualquer divergência volta para a task dona do texto (5 ou 6).

- [ ] **Step 4: anúncio nos issues**

Comentário bilíngue (`**EN:**`, `---`, `**PT-BR:**`, sem menção a IA) em #861, #1055, #1029, #1041, #1035, #1043, #1018 e #1039, com: link do pre-release; que ele traz para a 2.8.0 o fix do test2 do HD 630; a linha esperada em `user/last_run.log` (`GPU: vertex input dynamic state off (Intel Windows driver; SPARTAN_VERTEX_INPUT_DYNAMIC=1 turns it on)`); que o cache de pipelines reconstrói sozinho na primeira execução; pedido de `user/last_run.log` e de "funcionou?"; que freeze ou `Device lost` depois do 3D correto é o #334. Depois `gh issue edit <n> -R FelipePrado0/SpartanPort --add-label awaiting-test` nos oito.

- [ ] **Step 5: PR e #214**

`gh pr create -R FelipePrado0/SpartanPort --base main --head intel-vertex-input --title "Fix(GPU): vertex input dynamic state off on Intel Windows (#861)"` com corpo bilíngue: o que muda (spec §4), os knobs, `Closes #861`, `Refs #1055, #1029, #1041, #1035, #1043, #1018, #1039`, link do pre-release, "merge só depois dos testers aprovarem". Comentário bilíngue no #214 apontando o PR novo e `gh pr close 214 -R FelipePrado0/SpartanPort`.

- [ ] **Step 6: memória e handoff**

Atualizar a memória `issue35-intel-test-pending` (branch, tag, PR novo, o que falta: aprovação dos testers). `superpowers:finishing-a-development-branch` só quando os testers aprovarem; a integração é decisão do usuário. Depois do merge: `git branch -D worktree-intel-hd630` e `git push origin --delete worktree-intel-hd630`.
