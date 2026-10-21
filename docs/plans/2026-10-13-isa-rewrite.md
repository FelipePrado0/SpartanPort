# Plano de implementação: reescrita de instruções Haswell (ISA rewrite)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** fazer o jogo rodar em CPUs com AVX mas sem MOVBE, BMI1 e LZCNT (Sandy/Ivy Bridge, AMD FX), reescrevendo no load cada sítio dessas instruções para um stub que a emula em C.

**Architecture:** um localizador em Python (`iced-x86`) decodifica a imagem ligada e grava uma tabela `G3ISA1`; o probe, quando o CPUID não tem os conjuntos (ou em modo forçado), verifica cada sítio, gera um stub de 256 bytes por sítio numa área contígua à imagem e troca a instrução por `jmp rel32` (ou `int3` em raros sítios de 4 bytes sem instrução seguinte realocável). O stub salva estado, chama `isa_emulate` (C puro, flags exatas) e volta.

**Tech Stack:** C11 (clang MSYS2 CLANG64 no Windows, GCC no Linux), Python 3 + `iced-x86` 1.21.0, unittest, CMake/Ninja (`gpu/CMakeLists.txt`) e `build.sh`.

**Spec:** `docs/specs/2026-10-13-isa-rewrite-design.md` (leia antes de cada tarefa; as seções citadas abaixo são dela).

## Global Constraints

- C: `-std=c11 -Wall -Wextra -Werror`; headers com guarda `SPARTAN_*_H`; comentários em inglês como o resto de `src/`.
- `src/isa_emulate.c` compila com `-mgeneral-regs-only` e não chama a libc (nem `memcpy`).
- Convenção de chamada do stub para C: macro `ABI` de `src/runtime.h` (`__attribute__((sysv_abi))`).
- Registro da tabela: 64 bytes; cabeçalho: 64 bytes; magic `G3ISA1\0\0` (spec 4.4). Stub: no máximo 256 bytes.
- Textos de log exatos (spec 4, 5.1, 5.2, 5.8): `CPU: missing ...`, `CPU: complete`, `ISA: ... sites (...)`, `ISA: rewrite table unavailable (...)`, `ISA: CPU lacks ...: N sites rewritten (...)`, `ISA: forced rewrite of all N sites (SPARTAN_ISA_REWRITE=1)`, `ISA: most executed sites:`.
- Código de saída 20 para toda linha `STOP:`.
- Numeração de registradores: a do x86 (0 rax ... 4 rsp ... 15 r15); `0xFF` = ausente.
- Commits: mensagem gerada com a skill `caveman-commit`, formato do projeto `Tipo(Escopo): descrição em português` (ex.: `Feat(Probe): ...`), sem menção a IA, sem `Co-Authored-By`.
- Testes Python: `python -m unittest discover -s tests -p "test_*.py"` (4 falhas + 1 erro pré-existentes em `test_packaged_vulkan` e `test_mods` no Windows; não regredir o resto).
- Build no worktree (memória `worktree-build-msys2`): `subst W: "<caminho do worktree>"`, depois no MSYS2 CLANG64 `cmake -S gpu -B out/w -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSPARTAN_PGO=off -DSPARTAN_LTO=OFF -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` a partir de `/w`, e `ninja -C out/w <alvo>`. Executáveis clang64 precisam de `PATH=/c/msys64/clang64/bin:$PATH` no Git Bash.

## Review Focus

Entradas que a spec implica e que ficariam sem teste; cada uma ganhou o teste indicado na tarefa dona.

1. Dois sítios adjacentes de 5 bytes: a volta do primeiro stub cai no `E9` do segundo e precisa seguir para o segundo stub (Tarefa 5, `test_adjacent_sites`).
2. Sítio `JMP_RELOCATE` com instrução realocada de 15 bytes: o stub não pode passar de 256 bytes; `isa_rewrite_apply` falha alto, não corrompe o vizinho (Tarefa 5, `test_largest_stub_fits`).
3. `int3` que não é nosso (breakpoint real): `isa_rewrite_trap` devolve 0 e o caminho fatal de hoje segue (Tarefa 6, `test_trap_unknown_address`).
4. Tabela de outra build (sha256 ou tamanho diferentes): nada é escrito e a mensagem `STOP` nomeia o motivo (Tarefa 5, `test_table_mismatch`; Tarefa 7, inspeção manual do log).
5. `SPARTAN_ISA_REWRITE=0` em CPU antiga: `run.py` não roda o localizador nem passa a flag, e o probe para no `STOP` (Tarefa 8, `test_rewrite_disabled`).

---

### Task 1: `cpu_check` em dois grupos e modo `--cpu-features`

**Files:**
- Modify: `src/cpu_check.h`, `src/cpu_check.c`
- Modify: `src/probe.c` (bloco `SPARTAN_CPU_CHECK` em `main`, logo após o parse das opções; e o `if (argc == 2 && !strcmp(argv[1], "--vulkan-only"))`)
- Test: `tests/test_cpu_check.c`, `tests/test_probe.py`

**Interfaces:**
- Produces (`src/cpu_check.h`):
  ```c
  enum { CPU_SET_MOVBE = 1u << 0, CPU_SET_BMI1 = 1u << 1, CPU_SET_LZCNT = 1u << 2, CPU_SET_F16C = 1u << 3 };
  /* Fatal sets the host lacks (SSE4.2, POPCNT, AVX), comma separated in out; returns how many. */
  int cpu_missing_fatal(CpuFeatureWords words, char *out, size_t size);
  /* Rewritable sets the host lacks (MOVBE, F16C, BMI1, LZCNT): CPU_SET_* mask; names in out. */
  unsigned cpu_missing_rewritable(CpuFeatureWords words, char *out, size_t size);
  ```
  `cpu_missing_features` é removida (os dois chamadores migram nesta tarefa). Ordem dos nomes: a da tabela atual (SSE4.2, POPCNT, MOVBE, AVX, F16C, BMI1, LZCNT), então Sandy Bridge imprime `MOVBE, F16C, BMI1, LZCNT`.

- [ ] **Passo 1: reescrever `tests/test_cpu_check.c` para a API nova**

Asserções (mesmas constantes `L1_*`, `L7_*`, `E1_*` do teste atual):
```c
/* Jaguar-class host */
assert(cpu_missing_fatal((CpuFeatureWords){LEAF1_ALL, L7_BMI1, E1_LZCNT}, out, sizeof out) == 0 && !strcmp(out, ""));
assert(cpu_missing_rewritable((CpuFeatureWords){LEAF1_ALL, L7_BMI1, E1_LZCNT}, out, sizeof out) == 0 && !strcmp(out, ""));
/* Sandy Bridge (#33) */
assert(cpu_missing_fatal((CpuFeatureWords){L1_SSE42 | L1_POPCNT | L1_AVX, 0, 0}, out, sizeof out) == 0);
assert(cpu_missing_rewritable((CpuFeatureWords){L1_SSE42 | L1_POPCNT | L1_AVX, 0, 0}, out, sizeof out)
       == (CPU_SET_MOVBE | CPU_SET_F16C | CPU_SET_BMI1 | CPU_SET_LZCNT));
assert(!strcmp(out, "MOVBE, F16C, BMI1, LZCNT"));
/* Ivy Bridge (#66) */
assert(cpu_missing_rewritable((CpuFeatureWords){L1_SSE42 | L1_POPCNT | L1_AVX | L1_F16C, 0, 0}, out, sizeof out)
       == (CPU_SET_MOVBE | CPU_SET_BMI1 | CPU_SET_LZCNT));
assert(!strcmp(out, "MOVBE, BMI1, LZCNT"));
/* Nehalem: no AVX, fatal */
assert(cpu_missing_fatal((CpuFeatureWords){L1_SSE42 | L1_POPCNT, 0, 0}, out, sizeof out) == 1 && !strcmp(out, "AVX"));
/* short buffer: cut, never overrun; count complete */
memset(out, 'x', sizeof out);
assert(cpu_missing_fatal((CpuFeatureWords){0, 0, 0}, out, 8) == 3 && strlen(out) < 8);
/* this machine (newer than 2013) */
assert(cpu_missing_fatal(cpu_feature_words(), out, sizeof out) == 0 && cpu_missing_rewritable(cpu_feature_words(), out, sizeof out) == 0);
```

