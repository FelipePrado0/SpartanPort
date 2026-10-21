# Linhas pretas nas RX 6600 e outras RDNA2 (AMD no Windows)

Diagnóstico de 2026-10-12 e 2026-10-13, feito por leitura do código, dos relatos e dos logs, mais
quatro builds de teste rodados por quem tem a placa. Nenhuma Navi 23 está disponível localmente
(a máquina de desenvolvimento tem uma GTX 1650; a de teste tem uma RX 6700 XT, Navi 22, que não
mostrou o defeito, mas não foi rodada nas condições dos reporters: veja a seção 9.1). A corrupção
na Intel Arc está na PR separada da Intel (`docs/plans/2026-10-12-gpu-corruption-intel.md`
naquela branch). A seção 9 é a revisão de 2026-10-13 e corrige fatos das seções 1 e 6. **A seção
10 tem a causa encontrada por kalil03 e a correção candidata (`SPARTAN_NO_MSAA`), no build de teste
`windows-v2.1.7-teste3` (que substitui o `amd-teste1` e o `teste2`, apagados, e liga a correção também na Intel).**

Issues: [#65](https://github.com/FelipePrado0/SpartanPort/issues/65) (canônica),
[#61](https://github.com/FelipePrado0/SpartanPort/issues/61),
[#43](https://github.com/FelipePrado0/SpartanPort/issues/43),
[#27](https://github.com/FelipePrado0/SpartanPort/issues/27),
[#70](https://github.com/FelipePrado0/SpartanPort/issues/70) e
[#97](https://github.com/FelipePrado0/SpartanPort/issues/97) (fechadas como duplicatas da #65),
[#91](https://github.com/FelipePrado0/SpartanPort/issues/91) (RX 6750 XT, HUD); shadPS4 eltutz:
[cuesta4/shadPS4#8](https://github.com/cuesta4/shadPS4/issues/8).

## 1. Sintoma

| | AMD RDNA2 no Windows |
|---|---|
| Placas | RX 6600, 6600M, 6600 XT, 6650 XT (Navi 23). Também relatadas: RX 6800 XT com print no menu e RX 6900 XT só por relato (#70, Navi 21); RX 6750 XT com traços nas barras do HUD a 2560x1440 (#91, Navi 22, ainda sem comparação com um HUD saudável). A 6700 XT "limpa" da máquina de teste não rodou a 1080p, sem patch e com upscaler desligado. |
| Drivers | Adrenalin 26.6.4, 26.8.1 e 26.9.2 (Vulkan 2.0.353), com `VK_KHR_pipeline_binary` e `VK_EXT_subgroup_size_control`. Só Windows até agora. |
| Aparência | Faixas escuras de 4 linhas com período de 8: escuras as linhas com `y mod 8` em 0 a 3, intactas as de 4 a 7 (print 1920x1080 da #61: y = 920-923, 928-931, ...). Contínuas na horizontal por dezenas de px, só dentro das pegadas das luzes (pele do Kratos no menu, céu, fogo, brilho do HUD, especulares; nunca nas sombras). Nas linhas intactas o valor é o da área ao redor; na pedra dentro da pegada de uma luz, as linhas escuras ficam com a cor da pedra fora da pegada (falta a contribuição daquela luz). |
| Resolução | 1080p mostra. Abaixo de 1080p as faixas ficam maiores na tela (o período de 8 linhas é do render). 1800p: somem (FilipiEPTeixeira, 2.1.7). 4K: somem para FilipiEPTeixeira; para wyz-ez sobra um bloco na parte de baixo (seção 9.1). Os patches de resolução também mudam a Video Arena (2,25 / 2,75 / 3,25 / 3,75 GiB) e exigem mais DMEM. |
| Comportamento | Aleatório por lançamento (quase sempre aparece), estático durante a execução, no mesmo lugar da cena. Num lançamento sem linhas, o defeito das texturas amarelas apareceu no lugar (#70, com a "Texture Corruption Fix" desligada). |
| Causa | Leitura por shader de um depth D32S8 de 2 amostras (`texelFetch` em `sampler2DMS`) devolve 0 na metade de baixo de muitos tiles de 8x8 no driver AMD do Windows; a profundidade linear sai com blocos no near plane e luz, névoa e bloom herdam as faixas (seção 10). |
| Em outros builds | Igual no shadPS4 eltutz **e no shadPS4 0.19.0 oficial** (jleandrocampos, 6650 XT, 2026-10-13). Contradição: matheusnovak95 (cuesta4/shadPS4#8, RX 6600) diz que o 0.19.0 oficial não mostra as linhas e roda bem mais devagar; execução única, evidência fraca. O build `KrachDev/shadPS4_GOW3` `v1.0.0-gow3-fix` diz eliminar as linhas numa RX 6600 (seção 9.3, pista 1). |

## 2. Fatos apurados

- O quadro do jogo (docs/upscaler.md): pré-passe de profundidade, luz e sombras, materiais,
  transparências, pós com pirâmide de bloom em meia resolução, combine da pós em **compute**
  (hash `9a9cf8a9`) para a cor da cena RGBA8 sRGB, HUD por cima na mesma imagem.
- `Physical device subgroup size 64 (32 to 64, pinnable stages { Compute })` (build de teste 1,
  RX 6600): o driver AMD só aceita `requiredSubgroupSize` em compute. O pin de 64 nos estágios
  gráficos, herdado do shadPS4, é uso inválido pela especificação
  (VUID-VkPipelineShaderStageCreateInfo-pNext-02755); o driver escolhe o wave size do pixel
  shader sozinho. Uma wave32 de PS cobre 8x4 px, a geometria medida nos prints. **Testado e
  descartado como causa** (seção 4).
- `VkPipelineExecutablePropertiesKHR::subgroupSize` vem 0 para todo executável gráfico na AMD
  (Windows), 64 só em compute: o histograma de `SPARTAN_WAVE_STATS=1` é cego para vertex e
  fragment nessa placa. `VK_AMD_shader_info` (build 3) devolve estatísticas e ISA por estágio.
- Nenhum fragment shader depende do tamanho da wave: varredura de 1152 módulos SPIR-V do cache
  (jleandrocampos): 76 `OpGroupNonUniformQuadBroadcast` (quad, independe da wave) e 4 ballots,
  todos em compute. A varredura não procurou os builtins da AMD (`BaryCoord*AMD`,
  `InterpolateAtVertexAMD`): veja a seção 9.3, pista 6.
- O eltutz tem 104 commits próprios sobre a base de 2026-08-21 (256 atrás do main). O fork
  vendora o upstream `b4e7ae7` (2026-10-02, a data do 0.19.0) mais três correções do eltutz:
  alias de imagens (`189ca3b6`), depth sem uso não anexado (`eccbeae3`) e blend MIN/MAX com
  escala, com um "square pass" (`45f3d1f9`, chave `SPARTAN_SQUARE_PASS=0`).
- Limiares do coletor de VRAM numa placa discreta de 8 GB (`texture_cache.cpp`, construtor):
  `trigger_gc_memory = 0` (o coletor roda a cada submissão), pressão em cerca de 2,7 GB e
  crítico em cerca de 5,4 GB. Nos 12 GB: gatilho cerca de 1,35 GB, pressão 5,9 GB. Imagens
  tiled escritas pela GPU nunca são despejadas; alvos por frame nunca entram na LRU.
- Oito logs lidos por inteiro (seis reporters; um deles da Intel Arc B390, os demais de RX
  6600/6600M): execuções saudáveis, sem erro, sem pressão de memória, Fifo e Immediate, janela e
  tela cheia, quatro builds de eboot diferentes. Nenhuma linha se correlaciona com o defeito.
  Em todos os logs AMD aparece `Unimplemented IT_SET_PREDICATION` (o jogo predica cerca de mil
  draws por frame e todos rodam; é igual em todos os fornecedores). O log do Momexdxd (#65,
  RX 6650 XT, release) tem um `Unimplemented depth overlap copy` (`vk_runtime.cpp`,
  `CopyColorAndDepth`), ausente dos outros logs: uma cópia entre uma imagem de profundidade e
  outra que não é o par 1x cor para profundidade multisample.
- O pacote de release não inclui cache de shaders; cada reporter compila o próprio.
- O port só grava linhas de GPU em nível Warning no `last_run.log`
  (`gpu/shim/common/logging/log.cpp`); `SPARTAN_GPU_LOG=info` libera. Marcadores para testers
  precisam ser `std::printf("GPU: ...")`. Até o build 4 só o `Play SpartanPort.exe`
  (`--play`) gravava o arquivo; o botão PLAY da janela mandava a saída para a aba Log. Corrigido
  nesta PR. O arquivo fica na pasta de saves (a "Saves folder" do launcher, por exemplo
  `Documents\game saves`), não numa pasta `user\` ao lado do jogo.
- Qual build gerou um log, pelos números de linha dos avisos do `vk_instance.cpp`: 2.1.6, 2.1.7
  e `main` têm `Extension ... unavailable` em `:304` e `CollectImageFormatInfo` em `:855`; o
  build 2 tem `:309`/`:878`; os builds 3 e 4 têm `:309`/`:880`. Do build 2 em diante a linha
  `GPU: subgroup policy: ...` aparece em qualquer nível de log.

## 3. Hipóteses refutadas por leitura (2026-10-12)

Nenhuma sobreviveu à verificação adversarial. Motivos, para não repetir o trabalho:

| Hipótese | Por que caiu |
|---|---|
| Draws pulados pela compilação assíncrona deixam alvos indefinidos | Imagens novas nascem `Dirty` e passam por `RefreshImage` antes do primeiro uso; alvos de bloom são refeitos a cada frame; só existe no fork, não no eltutz; o primeiro uso de um pipeline é pulado em toda máquina, inclusive na 6700 XT. |
| Coletor de VRAM em 8 GB | A conta está certa, mas alvos tiled escritos pela GPU nunca são despejados, só imagens paradas há 16 a 160 ticks entram, imagens recriadas voltam da memória do guest, e o sintoma aparece no menu com pouca memória em uso. |
| VMA derramando para RAM | Só custa desempenho. Endurecimento: passar `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` em `vk_instance.cpp` (a extensão está ligada, a flag não). |
| Cópia de alias (`aliasing.cpp`) ou memo com imagem velha | Cópias de extensão inteira a partir da origem, só com layout igual. Não geram blocos de 8x4. |
| Memos, draw pipe, gravação em thread | Determinísticos e iguais na 6700 XT. Lacuna real confirmada, mas de outro efeito: uma meta-clear via compute ou `FillBuffer` com o render pass aberto e acerto no `begin_memo` pula `IsMetaCleared/TouchMeta` (alvo inteiro sem clear, não linhas). |
| Chave do cache de binários | `CreateCachedGraphicsPipeline` recebe a `GraphicsPipelineKey` e não a usa; o arquivo leva só a chave do driver (`vk_pipeline_serialization.cpp`). Efeito seria determinístico, na 6700 XT também, e não existe no eltutz. Endurecimento: incluir o hash da chave no nome e remover `LoadLegacyBinary`. |
| Toggle de binários sem efeito | Verdadeiro só sem `VK_KHR_pipeline_binary`; o log da RX 6600 tem a extensão. |
| Combine `9a9cf8a9` em wave32 | Todo compute passa pelo mesmo caminho com `requiredSubgroupSize=64`; compute não usa binários. |

## 4. Testes em hardware (2026-10-13)

**Bisseção nas otimizações do port (FilipiEPTeixeira e Gabrielsln, duas RX 6600, 2.1.6, #65):
os 7 testes falharam nas duas**, 5 aberturas cada, linhas em todas. Um de cada vez, cache de
shaders limpo antes:

| # | Teste | Como | Resultado |
|---|---|---|---|
| 1 | Sem cache de binários e sem carga em segundo plano | desmarcar "Use pipeline binary cache" e "Load shader cache in the background" | linhas |
| 2 | Sem compilação em segundo plano | desmarcar "Compile new shaders in the background" | linhas |
| 3 | Sem o square pass do MIN/MAX | `SPARTAN_SQUARE_PASS=0` | linhas |
| 4 | Sem o coletor de VRAM | `SPARTAN_GC_BUDGET_MB=20000` | linhas |
| 5 | Draw pipe desligado | "Pipeline de GPU em dois estágios" = Off | linhas |
| 6 | Gravação na thread principal | `SPARTAN_VK_RECORD_THREAD=0` | linhas |
| 7 | Leituras com espera | desmarcar "Game reads GPU data without waiting" e "Deferred GPU readbacks" | linhas |

Os toggles ao vivo não alcançam o que é decidido na criação das imagens (flags, formato,
tiling): esses sete testes eliminam as otimizações, não o núcleo compartilhado.

**Pin de subgrupo (build de teste 2, `windows-v2.1.6-test2`):** três execuções com cache limpo,
(1) política normal, (2) `SPARTAN_SUBGROUP_PIN=allowed`, (3) `SPARTAN_SUBGROUP_PIN=none` com
`SPARTAN_LDS_BARRIERS=1`. MarcosAlexandre30 (RX 6600): linhas nas três. jleandrocampos (RX 6650 XT):
linhas na 1 e na 3, com o log confirmando a política aplicada (`compute pinned to 64, fragment
pinned to 64` contra `compute pinned to 0, fragment pinned to 0, LDS barriers on`). **O pin não
é a causa.** As variáveis ficaram na PR Intel; esta PR não as traz. Ressalva: a AMD ignora
`requiredSubgroupSize` em gráficos, então o teste nunca variou o wave size do pixel shader.

**42 toggles desligados ao vivo** (`SPARTAN_TOGGLE_FILE` = 182395781688328191) e
`SPARTAN_SQUARE_PASS=0` (jleandrocampos): linhas continuam.

**shadPS4 0.19.0 oficial** na 6650 XT, mesma cena: linhas. Não há regressão do eltutz nem do
fork a procurar; a causa está no núcleo compartilhado ou no driver.

**Builds de teste 3 e 4** (`windows-v2.1.7-test3`: ISA via `VK_AMD_shader_info`;
`windows-v2.1.7-test4`: `SPARTAN_FULL_BARRIERS=1`), resultados até 2026-10-13 04:30 UTC:

- Ed1z19 (#61): o log "com configurações padrão" das 02:12 UTC é do binário de release
  (`:304`); o das 02:21 UTC é de um build de teste (`:309`, 2, 3 ou 4, o log não distingue):
  linhas no mesmo lugar.
- MarcosAlexandre30 (#65, 02:28 UTC): build 3 ou 4 (`:309`/`:880`) com `SPARTAN_VK_VALIDATION=1
  SPARTAN_VK_VALIDATION_SYNC=1`. A validação de sincronização acusou só
  `SYNC-HAZARD-WRITE-AFTER-PRESENT` nas imagens do swapchain (10 vezes, limite de repetição),
  com queda de frames; **nenhum hazard dentro do frame** e nenhum erro da validação core
  (sempre ligada, `emulator_settings.h`). Enfraquece a pista da barreira faltando, dentro do que
  as camadas conseguem modelar.
- FilipiEPTeixeira (#65, 02:38 UTC): build 3 ou 4; as linhas sumiram nas duas primeiras
  execuções e voltaram depois (os arquivos dos builds de teste ficaram em pastas erradas, e o
  build não registrava se `SPARTAN_FULL_BARRIERS` estava ligado: esta PR passa a imprimir
  `GPU: full barriers on`). O resultado do `SPARTAN_FULL_BARRIERS=1` continua desconhecido. E as
  linhas somem a 1800p e a 4K na 2.1.7; abaixo de 1080p ficam maiores.
- wyz-ez (#65, 04:08 UTC): o RenderDoc derruba o jogo antes de uns 45 frames; o Radeon Developer
  Tool Suite (RGP, RMV) não mostra o conteúdo das imagens, então não ajuda aqui.
- Ninguém com `SPARTAN_WAVE_STATS=1` ainda: os logs enviados não têm a linha `GPU: wave sizes`.

## 5. O que esta PR entrega

Nenhuma correção: diagnóstico. Nada muda para quem não liga as variáveis.

- `SPARTAN_WAVE_STATS=1` liga `VK_KHR_pipeline_executable_properties` e, na AMD,
  `VK_AMD_shader_info`. Grava `GPU: wave sizes after N pipelines: ...` com um item por par
  (conjunto de estágios, largura): na AMD `{ Fragment }/vgprV@W xN` (V = VGPRs físicos, que
  diferem entre wave32 e wave64; W = 0 nos estágios gráficos), nos outros fornecedores
  `{ Fragment }@W xN`. A ISA dos três primeiros executáveis de cada chave (estágios, VGPRs) vai
  para `user\shaders\wave_dumps\<estágio>_vgpr<V>_<n>_<W>.txt`, onde wave32 e wave64 aparecem
  direto (`vk_wave_stats.h`, `vk_pipeline_common.cpp`, teste `tests/test_wave_stats.cpp`, alvo
  `wave-stats-test`).
- `SPARTAN_FULL_BARRIERS=1` grava uma barreira completa de memória antes e depois de cada render
  pass e de cada dispatch de compute do jogo, e também dos dispatches do próprio port (tiling e
  detiling de imagens em `tile_manager.cpp`, cópias por shader em `vk_shader_hle.cpp`). Imprime
  `GPU: full barriers on` no log. Lento; só para o teste: linhas sumindo numa cena que as tinha
  = barreira faltando no núcleo; linhas iguais = sincronização sai da lista. É só uma
  `VkMemoryBarrier2` global, sem transição de layout: não testa metadados de compressão (DCC,
  HTILE), que o driver só resolve em mudança de layout.
- O botão **PLAY** do launcher grava `last_run.log` na pasta de saves como o
  `Play SpartanPort.exe` (`launcher/spartanport_launcher_win.py`, teste
  `tests/test_launcher_run_log.py`). Até aqui só o segundo gravava, por isso tantos reporters
  não achavam o arquivo. Exceção: com "Close the launcher when the game starts" ligado o
  launcher fecha 5 s depois e a leitura morre com ele, então nesse modo o PLAY não toca no
  arquivo (o log anterior fica intacto). Um erro de escrita no log não interrompe mais a leitura
  da saída do jogo.
- **Censo por passe na captura do analisador de quadro** (Insert > Diagnostics > Capture frame,
  ou `SPARTAN_CAPTURE_TRIGGER`): cada passe do `frame_*.txt` ganha uma linha
  `state samples c1 d4 r4 mixed-amd shading 1 | stencil t1 w1 c0 | zclear htile | ds
  DepthReadOnlyStencilAttachmentOptimal | fb 0 | area 1920x1080 (depth 1920x1088)` com as
  amostras dos alvos de cor e de profundidade do guest, as amostras do pipeline, o caminho de
  amostras que o `vk_pipeline_cache.cpp` escolheu (`equal`, `mixed-amd`, `mixed-nv`,
  `forced-equal`), `sampleShadingEnable`, teste, escrita e clear de estêncil, a origem do clear
  de profundidade (`register`, `htile`, `none`), o layout do alvo de profundidade, o feedback
  loop e a área de render contra o tamanho do alvo de profundidade. Só durante a captura
  (`vk_pass_census.h`, `vk_frame_capture.cpp`, `vk_rasterizer.cpp` em `BeginRenderingFull`;
  teste `tests/test_pass_census.cpp`, alvo `pass-census-test`). Responde, numa captura só, se o
  jogo usa MSAA ou amostras mistas, se algum passe testa ou amostra o estêncil, e se um alvo é
  menor que os outros.
- `SPARTAN_LIGHTING_TEST=shading,msdepth,dref,bias` (ou `all`): as quatro mudanças do build
  `KrachDev/shadPS4_GOW3` (seção 9.3, pista 1), cada uma atrás de um token para bissecção.
  Imprime `GPU: lighting test on: <tokens>`. `shading`: `sampleShadingEnable` sempre falso
  (`vk_graphics_pipeline.cpp`). `msdepth`: o blit de cor para profundidade multisample usa o
  canal vermelho para amostras que leem 0 (`color_to_ms_depth.frag`, variante
  `SAMPLE_FALLBACK`). `dref`: as comparações de sombra devolvem o resultado nos quatro canais
  em vez de `(v, 0, 0, 0)` (`emit_spirv_image.cpp`, campo `Profile::dref_broadcast`). `bias`:
  o slope do depth bias passa como o guest manda, sem dividir por 16 (`vk_rasterizer.cpp`).
  Os tokens que mudam o SPIR-V entram na versão do cache de binários de shader
  (`vk_pipeline_serialization.cpp`), então não precisa limpar o cache entre execuções
  (`vk_lighting_test.h`, teste `tests/test_lighting_test.cpp`, alvo `lighting-test-knob-test`).
- `SPARTAN_AMD_PATHS_OFF=mixed,shaderext` (ou `all`): roda o núcleo sem as extensões que só a AMD
  expõe. Imprime `GPU: amd paths off: <tokens>`. `mixed`: sem `VK_AMD_mixed_attachment_samples`,
  os passes com cor e profundidade de amostras diferentes passam a ter amostras iguais, como na
  Intel. `shaderext`: sem `VK_AMD_shader_explicit_vertex_parameter`, `VK_AMD_gcn_shader` e
  `VK_AMD_shader_trinary_minmax`, o código de shader usa `VK_KHR_fragment_shader_barycentric`
  (o log diz `GPU: fragment barycentrics: ...`) e os caminhos genéricos; também entra na versão
  do cache (`vk_amd_paths.h`, `vk_instance.cpp`, teste `tests/test_amd_paths.cpp`, alvo
  `amd-paths-test`).

- `SPARTAN_NO_MSAA` (seção 10, patch de kalil03): as imagens multisample do jogo são criadas e
  amostradas com 1 amostra; os tamanhos e endereços no guest continuam com a contagem real
  (`ImageInfo::guest_samples`, `GuestNumSamples()` em `regs_color.h`, `regs_depth.h`,
  `resource.h`). Ligado por padrão nos drivers proprietários da AMD e da Intel no Windows
  (`VK_DRIVER_ID_AMD_PROPRIETARY`, `VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS`; a regra fica em
  `video_core/amdgpu/spartanport_msaa_default.h`, em C, compartilhada com `spartanport-gpu-capabilities`);
  `SPARTAN_NO_MSAA=0` desliga e `=1` força em qualquer placa. O log diz `GPU: guest multisampling
  off (...)` ou `on` (`spartanport_msaa.h`, `vk_instance.cpp`; teste `tests/test_no_msaa.cpp`, alvo
  `no-msaa-test`, que também cobre a tabela da regra). **É a única mudança desta PR que altera o
  render por padrão, e só nos drivers da AMD e da Intel no Windows.**
  No launcher: Performance > GPU data > "Multisampling do jogo" (chave `no_msaa` em
  `settings.json`, vazia por padrão). `spartanport-gpu-capabilities --list` passou a imprimir, por GPU,
  o padrão do multisampling nesse driver (`off`/`on`) e qual GPU o jogo escolhe sem
  `SPARTAN_GPU_ID`; Automático mostra a placa que vai rodar e a decisão, por exemplo "Automático
  (Intel(R) Arc(TM) A750 Graphics: desligado)", acompanha a escolha em "Placa de vídeo" na mesma
  sessão e não exporta a variável (o jogo decide pela mesma regra); Desligado e Ligado exportam
  `SPARTAN_NO_MSAA=1` ou `0` em qualquer placa (testes `tests/test_launcher_no_msaa.py`,
  `tests/test_launcher_controls.py`). Ressalvas: a regra vale para toda GPU nos drivers da AMD
  e da Intel no Windows, integradas incluídas (UHD e Iris Xe sem relato ainda); em notebooks
  híbridos a preferência de GPU por aplicativo do Windows pode ordenar as GPUs de forma diferente
  para a ferramenta e para o jogo, e aí o que vale é a linha `GPU: guest multisampling ...` do
  log.
- `SPARTAN_DIAG_IMAGES=1` (patch de kalil03, exige `SPARTAN_DRAW_PIPE=0`): durante uma captura de quadro
  (`SPARTAN_CAPTURE_TRIGGER`), grava em `SPARTAN_DIAG_DIR` (padrão `out/diag`, a pasta precisa existir)
  o alvo de cor de cada passe e as imagens de cada dispatch, em raw
  (`<seq>_<tag>_<endereço>_<w>x<h>_p<pitch>_b<bytes>_f<formato vk>.raw`). Espera a GPU a cada
  imagem; só para diagnóstico. Foi assim que o passe das faixas foi encontrado.

## 6. Pistas restantes

Substituída pela seção 9.3. Da lista original de 2026-10-12: a DCC na cor (pista 1) está quase
morta pelo código aberto do driver, a barreira faltando (pista 2) continua sem resultado e perdeu
a parte da Infinity Cache, a resolução (pista 3) ganhou o 1440p e o 720p como casos que separam
as pistas, e a captura RenderDoc (pista 4) derruba o jogo nas placas dos reporters.

### Endurecimentos opcionais (não são a causa)

- Invalidar o `begin_memo` em `TextureCache::ClearMeta` e em `TouchMeta(is_clear=true)`.
- `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` quando `supports_memory_budget`.
- Registrar no log os limiares do coletor ao iniciar; `DownloadImageMemory` só grava o mip 0.
- Nome do arquivo de binário com o hash da `GraphicsPipelineKey`; remover `LoadLegacyBinary`.
- Pin de subgrupo só nos estágios que o driver lista (`SPARTAN_SUBGROUP_PIN=allowed` na PR Intel):
  tira o port de fora da especificação na AMD, sem efeito no defeito.

## 7. Como testar (build `windows-v2.1.7-teste3`)

O `windows-v2.1.7-teste3` (commit `e2cd838`, 2026-10-13) substitui o `amd-teste1` e o `teste2`,
apagados: mesma correção, agora ligada sozinha também no driver da Intel no Windows, e o launcher
mostra a placa e a decisão. Sem `.bat`. O teste2 parava em pasta nova com `NameError: name
'source' is not defined` (`scripts/link_libc.py` e `scripts/link_modules.py`, bug da main em
`5dc44e4`; corrigido em `6d580ba` e `e2cd838`). O pedido de validação foi postado em #65, #28 e nas duplicatas; os passos
abaixo continuam valendo (3 aberturas bastam; o controle com `SPARTAN_NO_MSAA=0` é opcional).

O build liga `SPARTAN_NO_MSAA` sozinho em toda placa AMD com o driver proprietário. Regras: 1080p,
sem patch de resolução, upscaler desligado, sempre as mesmas cenas (menu principal com o Kratos,
abertura em tempo real, primeira luta). Não precisa limpar o cache de shaders. O `last_run.log`
fica na pasta de saves (a "Saves folder" do launcher, por exemplo `Documents\game saves`)
e precisa ter a linha `GPU: guest multisampling off (single-sampled images; SPARTAN_NO_MSAA=0 keeps
it)`; sem ela, a execução não conta.

1. **Padrão (correção ligada):** 5 aberturas do zero, um print por abertura das três cenas. Diga
   em quantas as linhas apareceram e anexe um `last_run.log`.
2. **Controle (`SPARTAN_NO_MSAA=0`):** 2 aberturas pelo `teste.bat` abaixo. Linhas de volta confirma
   que é a mudança que resolve, e não o build.

   ```bat
   @echo off
   set SPARTAN_NO_MSAA=0
   start "" "SpartanPort.exe"
   ```

3. **Bordas e desempenho:** um print da mesma cena parada com e sem a correção (o jogo perde as
   2 amostras de profundidade; procurar serrilhado diferente nas bordas dos objetos) e o FPS
   nos dois casos (F3 ou "Frame stats" do launcher).
4. **Quem joga mais adiante:** diga se as linhas voltam em alguma fase (a correção só foi testada
   até a primeira luta) e em 1440p, 1800p ou 4K.
5. **RDNA2 sem as linhas antes (6700 XT) e RDNA3/RDNA4:** o build também muda o render de vocês.
   Testes 1 e 3: se algo piorar, `SPARTAN_NO_MSAA=0` é o caminho de volta.

Os testes de diagnóstico da seção 9 (`SPARTAN_LIGHTING_TEST`, `SPARTAN_AMD_PATHS_OFF`, barreiras,
captura por passe) continuam no build, mas só fazem sentido se a correção falhar em alguma placa.

## 8. Perguntas em aberto

1. A correção some com as linhas em todas as RDNA2 relatadas (6600, 6650 XT, 6750 XT, 6800 XT,
   6900 XT) e em todas as fases, ou só no começo?
2. Perder as 2 amostras de profundidade muda bordas ou desempenho de forma visível? Se mudar,
   vale restringir a correção ao fetch (expandir o depth antes da leitura por shader, ou copiar
   para uma imagem de 1 amostra só para a leitura) em vez de desligar o MSAA do guest inteiro.
3. Por que 1800p e 4K não mostravam (ou mostravam menos): o driver desliga a compressão do depth
   acima de um tamanho, ou muda o layout? Só interessa se a restrição da pergunta 2 for feita.
4. Os traços nas barras do HUD (#65, #91) vêm do mesmo depth linear ou de outra leitura?
5. O Steam Deck (RADV) mostra as linhas com `SPARTAN_NO_MSAA=0`? Decide se é bug do driver do
   Windows ou do hardware.

## 9. Revisão de 2026-10-13 (rodada adversarial)

Rodada só de leitura: sete lentes (tiling, sincronização, driver RDNA2, memória do guest,
pré-passe de luz, pesquisa web, mapas de código) e três verificações adversariais por hipótese,
com um crítico de completude no fim. **Nenhuma das 27 hipóteses sobreviveu** à verificação. O
que sobrou está em 9.3, em ordem de custo do teste decisivo.

### 9.1 Fatos corrigidos

| Item | Antes | Agora |
|---|---|---|
| Placas | Só Navi 23; 6700 XT limpa | Navi 23 (6600, 6600M, 6600 XT, 6650 XT). Navi 21: RX 6800 XT com print no menu (#70) e RX 6900 XT só por relato (#70). Navi 22: RX 6750 XT com traços nas barras do HUD a 2560x1440 (#91), ainda sem comparação com um HUD saudável. A 6700 XT "limpa" vale só para as condições em que rodou: repetir a 1080p, sem patch, upscaler desligado, 5 aberturas. |
| Drivers | 26.6.4, 26.8.1 | 26.6.4, 26.8.1 e 26.9.2. Só Windows. |
| Forma | Blocos de 8x4 px | Faixas de 4 linhas com período de 8: escuras as linhas com `y mod 8` em 0 a 3, intactas as de 4 a 7 (medido com PIL no print 1920x1080 da #61: y = 920-923, 928-931, ...; o print de 1365x767 da #65 dá o mesmo período de 8 linhas do render). **Contínuas na horizontal** por dezenas de pixels (36 px, 50 px), não traços de 8 px. A região das faixas segue a borda entre a parte iluminada e a parte na sombra do pescoço. |
| 4K | "Diminuem e vão para baixo"; "o fundo inteiro, independente do conteúdo" | Medição refeita no print 4K de wyz-ez (janela 1917x1078, escala 2,003, amostragem bilinear do pós): a fase é a mesma de 1080p (4 ligadas, 4 desligadas). A região é uma escada de bordas nítidas: topo na linha 1928 do render para x ≥ 960, topo na linha 2056 mais à esquerda (ali a borda esquerda fica escondida pelo painel do rosto). À esquerda de x = 960, entre 1928 e 2055, o conteúdo é claro e sem faixas. As bordas caem perto de 1920 = 15·128, 2048 = 16·128 e x = 960 = 15·64 dentro do erro da redução 2:1: a forma de "endereços a partir de A" numa superfície 2D-tiled do guest com macro tiles de 64x128, ou de um quad de UI. Para FilipiEPTeixeira as linhas somem por completo a 4K. |
| Infinity Cache | 32 MB contra 96 MB explicaria | Descartado. A 6800 XT (128 MB) também mostra as linhas, e a MALL é um cache do lado da memória, coerente para todos os agentes da GPU. |
| Video Arena e memória do guest | Confundida com a resolução | A 1080p, arenas de 1,39 e 2,39 GiB mostram linhas. A sequência de alocações de DMEM é idêntica byte a byte entre lançamentos (logs dos reporters; `runtime_memory.c` é first-fit determinístico), mas isso diz só dos mapeamentos: onde o jogo coloca cada superfície dentro da arena (pools `WAD_...` carregadas por threads de streaming) não foi medido. O patch 720p tem a mesma arena da "Texture Corruption Fix" e separa tamanho de render de arena. |
| Teste do pin (seção 4) | "O pin não é a causa" | Continua valendo, mas o teste não variou o wave size do pixel shader: a AMD ignora `requiredSubgroupSize` em gráficos. A varredura dos SPIR-V só procurou operações de subgrupo, não os builtins da AMD. |
| `SPARTAN_FULL_BARRIERS` | Decide "barreira faltando" | É só uma `VkMemoryBarrier2` global (`vk_scheduler.cpp`), sem transição de layout. Não testa metadados de compressão (DCC, HTILE). |
| Ferramentas | `SPARTAN_DUMP_DIR` = captura de quadro | `SPARTAN_DUMP_DIR` só serve ao `SPARTAN_FRAME_DUMP_TRIGGER`, que grava `display_*.raw` e `output_*.raw` (a pasta precisa existir antes; `vk_presenter.cpp`). O analisador de quadro (Insert > Diagnostics > Capture frame, `SPARTAN_CAPTURE_TRIGGER`) grava só texto em `SPARTAN_CAPTURE_DIR` ou `<saves>\captures`; antes desta PR não registrava amostras, estêncil nem layouts. `SPARTAN_DUMP_TRIGGER` e `SPARTAN_PRESENT_DUMP_TRIGGER` só funcionam com o upscaler ligado. |
| Estêncil | Não considerado | O emulador nunca envia (`texture_cache.cpp`, `RefreshImage` tira o aspecto de estêncil do upload), nunca copia por `CopyImage` (`vk_runtime.cpp`) e nunca limpa por HTILE o plano de estêncil: a primeira vez que um D32S8 novo é anexado, a profundidade é limpa (`htile_clear_mask = -1`) e o estêncil é carregado de uma imagem `UNDEFINED`. Draws com stencil op diferente de Keep e `Rasterizer::DepthStencilCopy` escrevem o estêncil. O SpartanPort lê o estêncil como textura R8Unorm pelo redirecionamento estêncil para profundidade (`vk_rasterizer.cpp`, descrição da PR upstream shadPS4#5118), com tipo de imagem float sobre o aspecto UINT (`image_view.cpp`, `spirv_emit_context.cpp`), o que a especificação deixa indefinido e os fornecedores podem resolver de forma diferente. |

Caminhos de código que só a AMD no Windows percorre, confirmados pelos logs (as únicas
extensões pedidas e ausentes são `VK_EXT_attachment_feedback_loop_dynamic_state` e
`VK_NV_framebuffer_mixed_samples`):

1. Imagem amostrada e alvo no mesmo draw vai para o layout `GENERAL` (`vk_instance.cpp`,
   `vk_rasterizer.cpp`). Entre draws há barreira (as máscaras de acesso alternam); dentro de um
   draw seria corrida e piscaria.
2. MSAA misto (cor e profundidade com número de amostras diferente) passa por
   `VK_AMD_mixed_attachment_samples` (`vk_instance.cpp`, `vk_pipeline_cache.cpp`). A Intel força
   amostras iguais; a NVIDIA usa a extensão NV.
3. Código de shader gerado com `VK_AMD_shader_explicit_vertex_parameter`, `gcn_shader` e
   `trinary_minmax` (`vk_instance.cpp`, `vk_pipeline_cache.cpp`, `emit_spirv.cpp`).

Precedente no mesmo jogo, verificado no repositório: o release `v1.0.0-gow3-fix` de
`KrachDev/shadPS4_GOW3` ("God of War III Remastered - Lighting, Scanline & AMD Fix",
2026-10-09, binário `shadps4-win64-sdl-2026-10-09-9f48028.zip`, PR aberta cuesta4/shadPS4#6)
diz que as linhas pretas somem numa RX 6600 com o commit `eaa1027f`, que muda exatamente quatro
coisas: `sampleShadingEnable = false`, o fallback de amostra em `color_to_ms_depth.frag`, o
resultado do Dref nos quatro canais e o slope do depth bias sem o `/16`. O texto do release diz
que o passe de iluminação do jogo usa profundidade 2x MSAA com cor 1x; isso não foi verificado
aqui (a captura por passe desta PR verifica). As explicações do release para o mecanismo não
batem com a geometria medida (ele descreve "listras de 1 px"), mas o resultado relatado é o único
fix candidato com teste em RX 6600 até agora. Os quatro tokens de `SPARTAN_LIGHTING_TEST` são essas
mudanças, uma a uma.

Verificação local na GTX 1650 (2026-10-13, build desta PR, 1080p, upscaler desligado): a captura
por passe da tela de ajuste de exibição do primeiro boot (menu "Exibição" com o rosto do Kratos ao
fundo) tem 9 passes, todos `c1 d1 r1 equal shading 0`, sem estêncil e sem clear: essa tela é 2D.
O menu principal e a cena com as luzes ainda não foram capturados localmente.

### 9.2 Refutadas nesta rodada

| Hipótese | Por que caiu |
|---|---|
| DCC na cor, metade de 128 B lida como "limpa" (pista 6.1 de 2026-10-12) | No PAL/XGL aberto, imagem `MUTABLE_FORMAT` sem `VkImageFormatListCreateInfo` vira `viewFormatCount = AllCompatibleFormats` (xgl `vk_image.cpp`). Isso dá `DccFormatEncoding::Incompatible` fora da GFX11 (PAL `gfx9Device.cpp`) e desliga o DCC (`gfx9MaskRam.cpp`, `mustDisableDcc`). Na GFX10 o CMASK só existe com MSAA. Todas as imagens do port são `MUTABLE_FORMAT` sem lista (`texture_cache/image.cpp`). Ressalva: o Adrenalin é fechado. |
| Linha de cache de 128 B velha, pipe desalinhado | O XOR de pipe e banco atua do bit 8 para cima, então as duas metades de 256 B ficam no mesmo canal. Linha velha piscaria entre quadros, e o defeito é estático. A passagem de alvo para amostragem sempre muda de layout e sempre gera barreira (`vk_rasterizer.cpp`, `texture_cache/image.cpp`). |
| Plano de profundidade nos volumes de luz | O HTILE é 8x8. A profundidade nunca usa o caminho de feedback em nenhum fornecedor e é limpa e reescrita a cada quadro. Não explica HUD, céu nem 4K. |
| Layouts separados de profundidade e estêncil (`DepthReadOnlyStencilAttachment`) | Os mesmos motivos; descritor e anexo em layouts diferentes no mesmo draw dariam erro de validação core no run do MarcosAlexandre30, e não deram. |
| Detile de superfície de 64 bpp ou com tile split | Todas as superfícies do tamanho do render são de 32 bpp. Tile split de cor nunca dispara (`amdgpu/tiling.cpp`). O caminho é igual em todos os fornecedores. |
| Feedback loop em `GENERAL` na AMD | Entre draws há barreira. Dentro de um draw seria corrida e piscaria. Falta só confirmar, de graça na GTX 1650, que o jogo nem passa por ali (coluna `fb` do censo). |
| Ordem dos anéis gráfico e compute, manutenção de cache ignorada | Há uma fila só no host (`vk_scheduler.cpp`) e `WAIT_REG_MEM` é respeitado (`liverpool.cpp`). A bisseção já mexeu no timing sem efeito. |
| Estêncil não inicializado como lixo de memória (forma original) | Lixo de memória não produziria o mesmo padrão exato (linhas 0 a 3 de cada 8) em máquinas e placas diferentes. Fica a variante estreita da pista 3 (9.3). |
| Limite de 2.097.152 px do clear de profundidade e estêncil do PAL (gráfico abaixo, compute acima) | O limite existe no código aberto (`gfx9RsrcProcMgr.cpp`, `settings_gfx9.json`), mas os dois caminhos tratam o HTILE do estêncil igual num clear só de profundidade, e nenhum dos dois toca 4 de 8 linhas (HTILE é por tile de 8x8). O 4K (8,3 Mpx) fica acima do limite e ainda mostra o bloco de baixo para wyz-ez. Fica só como motivo de incluir 1440p no mapa de resolução. |
| Wave64 do PS descartando a metade baixa (linhas 0 a 3) | O rasterizador empacota os quads que sobrevivem à cobertura, ao early-Z e ao estêncil, atravessando tiles e primitivas: a metade descartada não ficaria alinhada a `y mod 8`. As faixas medidas são contínuas por 36 a 50 px e seguem a borda de iluminação, não as arestas das primitivas. |
| Sample shading por si só | Com `minSampleShading = 0` e alvos de 1 amostra, `sampleShadingEnable` não muda nada; só faz diferença com MSAA (por isso o censo vem antes e `SPARTAN_LIGHTING_TEST=shading` é um token separado). |
| Cadeia do presenter (FSR/RCAS a 1:1) | O 0.19.0 oficial também mostra as linhas, a 4K o RCAS fica desligado, e um passe de tela inteira não ficaria restrito às luzes. |

Continuam valendo as refutadas da seção 3 e as eliminações por teste da seção 4.

### 9.3 Pistas restantes, em ordem

1. **As quatro mudanças do build KrachDev** (`SPARTAN_LIGHTING_TEST=all`, depois um token por
   vez). Único candidato com resultado positivo relatado numa RX 6600; nenhuma das quatro
   explica sozinha a geometria nem a aleatoriedade por lançamento, e só `shading` e `msdepth`
   tocam um caminho só da AMD (amostras mistas). Teste 1 da seção 7.
2. **MSAA misto via `VK_AMD_mixed_attachment_samples`.** Só vale se o jogo usa amostras > 1,
   o que o censo por passe responde (teste 2). Se usar: `SPARTAN_AMD_PATHS_OFF=mixed` (teste 6)
   força o caminho da Intel. Um micro tile de 32 bpp com 2 amostras é 8x4 px x 2 amostras =
   256 B: lido como 1x, as linhas 0 a 3 e 4 a 7 viriam de amostras diferentes, e
   `color_to_ms_depth.frag` escreve 0 na amostra 1 de uma fonte R32F. É a única leitura que
   liga a geometria a um caminho só da AMD, e depende de o jogo ter profundidade multisample.
3. **Conteúdo do plano de estêncil que o emulador nunca escreve**, lido pelo jogo (teste de
   estêncil ou a textura R8Unorm redirecionada). Explica aleatório por lançamento e estático por
   execução, mas exige um padrão estruturado de 32 B, não lixo, e a parte "só AMD" depende do
   que o driver faz com um estêncil `UNDEFINED` lido como float. O censo mostra se as luzes
   testam estêncil e se ele é limpo (`stencil t1 w1 c0`); se mostrar, o próximo build ganha uma
   variável que limpa o estêncil no primeiro bind (`SPARTAN_STENCIL_INIT=<valor>`).
4. **Barreira faltando entre passes** (`SPARTAN_FULL_BARRIERS=1` com o marcador; teste 3).
5. **Região que depende da posição na memória do guest.** Bordas da escada a 4K perto de
   128·k + 8 linhas e x = 960: macro tiles de 64x128 de uma superfície 2D-tiled, ou um quad de
   UI. O patch 4K só aumenta tamanhos e a arena, então um alvo 4K invadindo a alocação vizinha é
   plausível. Teste 5 em 3 aberturas: bordas que mudam de lugar = posição na memória.
6. **Código de shader só da AMD** (`SPARTAN_AMD_PATHS_OFF=shaderext`). Determinístico, não
   explica a aleatoriedade nem o 1800p; barato.
7. **DCC na cor.** Quase morta pelo código aberto do driver; uma sonda que compara o tamanho de
   memória de uma imagem com e sem lista de formatos fecharia a pista em um lançamento. Não
   implementada.

### 9.4 Verificações locais, sem reporter

- **GTX 1650:** censo e captura no menu principal e na entrada da Gaia (o jogo usa MSAA? as
  luzes testam estêncil? o estêncil é limpo?) e um print do HUD saudável para comparar com os
  traços das #65 e #91. O registro do guest é igual em todo fornecedor, então essa captura
  responde às perguntas 2 e 3 de 9.3 de graça.
- **Steam Deck** (RDNA2, RADV), 1080p: se mostrar as linhas, o driver do Windows sai da lista e
  `RADV_DEBUG=nodcc,nohiz,nofastclears` bissecta os metadados em minutos; `RADV_DEBUG=img`
  lista DCC e HTILE por imagem.
- **RX 6700 XT de teste:** rodar nas condições dos reporters antes de contar como controle.

## 10. Causa encontrada (kalil03, 2026-10-13)

Relato em [#65](https://github.com/FelipePrado0/SpartanPort/issues/65#issuecomment-6138650285), RX
6600, Adrenalin 26.8.1, 1080p nativo, sobre o main `3459726`.

**Mecanismo.** O jogo desenha com depth D32S8 de 2 amostras e cor de 1 fragmento (o "2x MSAA
depth + 1x colour" que o release KrachDev afirmava sem prova). O passe de profundidade linear (ps
`782d705bc1c01219`, um triângulo de tela cheia que escreve num R32F) lê esse depth com `texelFetch`
num `sampler2DMS`. Nessa leitura o driver AMD do Windows devolve 0 na metade de baixo de muitos
tiles de 8x8 pixels: a profundidade linear sai valendo 0,25 (o near plane) neles, e luz, névoa e
bloom, que usam essa imagem, herdam as faixas. Por isso as linhas só aparecem em área clara e
dentro das luzes. O dump por passe (`SPARTAN_DIAG_IMAGES`) mostrou cerca de 16.100 blocos de 8x4
errados nessa imagem a 1080p e tudo limpo antes dela. Os blocos que escapam ficam onde o depth varia
mais (bordas); em área lisa quase todos falham. Tem cara de tile de depth comprimido (compressão
Z do MSAA) lido pelo TC sem descompressão.

**Como fecha com o que estava medido.** A metade de baixo dos tiles no R32F vira as linhas 0 a 3
dos prints se há um flip vertical entre essa imagem e a apresentada (1080 = 135 x 8). A decisão
de comprimir é por imagem e por conteúdo, o que dá aleatório por abertura e estático por
execução. Era a pista 2 da seção 9.3 (amostras mistas); as outras caem com ela. Ler o depth em
layout `General` e desligar `VK_AMD_mixed_attachment_samples` não mudaram nada no teste dele, o
que fecha também `SPARTAN_AMD_PATHS_OFF=mixed`. O `Unimplemented depth overlap copy` do log do
Momexdxd é uma cópia envolvendo esse depth multisample; com a correção o caminho some.

**Correção (`SPARTAN_NO_MSAA`, patch de kalil03 aplicado nesta PR com teste).** As imagens
multisample do guest são criadas com 1 amostra e o sharp `Color2DMsaa` é tratado como `Color2D`,
então o shader é gerado com `texture2D` e sem índice de amostra (o tipo entra na especialização
do shader, `specialization.h`, e `emit_spirv_image.cpp` só emite o índice para `Color2DMsaa`:
um cache antigo não contamina). Tamanho e endereço no guest continuam com a contagem real
(`ImageInfo::guest_samples`, `GuestNumSamples()`). Efeitos colaterais conhecidos:
`aa_config.NumSamples()` vira 1 para shaders com sample coverage; `motion_possible`
(`vk_pipeline_cache.cpp`, exige depth de 1 amostra) passa a valer nos passes de 2 amostras (só
importa com o upscaler ligado); `ReinterpretColorAsMsDepth` e `SetBackingSamples` ficam sem uso
nos drivers da AMD e da Intel no Windows. NVIDIA, RADV, AMDVLK, ANV e MoltenVK não mudam.

Resultado dele, abrindo o jogo do zero até a abertura em tempo real e a primeira luta:

| | aberturas com linhas | blocos ruins na profundidade linear |
|---|---|---|
| sem o patch, ou `SPARTAN_NO_MSAA=0` | 10 de 11 | cerca de 16.100 |
| com o patch (padrão) | 0 de 5 | 30 a 80 (ruído do detector) |

A abertura limpa sem o patch bate com o "às vezes abre sem as linhas" dos relatos. Não testado:
o resto do jogo, FPS, outras resoluções, outras placas, bordas com e sem as 2 amostras. O build
`windows-v2.1.7-teste3` é para isso (seção 7).

**Resultados do build (2026-10-13, #65).** uviiitor (5 aberturas), Tempus3214,
eduhhh10zx-hue (RX 6650 XT, várias aberturas), rafaelsilvaiasd-lang (com o save dele) e
FilipiEPTeixeira: linhas sumiram. Nenhum relato de volta das linhas, de bordas piores ou de FPS
até agora; o controle com `SPARTAN_NO_MSAA=0` ninguém reportou.

**Intel Arc (#28).** Com o build `windows-v2.1.7-amd-teste1` e `SPARTAN_NO_MSAA=1` por um `.bat`:
ZeroTwoMiu (Arc A750, driver 101.9034), yisus230291, Hussein798o (Arc 140V; os logs anteriores
dele diziam `guest multisampling on`, o `.bat` do release era o de controle),
silvajeangomes2026-spec (MSI Claw 8 AI+, Arc 140V) e skinnersweet24 (Arc B580): menu e jogo
corretos; com o padrão (`guest multisampling on`) a corrupção continuava. Por isso o padrão
passou a desligar o multisampling também no driver proprietário da Intel no Windows
(`spartanport_msaa_default.h`), e o launcher mostra a placa e a decisão em "Multisampling do jogo"
(seção 5), sem `.bat`. A HD 630 tem outra causa (`VK_EXT_vertex_input_dynamic_state`, PR
Intel).

