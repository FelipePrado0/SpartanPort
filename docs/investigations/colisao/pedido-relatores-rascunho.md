# Rascunho: pedido aos relatores de #871 e #1000

**NÃO PUBLICADO.** Rascunho para o mantenedor revisar. Publicação só com autorização dele, depois
que existir uma build de diagnóstico (substituir `<LINK DA BUILD>`). Formato bilíngue de
`docs/agents/issue-tracker.md`. Em #1000, trocar o local do passo 2 por "the platform that
disappears" / "a plataforma que desaparece".

---

**EN:**

Thanks for the report and the log. We prepared a diagnostic build that records how game data is
loaded and how the game's memory is mapped around the place where the floor disappears. It does
not fix the problem yet; it helps us find where it starts.

1. Back up your `user` folder (saves and settings) before testing.
2. Download `<LINK DA BUILD>`, extract it to a new folder, and create a `teste.bat` next to
   `SpartanPort.exe` with:
   ```bat
   @echo off
   set SPARTAN_MEM_TRACE=1
   start "" "SpartanPort.exe"
   ```
   (We will add the loading trace variable here when the build is published.)
3. Start the game with `teste.bat`, load the same save, and go to the hallway after the Judges.
4. At that spot, please tell us separately: (a) is the area drawn? (b) does the floor hold
   Kratos when you walk, jump and land? (c) can you reach the next checkpoint? A short video helps.
5. Close the game and attach the whole `user/last_run.log` before starting the game again.
6. Tell us the game ID and update version (for example CUSA01715 01.02), and whether you still
   have the original game files you used when the problem started.

If you can, also extract the same game and update again into an empty folder and repeat steps 3
to 5 with only the game files changed. Please do not upload game files or PKGs; only the log and,
if we ask, a list of file names, sizes and hashes.

---

**PT-BR:**

Obrigado pelo relato e pelo log. Preparamos uma build de diagnóstico que registra como os dados do
jogo são carregados e como a memória do jogo é mapeada perto do lugar onde o chão desaparece. Ela
ainda não corrige o problema; ajuda a achar onde ele começa.

1. Faça backup da pasta `user` (saves e configurações) antes de testar.
2. Baixe `<LINK DA BUILD>`, extraia numa pasta nova e crie um `teste.bat` ao lado do
   `SpartanPort.exe` com:
   ```bat
   @echo off
   set SPARTAN_MEM_TRACE=1
   start "" "SpartanPort.exe"
   ```
   (Vamos acrescentar aqui a variável do trace de carregamento quando a build for publicada.)
3. Abra o jogo pelo `teste.bat`, carregue o mesmo save e vá até o corredor depois dos Juízes.
4. Nesse ponto, diga separadamente: (a) a área aparece? (b) o chão sustenta o Kratos quando você
   anda, salta e aterrissa? (c) você consegue chegar ao checkpoint seguinte? Um vídeo curto ajuda.
5. Feche o jogo e anexe o `user/last_run.log` inteiro antes de abrir o jogo de novo.
6. Informe o ID do jogo e a versão da atualização (por exemplo CUSA01715 01.02), e se você ainda
   tem os arquivos originais do jogo que usava quando o problema começou.

Se puder, extraia de novo o mesmo jogo e a mesma atualização numa pasta vazia e repita os passos 3
a 5 trocando só os arquivos do jogo. Por favor, não envie arquivos do jogo nem PKGs; só o log e,
se pedirmos, uma lista de nomes, tamanhos e hashes dos arquivos.