- [ ] **Passo 2: compilar o teste e ver falhar**

Run: `clang -std=c11 -O2 -g -Wall -Wextra -Werror -I. -Isrc tests/test_cpu_check.c -o out/cpu-check-test`
Expected: erro de compilação `call to undeclared function 'cpu_missing_fatal'`.

- [ ] **Passo 3: implementar em `src/cpu_check.c`**

Separe a tabela `sets[]` em duas (fatal e reescrevível, cada entrada da reescrevível com seu bit `CPU_SET_*`); uma função estática comum formata os nomes e devolve o contador ou a máscara.

- [ ] **Passo 4: compilar e rodar o teste**

Run: mesmo comando do passo 2, depois `out/cpu-check-test`
Expected: `cpu-check-test ok`

- [ ] **Passo 5: probe: `--cpu-features` e checagem em dois grupos**

Em `main`, ao lado de `--vulkan-only`: `if (argc == 2 && !strcmp(argv[1], "--cpu-features"))` imprime `STOP: this CPU lacks instruction sets the game's code uses: %s (Intel Sandy Bridge 2011 or newer, or AMD FX or newer, is needed)` e devolve 20 quando `cpu_missing_fatal` > 0 (respeitando `SPARTAN_CPU_CHECK=0`: então imprime `CPU: complete`); senão `CPU: missing %s` ou `CPU: complete` e devolve 0. No bloco `SPARTAN_CPU_CHECK` existente: fatal → mesmo `STOP`, `return 20`; reescrevível → guardar em `static unsigned cpu_missing_sets` e imprimir `CPU: missing %s` (a Tarefa 7 usa a máscara). Atualize a linha `Usage:`.

- [ ] **Passo 6: teste Python do modo novo em `tests/test_probe.py`**

Na classe existente (`skipUnless(EXE.exists())`):
```python
def test_cpu_features_mode_reports_this_machine(self):
    result = subprocess.run([str(EXE.resolve()), '--cpu-features'], capture_output=True, text=True)
    self.assertEqual(result.returncode, 0)
    self.assertRegex(result.stdout, r'^CPU: (complete|missing [A-Z0-9., ]+)\n$')
```

- [ ] **Passo 7: build do probe e testes**

Run (MSYS2, ver Global Constraints): `ninja -C out/w spartanport-probe cpu-check-test && out/w/cpu-check-test.exe`; copie `out/w/spartanport-probe.exe` para `out/spartanport-probe.exe` (com as DLLs no PATH) e `python -m unittest discover -s tests -p "test_probe.py" -v` (o `.exe` em `out/` faz o `skipUnless` passar).
Expected: `cpu-check-test ok`; `test_cpu_features_mode_reports_this_machine ... ok`.

- [ ] **Passo 8: commit**

`git add src/cpu_check.c src/cpu_check.h src/probe.c tests/test_cpu_check.c tests/test_probe.py`; mensagem via `caveman-commit` (sugestão: `Feat(Probe): separar conjuntos fatais dos reescrevíveis no CPUID`).

---

### Task 2: localizador, decodificação e classificação de sítios

**Files:**
- Create: `scripts/isa_rewrite.py`
- Test: `tests/test_isa_rewrite.py`

**Interfaces:**
- Produces (`scripts/isa_rewrite.py`):
  ```python
  KIND_MOVBE_LOAD, KIND_MOVBE_STORE, KIND_LZCNT, KIND_TZCNT, KIND_ANDN, KIND_BEXTR, KIND_BLSI, KIND_BLSMSK, KIND_BLSR, KIND_VCVTPH2PS, KIND_VCVTPS2PH = range(1, 12)
  MODE_JMP, MODE_JMP_RELOCATE, MODE_INT3, MODE_UNSUPPORTED = range(4)
  NO_REG = 0xFF
  SET_OF_KIND = {KIND_MOVBE_LOAD: 'MOVBE', KIND_MOVBE_STORE: 'MOVBE', KIND_LZCNT: 'LZCNT', KIND_TZCNT: 'BMI1', KIND_ANDN: 'BMI1', ..., KIND_VCVTPS2PH: 'F16C'}

  @dataclass
  class Site:
      offset: int; length: int; kind: int; width: int; mode: int
      dst: int; src1: int; src2: int; mem_slot: int          # mem_slot: 0 none, 1 dst, 2 src
      base: int; index: int; scale: int; mem_flags: int     # mem_flags bit 0: RIP-relative, disp absolute
      disp: int; original: bytes; relocated: bytes

  def scan_segment(code: bytes, address: int) -> list[Site]
  ```
  `iced_x86` é importado em `try/except ImportError` no topo do módulo (`iced_x86 = None` quando ausente); `scan_segment` levanta `RuntimeError('iced_x86 is not installed')` nesse caso.

- [ ] **Passo 1: escrever os testes (falhando)**

`tests/test_isa_rewrite.py`, `unittest`, `from paths import ROOT`, `import isa_rewrite`, classe decorada com `@unittest.skipUnless(isa_rewrite.iced_x86, 'pip install iced-x86')`. Bytes em hexadecimal (todos verificados com o decodificador):

