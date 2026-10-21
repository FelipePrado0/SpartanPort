# Reescrita de instruções Haswell para CPUs antigas (ISA rewrite)

Data: 2026-10-13. Estado: aprovado em conversa; aguarda revisão do texto.

## 1. Objetivo

Fazer o jogo rodar em CPUs que têm AVX mas não têm os conjuntos de instruções que o
código do jogo usa e que só chegaram ao Intel Haswell (2013): MOVBE, BMI1 e LZCNT.
Hoje o probe para com `STOP: this CPU lacks instruction sets the game's code uses`
(issues #33 e #66: Xeon E5-2689 Sandy Bridge, Xeon E3-1230 v2 e E5-2667 v2 Ivy Bridge).

CPUs alvo: Intel Sandy Bridge (2011) e Ivy Bridge (2012), AMD FX Bulldozer e Piledriver.
Fora do escopo: CPUs sem AVX (Nehalem, Core 2, Phenom II). Elas exigiriam emular todo o
código VEX do jogo e continuam no `STOP`.

Sucesso: o jogo abre e é jogável nessas CPUs, confirmado pelos relatores das issues com
uma build de teste. Em CPUs com todos os conjuntos nada muda: zero sítios tocados.

## 2. Contexto

O código do guest (`eboot.bin`, `libc.prx`, `libSceFios2.prx`) é compilado para o Jaguar
do PS4 e roda direto na CPU do host. Censo feito na build CUSA01623 com `llvm-objdump` e
confirmado com `iced-x86` (varredura linear dos segmentos executáveis):

| Módulo | BEXTR | ANDN | BLSI/BLSR | LZCNT | TZCNT | MOVBE | F16C |
|---|---|---|---|---|---|---|---|
| eboot.bin (1,07 M instruções) | 842-846 | 139 | 31 | 42 | 8 | 94 | 0 |
| libc.prx | 62 | 33 | 11 | 35 | 10 | 0 | 0 |
| libSceFios2.prx | 54 | 41 | 0 | 0 | 0 | 10 | 0 |

Cerca de 1400 sítios (eboot 1156, libc 151, libSceFios2 105). O sítio da falha da issue
#33 (libc offset 0x25225) é
`blsr rax, rsi`. Três fatos moldam o design:

1. **LZCNT e TZCNT não falham em CPU antiga.** Sem o conjunto, a CPU ignora o prefixo
   `F3` e executa BSR/BSF: resultado errado em silêncio. Emular só no trap de instrução
   ilegal nunca os veria. Por isso a reescrita é estática, antes do jogo rodar.
2. **As flags são consumidas.** `blsr eax,eax; jne` e `andn ...; jz` são padrão do clang.
   A emulação reproduz as flags definidas pelo SDM com exatidão.
3. **Trap é caro.** Uma exceção vetorizada no Windows custa cerca de 10 µs. Com 958
   BEXTR em código de jogo, "trap em tudo" seria injogável. O caminho principal é um
   salto para um stub (50-100 ns); o trap fica só como reserva rara.

Tamanhos: toda instrução VEX (BMI1) tem 5 bytes ou mais; LZCNT/TZCNT e MOVBE sem REX têm
4 bytes (85 sítios: 62 LZCNT/TZCNT, 23 MOVBE). Nos 67 sítios de 4 bytes da eboot, nenhum
tem a instrução seguinte como alvo de salto direto.

## 3. Visão geral

```
run.py
  spartanport-probe --cpu-features          -> "CPU: missing MOVBE, BMI1, LZCNT" | "CPU: complete"
  prepare.py / link_libc.py / link_modules.py   (como hoje)
  isa_rewrite.py out/boot-linked.bin --out out  (só se faltar algo ou SPARTAN_ISA_REWRITE=1)
  spartanport-probe out/boot-linked.bin ... --isa-rewrite out/isa-rewrite.bin
      cpu_check: conjuntos fatais -> STOP; reescrevíveis -> lista
      carrega imagem, relocações, patches, cheats, hooks, TCB (como hoje)
      isa_rewrite_apply: verifica bytes, gera stubs, aplica jmp/int3
      protect(); módulos iniciam; jogo roda
```

Componentes novos:

- `scripts/isa_rewrite.py`: localizador. Decodifica os segmentos executáveis da imagem
  ligada com `iced-x86` e grava a tabela `out/isa-rewrite.bin`.
- `src/isa_emulate.c/.h`: emulação pura de cada instrução sobre um struct de
  registradores. Sem dependência de SO, sem libc, sem registradores vetoriais.
- `src/isa_rewrite.c/.h`: lê a tabela, verifica os sítios, gera os stubs, aplica a
  reescrita, atende o trap `int3`, conta execuções e loga.

Componentes alterados: `src/probe.c`, `src/cpu_check.c/.h`, `run.py`, `build.sh`,
`gpu/CMakeLists.txt`, `packaging/windows/package.sh`, `packaging/linux/build.sh`,
`README.md`, `README.pt-BR.md`, `packaging/windows/README-Windows.txt`.

## 4. Localizador: `scripts/isa_rewrite.py`

Uso: `isa_rewrite.py <boot-linked.bin> --out <dir>`. Escreve `<dir>/isa-rewrite.bin` e
imprime uma linha de resumo:
`ISA: 1412 sites (MOVBE 104, BMI1 1231, LZCNT 77, F16C 0); 85 relocated, 0 int3, 0 unsupported`.

### 4.1 Entrada

Lê o formato G3PROBE5 que `link_modules.py` escreve: cabeçalho, tabela de módulos,
bindings, segmentos (`address, size, flags`), nomes, relocações e a imagem. Só os
segmentos com `flags & 1` (executáveis) são decodificados. Os bytes são os da imagem
antes das relocações (o probe aplica relocações em dados, não em código; o probe
verifica os bytes de cada sítio antes de patchar).

### 4.2 Decodificação

`iced_x86.Decoder(64, bytes, ip=address)` em varredura linear por segmento (1,07 M
instruções em 0,28 s). Durante a varredura coleta:

- os sítios: instruções cujo mnemônico está em MOVBE, LZCNT, TZCNT, ANDN, BEXTR, BLSI,
  BLSMSK, BLSR, VCVTPH2PS, VCVTPS2PH;
- o conjunto de alvos de salto direto: `near_branch_target` de jmp, jcc e call com
  operando `NEAR_BRANCH64`.

### 4.3 Decisão por sítio

| Caso | Modo | Bytes no sítio |
|---|---|---|
| tamanho ≥ 5 | `JMP` | `E9 rel32` seguido de `90` até o tamanho original |
| tamanho 4 e instrução seguinte plana | `JMP_RELOCATE` | `E9 rel32` cobre os 4 bytes e o primeiro da seguinte; o resto da seguinte vira `90` |
| tamanho 4 sem seguinte plana | `INT3` | `CC` no primeiro byte; os outros 3 ficam |
| encoding fora do emulador | `UNSUPPORTED` | nada |

Instrução seguinte "plana": `flow_control == NEXT`, não RIP-relativa, sem prefixo REP,
não é outro sítio, termina dentro do mesmo segmento e seu início não está no conjunto de
alvos de salto direto. Sítio de 4 bytes no fim do segmento vira `INT3`.

Encodings fora do emulador (`UNSUPPORTED`): prefixo de segmento no operando de memória,
address-size de 16 bits, prefixo LOCK, e as duas instruções F16C (zero sítios hoje; o
emulador não as implementa). O probe para com `STOP` se a CPU precisar de um sítio
`UNSUPPORTED`.

### 4.4 Formato da tabela `G3ISA1`

Little-endian. Cabeçalho de 64 bytes:

| Offset | Campo |
|---|---|
| 0 | `"G3ISA1\0\0"` |
| 8 | u64 tamanho da imagem |
| 16 | sha256 da imagem (32 bytes) |
| 48 | u32 número de sítios |
| 52 | u32 número de sítios `UNSUPPORTED` |
| 56 | reservado (zeros) |

Registros de 64 bytes, ordenados por offset:

| Offset | Campo |
|---|---|
| 0 | u64 offset do sítio na imagem |
| 8 | u8 tamanho da instrução original (4-15) |
| 9 | u8 tipo: 1 MOVBE load, 2 MOVBE store, 3 LZCNT, 4 TZCNT, 5 ANDN, 6 BEXTR, 7 BLSI, 8 BLSMSK, 9 BLSR, 10 VCVTPH2PS, 11 VCVTPS2PH |
| 10 | u8 largura em bits: 16, 32 ou 64 |
| 11 | u8 modo: 0 JMP, 1 JMP_RELOCATE, 2 INT3, 3 UNSUPPORTED |
| 12 | u8 registrador destino (0-15; 0xFF se o destino é memória) |
| 13 | u8 registrador fonte 1 (0-15; 0xFF ausente ou memória) |
| 14 | u8 registrador fonte 2 (BEXTR: controle; ANDN: segunda fonte; 0xFF ausente) |
| 15 | u8 operando de memória: 0 nenhum, 1 destino, 2 fonte |
| 16 | u8 base (0-15; 0xFF nenhuma), u8 índice (0-15; 0xFF nenhum), u8 escala (1, 2, 4, 8; 0 sem operando de memória), u8 flags: bit 0 = RIP-relativo já resolvido |
| 20 | reservado (4 bytes) |
| 24 | i64 deslocamento; se RIP-relativo, offset absoluto na imagem |
| 32 | bytes originais (15 bytes) |
| 47 | u8 tamanho da instrução realocada (0 se não há) |
| 48 | bytes da instrução realocada (15 bytes) |
| 63 | reservado |

Numeração de registradores: a do x86 (0 rax, 1 rcx, 2 rdx, 3 rbx, 4 rsp, 5 rbp, 6 rsi,
7 rdi, 8-15 r8-r15).

### 4.5 Falhas

Sem `iced_x86` importável, ou arquivo de entrada inválido: imprime
`ISA: rewrite table unavailable (<motivo>)` e sai com código 1, sem gravar tabela.
`run.py` segue sem `--isa-rewrite`.

## 5. Probe

### 5.1 Ativação

Opção nova `--isa-rewrite <arquivo>`. Variável `SPARTAN_ISA_REWRITE`:

- ausente: automático. Reescreve só os sítios dos conjuntos que o CPUID não tem.
- `1`: reescreve todos os sítios, mesmo com CPU completa. Modo de teste.
- `0`: nunca reescreve. CPU antiga para no `STOP`, como hoje.

Ordem no `main`: a checagem de CPU continua antes de tudo. Conjunto fatal ausente (SSE4.2,
POPCNT, AVX) → `STOP` com texto novo: `(Intel Sandy Bridge 2011 or newer, or AMD FX or
newer, is needed)`, código 20. Conjunto reescrevível ausente (MOVBE, BMI1, LZCNT, F16C)
sem `--isa-rewrite`, com tabela inválida, ou com sha256 diferente da imagem → `STOP:
this CPU lacks MOVBE, BMI1, LZCNT and no rewrite table matches the image`, código 20.
Com `SPARTAN_ISA_REWRITE=0` o texto é `STOP: this CPU lacks MOVBE, BMI1, LZCNT and the
rewrite is disabled (SPARTAN_ISA_REWRITE=0)`, código 20. Com tabela válida → segue.

### 5.2 `cpu_check`

`cpu_missing_features` passa a separar os dois grupos (dois buffers ou uma máscara por
conjunto). Modo novo `spartanport-probe --cpu-features`: imprime `CPU: missing MOVBE, BMI1,
LZCNT` ou `CPU: complete` e sai 0; conjunto fatal ausente imprime a linha `STOP:` e sai 20.
`SPARTAN_CPU_CHECK=0` continua pulando a checagem inteira.

### 5.3 Memória

A área de stubs é reservada no mesmo `allocate` da imagem, logo após `round_page(size)`:
256 bytes por sítio, arredondado a página (cerca de 360 KiB para 1400 sítios). Assim
todo `rel32` alcança (imagem ≤ 512 MiB, limite de `prepare.py`). A área entra no
`protect()` final como executável e só leitura. Os registros dos sítios ficam no heap,
com o campo `hits` (u64) por sítio.

Falha de CPU com RIP dentro da área de stubs é reportada pelos handlers de hoje com a
linha extra `While emulating site 0x<offset> (<mnemônico>)`.

### 5.4 Aplicação (`isa_rewrite_apply`)

Último passo antes de `protect()`, depois de `apply_patches`, cheats, hooks e
`patch_tcb_loads`. Para cada sítio selecionado:

1. Compara os bytes da imagem com os bytes originais do registro. Diferentes (um patch
   mexeu ali): pula, conta em `skipped` e imprime `ISA: site 0x<offset> <mnemônico>
   skipped: bytes changed`. Em `JMP_RELOCATE` compara também a instrução seguinte com a
   cópia realocada; diferente (um patch ou hook mudou a seguinte), o sítio passa a `INT3`,
   conta em `int3` e imprime `ISA: site 0x<offset> <mnemônico>: next instruction
   changed, int3 fallback`.
2. Gera o stub na posição `stub_base + 256 * i`.
3. Escreve os bytes do modo (tabela em 4.3). Em `JMP_RELOCATE` a volta é para
   `sítio + 4 + tamanho_realocado`; em `JMP`, para `sítio + tamanho`.

Sítios `UNSUPPORTED` de um conjunto que a CPU não tem param o probe com `STOP` antes de
qualquer escrita.

### 5.5 Stub

Um stub completo por sítio, sem stub compartilhado. Pseudo-asm:

```
lea  rsp, [rsp-0x100]        ; não altera flags; sai da red zone SysV e de frames de sinal
pushfq                       ; rflags em [rsp]: início do IsaRegs
mov  [rsp+8+8*0], rax        ; gpr[0..15] exceto rsp
mov  [rsp+8+8*1], rcx
...                          ; rdx rbx rbp rsi rdi r8..r15
lea  rax, [rsp+0x108]        ; rsp original
mov  [rsp+8+8*4], rax        ; gpr[4]
mov  rax, imm64 &record.hits
inc  qword [rax]             ; contador aproximado (sem lock)
mov  rbp, rsp                ; IsaRegs*
and  rsp, -16                ; alinhamento SysV
mov  rdi, imm64 record
mov  rsi, rbp
mov  rax, imm64 isa_emulate
call rax
mov  rsp, rbp
popfq                        ; flags calculadas pelo emulador
mov  rax, [rsp+8*0]          ; restaura gpr exceto rsp
...
lea  rsp, [rsp+0x100]
<instrução realocada>        ; só em JMP_RELOCATE
jmp  rel32 volta
```

`isa_emulate` tem a assinatura `void ABI isa_emulate(const IsaSite *site, IsaRegs *regs)`
com o macro `ABI` (SysV) que o probe já usa para código do guest, igual nos dois SOs.
`IsaRegs` é `{ uint64_t rflags; uint64_t gpr[16]; }`.

### 5.6 Emulador (`isa_emulate.c`)

Compilado com `-mgeneral-regs-only` e sem chamadas à libc: nenhum registrador XMM/YMM é
tocado, por isso o stub não os salva. Semântica do Intel SDM. Flags definidas são
escritas; indefinidas ficam como estavam.

| Instrução | Resultado | Flags |
|---|---|---|
| ANDN dst, s1, s2 | `~s1 & s2` | SF, ZF pelo resultado; OF=CF=0 |
| BEXTR dst, src, ctrl | `start = ctrl[7:0]`, `len = ctrl[15:8]`; `(src >> start) & ((1 << len) - 1)`; `start ≥ largura` → 0; `start + len > largura` → só os bits até `largura-1` | ZF pelo resultado; OF=CF=0 |
| BLSI dst, src | `-src & src` | SF, ZF pelo resultado; CF = src ≠ 0; OF=0 |
| BLSMSK dst, src | `(src - 1) ^ src` | SF pelo resultado; CF = src == 0; ZF=0; OF=0 |
| BLSR dst, src | `(src - 1) & src` | SF, ZF pelo resultado; CF = src == 0; OF=0 |
| LZCNT dst, src | src == 0 → largura; senão `largura - 1 - bsr(src)` | CF = src == 0; ZF = dst == 0 |
| TZCNT dst, src | src == 0 → largura; senão `bsf(src)` | CF = src == 0; ZF = dst == 0 |
| MOVBE r, m / m, r | troca de bytes na largura | nenhuma |

Escrita no destino: 64 bits escrevem tudo; 32 bits zeram os 32 altos; 16 bits (LZCNT,
TZCNT, MOVBE) preservam os bits altos. Operando de memória: os registradores do guest já
guardam endereços do host, então endereço = `base + índice × escala + deslocamento`;
RIP-relativo vem da tabela como offset absoluto e vira `image + deslocamento`. Um store em
página rastreada pela GPU cai no `spartanportgpu_handle_fault` existente e continua, como um
store do guest.

### 5.7 Trap `int3`

Windows: `EXCEPTION_BREAKPOINT` em `vectored_fault`, antes do caminho fatal. Linux:
`SIGTRAP` em `fault()`, registrado junto de `SIGSEGV`, `SIGILL` e `SIGBUS`. Sítio: no
Windows `ExceptionAddress` já aponta para o `int3`; no Linux é `REG_RIP − 1`. Busca
binária na tabela.
Sem sítio: caminho de hoje. Com sítio: monta `IsaRegs` a partir do `CONTEXT`/`ucontext`,
chama `isa_emulate`, copia de volta registradores e flags, RIP = sítio + tamanho (4), incrementa
`hits`.

### 5.8 Log

Na aplicação:
`ISA: CPU lacks MOVBE, BMI1, LZCNT: 1412 sites rewritten (1327 jmp, 85 relocated, 0 int3), 0 skipped`.
Em modo forçado: `ISA: forced rewrite of all 1412 sites (SPARTAN_ISA_REWRITE=1), 0 skipped`.
Na saída normal e no relatório de falha: `ISA: most executed sites:` seguido dos 10
sítios com mais `hits` (`0x<offset> <mnemônico> <hits>`). Serve para decidir se um dia
vale um caminho rápido em asm para algum sítio.

## 6. `run.py`

1. Antes dos scripts de preparação, roda `spartanport-probe --cpu-features`. Saída 20: imprime a
   linha `STOP:` e termina. Lê a lista de conjuntos ausentes.
2. Depois de `link_modules.py`, se a lista não está vazia ou `SPARTAN_ISA_REWRITE=1`:
   `run_script('isa_rewrite.py', out / 'boot-linked.bin', '--out', out)`. Sucesso →
   acrescenta `--isa-rewrite out/isa-rewrite.bin` ao comando do probe. Falha → imprime a
   linha do script e segue sem a flag (o probe dá o `STOP` com o motivo).
3. `SPARTAN_ISA_REWRITE=0`: não roda o script nem passa a flag.

## 7. Empacotamento e documentação

- `packaging/windows/package.sh`: `pip install iced-x86` no `out/pyenv` e
  `--hidden-import iced_x86` no PyInstaller.
- `packaging/linux/build.sh`: `--hidden-import iced_x86`; `packaging/windows/README.md` e um
  comentário em `packaging/linux/build.sh` registram que o Python da build precisa do wheel
  `iced-x86` (o fork do AppImage instala).
- `build.sh` e `gpu/CMakeLists.txt`: `src/isa_rewrite.c` e `src/isa_emulate.c` no probe;
  `isa_emulate.c` com `-mgeneral-regs-only`; alvos de teste novos.
- READMEs (en, pt-BR, README-Windows.txt): o requisito de CPU passa a "64 bits com AVX:
  Intel Sandy Bridge 2011, AMD FX ou mais novo". Parágrafo curto: em CPUs sem MOVBE,
  BMI1 ou LZCNT o probe emula essas instruções e registra a linha `ISA:` no log; o
  desempenho nessas CPUs ainda não foi medido.
- Launcher: sem mudança. A aba Log mostra as linhas.

## 8. Testes

- `tests/test_isa_rewrite.py` (unittest, `skipUnless(iced_x86)`): monta um G3PROBE5
  falso com dois segmentos executáveis a partir de bytes escritos à mão. Cobre cada
  mnemônico e largura, operando de memória e RIP-relativo, as quatro regras de 4 bytes
  (seguinte plana → `JMP_RELOCATE`; alvo de salto, RIP-relativa, salto ou outro sítio →
  `INT3`; fim de segmento → `INT3`), `UNSUPPORTED`, round-trip da tabela e sha256. Sem
  binários do jogo.
- `tests/test_isa_emulate.c`: oráculo nativo. Compilado com `-mbmi -mlzcnt -mmovbe`,
  executa milhares de entradas aleatórias mais bordas (0, 1, bit alto, tudo 1, `start` e
  `len` saturados, `start + len > largura`) por instrução e largura, e compara resultado
  e flags definidas com a instrução real via inline asm. Pula se o CPUID da máquina não
  tiver os conjuntos.
- `tests/test_isa_stub.c`: imagem RWX pequena com trechos reais, inclusive casos de 4
  bytes com instrução seguinte e um `INT3`; tabela fixture em `tests/data/`; aplica
  `isa_rewrite_apply`, executa trecho reescrito e nativo, compara registradores, flags e
  memória; confere XMM0-15 e os 128 bytes abaixo de `rsp` intactos; exercita o trap pelo
  handler da plataforma.
- Ponta a ponta: `SPARTAN_ISA_REWRITE=1` no Windows e no Linux de desenvolvimento. O jogo
  abre e joga com `ISA: forced rewrite of all N sites`, sem falha nova. Build de teste
  para os relatores das issues #33 e #66.
- `build.sh --test` e `gpu/CMakeLists.txt` ganham `isa-emulate-test` e `isa-stub-test`
  no padrão do `cpu-check-test`. Python: `python -m unittest discover -s tests`.

## 9. Riscos e limites aceitos

- Alvo de salto indireto (jump table) na instrução realocada de um sítio de 4 bytes: o
  localizador só conhece saltos diretos. Mitigação: só 85 sítios, e a rodada ponta a
  ponta em modo forçado exercita o jogo real. Um sítio problemático vira `INT3` por
  ajuste no localizador.
- Hooks (cheats, multiplicadores) que copiam instruções do guest para trampolins
  próprios não são cobertos. Numa cópia, MOVBE e BMI1 falham em CPU antiga com o
  relatório fatal de hoje, que nomeia o offset; LZCNT e TZCNT não falham: executam como
  BSR e BSF e dão resultado errado em silêncio.
- Desempenho nas CPUs alvo não medido; os contadores de `hits` dizem onde otimizar.
- O stub usa cerca de 0x120 bytes da pilha do guest mais o frame C, comparável a uma
  entrega de sinal.
- `-mgeneral-regs-only` precisa existir no clang do MSYS2 e no GCC do Linux; o plano
  verifica na primeira compilação.

## 10. Fora do escopo

- Stubs em asm especializado por sítio (abordagem C). Fica como otimização futura se os
  contadores mostrarem um sítio quente.
- Emulação de F16C (zero sítios nas builds conhecidas).
- CPUs sem AVX.
- Interface no launcher.
