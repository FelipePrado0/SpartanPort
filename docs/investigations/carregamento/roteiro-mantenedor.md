# Roteiro do mantenedor: carregamento infinito

Data: 2026-10-29. Branch: `diag/infinite-loading`. Uma linha ou duas por item. Preencha uma
[ficha](ficha-reproducao.md) por execução. Copie `user/last_run.log` antes de abrir o jogo de novo.

## 0. Preparar

1. Copie a pasta de saves e a pasta do jogo atual para um lugar seguro; não teste com o save original.
2. Anote CUSA/APP_VER, ferramenta e versão de extração, base + update usados e mods (nenhum na cópia de controle).

## 1. Manifesto da instalação (PowerShell, na pasta do port clonado ou com o `.py` copiado)

3. `python -I tools\game_manifest.py create "D:\Jogos\CUSA01623" -o atual.json` (minutos para ~40 GB; `--quick` = só tamanhos).
4. Código 1 = arquivos ilegíveis (listados em `errors`); guarde o JSON, ele só tem o nome da pasta, não o caminho.

## 2. Reextração em pasta vazia

5. Extraia o mesmo PKG base + update compatível (mesmo title ID) numa pasta **nova e vazia**; guarde o log da ferramenta.
6. `python -I tools\game_manifest.py create "D:\Jogos\CUSA01623-novo" -o novo.json`.
7. `python -I tools\game_manifest.py compare atual.json novo.json` → `missing`/`extra`/`different`/`case-only`; código 0 = idênticos.
8. Rode o mesmo checkpoint nas duas pastas, mesma build e mesma cópia de save. Se só a nova passar, envie a saída do passo 7.

## 3. Release 2.7.0 × build da branch

9. Instale lado a lado `windows-v2.7.0` e a build da branch (anote o SHA-256 dos dois `bin\spartanport-probe.exe`).
10. Mesma pasta do jogo, mesmo save copiado, mesmas opções; o trace `Load:` só existe na build da branch.

## 4. Variáveis de ambiente (build da branch; o launcher repassa o ambiente)

11. `$env:SPARTAN_FILE_TRACE="1"; $env:SPARTAN_LOAD_WATCHDOG="30"; & ".\SpartanPort.exe"` → PLAY.
12. Confira no log `Load: trace on (mode=wad, limit=20000)` e `Load: watchdog on (30 s …)`; sem essas linhas, a build não tem o trace.
13. `SPARTAN_FILE_TRACE=all` registra todos os arquivos (mais volume); `SPARTAN_FILE_TRACE_LIMIT=0` tira o limite de linhas.
14. Defina o N do watchdog maior que o tempo de uma carga normal (meça numa transição que funciona); 30 s é o ponto de partida.

## 5. Durante o travamento

15. Espere pelo menos 2×N segundos; o log deve ganhar `Load: STALL`, `Load: state` e duas levas `Load: thread sample=1/2`.
16. Opcional: Process Explorer → propriedades do processo → Threads → Stack de 2–3 threads com CPU, duas vezes com 10 s de intervalo.

## 6. Que linhas `Load:` enviar

17. Todas as `Load: trace on`/`watchdog on`, o bloco a partir de `Load: STALL` até a última `Load: thread sample=2`.
18. As últimas ~40 linhas `Load: #` antes do travamento (o último `open` do WAD da área e as últimas `begin`/`end`).
19. Qualquer `begin` sem `end` com o mesmo `#id` (operação parada) e as linhas `Load:   open fd=… read=X/Y` (WAD lido inteiro ou não).
20. Mais `Runtime:` de configuração (engine FPS, readback/deferred/stale) e o resumo de saída se o jogo for fechado.

## 7. Como ler o resultado

21. `read=Y/Y` e nenhuma operação em andamento → leitura completa, o bloqueio é depois do I/O (comparar CPU entre as amostras).
22. `begin` sem `end` → o I/O do host está parado; `read=X/Y` com X<Y e sem operação → o jogo parou de pedir bytes.
23. `cpu_ms` subindo entre `sample=1` e `sample=2` numa thread → laço ativo; tudo parado → espera (lock/fence).
