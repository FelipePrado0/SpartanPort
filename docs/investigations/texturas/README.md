# Corrupção progressiva de texturas: investigação (rodada de 29/10/2026)

Roadmap: `docs/plans/2026-10-29-roadmap-corrupcao-texturas.md` (no clone do líder). Branch `diag/texture-corruption`, base `origin/main` `f593377`.

| Documento | Para quê |
|---|---|
| [revalidacao-2026-10-29.md](revalidacao-2026-10-29.md) | O que mudou nas issues, na PR #1104 e no código desde o roadmap |
| [ficha-reproducao.md](ficha-reproducao.md) | Ficha preenchível por execução |
| [roteiro-mantenedor.md](roteiro-mantenedor.md) | Testes curtos para o mantenedor (RX 6700 XT), viáveis mesmo sem reproduzir o bug |
| [rascunho-pedido-relatores.md](rascunho-pedido-relatores.md) | Rascunho bilíngue do pedido aos relatores. **Não publicado** |
| `docs/specs/2026-10-29-tex-alias-backing-eviction.md` | Spec + TDD da correção do backing de alias descartado |
| `docs/specs/2026-10-29-tex-trace.md` | Spec + TDD do diagnóstico `Tex:` |

## Resultado dos testes de regressão em nível de estado (CPU)

| Invariante do roadmap | Teste | Base `f593377` |
|---|---|---|
| Alias sobrevivente não referencia imagem removida nem UID reutilizado | `aliasing-test`, `TestAliasAuthority` (writer descartado + slot reutilizado; referência antiga) | Passa |
| Backing descartado com writer vivo: sobreviventes continuam recebendo o conteúdo mais novo | `aliasing-test`, bloco "backing is evicted while the writer lives on" | **Falha** (autoridade inteira zerada). Corrigido em `Fix(GPU): kept the alias writer…` |
| Writeback de geração antiga após remap não sobrescreve a nova geração | `readback-queue-test`, bloco "Remap" | Passa |
| Cópias da mesma região em gerações diferentes: a mais nova vence, em ordem | `readback-queue-test`, bloco "Two copies" | Passa |
| Vizinhos na mesma página: descartar um não cancela nem sobrescreve o outro | `readback-queue-test`, bloco "Neighbors" | Passa |

Leitura: a fila de readback (remap, ordem, vizinhos) se comporta como o roadmap exige; isso enfraquece a hipótese "writeback de geração antiga" **no nível de estado** (não testa Vulkan nem o runtime de memória). A autoridade de alias tinha uma lacuna real; ela só afeta grupos de 3 ou mais imagens no mesmo endereço (típico das views de altura parcial do Fast Screen Effects, `SPARTAN_PARTIAL_HEIGHT_VIEWS`) e só quando o GC descarta o backing antes de o writer ser copiado nele. É candidata para #1103; não há prova de que explique #1059/#1016.

Lacunas que continuam abertas (não testadas):
- Writer descartado pelo GC: os bytes mais novos vão à RAM (download síncrono), mas aliases sobreviventes `GpuModified` não releem a RAM (`ApplyBackingWrites` os ignora). Corrigir pede copiar o writer antes do descarte (comando de GPU no GC); sem teste que reproduza no jogo, ficou fora desta rodada.
- Unmap não alinhado à página cancela cópias de vizinhos na mesma página (`Cancel` não exato). Hoje o runtime só notifica unmaps alinhados a 16 KiB (`direct_unmap`), então não ocorre na prática.
- Nada aqui valida uma transferência Vulkan real nem o conteúdo das texturas no jogo.
