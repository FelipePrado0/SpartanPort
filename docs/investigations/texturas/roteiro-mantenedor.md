# Roteiro do mantenedor: corrupção de texturas (RX 6700 XT, 240 Hz, CUSA01623 v01.02)

Build: pacote Windows da branch `diag/texture-corruption` (workflow Windows zip), em pasta separada da instalação normal. Cópia de `user\` (saves/config) antes de começar. Preserve o `last_run.log` de cada execução antes de abrir o jogo de novo.

Como ligar as variáveis: num `cmd`, `set NOME=valor` e abra o launcher desse mesmo `cmd`. Confirme no log: com `SPARTAN_TEX_TRACE=1` aparecem linhas `Tex: summary` a cada ~5 s. Cada execução = um processo novo.

O bug pode não aparecer na sua máquina: os itens 1 a 4 valem mesmo sem artefato, porque medem o GC e exercitam o caminho corrigido.

1. **Base com diagnóstico.** `SPARTAN_TEX_TRACE=1`, orçamento padrão, FSE ligado; 30 min com transições (portas, cinematics, menu de melhorias, Fúria de Esparta).
   Esperado: jogo normal. Enviar: 3 linhas `Tex: summary` (início, meio, fim) e as linhas `Tex: heap`.
2. **Pressão de placa de 8 GB.** Item 1 + `SPARTAN_GC_BUDGET_MB=4000` (pressão em ~3,4 GB, como as placas de 8 GB no padrão); mesma rota, 30 min.
   Esperado: muitas linhas `Tex: evict`/`download`; sem artefato é bom sinal para a correção. Enviar: `Tex: summary` com `evict` > 0 e qualquer `Tex: alias-forget`.
3. **Mesmo que 2, FSE desligado.** Launcher, Performance, "Fast screen effects" desligado (ou `SPARTAN_PARTIAL_HEIGHT_VIEWS=0`).
   Esperado: menos `alias-forget`. Enviar: o mesmo do item 2, para comparar contagens.
4. **Orçamento amplo para 12 GB.** Item 1 + `SPARTAN_GC_BUDGET_MB=9000` (pressão em ~7,6 GB, crítico em ~8,5 GB); 30 min.
   Esperado: `evict` perto de zero. Enviar: `Tex: summary` do fim e `Tex: heap` (uso real da VRAM).
5. **Sessão longa (se tiver tempo).** Item 2 por 60–90 min, sem sair ao menu.
   Esperado: sem artefato. Se aparecer: anote a hora, saia ao menu e recarregue; envie as linhas `Tex:` dos 10 s antes do artefato e preencha a `ficha-reproducao.md`.
6. **Controle da release (só se o 5 mostrar artefato).** Mesma rota/variáveis na release `windows-v2.7.0` (sem `Tex:`).
   Esperado: compara tempo até o artefato. Enviar: hora do artefato e linhas `Texture cache: memory pressure`.

Parar na hora se houver crash, travamento de carga, perda de dispositivo ou falta de VRAM; guardar o log.

## Variáveis

| Variável | Valores | Efeito |
|---|---|---|
| `SPARTAN_TEX_TRACE` | `1` liga; qualquer outro valor desliga | Linhas `Tex:` no log (resumo a cada 5 s, heaps, até 64 eventos por intervalo) |
| `SPARTAN_GC_BUDGET_MB` | MiB | Limiares do GC em 70/85/95% desse valor (gatilho/pressão/crítico); lido uma vez por processo |
| `SPARTAN_PARTIAL_HEIGHT_VIEWS` | `0` desliga | Mesmo que desligar "Fast screen effects" |

Referência do orçamento padrão estimado para 12 GB (budget do driver ~11 GB após a reserva de 1 GB): gatilho ~1,5 GB, pressão ~6,2 GB, crítico ~9,4 GB. Para 8 GB: pressão ~2,8 GB, crítico ~5,6 GB. Valores aproximados; os exatos aparecem em `Tex: summary` (`trigger/pressure/critical`).