```python
BLSR_ECX = bytes.fromhex('c4e270f3c9')          # blsr ecx, ecx            (5)
MOVBE_STORE = bytes.fromhex('0f38f108')          # movbe [rax], ecx         (4)
BEXTR_RIP = bytes.fromhex('c4e270f7050c000000')  # bextr eax, [rip+0xc], ecx (9)
LZCNT_AX = bytes.fromhex('66f30fbdc1')           # lzcnt ax, cx             (5)
TZCNT_RAX = bytes.fromhex('f3480fbcc1')          # tzcnt rax, rcx           (5)
NOP, RET, JMP_SELF = b'\x90', b'\xc3', bytes.fromhex('ebfe')
MOV_RIP = bytes.fromhex('8b0500000000')          # mov eax, [rip+0]         (6)
MOV_FS = bytes.fromhex('648b042500000000')       # mov eax, fs:[0]          (8): not a site

def test_blsr_is_a_jmp_site(self):
    s, = isa_rewrite.scan_segment(BLSR_ECX, 0x1000)
    self.assertEqual((s.offset, s.length, s.kind, s.width, s.mode), (0x1000, 5, isa_rewrite.KIND_BLSR, 32, isa_rewrite.MODE_JMP))
    self.assertEqual((s.dst, s.src1, s.src2, s.mem_slot), (1, 1, isa_rewrite.NO_REG, 0))
    self.assertEqual((s.original, s.relocated), (BLSR_ECX, b''))

def test_bextr_rip_relative_memory_source(self):
    s, = isa_rewrite.scan_segment(BEXTR_RIP, 0x1000)
    self.assertEqual((s.kind, s.width, s.dst, s.src1, s.src2, s.mem_slot), (isa_rewrite.KIND_BEXTR, 32, 0, isa_rewrite.NO_REG, 1, 2))
    self.assertEqual((s.base, s.index, s.mem_flags, s.disp), (isa_rewrite.NO_REG, isa_rewrite.NO_REG, 1, 0x1000 + 9 + 0xc))

def test_sixteen_bit_lzcnt_and_sixty_four_bit_tzcnt(self):
    a, b = isa_rewrite.scan_segment(LZCNT_AX + TZCNT_RAX, 0)
    self.assertEqual((a.kind, a.width, a.length), (isa_rewrite.KIND_LZCNT, 16, 5))
    self.assertEqual((b.kind, b.width, b.dst, b.src1), (isa_rewrite.KIND_TZCNT, 64, 0, 1))

def test_four_byte_store_relocates_a_plain_next_instruction(self):
    s, = isa_rewrite.scan_segment(MOVBE_STORE + NOP + RET, 0)
    self.assertEqual((s.kind, s.mode, s.mem_slot, s.base, s.dst, s.src1), (isa_rewrite.KIND_MOVBE_STORE, isa_rewrite.MODE_JMP_RELOCATE, 1, 0, isa_rewrite.NO_REG, 1))
    self.assertEqual(s.relocated, NOP)

def test_four_byte_site_falls_back_to_int3(self):
    for tail in (JMP_SELF, MOV_RIP, BLSR_ECX, b''):       # branch, RIP-relative, another site, end of segment
        s = isa_rewrite.scan_segment(MOVBE_STORE + tail, 0)[0]
        self.assertEqual((s.mode, s.relocated), (isa_rewrite.MODE_INT3, b''), tail.hex())

def test_next_instruction_that_is_a_branch_target_blocks_relocation(self):
    code = bytes.fromhex('eb04') + MOVBE_STORE + NOP + RET   # jmp +4 lands on the nop
    s, = isa_rewrite.scan_segment(code, 0)
    self.assertEqual(s.mode, isa_rewrite.MODE_INT3)

def test_unsupported_encodings(self):
    for code, why in ((b'\x64' + BEXTR_RIP, 'segment prefix'), (b'\x67' + MOVBE_STORE, '32-bit address'),
                      (bytes.fromhex('c4e2791310'), 'vcvtph2ps xmm2, [rax]')):
        s, = isa_rewrite.scan_segment(code + RET, 0)
        self.assertEqual(s.mode, isa_rewrite.MODE_UNSUPPORTED, why)
    self.assertEqual(isa_rewrite.scan_segment(bytes.fromhex('c4e2791310') + RET, 0)[0].kind, isa_rewrite.KIND_VCVTPH2PS)

def test_non_sites_are_ignored(self):
    self.assertEqual(isa_rewrite.scan_segment(MOV_FS + RET, 0), [])
```

- [ ] **Passo 2: rodar e ver falhar**

Run: `python -m unittest discover -s tests -p "test_isa_rewrite.py" -v`
Expected: `ModuleNotFoundError: No module named 'isa_rewrite'` (ou skip se `iced_x86` não estiver no Python do repositório: instale `pip install iced-x86==1.21.0` nesse Python antes de seguir).

- [ ] **Passo 3: implementar `scan_segment`**

`Decoder(64, code, ip=address)` em uma passada. Para cada instrução guarde `targets.add(ins.near_branch_target)` quando `ins.flow_control` é `UNCONDITIONAL_BRANCH`, `CONDITIONAL_BRANCH` ou `CALL` e `ins.op0_kind == OpKind.NEAR_BRANCH64`. Mnemônico em `Mnemonic.MOVBE/LZCNT/TZCNT/ANDN/BEXTR/BLSI/BLSMSK/BLSR/VCVTPH2PS/VCVTPS2PH` cria um `Site` (MOVBE: load se `ins.op0_kind == OpKind.REGISTER`). Registradores: `RegisterExt.number(ins.op_register(i))`; largura: `RegisterExt.size(reg) * 8` do operando registrador (op1 no store). Memória: `ins.memory_base/memory_index/memory_index_scale/memory_displacement`; RIP-relativo (`ins.is_ip_rel_memory_operand`): `base = index = NO_REG`, `mem_flags = 1`, `disp = ins.ip_rel_memory_address`. `UNSUPPORTED`: `ins.segment_prefix != Register.NONE`, `ins.has_lock_prefix`, base ou índice com `RegisterExt.size != 8`, ou kind F16C. Sítios de 4 bytes ficam pendentes até a instrução seguinte: ela é "plana" se `flow_control == FlowControl.NEXT`, não `is_ip_rel_memory_operand`, não `has_rep_prefix`/`has_repne_prefix`, não é sítio, `ins.next_ip <= address + len(code)` e `ins.ip not in targets` ao fim da passada (a decisão sobre alvos é tomada depois da varredura completa, por isso guarde o candidato e resolva no fim). Sítio pendente sem instrução seguinte → `INT3`.

- [ ] **Passo 4: rodar os testes**

Run: `python -m unittest discover -s tests -p "test_isa_rewrite.py" -v`
Expected: 8 testes `ok`.

- [ ] **Passo 5: commit**

`git add scripts/isa_rewrite.py tests/test_isa_rewrite.py`; mensagem via `caveman-commit` (sugestão: `Feat(ISA): localizar sítios MOVBE, BMI1 e LZCNT na imagem`).

---

### Task 3: tabela `G3ISA1`, leitura do `boot-linked.bin` e linha de comando

**Files:**
- Modify: `scripts/isa_rewrite.py`
- Create: `tests/data/isa_fixture.bin`
- Test: `tests/test_isa_rewrite.py`

**Interfaces:**
- Produces (`scripts/isa_rewrite.py`):
  ```python
  HEADER = struct.Struct('<8sQ32sII8x')                 # magic, image size, sha256, sites, unsupported  (64 bytes)
  RECORD = struct.Struct('<QBBBBBBBBBBBB4xq15sB15sx')   # spec 4.4 (64 bytes)
  def read_boot_linked(data: bytes) -> tuple[list[tuple[int, int, int]], bytes]   # segments (address, size, flags), image
  def encode_table(image: bytes, sites: list[Site]) -> bytes
  def summary(sites: list[Site]) -> str
  def main(argv: list[str]) -> int                      # 0 ok, 1 = table unavailable
  def fixture_code() -> bytes                            # the code bytes behind tests/data/isa_fixture.bin
  ```
  `main`: `isa_rewrite.py <boot-linked.bin> --out <dir>`; grava `<dir>/isa-rewrite.bin`, imprime `summary`. Sem `iced_x86`, arquivo ausente ou formato inválido: imprime `ISA: rewrite table unavailable (<motivo>)` e devolve 1.

- [ ] **Passo 1: testes (falhando)**

