# Revalidação do roadmap contra a `main` (29/10/2026, fim do dia)

Consulta feita com `gh api` (REST, só leitura) e `git ls-remote`.

## Base e PR

- `origin/main` continua em `f593377bbaf1934bf606171973d7a58210510991`: nenhuma mudança desde o roadmap.
- PR #1104: aberta, não mesclada, head `d41dc689914cb7b1568dda9374650f34858e82a8` (inalterado; último commit 28/10 04:44). Atividade de 28/10: só rótulos (`confirmed`, `area:graphics`, `area:runtime`, `performance`). Sem revisões nem comentários novos.

## Issues

| Issue | Estado | Comentários novos desde 29/10 00:00 UTC | Efeito no roadmap |
|---|---|---|---|
| #1059 | aberta | [3060 Ti](https://github.com/FelipePrado0/SpartanPort/issues/1059#issuecomment-6485631439) (já citado no roadmap); [relator: mais de 4 h sem recorrência](https://github.com/FelipePrado0/SpartanPort/issues/1059#issuecomment-6494489617) com a mitigação que ele propôs; [agradecimento](https://github.com/FelipePrado0/SpartanPort/issues/1059#issuecomment-6494515129) | Reforça a mitigação `SPARTAN_GC_BUDGET_MB` como relato de usuário, ainda sem log pareado nem contagem de descartes. Não muda a hipótese |
| #1016 | aberta | nenhum | — |
| #1103 | aberta | nenhum | — |
| #863 | aberta | nenhum | — |
| #917 | aberta | nenhum (0 comentários no total) | — |
| #1080 | aberta | [participante aguardando nova versão](https://github.com/FelipePrado0/SpartanPort/issues/1080#issuecomment-6498584552), sem dados técnicos | — |
| #915 | aberta | nenhum | — |
| #1107 | aberta | [texturas brancas piscando desde o início](https://github.com/FelipePrado0/SpartanPort/issues/1107#issuecomment-6484475953), com vídeo | Sintoma imediato (desde o launch), não progressivo: mantém #1107 na trilha separada de flicker/pop-in |

## Código citado

Todas as faixas de linha do roadmap em `gpu/shadps4/video_core/texture_cache/texture_cache.cpp` batem com `f593377` (construtor 51–71, download 387–428, aliases 496–553, invalidação 649–671, GC/override 1657–1678, descarte 1696–1721, remoção 1822–1848). Nesta branch elas se deslocam pelos commits de refactor/correção/diagnóstico.

## Divergências e notas

- Nenhuma divergência de estado (todas abertas, PR aberta, mesma base).
- `readback-sources-test` não compilava na base com o GCC 16 do `shell.nix` (`-Werror=unused-result` em `fread`); corrigido no commit `Test(GPU): checked fread's result…`.
- `DownloadImageMemory(…, sync=false)` (caminho diferido que não passa pela fila cancelável) não tem chamador na base: o único chamador é o GC, síncrono. Não é um risco ativo de writeback pós-remap.
- O runtime só notifica unmaps alinhados a 16 KiB (`src/runtime_memory.c`, `direct_unmap`); não existe "geração de mapeamento" exportada. O diagnóstico usa um contador local (`mapgen`, unmaps vistos pelo cache). Dependência para a frente de memória: se ela criar uma geração no runtime, o `Tex:` pode passar a usá-la.
- O "Fast Screen Effects" do launcher é `SpartanToggle::PartialHeightViews` (`SPARTAN_PARTIAL_HEIGHT_VIEWS=0` desliga), o caminho que cria grupos de aliases de alturas diferentes no mesmo endereço.
