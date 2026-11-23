# WV-A: inventário das ferramentas de validação sem jogador

Executado em 2026-11-18, Windows 11 (MSYS2 CLANG64 e Git Bash), Ryzen 5 5600X, AMD RX 6700 XT,
build `origin/main` @ `b358431d` num worktree limpo (`perf/wv`). Jogo CUSA01623 com eboot
`d85c8135…` (código da 1.00; o `param.sfo` diz 01.02 porque só o `sce_sys` veio do pacote 1.02),
dump em disco local. Linux não foi testado nesta máquina: toda linha "Linux" abaixo é
leitura de código, marcada como tal.

Legenda: **OK** rodou e fez o que promete; **FALHA** rodou e não fez; **NÃO RODA** não chega a rodar
neste SO; **LIMITADO** roda, com ressalva.

## L0: só o código

| Ferramenta | Windows | Saída real | Observação |
|---|---|---|---|
| `bash build.sh --test` | FALHA, depois OK | 1ª vez: `error: patch failed: src/ffx_vk_fsr4_v07.c:35`. Depois do ajuste: 8 alvos ninja + `services-test` + `savedata-test` passam, 3 min 28 s | Ver "Achado 1" |
| Demais alvos `*-test` do `gpu/CMakeLists.txt` | ver "L0 completo" | | `build.sh --test` só roda 8 de ~85 alvos de teste |
| `cargo fmt --check` | OK | sem diferença | |
| `cargo build --release` | OK | 3 min 34 s | |
| `cargo test` | OK | 866 + 1 + 29 + 5 testes passam | `clippy` não rodado (launcher não mudou) |
| `perf/harness/tests/` (citado no roadmap) | NÃO EXISTE | | Não há `perf/` em `origin/main`; criado agora como `perf/auto/tests` |
| `perf/auto/tests` (novo) | OK | 19 testes, 0,1 s | Parser de log, gate, cenário, estrutura de captura, `shader_golden` |

## L1: pasta de dados do jogo, sem abrir o jogo

| Ferramenta | Windows | Saída real |
|---|---|---|
| `dump-index-test <dump> <índice>` | OK (com ressalva) | `97567 shaders in 104.2 s`. Fora do shell MSYS2: `error while loading shared libraries: libc++.dll` (precisa do `PATH` de `msys2-env.sh`) |
| `dump-index-test` sem argumentos | OK | `PASS: dump index` |
| `shader-replay-test <cache> <índice>` | OK | 27.826 traduzidos, **0 diferentes** (Fragment 14.032, Vertex+fetch 13.789, Vertex 2, Compute 3), 1,90 ms cada, 9 min de parede (leitura de 71 mil arquivos) |
| `shader-replay-test` código de saída | LIMITADO | sai com 1 quando há diferença: script que só olha o código não separa "diferente" de "quebrou" (o `shader_golden` separa pela linha `translated`) |
| `shader-manifest info shaders/CUSA01623.spm` | OK | `CUSA01623 id 63a3b191d0f97041, 3.6 MB: 25230 programs, 27742 permutations (5937 title), 15463 pipelines` |
| `shader-manifest-test` | OK | `PASS: shader manifest` |
| `spirv-val` / `spirv-dis` | OK | presentes no Vulkan SDK e em `/clang64/bin` |
| `perf/auto/shader_golden.py --self-test` (novo) | OK | fatia de 200 intacta passa; `.spv` com uma palavra trocada é apontado sozinho (`0x00dc0eafb14fc268.meta`) |
| `perf/auto/shader_golden.py --before <build antigo> --after <build novo>` | OK | 2.000 shaders, 0 mudanças entre o build de 2026-11-08 e `b358431d` |

## L2: jogo rodando, sem jogador