```python
def boot_linked(segments):      # helper: fake G3PROBE5 as link_modules.py writes it, no modules/bindings/names/relocs
    image = b''.join(code.ljust(size, b'\0') for code, size, flags in segments)
    table = [(a, s, f) for a, (c, s, f) in zip(accumulate_addresses(segments), segments)]
    return (struct.pack('<8s6Q', b'G3PROBE5', len(image), 0, len(table), 0, 0, 1) + struct.pack('<Q', 0)
            + struct.pack('<4Q', 0, 0, 0, 0) + struct.pack('<Q', 0) + struct.pack('<Q', 0)
            + b''.join(struct.pack('<3Q', *t) for t in table) + image)

def test_read_boot_linked_returns_segments_and_image(self): ...   # two segments, one executable (flags 5), one data (flags 6)

def test_encode_table_header_and_record(self):
    sites = isa_rewrite.scan_segment(BLSR_ECX + MOVBE_STORE + NOP + RET, 0x2000)
    image = bytes(0x2000) + BLSR_ECX + MOVBE_STORE + NOP + RET
    table = isa_rewrite.encode_table(image, sites)
    magic, size, sha, count, unsupported = isa_rewrite.HEADER.unpack_from(table, 0)
    self.assertEqual((magic, size, sha, count, unsupported), (b'G3ISA1\0\0', len(image), hashlib.sha256(image).digest(), 2, 0))
    self.assertEqual(len(table), 64 + 2 * 64)
    offset, length, kind, width, mode, dst, src1, src2, mem_slot, base, index, scale, flags, disp, original, rl, relocated = isa_rewrite.RECORD.unpack_from(table, 64 + 64)
    self.assertEqual((offset, length, kind, mode, mem_slot, base, rl, relocated[:rl]), (0x2005, 4, isa_rewrite.KIND_MOVBE_STORE, isa_rewrite.MODE_JMP_RELOCATE, 1, 0, 1, NOP))
    self.assertEqual(original[:length], MOVBE_STORE)

def test_summary_line(self):
    sites = isa_rewrite.scan_segment(BLSR_ECX + MOVBE_STORE + NOP + bytes.fromhex('c4e2791310') + RET, 0)
    self.assertEqual(isa_rewrite.summary(sites), 'ISA: 3 sites (MOVBE 1, BMI1 1, LZCNT 0, F16C 1); 1 relocated, 0 int3, 1 unsupported')

def test_main_writes_table_for_executable_segments_only(self):   # tmp dir; data segment holds BLSR bytes too but is not scanned
def test_main_reports_unavailable_input(self):                     # missing file -> returncode 1, stdout startswith 'ISA: rewrite table unavailable ('
def test_fixture_file_is_current(self):
    code = isa_rewrite.fixture_code()
    expected = isa_rewrite.encode_table(code, isa_rewrite.scan_segment(code, 0))
    self.assertEqual((ROOT / 'tests/data/isa_fixture.bin').read_bytes(), expected)
```
`fixture_code()` devolve, nesta ordem: `BLSR_ECX`, `MOVBE_STORE + NOP`, `BEXTR_RIP`, `LZCNT_AX`, `TZCNT_RAX`, `MOVBE_STORE + JMP_SELF` (vira `INT3`), `RET`. Esse arquivo é o que o teste C da Tarefa 5 carrega.

- [ ] **Passo 2: rodar e ver falhar**

Run: `python -m unittest discover -s tests -p "test_isa_rewrite.py" -v`
Expected: `AttributeError: module 'isa_rewrite' has no attribute 'HEADER'`.

- [ ] **Passo 3: implementar leitura, codificação, resumo e `main`**

`read_boot_linked`: use `unpack`/`span` de `prepare.py` e a ordem do escritor em `link_modules.py` (cabeçalho `<8s6Q`, procparam `Q`, TLS `4Q`, `nmodules` + `7Q` cada, `nbindings` + `3Q` cada, `ns` segmentos `3Q`, `ni` nomes de 128 bytes, `nr` relocações `QQqq`, imagem até o fim). Exija magic `G3PROBE5`. `summary`: contagens por conjunto via `SET_OF_KIND`, `relocated` = modo 1, `int3` = modo 2, `unsupported` = modo 3. Gere `tests/data/isa_fixture.bin` com `python -c "import isa_rewrite as r; c = r.fixture_code(); open('tests/data/isa_fixture.bin', 'wb').write(r.encode_table(c, r.scan_segment(c, 0)))"` a partir de `scripts/`.

- [ ] **Passo 4: rodar os testes**

Run: `python -m unittest discover -s tests -p "test_isa_rewrite.py" -v`
Expected: todos `ok` (14 testes).

- [ ] **Passo 5: commit**

`git add scripts/isa_rewrite.py tests/test_isa_rewrite.py tests/data/isa_fixture.bin`; mensagem via `caveman-commit` (sugestão: `Feat(ISA): gravar a tabela G3ISA1 a partir do boot-linked.bin`).

---

### Task 4: emulador `isa_emulate` com oráculo nativo

**Files:**
- Create: `src/isa_emulate.h`, `src/isa_emulate.c`
- Test: `tests/test_isa_emulate.c`
- Modify: `gpu/CMakeLists.txt` (bloco `if (WIN32)`, junto de `cpu-check-test`), `build.sh` (bloco `--test`)

**Interfaces:**
- Produces (`src/isa_emulate.h`):
  ```c
  #include <stdint.h>
  #include "runtime.h"   /* ABI */
  enum { ISA_MOVBE_LOAD = 1, ISA_MOVBE_STORE, ISA_LZCNT, ISA_TZCNT, ISA_ANDN, ISA_BEXTR, ISA_BLSI, ISA_BLSMSK, ISA_BLSR, ISA_VCVTPH2PS, ISA_VCVTPS2PH };
  enum { ISA_MODE_JMP = 0, ISA_MODE_JMP_RELOCATE, ISA_MODE_INT3, ISA_MODE_UNSUPPORTED };
  enum { ISA_NO_REG = 0xff, ISA_MEM_NONE = 0, ISA_MEM_DST = 1, ISA_MEM_SRC = 2, ISA_MEM_RIP_RELATIVE = 1 };
  typedef struct {                       /* one G3ISA1 record (spec 4.4), as stored in the file */
      uint64_t offset;
      uint8_t length, kind, width, mode, dst, src1, src2, mem_slot, base, index, scale, mem_flags;
      uint8_t reserved[4];
      int64_t disp;
      uint8_t original[15];
      uint8_t relocated_length;
      uint8_t relocated[15];
      uint8_t reserved2;
  } IsaSite;
  _Static_assert(sizeof(IsaSite) == 64, "G3ISA1 record");
  typedef struct { uint64_t rflags; uint64_t gpr[16]; } IsaRegs;   /* gpr: x86 numbering; gpr[4] = guest rsp */
  enum { ISA_CF = 1u << 0, ISA_ZF = 1u << 6, ISA_SF = 1u << 7, ISA_OF = 1u << 11 };
  /* Memory operands with mem_flags & ISA_MEM_RIP_RELATIVE address image + disp. */
  void isa_emulate_set_image(unsigned char *image);
  /* Executes the site's instruction on regs (spec 5.6). */
  void ABI isa_emulate(const IsaSite *site, IsaRegs *regs);
  /* Flags the instruction defines (the test compares only these). */
  uint64_t isa_defined_flags(uint8_t kind);
  ```

- [ ] **Passo 1: escrever `tests/test_isa_emulate.c` (falhando)**

Compilado com `-mbmi -mlzcnt -mmovbe`. Pula com `puts("isa-emulate-test skipped: CPU lacks BMI1, LZCNT or MOVBE")` e `return 0` se `!__builtin_cpu_supports("bmi") || !__builtin_cpu_supports("lzcnt") || !__builtin_cpu_supports("movbe")`. Oráculo: macro que executa a instrução nativa via inline asm e captura flags com `pushfq; pop`. Para cada largura em {16, 32, 64} e cada kind de registrador (LZCNT, TZCNT, ANDN, BEXTR, BLSI, BLSMSK, BLSR), com `dst = 0 (rax)`, `src1 = 1 (rcx)`, `src2 = 2 (rdx)`:
```c
static const uint64_t EDGES[] = {0, 1, 2, 0x7f, 0x80, 0xff, 0x7fff, 0x8000, 0xffff, 0x7fffffff, 0x80000000, 0xffffffff,
                                 0x7fffffffffffffff, 0x8000000000000000, 0xffffffffffffffff};
/* ctrl for BEXTR: start and len in {0, 1, 7, 8, 15, 16, 31, 32, 63, 64, 255} crossed */
```
mais 20 000 entradas de um xorshift64 por caso. Asserções por entrada: `regs.gpr[0]` igual ao nativo (para 16 bits o nativo e o emulado partem de `rax = 0x1122334455667788` e o valor inteiro de `rax` é comparado: bits altos preservados; para 32 bits o resultado zera os 32 altos) e `(regs.rflags ^ native_flags) & isa_defined_flags(kind) == 0` com `regs.rflags` inicial alternando `0` e `0xcd5` (todas as flags definidas ligadas) para pegar flag não escrita. Memória: `MOVBE_LOAD`/`MOVBE_STORE` nas três larguras com `base = 1 (rcx)` apontando para um buffer, `disp = 8`, e `BEXTR` 32 bits com `mem_slot = 2`, `mem_flags = 1`, `disp` = offset do buffer dado por `isa_emulate_set_image`. Falha imprime kind, largura, entrada, esperado e obtido.

