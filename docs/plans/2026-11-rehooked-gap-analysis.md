# Análise de lacunas: SpartanPort e o port rehooked do cuesta4 (W1)

Data: 2026-11-18. SpartanPort em `b358431d`; rehooked em `a8caa54` (v0.1.2-alpha), clone raso
(o roadmap citava `fb6ee9a`, v0.1-alpha, que não está no clone; os caminhos citados no roadmap
existem iguais em `a8caa54`). Só leitura de código dos dois lados: nada do rehooked foi compilado,
executado ou copiado. Linhas citadas podem variar em poucas posições.

Tags: **[VERIFICADO]** lido no código; **[INFERIDO]** dedução; **[A CONFIRMAR]** precisa de medida.

## Mecanismo por mecanismo

| Mecanismo (rehooked) | Onde no rehooked | Equivalente no SpartanPort | Classificação | Fatia do tempo | Tag |
|:--|:--|:--|:--|:--|:--|
| 1. Preparo antecipado derivado dos dados do jogo (lê tabelas do EBOOT e dos WADs e enumera técnicas, materiais e meshes) | `src/gpu/shaders/shader_aot.h:15`, `shader_catalog.h`, `shader_contract.h:10` | Manifesto distribuído + materialização (`vk_shader_manifest.h`, `vk_shader_materialize.h`, plano `2026-11-01-shader-precompile.md`); a lista vem de sessões gravadas, não dos dados do jogo | existe, mas diferente | compilação e carga de shader (estalo), não o quadro em regime | [VERIFICADO] |
| 2. Receitas (campos da chave, sobrevivem a troca de formato; o que o JIT traduziu vira receita da próxima execução; taxa de acerto medida) | `shader_recipes.h` | `.meta` com `ReplayInputs` (`vk_shader_manifest.h`), retomada em `vk_shader_materialize.cpp`; cache incompatível é refeito (`cache_storage.h`). Não há laço "miss do JIT vira receita" nem taxa de acerto | existe, mas diferente | compilação de shader | [VERIFICADO] o que existe; ausência do laço [INFERIDO] |
| 3. Cache com zstd + dicionário treinado, deduplicação de SPIR-V, leitura antecipada por WAD | `shader_cache.h` | `cache_storage.cpp` (zip miniz ou arquivos soltos, sem dicionário); binário do driver deduplicado, SPIR-V não; prefetch de tudo no início, não por WAD (`vk_pipeline_serialization.cpp`) | existe, mas diferente | carga de shader e E/S | [VERIFICADO] |
| 4. Preparo dividido em até 4 execuções (25 % das variantes por vez) | `shader_aot.cpp:416-420`, `config.h:78` | Esc pula e a próxima abertura tenta de novo; prioriza a tela título; sem orçamento por execução | não existe | tempo da 1ª abertura (medido no SpartanPort: ~7,5 min nesta máquina) | [VERIFICADO] |
| 5. Espelho da memória do jogo + hash por pedaço de 4 KB (regiões do quadro nunca protegidas; página escrita em 2 quadros seguidos fica aberta; pedaço igual não é reenviado) | `src/gpu/buffers.h`, `buffers.cpp`, `piece_hash.h` | Janela de falha de 256 KiB (`buffer_cache.cpp`); "hot pages" opt-in rejeitado (`region_manager.h:224`, `parallel_gpu.md`); sem hash por pedaço | existe, mas diferente | ligação de buffers e cópias | [VERIFICADO] |
| 6. Barreiras preguiçosas por passe | `src/gpu/hazards.h`, `hazards.cpp` | Rastreador por intervalo de endereço vindo do DXVK (`vk_barrier_tracker.h`), memo de acesso, `FlushBarriers` | existe, mas diferente | barreiras | [VERIFICADO] código; efeito [A CONFIRMAR] |
| 7. Gravação paralela com N command buffers e plano de threads por topologia (núcleos rápidos primeiro, assentos, prioridade) | `src/main/cpu.h`, `vk_gpu.cpp` | Pipeline de draw em 2 estágios (`vk_draw_pipe.h`), workers de preparação; dimensionamento só por contagem de afinidade (`gpu/shim/spartanport_threads.h`) | existe, mas diferente (1 gravador) | estágio B do pipeline de draw | [VERIFICADO] |
| 8. Laço de quadro acoplado (cópias sincronizadas com o reset das arenas) | `gpu_thread.h`, `patches/hooks.cpp` (endereços fixos da 1.02) | só a ideia de cópias antes de fences | não existe; **não portar** (endereço fixo) | cópias e espera de fence | [VERIFICADO] |

## Observações

- **Por que o "hot pages" deles funciona e o nosso falhou** [INFERIDO]: eles combinam página aberta,
  pulo por hash de 4 KB e regiões do quadro nunca protegidas. O nosso reenviava a página inteira a
  cada uso (14 mil páginas, 33 FPS). A peça que falta é o hash por pedaço. Candidato W4-a.
- **Binário do driver por shader object** (`VK_EXT_shader_object`, `shader_cache.h:87`)
  [VERIFICADO no cabeçalho]: evita a explosão de pipelines que a F6 do plano de pré-compilação ainda
  enfrenta (15.460 pipelines em 325 a 391 s na 1ª abertura medida). Efeito prático [A CONFIRMAR].
- **Licença** [VERIFICADO]: ainda sem LICENSE na raiz; pasta `licenses/` com 11 textos (GPL-2.0-or-later,
  MIT, Zlib e outros); README declara GPL v2.0 ou posterior; `piece_hash.h` embute rapidhash (MIT).
- **Desempenho declarado no README** (não medido aqui): Ryzen 5 5600 + RTX 3060, 150 FPS em 4K e
  403 FPS em 480p na mesma cena.

## Decisão G1

**G1 = "sem dados".** O W0 não mediu o rehooked. Ele só aceita o eboot da 1.02 real
(sha256 `6e72330e…`), e o único dump desta máquina tem o eboot `d85c8135…`, que é o código da 1.00
(procurado em todos os discos em 2026-11-18). Sem um dump 1.02 real, a comparação não roda. Pela regra do
roadmap, W4 fica bloqueado; W2 e W3 seguem. O perfil por thread do SpartanPort (passo 2 do W1) também
não foi feito: no Windows o `us/draw` sai `0.00` e o perfil precisa de ETW/WPA ou de Linux.

Quando houver dados: candidatos em ordem, se a fatia no perfil passar de 5 %: W4-a (hash por pedaço),
W4-b (barreiras por passe), W4-c (plano por topologia), W4-d (cache), W4-e (enumeração pelos dados,
só relatório de viabilidade).
