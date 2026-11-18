# Relatório W0: SpartanPort e o port de comparação

Gerado por `perf/harness/report.py` a partir dos resultados combinados de `perf/auto`.

| Cenário | Cache | SpartanPort FPS (mín a máx) | Comparação FPS | Razão SP/comp. | p99 SP | > 40 ms SP | Compilações SP |
|---|---|---|---|---|---|---|---|
| abertura-primeira-onda | warm | 117.8 (103.2 a 127.9) | sem dados | - | 13.12 | 0 | 2 |
| cena-estatica | cold | 186.8 (185.7 a 191.9) | sem dados | - | 9.05 | 0 | 2 |
| cena-estatica | warm | 191.5 (188.2 a 193.1) | sem dados | - | 9.03 | 0 | 2 |
| soak-20min | warm | 127.7 (127.7 a 127.7) | sem dados | - | 20.41 | 0 | 2 |
| troca-de-area | warm | 203.3 (201.2 a 206.5) | sem dados | - | 12.96 | 2 | 2 |

GPU: AMD Radeon RX 6700 XT. Build: b358431d+dirty.

## Condições não igualadas

- O port de comparação não foi medido: ele só aceita o eboot da 1.02 real (`6e72330e…`) e o único dump desta máquina tem o eboot `d85c8135…` (código da 1.00).
- Saída em janela 1920x1080 no monitor principal (2560x1440); sem upscaler, frame generation, overlay nem limite de FPS; motor a 240.
- `us/draw` não existe no Windows (sai 0.00); só FPS e frametime contam.
- A baseline vale para o estado da máquina na sessão em que foi gravada (deriva de ~7 % medida em 3 h).