- [ ] **Passo 2: alvos de build**

`gpu/CMakeLists.txt`: `add_executable(isa-emulate-test EXCLUDE_FROM_ALL ../tests/test_isa_emulate.c ../src/isa_emulate.c)` com as mesmas propriedades do `cpu-check-test` mais `set_source_files_properties(../src/isa_emulate.c PROPERTIES COMPILE_OPTIONS "-mgeneral-regs-only")` e `set_source_files_properties(../tests/test_isa_emulate.c PROPERTIES COMPILE_OPTIONS "-mbmi;-mlzcnt;-mmovbe")`. `build.sh --test`: `"$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -mgeneral-regs-only -c src/isa_emulate.c -o out/isa_emulate.o` e `"$CC" ... -mbmi -mlzcnt -mmovbe -I. -Isrc tests/test_isa_emulate.c out/isa_emulate.o -o out/isa-emulate-test && out/isa-emulate-test`.

- [ ] **Passo 3: compilar e ver falhar**

Run: `ninja -C out/w isa-emulate-test`
Expected: erro `'isa_emulate.h' file not found`.

- [ ] **Passo 4: implementar `src/isa_emulate.c`**

Funções estáticas `read_operand(site, regs, which)`, `write_dst(site, regs, value)` (regras de largura da spec 5.6), `effective_address(site, regs)`, `set_flags(regs, mask, value)`. Sem libc: leituras e escritas de memória com ponteiros `uint16_t*/uint32_t*/uint64_t*` (acessos desalinhados são válidos em x86). `bsr`/`bsf` com `__builtin_clzll`/`__builtin_ctzll` protegidos do zero. BEXTR: `start > width - 1 → 0`; `len` saturado em `width - start`. Kinds F16C: nada (nunca aplicados). `isa_defined_flags`: ANDN/BEXTR/BLSI/BLSMSK/BLSR → `ISA_SF|ISA_ZF|ISA_CF|ISA_OF` (BEXTR: `ISA_ZF|ISA_CF|ISA_OF`), LZCNT/TZCNT → `ISA_CF|ISA_ZF`, MOVBE → 0.

- [ ] **Passo 5: compilar e rodar**

Run: `ninja -C out/w isa-emulate-test && PATH=/c/msys64/clang64/bin:$PATH out/w/isa-emulate-test.exe`
Expected: `isa-emulate-test ok`.

- [ ] **Passo 6: commit**

`git add src/isa_emulate.c src/isa_emulate.h tests/test_isa_emulate.c gpu/CMakeLists.txt build.sh`; mensagem via `caveman-commit` (sugestão: `Feat(ISA): emular MOVBE, BMI1 e LZCNT em C com flags exatas`).

---

### Task 5: tabela, stubs e aplicação (`isa_rewrite.c`)

**Files:**
- Create: `src/isa_rewrite.h`, `src/isa_rewrite.c`
- Test: `tests/test_isa_stub.c`
- Modify: `gpu/CMakeLists.txt`, `build.sh` (alvo `isa-stub-test`: `tests/test_isa_stub.c ../src/isa_rewrite.c ../src/isa_emulate.c`, mesmas flags da Tarefa 4; Windows liga `bcrypt -pthread` como os outros)

**Interfaces:**
- Consumes: `IsaSite`, `IsaRegs`, `isa_emulate`, `isa_emulate_set_image` (Tarefa 4); `CPU_SET_*` (Tarefa 1).
- Produces (`src/isa_rewrite.h`):
  ```c
  #include "isa_emulate.h"
  #include "cpu_check.h"
  typedef struct {
      IsaSite *sites; uint64_t *hits;      /* hits[i] belongs to sites[i] */
      uint32_t count, unsupported;
      uint64_t image_size; uint8_t sha256[32];
      unsigned char *image, *stubs;        /* set by isa_rewrite_apply */
  } IsaTable;
  typedef struct { uint32_t rewritten, jmp, relocated, int3, skipped, unsupported_needed; } IsaReport;
  enum { ISA_STUB_BYTES = 256 };
  /* Parses a G3ISA1 file (spec 4.4). NULL and one stderr line ("ISA: <reason>") when invalid. */
  IsaTable *isa_table_load(const char *path);
  /* 0 when size and sha256 match the image. */
  int isa_table_matches(const IsaTable *t, const unsigned char *image, uint64_t size);
  size_t isa_stub_area_size(const IsaTable *t);           /* count * ISA_STUB_BYTES */
  unsigned isa_site_set(const IsaSite *s);                 /* CPU_SET_* of the kind */
  /* Rewrites the sites whose set is in `sets` (spec 5.4). Writes nothing and returns
   * unsupported_needed > 0 when a needed site is UNSUPPORTED. `stubs` has isa_stub_area_size bytes, RW. */
  IsaReport isa_rewrite_apply(IsaTable *t, unsigned char *image, unsigned char *stubs, unsigned sets);
  ```

- [ ] **Passo 1: escrever `tests/test_isa_stub.c` (falhando)**

