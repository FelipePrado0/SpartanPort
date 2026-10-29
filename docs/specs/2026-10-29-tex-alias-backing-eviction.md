# Spec: backing de alias descartado com writer vivo

Frente: corrupção progressiva de texturas (roadmap `docs/plans/2026-10-29-roadmap-corrupcao-texturas.md`, fase "testes de regressão antes da correção"). Branch `diag/texture-corruption`.

## SDD

* **Objetivo:** como jogador, quero que imagens que compartilham memória (aliases, p. ex. views de altura parcial do Fast Screen Effects) continuem recebendo a escrita mais nova da GPU quando o cache descarta uma delas, para que superfícies não fiquem com conteúdo antigo até recarregar a área.
* **Contrato:** `ForgetAliasMember(AliasState&, ImageId, u64 uid, is_live)` em `gpu/shadps4/video_core/texture_cache/alias_state.h`, chamada por `TextureCache::DeleteImage`. Entrada: o estado do grupo de aliases e a imagem que sai do cache. Saída: o estado deixa de nomear a imagem; se a imagem era o `backing` e o `writer` (conteúdo mais novo, ainda não copiado) está vivo, o `writer` vira o `backing`, como `SynchronizeAlias` já faz quando o slot do backing foi apagado. Se a imagem era o `writer`, a autoridade é zerada (comportamento atual, inalterado).
* **Critérios de aceite:**
  * QUANDO o backing de um grupo com 3 ou mais membros for descartado e o writer estiver vivo, O SISTEMA DEVE manter o writer como autoridade (backing = writer, writer vazio).
  * QUANDO o writer for descartado, O SISTEMA DEVE deixar de nomeá-lo (inalterado: autoridade zerada).
  * QUANDO o slot de uma imagem descartada for reutilizado por outra imagem (outro UID), O SISTEMA NÃO DEVE tratá-la como writer/backing.
  * QUANDO a imagem descartada não for writer nem backing (ou for o mesmo slot com outro UID), O SISTEMA NÃO DEVE alterar a autoridade.
* **Requisitos não funcionais:** nenhuma cópia nem comando de GPU em `DeleteImage` (roda também em threads do guest via `UnmapMemory`); custo O(1); sem alocação; sem mudança em grupos de 2 membros (o estado é apagado como hoje).
* **Arquivos afetados:** `gpu/shadps4/video_core/texture_cache/alias_state.h`, `gpu/shadps4/video_core/texture_cache/texture_cache.cpp` (chamada em `DeleteImage`), `tests/test_aliasing.cpp`.
* **Plano técnico:** em `ForgetAliasMember`, se a imagem é o writer: `ResetAuthority()` (como hoje). Se é o backing: limpar `backing` e chamar `ResolveAliasBacking` com `is_live` (promove o writer vivo ou zera). Mecanismo do defeito na base: `DeleteImage` zerava toda a autoridade ao descartar o backing; o próximo `SynchronizeAlias` de um alias sobrevivente não achava backing nem writer e retornava sem copiar, e `ApplyBackingWrites` ignora imagens `GpuModified`/`Aliased`. O sobrevivente ficava com conteúdo anterior à última escrita do writer até o writer ser reescrito. Riscos: (1) o writer promovido pode cobrir menos linhas que o backing antigo; é o mesmo estado que `SynchronizeAlias` já aceita quando o slot do backing é apagado; as linhas fora do writer ficam como estavam em cada imagem (igual à base). (2) O caso "writer descartado" continua perdendo a escrita mais nova para os sobreviventes (os bytes vão à RAM pelo download síncrono do GC, mas sobreviventes `GpuModified` não releem a RAM); corrigir exige copiar o writer antes do descarte no GC (comando de GPU) e fica como pendência, sem teste que prove o efeito no jogo. (3) Sem GPU real nem jogo nesta rodada: a correção é provada só no nível de estado; o efeito visual precisa do roteiro do mantenedor.
* **Validação:** `aliasing-test` (Red na base, Green com a correção), `readback-queue-test`, `readback-sources-test`, compilação do objeto `texture_cache.cpp`. No jogo: roteiro em `docs/investigations/texturas/roteiro-mantenedor.md` (FSE ligado/desligado, sessão longa com transições).
* **Aprovação:** aprovada pelo líder por delegação do mantenedor (29/10/2026), dentro do escopo da frente de texturas.

## TDD

* **Comportamento:** QUANDO o backing for descartado e o writer estiver vivo, O SISTEMA DEVE manter o writer como autoridade.
* **Teste:** `tests/test_aliasing.cpp`, `TestAliasAuthority()`, bloco "The backing is evicted while the writer (the newest contents) lives on" (alvo `aliasing-test`).
* **Red:** na base com o refactor `3b40351` (comportamento inalterado):
  ```
  /home/user/wt/heavy.sh "... ninja -C out/gpu aliasing-test readback-queue-test readback-sources-test; ./aliasing-test ..."
  aliasing-test: /home/user/wt/texturas/tests/test_aliasing.cpp:75: void TestAliasAuthority(): Assertion `ResolveAliasBacking(state, slots.Live())' failed.
  aliasing-test rc=134
  readback-queue-test rc=0
  readback-sources-test rc=0
  ```
* **Green:** `ForgetAliasMember` passou a receber `is_live`; writer descartado → `ResetAuthority()` (inalterado); backing descartado → limpa o backing e chama `ResolveAliasBacking` (promove o writer vivo ou zera). `DeleteImage` passa `IsLiveImage`. Testes adicionais: backing descartado sem writer (zera), referência antiga por slot reutilizado.
  ```
  aliasing-test: OK
  aliasing-test rc=0
  readback-queue-test rc=0
  readback-sources-test rc=0
  ```
* **Refactor:** nenhum além do commit prévio `3b40351` (extração sem mudança de comportamento de `AliasState`, `ResolveAliasBacking` e `ForgetAliasMember` para `alias_state.h`).
* **Validação final:** `/home/user/wt/heavy.sh "nix-shell shell.nix --run 'ninja -C out/gpu aliasing-test readback-queue-test readback-sources-test CMakeFiles/spartanportgpu.dir/shadps4/video_core/texture_cache/texture_cache.cpp.o'"` → build=0; os três testes rc=0. Não executado: build completa da biblioteca/pacote Windows, Vulkan real, jogo. O efeito visual depende do roteiro do mantenedor.