| Ferramenta | Windows | Saída real |
|---|---|---|
| `SPARTAN_PAD_FILE` | OK | `Runtime: pad file: buttons 0x40 ...` a cada mudança; menus do título e da pausa respondem com o jogo sem foco |
| `PostMessage` (Enter, S) no diálogo Load | OK | `Runtime: SaveDataDialog list: chose AUTOSAVE` / `chose SD-261111043114` |
| `PrintWindow` (captura da janela sem foco) | OK | PNG 1920x1080 do título, da lista Load, da cena e da pausa |
| Exit to Desktop pelo pad | OK depois de ajuste | a linha pede confirmação ("Are you sure you want to Quit?", No selecionado): `touchpad_right`, 5x `down`, `cross`, `up`, `cross` → `Pause menu: Exit to Desktop`. A 1ª versão (sem a confirmação) caiu no watchdog |
| `SPARTAN_CAPTURE_TRIGGER` | LIMITADO (e caro) | `Frame capture: 7 passes written to ...` na cena estática: só os passes depois do último buffer de vídeo entram, não o quadro inteiro. Estrutura idêntica nas 6 execuções da baseline, contagem de draws inclusive. Feita no meio da janela medida, a captura + PNG derrubaram o FPS e abriram a dispersão para ±7 %; feita depois da janela, ±1,3 % |
| `Frame stats` | LIMITADO | `GPU thread 0.00 us/draw` em todas as janelas no Windows (sem relógio de CPU por thread). FPS, worst frame, draws/frame e compilações vêm certos |
| `Frame pacing` / `Stall:` | OK | p99 e quadros longos por janela de 5 s; `SPARTAN_STALL_MS=25` dá o limiar de 25 ms (máximo 10 linhas por janela) |
| `Disk shaders:` / `Pipeline binary cache:` | OK | presentes; `compiled` é a métrica de estalo de shader |
| Marcadores de tela | OK | `Free camera: title screen ... (SHELL.WAD)` (título), `Free camera: in play (<WAD>)` (nível), `draws/frame` (cena pesada) |
| `tools/press.sh` | OK (leitura) | só escreve `out/pad`; funciona em qualquer bash |
| `tools/restart.sh` | NÃO RODA no Windows | usa `pkill -x spartanport-probe` (`pkill` não existe no Git Bash nem no MSYS2), `setsid` (só MSYS2) e `run.sh` com a pasta de dados = repositório (usaria os saves reais) |
| `tools/soak.sh` | NÃO RODA no Windows | `pgrep` não existe no Git Bash nem no MSYS2: o laço reportaria `DIED` na 1ª volta |
| `tools/ab.sh` | OK | ao vivo no log de uma execução do orquestrador: bit 8192 (preparação de draws) `on: 182.31 FPS`, `off: 167.65 FPS` (+8,7 %), 6 fases de 20 s. `us/draw` sai `0.000` (mesmo limite do Windows) |
| `SPARTAN_PAD_REPLAY` | OK | arquivo sintético `perf/auto/scenarios/checks/camera-turn.pad` (formato de `runtime_pad.c`): `Runtime: pad replay of 17 samples` e `Runtime: pad replay finished`. Não precisa de F9 |
| `SPARTAN_PAD_RECORD` | não testado | precisa de F9 no teclado de uma pessoa |

## Achados

1. **`build.sh` quebra num clone/worktree feito pelo Git do Windows.** Os submódulos saem com CRLF;
   o `git` do MSYS2 que o `build.sh` usa não reconhece os patches de `gpu/patches/fsr-vulkan` como
   aplicados (`apply --reverse --check` falha) e tenta aplicar de novo. Contorno usado: `core.autocrlf
   false` nos submódulos e checkout de novo. **Corrigido depois** com `--ignore-whitespace` nos dois
   `git apply` do `build.sh` (reproduzido num clone CRLF isolado: aplica uma vez e a 2ª passada
   reconhece como aplicado; o checkout LF continua reconhecido; a parte Linux não foi testada).
2. **`us/draw` não existe no Windows.** O roadmap usa µs/draw como métrica principal de A/B; no
   Windows só FPS e frametime valem. O gate ignora o campo quando ele vem vazio.
3. **Ferramentas de jogo do projeto são Linux.** `restart.sh` e `soak.sh` dependem de `pkill`,
   `pgrep`, `setsid` e `taskset`. O orquestrador novo (`perf/auto/run_scenario.py`) foi
   testado só no Windows. No Linux ele compila e lança o jogo (`pgrep`/`pkill`), mas não captura a
   janela e não escolhe save na lista Load (que precisa de `PostMessage`): ali só `start = "new-game"`
   pode funcionar, e nada disso foi executado.
4. **`build.sh --test` cobre pouco.** 8 alvos ninja de ~85 `*-test` no `gpu/CMakeLists.txt`.
5. **Checkout não usa o manifest de shaders.** O jogo lê `shaders/<serial>.spm` ao lado do
   executável (`vk_presenter.cpp`, `SDL_GetBasePath()`), ou seja, `out/shaders/`. Só os scripts de
   `packaging/` copiam o arquivo; `build.sh` não. Num checkout limpo a 1ª abertura compila tudo sob
   demanda (medido: `Disk shaders: 0 programs read ... 459 a 473 compiled` na cena estática). O
   orquestrador espelha `shaders/*.spm` em `out/shaders/` como o pacote faz, e o `build.sh` passou a
   copiar também (Windows e Linux).