O teste aloca uma região RWX (`VirtualAlloc(..., PAGE_EXECUTE_READWRITE)` / `mmap(PROT_READ|PROT_WRITE|PROT_EXEC)`) de 3 páginas: página 0 = "imagem" com trechos, páginas 1-2 = área de stubs. Trechos são funções SysV (`typedef uint64_t (ABI *Snippet)(uint64_t in, uint64_t *out)`), escritos byte a byte:
```c
/* S1 at 0x000: mov qword [rsp-8], 0xdeadbeef (sign-extended) ; mov ecx, edi ; blsr ecx, ecx ; mov rdx, [rsp-8] ;
 *              mov [rsi+8], rdx ; pushfq ; pop qword [rsi] ; mov eax, ecx ; ret     -> site blsr (5 bytes, JMP)
 *              (the red-zone slot is read before pushfq overwrites it; mov never touches flags) */
/* S2 at 0x040: mov ecx, esi ; movbe [rdi], ecx ; mov eax, ecx ; ret                 -> site movbe store (4 bytes) + relocated `mov eax, ecx` (89 c8) */
/* S3 at 0x080: mov ecx, edi ; blsr ecx, ecx ; blsr ecx, ecx ; mov eax, ecx ; ret     -> two adjacent JMP sites */
/* S4 at 0x0c0: lzcnt eax, edi ; ret                                                 -> 4-byte site, mode INT3 (Task 6 runs it) */
```
Tabela montada à mão (`IsaSite` por trecho, `original` copiado dos bytes). Testes (`assert`):
- `test_jmp_site`: aplica com `sets = 0xffffffff`; chama S1 com `in = 0xf0`: devolve `_blsr_u32(0xf0) == 0xe0`, flags em `out[0]` com `(out[0] ^ native) & isa_defined_flags(ISA_BLSR) == 0` para `in` em `{0, 1, 0xf0, 0x80000000}`, `out[1] == 0xffffffffdeadbeef` (red zone intacta); os primeiros 5 bytes do sítio são `E9 rel32` e `stubs + 0*256` começa com `48 8D A4 24 00 FF FF FF`.
- `test_relocated_site`: S2 com `in = 0x11223344` escreve `0x44332211` no buffer e devolve `0x11223344` (a instrução realocada rodou); bytes do sítio: `E9 rel32` + `90`.
- `test_adjacent_sites`: S3 com `in = 0xf0` devolve `0xc0`.
- `test_xmm_preserved`: preenche xmm0-15 com padrões via `vmovdqu`/`movdqu`, chama S1, lê de volta: iguais.
- `test_sets_filter`: nova aplicação numa cópia da imagem com `sets = CPU_SET_LZCNT`: só S4 muda (`report.rewritten == 1`, S1 intacto).
- `test_skipped_when_bytes_differ`: altera um byte de S1 antes de aplicar: `report.skipped == 1`, bytes do sítio inalterados.
- `test_table_mismatch`: `isa_table_matches` com outro tamanho ou outro sha256 → `!= 0`.
- `test_unsupported_blocks_apply`: tabela com um sítio `ISA_MODE_UNSUPPORTED` de kind ANDN: `report.unsupported_needed == 1`, imagem byte a byte igual à de antes.
- `test_largest_stub_fits`: sítio `JMP_RELOCATE` com `relocated_length = 15` (15 `90`): aplica sem falha e `stub` termina dentro de 256 bytes (o byte 255 do stub é 0).
- `test_table_load_fixture`: `isa_table_load("tests/data/isa_fixture.bin")` (caminho relativo à raiz, passado por `argv[1]`): `count == 6`, `unsupported == 0`, `sites[0].kind == ISA_BLSR`, `sites[1].mode == ISA_MODE_JMP_RELOCATE`, `sites[5].mode == ISA_MODE_INT3`, `isa_stub_area_size == 6 * 256`.

- [ ] **Passo 2: compilar e ver falhar**

Run: `ninja -C out/w isa-stub-test`
Expected: `'isa_rewrite.h' file not found`.

- [ ] **Passo 3: implementar `src/isa_rewrite.c`**

`isa_table_load`: `fread` do cabeçalho, valida magic e `count <= 1u << 20`, aloca `sites` e `hits` (`calloc`). `isa_rewrite_apply`: passada 1 conta `unsupported_needed` (modo 3 e `isa_site_set` em `sets`) e devolve cedo se > 0; passada 2 por sítio selecionado: `memcmp(image + offset, original, length)` → `skipped`; gera o stub em `stubs + i * ISA_STUB_BYTES` com uma função `emit_stub(unsigned char *stub, const IsaSite *s, uint64_t *hits, unsigned char *image)` que escreve o pseudo-asm da spec 5.5 e devolve o tamanho (`fail` se > 256; use um `assert` interno mais retorno de erro); depois escreve os bytes do sítio conforme o modo (spec 4.3). Encodings fixos, obtidos com `llvm-mc --show-encoding -x86-asm-syntax=intel` (em `C:/msys64/clang64/bin`), por exemplo: `lea rsp,[rsp-0x100]` = `48 8D A4 24 00 FF FF FF`; `pushfq` = `9C`; `popfq` = `9D`; `mov [rsp+disp8], rax` = `48 89 44 24 disp8` (REX.R `4C` para r8-r15, campo reg no modrm); `mov rbp, rsp` = `48 89 E5`; `and rsp, -16` = `48 83 E4 F0`; `mov rax, imm64` = `48 B8 imm64` (`48 BF` rdi, `48 BE` rsi); `inc qword [rax]` = `48 FF 00`; `call rax` = `FF D0`; `jmp rel32` = `E9 rel32`. Ordem de salvamento: `lea rsp,[rsp-0x100]`, 15 `mov [rsp+8*i], reg` (i ≠ 4), `lea rax,[rsp+0x100]`, `mov [rsp+0x20], rax`, `pushfq` (então `IsaRegs* == rsp`), contador, `mov rbp, rsp`, `and rsp, -16`, `mov rdi, site`, `mov rsi, rbp`, `mov rax, isa_emulate`, `call rax`, `mov rsp, rbp`, `popfq`, 15 `mov reg, [rsp+8*i]`, `lea rsp,[rsp+0x100]`, instrução realocada, `jmp` para `image + offset + length` (modo 0) ou `image + offset + 4 + relocated_length` (modo 1). `rel32 = destino - (endereço do byte seguinte ao jmp)`, verificado em `int32_t`.

- [ ] **Passo 4: compilar e rodar**

Run: `ninja -C out/w isa-stub-test && PATH=/c/msys64/clang64/bin:$PATH out/w/isa-stub-test.exe tests/data/isa_fixture.bin`
Expected: `isa-stub-test ok` (o teste de S4/INT3 só na Tarefa 6).

- [ ] **Passo 5: commit**

`git add src/isa_rewrite.c src/isa_rewrite.h tests/test_isa_stub.c gpu/CMakeLists.txt build.sh`; mensagem via `caveman-commit` (sugestão: `Feat(ISA): gerar stubs e reescrever os sítios da tabela`).

---

### Task 6: trap `int3`, relatório de execuções e mapeamento de stubs

**Files:**
- Modify: `src/isa_rewrite.h`, `src/isa_rewrite.c`
- Test: `tests/test_isa_stub.c`

**Interfaces:**
- Produces (`src/isa_rewrite.h`):
  ```c
  /* int3 at `address` (the CC byte): 1 when it is a rewritten INT3 site; regs updated, *next_rip = site + 4. */
  int isa_rewrite_trap(IsaTable *t, uintptr_t address, IsaRegs *regs, uintptr_t *next_rip);
  /* Site index whose stub holds rip, or -1. */
  int isa_rewrite_site_of_stub(const IsaTable *t, uintptr_t rip);
  /* "ISA: most executed sites:" then up to 10 lines "  0x<offset> <mnemonic> <hits>" (hits > 0 only). */
  void isa_rewrite_report(const IsaTable *t, FILE *out);
  const char *isa_kind_name(uint8_t kind);   /* "movbe", "lzcnt", "tzcnt", "andn", "bextr", "blsi", "blsmsk", "blsr", "vcvtph2ps", "vcvtps2ph" */
  ```

- [ ] **Passo 1: testes (falhando)**

- `test_int3_site_via_handler`: o teste instala seu próprio handler (Windows: `AddVectoredExceptionHandler` tratando `EXCEPTION_BREAKPOINT`: monta `IsaRegs` do `CONTEXT` (`Rax..R15`, `EFlags`), chama `isa_rewrite_trap(t, (uintptr_t)record->ExceptionAddress, &regs, &rip)`, copia de volta e `Rip = rip`, `EXCEPTION_CONTINUE_EXECUTION`; Linux: `sigaction(SIGTRAP)` com `REG_RIP - 1`). Chama S4 com `in = 0x1000`: devolve `_lzcnt_u32(0x1000) == 19`; o `hits` do índice de S4 na tabela (o quinto sítio: S3 tem dois) vale 1 após uma chamada.
- `test_trap_unknown_address`: `isa_rewrite_trap(t, (uintptr_t)image + 0x3ff, ...)` devolve 0 e não altera `regs`.
- `test_site_of_stub`: `isa_rewrite_site_of_stub(t, (uintptr_t)stubs + 256 + 10) == 1`; fora da área → -1.
- `test_report_top_sites`: tabela própria com quatro sítios nos offsets `0x10` (blsr), `0x20` (movbe), `0x30` (lzcnt), `0x40` (bextr) e `hits = {5, 0, 7, 1}`; escreve em `tmpfile()`, lê de volta: `ISA: most executed sites:\n  0x30 lzcnt 7\n  0x10 blsr 5\n  0x40 bextr 1\n` e nada mais (sítio com 0 execuções fica de fora).

