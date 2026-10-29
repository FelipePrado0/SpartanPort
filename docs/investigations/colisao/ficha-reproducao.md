# Ficha de reprodução: cenários ausentes e sem colisão

Data: 2026-10-29. Branch `diag/missing-collision` (base `origin/main` = `f593377`). Roadmap:
`docs/plans/2026-10-29-roadmap-cenarios-sem-colisao.md`. Protocolo comum:
`docs/plans/2026-10-29-roadmap-investigacao-issues.md`.

Esta ficha separa o que foi observado do que é hipótese. Nenhuma linha abaixo foi reproduzida no
jogo por esta frente (modo nuvem, sem jogo).

## 1. Revalidação do roadmap (29/10/2026)

| Item | Estado na consulta do roadmap | Estado agora | Mudou? |
|---|---|---|---|
| `main` | `f593377` | `f593377` | Não |
| Release estável | `windows-v2.7.0` (`003e42b`) | a mesma | Não |
| #279 | fechada | fechada (`not_planned`, 23/10) | Não |
| #405 | fechada como duplicata | idem | Não |
| #871 | aberta, 3 comentários | aberta, sem comentários novos desde 21/10 | Não |
| #899 | fechada com confirmação | fechada (`completed`) em 29/10 12:22 UTC; o comentário de encerramento é o já citado no roadmap | Não (só a data de fechamento) |
| #1000 | aberta, 1 comentário (log) | aberta, `awaiting-response`, sem comentários novos | Não |
| #839, #975 | abertas, sem comentários | idem | Não |
| #535, #1011 | abertas | idem (fronteira: Alto FPS) | Não |
| Caso novo | — | **#1051** (aberta, 2.7.0, GTX 1050 Ti, CUSA01715): depois de Helios, no Caminho de Eos, o fundo fica azul, a cena não carrega e Kratos cai no vazio; sem log | **Sim**: candidato a este agrupamento, ainda não classificado (pode ser carga, não colisão) |
| #1100 | — | "Kratos preso" depois dos Juízes, mas em Intel Iris Xe com device lost; resolvido com a build Intel de teste | Fora do escopo (Intel) |
| #1107 | — | objetos e sombras somem conforme o ângulo da câmera (culling/pop-in), sem queda | Fora: visual, sem perda de colisão |

Código citado pelo roadmap, conferido em `f593377`:

| Ponto | Linhas no roadmap | Linhas reais | Bate? |
|---|---|---|---|
| `do_read`/`do_pread`, `src/runtime_file.c` | 286-317 | `touch_for_write` 286, `do_read` 297, `do_pread` 308-319 | Sim (o intervalo cobre as funções) |
| `retire_fixed`/`announce_mapped_locked`, `src/runtime_memory.c` | 552-581 | comentário 552, funções 561 e 573-581 | Sim (antes desta branch; os commits daqui deslocam as linhas) |
| `CopySparseSerial`, `gpu/shim/spartanportgpu.cpp` | 111-126 | 114-126 | Sim |
| `ApplyBackingWrites`, `texture_cache.cpp` | 649-671 | 649 | Sim |
| `UnmapMemory`, `texture_cache.cpp` | 690-700 | 690-700 | Sim |
| `eboot_sha256`, `scripts/patches.py` | 202-208 | 202-208 | Sim |

## 2. Casos

| Caso | Local | CUSA / APP_VER | Save | Engine FPS / readback | Visual | Físico | Falta |
|---|---|---|---|---|---|---|---|
| #871 | corredor após os Juízes (Pit of Tartarus) | log: CUSA01715 01.02 (relato: também CUSA01623 01.00/01.02) | manual `SD-261021002258`; relata também jogo novo | 60 / deferred+stale | duplicado / ausente | cai | manifesto, vídeo, log com `Col:`/`Load:` |
| #1000 | plataforma visível de longe some ao se aproximar | CUSA01715 01.00 | `SD-261022201452` | 120 / deferred desligado | some ao aproximar | sem colisão | log com `Col:`/`Load:`, separar visual de físico |
| #279/#405 | corredor dos Juízes / área de Hefesto | 279: CUSA01715 1.02; 405: não confirmado | contorno: save manual anterior + ~8 min | — | vazio | cai | histórico (fechadas) |
| #899 | (fechada) | USA 1.02 falhava; CUSA01715 EUR funcionou | — | — | — | — | hashes das duas instalações |
| #975 | cenário some após a cena de Cronos (misturado com montaria) | CUSA01623 01.02 | `AUTOSAVE` | 120 / deferred+stale | some | cai | separar combate/montaria da área |
| #1051 (novo) | Caminho de Eos, após Helios | CUSA01715 | ? | ? | fundo azul, cena não carrega | cai no vazio | log, APP_VER, save |

## 3. O que o código desta branch mostrou (testes sintéticos)

- **Substituição A→B com retirada síncrona** (`fixed_map_retires_before_replacing`, já da #1005;
  `flexible_replacement_keeps_new_generation`, novo): passam na base. O readback de A cai nas
  páginas de A; B fica intacta. **Enfraquece** a hipótese de sobrescrita no caminho comum.
- **Substituição recusada** (`refused_replacement_keeps_old_mapping`, novo; e os casos da #1005):
  passam na base. Mapeamento anterior, proteção, conteúdo e visibilidade para a GPU preservados.
  **Enfraquece** a hipótese de perda por mapeamento substituto recusado.
- **Notificação `unmap` atrasada de A depois de B mapeada** (`late_unmap_of_old_generation_keeps_new`):
  **falhou na base** (B sobrescrita pelo readback de A). Exige que a notificação de A esteja em voo
  enquanto outra thread mapeia B. Corrigido para escritas feitas pela própria thread da notificação
  (commit `48e8b6c`); escritas de outra thread a pedido da notificação continuam sem filtro. Não há
  prova de que isso aconteça no jogo; o trace (`Col: stale-write` e a linha
  `late GPU writes of unmapped memory dropped=` no fim do log) mostra se acontece.
- **Observação sem teste nem correção:** a operação de unmap dentro de `sceKernelBatchMap`
  (`batch_map2`, op 1) avisa a GPU depois de desmapear as páginas, ao contrário de `munmap`
  (`direct_unmap`), que avisa antes. Um readback da GPU nesse momento não acha mais o mapeamento e
  é perdido. Só importa se o jogo usar batch unmap e remapear as mesmas páginas físicas; as linhas
  `Col: unmap ... src=batch` dizem se o jogo usa esse caminho.

## 4. Modelo de registro por execução

Copiar por execução (dossiê mínimo do protocolo comum):

```
Execução: <issue>-<data>-<n>    Build: <tag/SHA, checksum do zip>
SO/CPU/GPU/driver:              CUSA / APP_VER:             hash do eboot.bin:
Manifesto: <arquivo do tools/game_manifest.py da branch diag/infinite-loading>
Save: <nome, manual/auto, cópia preservada em>   Ponto de entrada:
Engine FPS / limite / VSync / readback (do log):  Variáveis: SPARTAN_MEM_TRACE=1 ...
Visual (área aparece?):        Físico (piso sustenta? salto/aterrissagem?):
Checkpoint seguinte alcançado?: Recarregar o checkpoint novo repete o defeito?:
Tentativas: <n>, defeito em <k>  Log: last_run.log preservado como <nome>
```
