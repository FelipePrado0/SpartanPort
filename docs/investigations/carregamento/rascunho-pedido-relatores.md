# Rascunho: pedido aos relatores (#345, #982, #990)

**NÃO PUBLICADO.** Rascunho para o mantenedor revisar. Publicar só com autorização explícita, depois
que houver uma build de diagnóstico publicada (substituir `<LINK DA BUILD>` e `<SHA>`). Formato do
`docs/agents/issue-tracker.md`: inglês primeiro, depois português, um único `---` entre os dois.

## Comentário para a #345 (Hades → HDS690)

```markdown
**EN:**

Thanks for the detailed log. It shows HDS690.WAD read in full (98,197,264 bytes, no errors) and then no progress, so we want to tell a broken install apart from a wait inside the port. Three things would help, in this order:

1. **Inventory of your game folder** (nothing is uploaded; it only reads the files). With Python 3 installed, in PowerShell: `python -I game_manifest.py create "<your game folder>" -o current.json`. The script is `tools/game_manifest.py` in the repository. Attach `current.json`: it holds file names, sizes and SHA-256 hashes, not your folder's full path.
2. **Fresh extraction in a new, empty folder** from the same base game and update you used before (same title ID, no mods), then `python -I game_manifest.py create "<new folder>" -o fresh.json` and try the same save there. Tell us whether it passes, and attach `fresh.json` and the extraction tool's name/version.
3. **Diagnostic build** `<LINK DA BUILD>` (`<SHA>`): in PowerShell, `$env:SPARTAN_FILE_TRACE="1"; $env:SPARTAN_LOAD_WATCHDOG="30"; & ".\SpartanPort.exe"`, load a **copy** of the save, and when it hangs wait at least one minute before closing. Attach `user/last_run.log` before starting the game again. It should contain lines starting with `Load: STALL` and `Load: thread`.

Please do not attach game files or your original save. Copies of the save are safest, since the game may overwrite the autosave.

---

**PT-BR:**

Obrigado pelo log detalhado. Ele mostra o HDS690.WAD lido por inteiro (98.197.264 bytes, sem erros) e depois nenhum progresso; queremos separar instalação com defeito de uma espera dentro do port. Três coisas ajudariam, nesta ordem:

1. **Inventário da pasta do jogo** (nada é enviado; ele só lê os arquivos). Com Python 3 instalado, no PowerShell: `python -I game_manifest.py create "<pasta do jogo>" -o atual.json`. O script é o `tools/game_manifest.py` do repositório. Anexe o `atual.json`: ele tem nomes, tamanhos e hashes SHA-256 dos arquivos, não o caminho completo da sua pasta.
2. **Extração nova numa pasta nova e vazia**, do mesmo jogo base e da mesma atualização que você usou (mesmo title ID, sem mods); depois `python -I game_manifest.py create "<pasta nova>" -o nova.json` e teste o mesmo save nela. Diga se passou e anexe o `nova.json` e o nome/versão da ferramenta de extração.
3. **Build de diagnóstico** `<LINK DA BUILD>` (`<SHA>`): no PowerShell, `$env:SPARTAN_FILE_TRACE="1"; $env:SPARTAN_LOAD_WATCHDOG="30"; & ".\SpartanPort.exe"`, carregue uma **cópia** do save e, quando travar, espere pelo menos um minuto antes de fechar. Anexe o `user/last_run.log` antes de abrir o jogo de novo. Ele deve ter linhas começando com `Load: STALL` e `Load: thread`.

Por favor, não anexe arquivos do jogo nem o save original. Use cópias do save, porque o jogo pode sobrescrever o autosave.
```

## Comentário para a #982 (Helios → BAS400)

