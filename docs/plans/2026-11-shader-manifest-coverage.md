# Cobertura da lista de shaders (W2)

Medido em 2026-11-18, sem jogador, com `perf/auto/run_scenario.py --cache cold` (pasta de dados
apagada a cada execução) e `PERF_SHADER_LISTS=0` para a execução sem lista. Máquina em
`perf/baseline/MAQUINA.md`. Build `b358431d` + `perf/`. Lista: `shaders/CUSA01623.spm`
(25.230 programas, 27.742 permutações, 15.463 pipelines). Uma execução por caso, então os números
de quadro têm o ruído de uma execução só.

## (a) A lista serve para outras versões e seriais?

**Não testado.** Esta máquina só tem o dump CUSA01623 com o eboot `d85c8135…` (código da 1.00). Não
há CUSA01715 nem uma 1.02 real para medir a taxa de acerto da materialização neles.

## (b) Abertura fria, com e sem a lista

| Cenário | Lista | Shaders lidos do cache | Compilados em jogo | Taxa de acerto | FPS | p99 | Quadros > 40 ms | Até o título |
|---|---|---|---|---|---|---|---|---|
| `troca-de-area` (CAV300) | sim | 943 | 21 | 97,8 % | 182,1 | 13,8 ms | 2 | 566 s |
| `troca-de-area` (CAV300) | não | 0 | 902 | 0 % | 116,2 | 432,6 ms | 31 | 46 s |
| `abertura-primeira-onda` (POS050) | sim | 552 | 20 | 96,5 % | 97,9 | 37,7 ms | 9 | 696 s |
| `abertura-primeira-onda` (POS050) | não | 0 | 573 | 0 % | 103,7 | 53,3 ms | 9 | 204 s |

Taxa de acerto = lidos / (lidos + compilados), da linha `Disk shaders: ... used, ... compiled`.

## Leitura

- **A lista quase elimina a compilação em jogo**: de 573 a 902 shaders para 20 a 21, nos dois
  cenários. No `troca-de-area`, que entra numa área nova, isso tira os estalos: p99 de 432 ms para
  14 ms e quadros acima de 40 ms de 31 para 2.
- Na abertura, os quadros acima de 40 ms ficaram iguais (9 e 9). Os estalos ali não vêm de shader:
  nos dois casos o combate e o carregamento de cena dominam. Uma execução só; não dá para separar
  mais que isso.
- **O preço é a 1ª abertura**: 566 a 696 s até o título (preparo de shaders 114 a 126 s e de
  pipelines 374 a 390 s), contra 46 a 204 s sem a lista. A maior parte é o preparo de pipelines
  (F6 do plano de pré-compilação). Esse tempo é pago uma vez por GPU e driver.
- Linha de base do roadmap (issue #1260, 3.1.0 sem lista): `compiled` subia de 6 para ~206 na
  primeira luta. Aqui, sem lista, foram 573 na abertura inteira; com a lista, 20.

## Pendências (decisão do mantenedor, F6 a F10 do plano existente)

- Reduzir os ~6 min de preparo de pipelines na 1ª abertura. O port do cuesta4 guarda binários do
  driver por shader object (`VK_EXT_shader_object`), não por pipeline
  (`docs/plans/2026-11-rehooked-gap-analysis.md`).
- Os 20 a 21 shaders que a lista não cobre podem entrar numa gravação nova da lista (F9 do plano).
- Matriz de versões e seriais: precisa de dumps CUSA01715 e 1.02 real.