- [ ] **Passo 2: compilar e ver falhar**

Run: `ninja -C out/w isa-stub-test`
Expected: `call to undeclared function 'isa_rewrite_trap'`.

- [ ] **Passo 3: implementar**

`isa_rewrite_trap`: busca binária por `offset == address - image` entre os sítios com modo `ISA_MODE_INT3` e byte `CC` presente (aplicados), `isa_emulate`, `hits[i]++`, `*next_rip = address + 4`. Relatório: ordenação parcial dos 10 maiores sem alocar.

- [ ] **Passo 4: compilar e rodar**

Run: `ninja -C out/w isa-stub-test && PATH=/c/msys64/clang64/bin:$PATH out/w/isa-stub-test.exe tests/data/isa_fixture.bin`
Expected: `isa-stub-test ok`.

- [ ] **Passo 5: commit**

`git add src/isa_rewrite.c src/isa_rewrite.h tests/test_isa_stub.c`; mensagem via `caveman-commit` (sugestão: `Feat(ISA): atender int3 dos sítios restantes e relatar os mais executados`).

---

### Task 7: integração no probe

**Files:**
- Modify: `src/probe.c` (parse das opções em `main`; `allocate(round_page(size))` da imagem; bloco após `patch_tcb_loads` e antes do primeiro `protect`; `vectored_fault`, `fatal_exception`, `describe_address` no Windows; `fault()` e a instalação de `sigaction` no Linux), `src/runtime.h`, `src/runtime.c` (`runtime_report`)
- Modify: `build.sh` (linha do `spartanport-probe`: `out/isa_emulate.o` compilado com `-mgeneral-regs-only` e `src/isa_rewrite.c`), `gpu/CMakeLists.txt` (`add_executable(spartanport-probe ...)` ganha `../src/isa_rewrite.c ../src/isa_emulate.c` com a `COMPILE_OPTIONS` do `isa_emulate.c`)

**Interfaces:**
- Consumes: `IsaTable`, `isa_table_load`, `isa_table_matches`, `isa_stub_area_size`, `isa_rewrite_apply`, `isa_rewrite_trap`, `isa_rewrite_site_of_stub`, `isa_rewrite_report`, `isa_emulate_set_image`; `cpu_missing_sets` da Tarefa 1.
- Produces: `src/runtime.h`: `void runtime_set_report_hook(void (*hook)(void));` chamado dentro de `runtime_report()` quando definido (o probe registra uma função que chama `isa_rewrite_report(table, stdout)`).

Sem teste unitário novo: a verificação é o probe construído, o `isa-stub-test` e a rodada ponta a ponta da Tarefa 10.

- [ ] **Passo 1: opção e política**

`--isa-rewrite <file>` no loop de opções (`isa_file`), na linha `Usage:`. Após o bloco de CPU: `const char *policy = getenv("SPARTAN_ISA_REWRITE")`; `unsigned isa_sets = policy && !strcmp(policy, "1") ? 0xffffffffu : policy && !strcmp(policy, "0") ? 0 : cpu_missing_sets;`. Se `cpu_missing_sets && !isa_sets` (política `0`) ou `isa_sets && cpu_missing_sets && !isa_file` → `printf("STOP: this CPU lacks %s and no rewrite table matches the image\n", missing)`, `return 20`.

- [ ] **Passo 2: carregar a tabela e reservar a área de stubs**

Declare no topo de `probe.c`, junto de `image`: `static IsaTable *isa_table;` (os handlers e o hook de relatório leem daqui) e `static void isa_report_hook(void) { isa_rewrite_report(isa_table, stdout); }`. Antes de `image = allocate(round_page(size))`: `isa_table = isa_sets && isa_file ? isa_table_load(isa_file) : NULL;` (`NULL` com `cpu_missing_sets` → mesmo `STOP` do passo 1). `size_t stub_area = isa_table ? round_page(isa_stub_area_size(isa_table)) : 0; image = allocate(round_page(size) + stub_area); unsigned char *stubs = image + round_page(size);`. Após o `fread` da imagem: `if (isa_table && isa_table_matches(isa_table, image, size))` → `STOP` idem, `return 20`. No passo 3, leia `isa` como `isa_table`.

- [ ] **Passo 3: aplicar**

Logo após a linha `Guest TCB loads redirected` (dentro do `#ifdef _WIN32`, mas a chamada fica fora dele) e antes dos `protect`:
```c
if (isa) {
    isa_emulate_set_image(image);
    IsaReport r = isa_rewrite_apply(isa, image, stubs, isa_sets);
    if (r.unsupported_needed) { printf("STOP: this CPU lacks %s and %u sites use encodings the rewrite cannot emulate\n", missing, r.unsupported_needed); return 20; }
    if (isa_sets == 0xffffffffu) printf("ISA: forced rewrite of all %u sites (SPARTAN_ISA_REWRITE=1)\n", r.rewritten);
    else printf("ISA: CPU lacks %s: %u sites rewritten (%u jmp, %u relocated, %u int3), %u skipped\n", missing, r.rewritten, r.jmp, r.relocated, r.int3, r.skipped);
    runtime_set_report_hook(isa_report_hook);
    protect(stubs, stub_area, 5);
}
```
`missing` é o buffer de nomes de `cpu_missing_rewritable` (em modo forçado com CPU completa imprima a linha `forced`). Os `protect` da imagem seguem como estão.

- [ ] **Passo 4: handlers**

Windows, em `vectored_fault` antes do bloco de falhas fatais: `if (code == EXCEPTION_BREAKPOINT && isa_table)` → monta `IsaRegs` do `CONTEXT` (`Rax, Rcx, Rdx, Rbx, Rsp, Rbp, Rsi, Rdi, R8..R15`, `EFlags`), `isa_rewrite_trap(isa_table, (uintptr_t)record->ExceptionAddress, &regs, &rip)`; se 1, copia de volta, `Rip = rip`, `EXCEPTION_CONTINUE_EXECUTION`. Linux: `sigaction(SIGTRAP, &sa, NULL)` junto dos outros; em `fault()`, `sig == SIGTRAP && isa_table` → `IsaRegs` de `uc_mcontext.gregs[REG_RAX...]` e `REG_EFL`, `address = REG_RIP - 1`; se 1, copia de volta, `REG_RIP = rip`, `return`. Em `describe_address` (Windows) e no `snprintf` de "Guest fault" (Linux): se `isa_rewrite_site_of_stub(isa_table, rip) >= 0`, acrescente `While emulating site 0x<offset> (<isa_kind_name>)`. `fatal_exception` e `fault()` chamam `isa_rewrite_report(isa_table, stderr)` antes de terminar quando `isa_table`.

- [ ] **Passo 5: build e testes**

Run: `ninja -C out/w spartanport-probe isa-stub-test isa-emulate-test cpu-check-test` e os três testes; `python -m unittest discover -s tests -p "test_*.py"`.
Expected: build sem avisos; `isa-stub-test ok`, `isa-emulate-test ok`, `cpu-check-test ok`; Python sem regressão (só as 5 falhas pré-existentes).

- [ ] **Passo 6: commit**

`git add src/probe.c src/runtime.c src/runtime.h build.sh gpu/CMakeLists.txt`; mensagem via `caveman-commit` (sugestão: `Feat(Probe): aplicar a reescrita ISA quando a CPU não tem os conjuntos`).

