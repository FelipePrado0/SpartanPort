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

O README dele mede a cidade de Olímpia num Ryzen 5 5600 com RTX 3060: 150 FPS em 4K e 403 FPS em
480p. Nenhum desses números foi medido aqui. Diferenças entre as duas máquinas:

- CPU quase igual (5600 contra 5600X: mesmos núcleos, clock um pouco maior aqui).
- GPU mais forte aqui (RX 6700 XT contra RTX 3060).
- O SpartanPort roda sem MSAA (`guest multisampling off`); o port dele usa o 2x original. Menos
  trabalho de GPU aqui.
- O motor do SpartanPort vai até 240 FPS; o port dele, até 480.
- RAM desta máquina: 3 pentes a 2666 MT/s. Com número ímpar de pentes, parte da memória pode rodar
  sem dual channel, o que pesa no lado da CPU. Não medido aqui.

Então, se o SpartanPort ficar abaixo do port dele em Olímpia nesta máquina, a diferença é do
software, não do hardware.
