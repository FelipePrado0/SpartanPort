# Máquina das baselines

As baselines desta pasta valem para esta máquina. Coletado em 2026-11-18 (WMI, `vulkaninfo`).

| Item | Valor |
|---|---|
| CPU | AMD Ryzen 5 5600X, 6 núcleos / 12 threads |
| RAM | 32 GB em 3 pentes, 2666 MT/s |
| GPU | AMD Radeon RX 6700 XT (12 GB) |
| Driver | AMD 26.10.1 (Windows 32.0.21045.16010, 2026-10-15), Vulkan 1.4.315 |
| Sistema | Windows 11 Pro, build 26200, plano de energia Alto desempenho |
| Disco do jogo | SSD SATA (XrayDisk 1 TB) |
| Monitores | 3440x1440 (primário), 2560x1440, 1920x1080 |
| Medição | janela 1920x1080, sem upscaler, frame generation nem overlay, motor a 240, sem limite de FPS |

## Comparação com o port do cuesta4

O README dele mede a cidade de Olímpia num Ryzen 5 5600 com RTX 3060: 150 FPS em 4K (6,5 ms, GPU
100%) e 403 FPS em 480p (2,6 ms, CPU 65%, 1% low 10 FPS). A cena dos cenários `cidade-olimpia*` é
a mesma do print dele: o pátio com as duas bandeiras vermelhas, as colunas, os braseiros e o
círculo no chão, câmera quase no mesmo ângulo (`route-end.png` de qualquer execução). A diferença:
no print os soldados já chegaram à borda, perto do Kratos; no cenário estão no fundo do pátio.

Medido aqui (2026-11-18, mediana de 3 execuções, `perf/baseline/cidade-olimpia-*-warm.json`):

| resolução | SpartanPort (RX 6700 XT) | port dele (RTX 3060) | diferença |
|---|---|---|---|
| 480p | 99,2 FPS (10,1 ms; 94,8 a 100,7) | 403 FPS (2,6 ms) | 4,1x |
| 4K | 90,7 FPS (11,0 ms; 88,8 a 99,7) | 150 FPS (6,5 ms) | 1,7x |

O FPS quase não muda de 480p para 4K aqui: o limite é o processador, não a placa. Os 480p isolam o
processador, então a meta das otimizações é o tempo de CPU por quadro dele, ~2,5 ms (403 FPS). O
motor do SpartanPort para em 240 FPS (4,2 ms), então acima disso a medida é o tempo de CPU por
quadro, não o FPS.

Diferenças entre as duas máquinas e condições:

- CPU quase igual (5600 contra 5600X: mesmos núcleos, clock um pouco maior aqui).
- GPU mais forte aqui (RX 6700 XT contra RTX 3060).
- O SpartanPort roda sem MSAA (`guest multisampling off`); o port dele usa o 2x original. Menos
  trabalho de GPU aqui.
- O motor do SpartanPort vai até 240 FPS; o port dele, até 480.
- Janela: os cenários rodam em janela de 1920x1080 no monitor de 2560x1440; o print dele também é
  1920x1080. Modo de tela dele não informado.
- RAM desta máquina: 3 pentes a 2666 MT/s. Com número ímpar de pentes, parte da memória pode rodar
  sem dual channel, o que pesa no lado da CPU. Não medido aqui.

Então, se o SpartanPort ficar abaixo do port dele em Olímpia nesta máquina, a diferença é do
software, não do hardware.