---

### Task 8: `run.py` chama `--cpu-features` e o localizador

**Files:**
- Modify: `run.py` (`main`, antes de `run_script('prepare.py', ...)` e após `run_script('link_modules.py', ...)`; montagem de `command`)
- Test: `tests/test_run_isa.py`

**Interfaces:**
- Produces (`run.py`, funções puras no nível do módulo):
  ```python
  def parse_cpu_features(output: str) -> list[str]     # 'CPU: missing MOVBE, BMI1, LZCNT\n' -> ['MOVBE', 'BMI1', 'LZCNT']; 'CPU: complete' -> []
  def isa_rewrite_wanted(missing: list[str], env: Mapping[str, str]) -> bool   # env SPARTAN_ISA_REWRITE: '1' -> True, '0' -> False, else bool(missing)
  ```

- [ ] **Passo 1: testes (falhando)**

`tests/test_run_isa.py` (`sys.path.insert(0, str(ROOT))`, `import run`):
```python
def test_parse_cpu_features(self):
    self.assertEqual(run.parse_cpu_features('CPU: missing MOVBE, BMI1, LZCNT\n'), ['MOVBE', 'BMI1', 'LZCNT'])
    self.assertEqual(run.parse_cpu_features('CPU: complete\n'), [])
def test_rewrite_wanted_follows_cpu_by_default(self):
    self.assertTrue(run.isa_rewrite_wanted(['MOVBE'], {}))
    self.assertFalse(run.isa_rewrite_wanted([], {}))
def test_rewrite_forced(self):
    self.assertTrue(run.isa_rewrite_wanted([], {'SPARTAN_ISA_REWRITE': '1'}))
def test_rewrite_disabled(self):
    self.assertFalse(run.isa_rewrite_wanted(['MOVBE', 'BMI1'], {'SPARTAN_ISA_REWRITE': '0'}))
```

- [ ] **Passo 2: rodar e ver falhar**

Run: `python -m unittest discover -s tests -p "test_run_isa.py" -v`
Expected: `AttributeError: module 'run' has no attribute 'parse_cpu_features'`.

- [ ] **Passo 3: implementar**

Em `main`, depois de resolver `probe`: `features = subprocess.run([str(probe), '--cpu-features'], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, text=True, creationflags=no_console())`; `returncode == 20` → `print(features.stdout, end='', flush=True)` e `sys.exit(20)`; `missing = parse_cpu_features(features.stdout)`. Depois de `link_modules.py`: `isa_args = []`; se `isa_rewrite_wanted(missing, env)`: `result = subprocess.run([... run_script-style command for 'isa_rewrite.py', out / 'boot-linked.bin', '--out', out], stdout=PIPE, text=True, ...)`; imprime `result.stdout`; `returncode == 0` → `isa_args = ['--isa-rewrite', str(out / 'isa-rewrite.bin')]`. (Reaproveite `run_script` com um parâmetro `check=False` que devolve `(returncode, stdout)` em vez de `sys.exit`.) Acrescente `*isa_args` ao `command` do probe.

- [ ] **Passo 4: rodar os testes**

Run: `python -m unittest discover -s tests -p "test_run_isa.py" -v`
Expected: todos `ok`.

- [ ] **Passo 5: commit**

`git add run.py tests/test_run_isa.py`; mensagem via `caveman-commit` (sugestão: `Feat(Run): gerar a tabela ISA quando o probe reporta conjuntos ausentes`).

---

### Task 9: empacotamento, READMEs e documentação

**Files:**
- Modify: `packaging/windows/package.sh` (após o `pip install ... pyinstaller`; lista `hidden`), `packaging/linux/build.sh` (lista `hidden`; comentário sobre o wheel), `packaging/windows/README.md` (parágrafo do PyInstaller), `README.md` e `README.pt-BR.md` (seção Requisitos, item do processador), `packaging/windows/README-Windows.txt` (item do processador)

- [ ] **Passo 1: empacotamento**

`package.sh`: `out/pyenv/Scripts/python.exe -m pip install -q --disable-pip-version-check pyinstaller iced-x86==1.21.0`; `iced_x86` na lista `for module in ...` dos dois scripts. `packaging/linux/build.sh`: comentário acima da lista: `# iced-x86 (pip install iced-x86==1.21.0): scripts/isa_rewrite.py, CPUs without MOVBE/BMI1/LZCNT.` `packaging/windows/README.md`: uma frase no parágrafo do PyInstaller dizendo que o venv recebe `iced-x86`.

- [ ] **Passo 2: READMEs**

Item do processador (en): `- A 64-bit processor with AVX: Intel Sandy Bridge (2011) or newer, AMD FX or newer. On CPUs without MOVBE, BMI1 or LZCNT (Sandy/Ivy Bridge, AMD FX) the loader rewrites the game's instructions that use them to emulated stubs and logs an `ISA:` line; performance on those CPUs has not been measured yet. Older CPUs stop at start with a `STOP: this CPU lacks` line in the log.` Tradução equivalente em `README.pt-BR.md`; versão curta em `README-Windows.txt`.

- [ ] **Passo 3: verificação**

Run: `grep -n "iced" packaging/windows/package.sh packaging/linux/build.sh packaging/windows/README.md && grep -n "Sandy Bridge" README.md README.pt-BR.md packaging/windows/README-Windows.txt`
Expected: cada arquivo aparece ao menos uma vez; `bash -n packaging/windows/package.sh packaging/linux/build.sh` sem erro.

- [ ] **Passo 4: commit**

`git add packaging/windows/package.sh packaging/linux/build.sh packaging/windows/README.md README.md README.pt-BR.md packaging/windows/README-Windows.txt`; mensagem via `caveman-commit` (sugestão: `Docs(README): requisito de CPU passa a AVX com reescrita ISA`).

---

### Task 10: verificação ponta a ponta em modo forçado

**Files:** nenhum (verificação).

- [ ] **Passo 1: build completo e testes**

Run (MSYS2 em `/w`): `ninja -C out/w spartanport-probe spartanport-gpu-capabilities spartanport-play cpu-check-test isa-emulate-test isa-stub-test`, os três testes, e `python -m unittest discover -s tests -p "test_*.py"`.
Expected: três `ok`; Python só com as 5 falhas pré-existentes.

- [ ] **Passo 2: jogo em modo forçado**

Com `out/spartanport-probe.exe` atualizado e o Python do repositório com `iced-x86`: `SPARTAN_ISA_REWRITE=1 SPARTAN_GAME_DIR="C:/Users/Lucas Emiliano/Documents/My Games/CUSA01623/Image0" python run.py`.
Expected no log: `CPU: complete`, `ISA: 14xx sites (MOVBE 104, BMI1 12xx, LZCNT 77, F16C 0); 8x relocated, 0 int3, 0 unsupported`, `ISA: forced rewrite of all 14xx sites (SPARTAN_ISA_REWRITE=1)`; o jogo abre, chega ao menu e a uma luta sem falha nova; ao fechar, `ISA: most executed sites:` com até 10 linhas. Registre as linhas `ISA:` no resumo da tarefa (elas dizem se a abordagem C da spec algum dia compensa).

- [ ] **Passo 3: sem a variável**

Mesmo comando sem `SPARTAN_ISA_REWRITE`: nenhuma linha `ISA:`, jogo igual a antes.

- [ ] **Passo 4: Linux (quando houver a máquina)**

`bash build.sh --test` e a mesma rodada forçada com `run.sh`. Registre o resultado; falha aqui não bloqueia o merge do Windows, vira issue.