6. **1ª abertura com manifest leva ~7,5 min nesta máquina.** `Shader preparation: 27742 written ...
   in 116.9 s (11 threads)` e `Pipeline preparation: 15460 built ... in 331.2 s (11 threads)`. O
   modo frio do orquestrador dá 25 min de folga para isso.
7. **Ruído.** Com uma pessoa usando o PC (navegador com ~1,8 núcleo ocupado), a cena estática varia
   ±1,3 % entre execuções (FPS) e o p99 até ±21 %. O combate com ataque por pad varia ±10 %
   (câmera e inimigos não são determinísticos). O gate usa como limite o maior entre o padrão do
   roadmap e 2x o ruído medido na baseline.
8. **2 compilações em toda abertura com cache quente.** `Frame stats: ... 1 shader/pipeline
   compiles, 0.1 ms` duas vezes durante o carregamento, em todas as execuções (inclusive a
   baseline), com `Disk shaders: ... 0 compiled`. Custo desprezível; o gate conta só as novas em
   relação à baseline, como o roadmap pede ("> 0 novo").

## Aceite do WV (execuções reais, 2026-11-18, RX 6700 XT, build `b358431d` + `perf/`)

Todas sem ninguém no controle: entrada só por `SPARTAN_PAD_FILE` e `PostMessage`, saída pelo menu
de pausa (Exit to Desktop, Yes) em todas as execuções listadas.

| Cenário | Cache | Execuções | FPS mediano (mín a máx) | p99 frametime | Quadros > 40 ms | Compilações em jogo |
|---|---|---|---|---|---|---|
| `cena-estatica` (baseline) | quente | 1 aquec. + 5 | 191,5 (188,2 a 193,1) | 9,03 ms | 0 | 2 (fixas por abertura) |
| `cena-estatica` | frio, com manifest | 3 (+1 INFRA_FAIL repetido) | 186,8 (185,7 a 191,9) | 9,05 ms | 0 | 2; preparo 95 a 101 s (shaders) + 325 a 391 s (pipelines) |
| `cena-estatica` | frio, sem manifest | 3 | 190,8 (187,3 a 191,0) | 9,0 ms | 0 | 4 na janela; 459 a 473 shaders compilados em jogo |
| `abertura-primeira-onda` | quente | 1 aquec. + 3 | 117,9 (103,3 a 127,9) | 13,1 ms | 0 | 2 |
| `troca-de-area` | quente | 1 aquec. + 3 | 203,3 (201,2 a 206,5) | 13,0 ms | 2 | 2 |
| `soak-20min` | quente | 1 (20 min) | 127,7 | 20,4 ms | 0 | 2; sem crash |

- `perf/auto/quick`: **PASS 3 vezes seguidas**, 128 s cada (L0 + L1 em 3.000 shaders + 1 execução
  L2 + gate). Antes disso, 3 execuções reprovaram por limiares fixos que o ruído real passava;
  corrigido como descrito no achado 7.
- `shader_golden --self-test`: PASS (pega um `.spv` alterado em uma palavra e só ele).
- Abertura: New Game, a abertura e o combate da primeira onda na Gaia (POS050) rodaram 4 de 4
  vezes sem jogador, com QTE automático e vida infinita.
- O que não foi feito: `qte-fps` (falta um save parado num QTE conhecido; a rota da abertura passa
  por QTE, mas a troca 120/60/120 não foi isolada), Linux, outra GPU, o port de comparação.
9. **A lista de shaders não derruba o FPS; a máquina derivou.** As 3 últimas execuções do `quick`
   reprovaram (−7 %, −22 %, −12 %). A/B alternado com e sem `out/shaders/CUSA01623.spm`, 3 pares:
   mediana 177,1 FPS com a lista e 178,6 FPS sem (0,8 %, dentro do ruído). As duas ficaram ~7 %
   abaixo da baseline gravada 3 h antes (191,5), sem outro programa acima de 5 % de GPU nos contadores.
   É deriva do ambiente (estado da máquina), que o detector de GPU ocupada não pega. Limite conhecido:
   a baseline vale para o estado da máquina em que foi gravada; antes de um A/B importante, gravar a
   baseline na mesma sessão.