```markdown
**EN:**

Thanks for the report. One detail to sort out first: the attached log has deferred and stale readback **on**, while the report says they were off. A new log with the setting you want to test lets us read the result correctly.

1. **Inventory and fresh extraction**, as described here: `python -I game_manifest.py create "<your game folder>" -o current.json` (script: `tools/game_manifest.py` in the repository; it only reads, nothing is uploaded). Then extract the same base game and update (CUSA01715 01.02, no mods) into a new, empty folder, run the same command with `-o fresh.json`, and try the same save copy there. Attach both JSON files and say whether the fresh folder passes Helios.
2. **Diagnostic build** `<LINK DA BUILD>` (`<SHA>`): in PowerShell, `$env:SPARTAN_FILE_TRACE="1"; $env:SPARTAN_LOAD_WATCHDOG="30"; & ".\SpartanPort.exe"`, reach the hang with a save copy and wait at least one minute. Attach `user/last_run.log` (it should have `Load: trace on` near the top and `Load: STALL` near the end) and tell us the readback options you chose.

---

**PT-BR:**

Obrigado pelo relato. Um detalhe primeiro: o log anexado tem deferred e stale readback **ligados**, mas o relato diz que estavam desligados. Um log novo com a opção que você quer testar permite ler o resultado corretamente.

1. **Inventário e extração nova**: `python -I game_manifest.py create "<pasta do jogo>" -o atual.json` (script: `tools/game_manifest.py` do repositório; ele só lê, nada é enviado). Depois extraia o mesmo jogo base e atualização (CUSA01715 01.02, sem mods) numa pasta nova e vazia, rode o mesmo comando com `-o nova.json` e teste a mesma cópia do save nela. Anexe os dois JSON e diga se a pasta nova passa de Helios.
2. **Build de diagnóstico** `<LINK DA BUILD>` (`<SHA>`): no PowerShell, `$env:SPARTAN_FILE_TRACE="1"; $env:SPARTAN_LOAD_WATCHDOG="30"; & ".\SpartanPort.exe"`, chegue ao travamento com uma cópia do save e espere pelo menos um minuto. Anexe o `user/last_run.log` (ele deve ter `Load: trace on` no começo e `Load: STALL` no fim) e diga quais opções de readback você escolheu.
```

## Comentário para a #990 (Coração de Gaia)

```markdown
**EN:**

Thanks for the report. The text pasted in the form shows an `AUTOSAVE` load and `Pos700`, but the attached `last_run.log` does not contain that sequence and ends with a normal exit report (it also reports CUSA01623 01.00). Could you send the log of a run that actually hangs?

1. Copy your save folder first. Start the game, load the save that hangs, wait at least one minute at the loading screen, close the game, then attach `user/last_run.log` **before** starting the game again (each start replaces it).
2. If you can, also use the diagnostic build `<LINK DA BUILD>` (`<SHA>`) with `$env:SPARTAN_FILE_TRACE="1"; $env:SPARTAN_LOAD_WATCHDOG="30"; & ".\SpartanPort.exe"` in PowerShell, and the folder inventory `python -I game_manifest.py create "<your game folder>" -o current.json` (script `tools/game_manifest.py`; it only reads, nothing is uploaded).
3. Tell us which update you use (01.00 or 01.02) and whether the same save hangs every time.

---

**PT-BR:**

Obrigado pelo relato. O texto colado no formulário mostra o carregamento de um `AUTOSAVE` e `Pos700`, mas o `last_run.log` anexado não tem essa sequência e termina com o relatório de saída normal (e indica CUSA01623 01.00). Você pode enviar o log de uma execução que realmente trava?

1. Copie antes a pasta de saves. Abra o jogo, carregue o save que trava, espere pelo menos um minuto na tela de carregamento, feche o jogo e anexe o `user/last_run.log` **antes** de abrir o jogo de novo (cada início substitui o arquivo).
2. Se puder, use também a build de diagnóstico `<LINK DA BUILD>` (`<SHA>`) com `$env:SPARTAN_FILE_TRACE="1"; $env:SPARTAN_LOAD_WATCHDOG="30"; & ".\SpartanPort.exe"` no PowerShell, e o inventário da pasta `python -I game_manifest.py create "<pasta do jogo>" -o atual.json` (script `tools/game_manifest.py`; ele só lê, nada é enviado).
3. Diga qual atualização você usa (01.00 ou 01.02) e se o mesmo save trava todas as vezes.
```
