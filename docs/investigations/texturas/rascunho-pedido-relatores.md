# Rascunho: pedido aos relatores (#1059, #1016, #1103, #863, #917, #1080)

**Status: rascunho. NÃO publicar** sem autorização do mantenedor. Só publicar depois que houver um pacote de diagnóstico com SHA e checksum; substituir `<LINK>` e `<SHA>`.

---

**EN:**

Thanks for the reports and for testing `SPARTAN_GC_BUDGET_MB`. Raising the budget delays the texture cache's evictions, which matches what you saw, but it does not tell us which eviction loses the texture data. We would like to find and fix that instead of only raising the limit.

If you can still reproduce the corruption, could you try the diagnostic build `<LINK>` (commit `<SHA>`)? It changes no settings by itself.

1. Install it in a separate folder and copy your `user` folder there (keep your original).
2. Start the launcher from a command prompt after `set SPARTAN_TEX_TRACE=1`. Use the budget you normally use (default, or your `SPARTAN_GC_BUDGET_MB`) and tell us which.
3. Play your usual route until the corruption appears. Note the time and the scene, take a screenshot, then quit.
4. Before starting the game again, attach `user\last_run.log` here and tell us: GPU and driver version, resolution, engine FPS, whether "Fast screen effects" was on, and minutes until the first artifact.

If nothing appears within the time it used to take, that result helps too: please send the log anyway, with the play time. Please don't attach game files or saves.

---

**PT-BR:**

Obrigado pelos relatos e pelos testes com `SPARTAN_GC_BUDGET_MB`. Aumentar o orçamento atrasa os descartes do cache de texturas, o que bate com o que vocês viram, mas não mostra qual descarte perde os dados da textura. Queremos achar e corrigir isso em vez de só aumentar o limite.

Se vocês ainda conseguem reproduzir a corrupção, poderiam testar a build de diagnóstico `<LINK>` (commit `<SHA>`)? Ela não muda nenhuma configuração por conta própria.

1. Instale numa pasta separada e copie para lá a sua pasta `user` (guarde a original).
2. Abra o launcher por um prompt de comando depois de `set SPARTAN_TEX_TRACE=1`. Use o orçamento que vocês costumam usar (padrão, ou o seu `SPARTAN_GC_BUDGET_MB`) e digam qual.
3. Jogue a rota de sempre até a corrupção aparecer. Anote a hora e a cena, tire uma captura de tela e feche o jogo.
4. Antes de abrir o jogo de novo, anexe aqui o `user\last_run.log` e informe: GPU e versão do driver, resolução, FPS da engine, se "Efeitos de tela rápidos" estava ligado e quantos minutos até o primeiro artefato.

Se nada aparecer no tempo em que costumava aparecer, esse resultado também ajuda: mandem o log mesmo assim, com o tempo de jogo. Por favor, não anexem arquivos do jogo nem saves.
