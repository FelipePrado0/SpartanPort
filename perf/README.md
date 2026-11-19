# perf: validação sem jogador

Tudo aqui roda sem ninguém no controle nem olhando a tela. O jogo abre de verdade, mas é conduzido
por `SPARTAN_PAD_FILE` e `PostMessage` (nunca entrada global: teclado e mouse continuam livres) e cada
tela é confirmada por uma linha do log. Roadmap: workstream WV do roadmap de GPU.

## Três comandos

No Windows, num Git Bash na raiz do repositório (o Python precisa ser 3.11 ou mais novo). Os dados do
jogador são só copiados, nunca usados no lugar:

```bash
export PERF_SETTINGS=$PWD/settings.json PERF_SAVES=$PWD/user/savedata PERF_CACHE_SEED=$PWD/user/cache
# Monitor da janela de medição: o índice que `out/spartanport-probe --displays` lista (padrão 0).
# Use sempre o mesmo, ou as baselines deixam de ser comparáveis.
export PERF_MONITOR=2

# 1. Ciclo curto (L0 + L1 em paralelo, depois 1 execução de cena-estatica e o gate): ~6 min
bash perf/auto/quick

# 2. Ciclo completo (todos os cenários, cache quente e frio, 1 aquecimento + 3 medidas, cargo)
bash perf/auto/full

# 3. Um cenário só, por exemplo para gravar uma baseline nova
python perf/auto/run_scenario.py --scenario perf/auto/scenarios/cena-estatica.toml --cache warm --runs 3
```

Num worktree sem `settings.json` e `user/`, aponte as três variáveis para a cópia principal e
`PERF_CORPUS` para `user/cache/CUSA01623` dela.

## O que cada nível prova

| Nível | Comando | Prova |
|---|---|---|
| L0 | `bash build.sh --test`, `python -m unittest discover -s perf/auto/tests`, `cargo test` (full) | lógica pura, parser, gate, formatos |
| L1 | `python perf/auto/shader_golden.py` | o recompilador traduz o corpus gravado igual, byte a byte, sem abrir o jogo |
| L2 | `python perf/auto/run_scenario.py` | FPS, frametime p99, quadros acima de 25/40 ms, shaders compilados em jogo, estrutura do quadro, sobrevivência |

## Peças

- `auto/run_scenario.py`: orquestrador. Pasta de dados isolada em `out/perf/{warm,cold}` (`SPARTAN_DATA_DIR`),
  saves de teste congelados uma vez em `out/perf/saves`, ini de medição (1080p nativo, sem upscaler,
  sem frame generation, sem overlay, motor a 240, sem limite de FPS, vida infinita, QTE automático,
  `pause_game=0`). Sai pelo menu de pausa (Exit to Desktop, Yes); só encerra a árvore de processos
  se isso falhar ou o watchdog (`timeout_s`) estourar. Recusa começar se o jogo já estiver aberto e
  descarta as `SPARTAN_*` herdadas do shell (podiam apontar para os saves reais).
- `auto/scenarios/*.toml`: cenários como dados (save, script de pad ou replay, duração, limites).
- `auto/gate.py`: PASS / FAIL / INFRA_FAIL contra `perf/baseline/<cenário>-<cache>.json`. Limiares da
  seção 5 do roadmap, alargados para 2x o ruído medido entre as execuções da baseline (a própria regra
  do roadmap para o que conta como ganho). Os limites vêm da baseline, não do branch testado. Compara
  a estrutura da captura com `capture_ref` da baseline; GPU diferente da baseline é INFRA_FAIL; crash
  do jogo é FAIL e nunca é repetido.
- `auto/shader_golden.py`: L1; `--self-test` prova o detector (fatia intacta passa, um `.spv` com uma
  palavra trocada é apontado sozinho).
- `auto/logparse.py`, `auto/capstruct.py`, `auto/window.py` (PostMessage e PrintWindow, só Windows).

## Limites conhecidos

- `GPU thread us/draw` sai `0.00` no Windows (sem relógio de CPU por thread): o campo fica vazio e o
  gate o ignora (e avisa em `note:`). No Linux o código lê o relógio por thread; não testado aqui.
- Quadros acima de 25/40 ms vêm das linhas `Stall:`; o jogo imprime no máximo 10 por janela de 5 s.
- A captura estrutural (`SPARTAN_CAPTURE_TRIGGER`) registra só os passes depois do último buffer de
  vídeo; serve para pegar mudança de formato e de shader nesses passes, não do quadro inteiro.
- O port de comparação do cuesta4 não tem entrada por arquivo verificada: `--target rehooked` para com "blocked".
