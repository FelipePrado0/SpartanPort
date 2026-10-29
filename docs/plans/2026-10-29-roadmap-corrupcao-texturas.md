# Roadmap: corrupção progressiva de texturas


## Progresso (29/10/2026): frente pausada

**Branch:** `diag/texture-corruption` (de `main` `f593377`). **Estado:** pausada pelo mantenedor para priorizar o Alto FPS. Nada foi testado no jogo.

**Feito:**
- [x] Revalidação das issues e do código contra a `main` (`docs/investigations/texturas/revalidacao-2026-10-29.md`).
- [x] Testes de regressão de integridade no nível CPU: readback de geração antiga não cai sobre faixa remapeada nem sobre vizinho de página (`tests/test_readback_queue.cpp`, passou na base).
- [x] **Bug achado e corrigido com TDD:** descartar o backing de um grupo de aliases zerava toda a autoridade, e os aliases sobreviventes deixavam de receber o conteúdo novo do writer. Correção mínima em `alias_state.h`/`texture_cache.cpp`; `aliasing-test` falhava na base e passa com a correção. Spec: `docs/specs/2026-10-29-tex-alias-backing-eviction.md`. Ainda não há prova de que esse seja o mecanismo das issues.
- [x] Ficha de reprodução, roteiro do mantenedor e rascunho bilíngue aos relatores em `docs/investigations/texturas/` (não publicado).

**Falta:**
- [ ] Terminar o trace `Tex:` (`SPARTAN_TEX_TRACE=1`): está num commit `WIP` incompleto, sem os testes verdes registrados.
- [ ] Build Windows de diagnóstico com a correção e o trace.
- [ ] Teste no jogo pelo mantenedor (roteiro) e pares de logs com/sem artefato.
- [ ] Decidir se a correção de autoridade de alias vira PR própria.

## Objetivo, escopo e estado

Documento de investigação e execução futura, preparado em 29/10/2026. Objetivo: identificar e corrigir a operação que perde ou substitui conteúdo de texturas durante uma sessão, especialmente em transições, efeitos e pressão de memória. Aumentar o tempo até o defeito não basta para declarar correção.

- Repositório: `FelipePrado0/SpartanPort`.
- Base remota consultada: [`f593377bbaf1934bf606171973d7a58210510991`](https://github.com/FelipePrado0/SpartanPort/commit/f593377bbaf1934bf606171973d7a58210510991).
- Release estável consultada: [`windows-v2.7.0`](https://github.com/FelipePrado0/SpartanPort/releases/tag/windows-v2.7.0), alvo `003e42b6e004ed686aae58c053d2301fe9c8c68e`.
- As issues de sintomas inventariadas abaixo estavam abertas na consulta; referências históricas têm seu próprio estado. A PR #1104 estava aberta e não mesclada. Revalidar o estado antes da execução.
- Execução realizada para este documento: leitura de corpos e comentários das issues, download e análise dos logs indicados, leitura do código remoto e do plano histórico existente.
- Não realizados: execução do jogo, alteração do renderer, testes sintéticos, compilação, publicação ou confirmação de correção. Todas as fases abaixo são propostas.
- Antes de implementar mudanças, apresentar a spec exigida pelas instruções do projeto e obter aprovação. Fazer correção e validação em branch/worktree isoladas.

## Classificação dos sintomas

| Grupo | Como reconhecer | Encaminhamento |
|---|---|---|
| Corrupção progressiva | Superfícies/armas mudam de cor ou conteúdo após minutos, aumenta com o tempo e melhora ao reiniciar/recarregar | Escopo principal: #1059, #1016, #1103 e parte dos relatos de #863 |
| Corrupção localizada | Conteúdo incorreto em trecho específico, sem tempo até início bem registrado | Investigar #917 e #1080; confirmar se compartilham o mecanismo progressivo |
| Pop-in e flicker por câmera | Objetos/sombras aparecem ou somem conforme ângulo, inclusive em execução recém-iniciada | Manter #915 e #1107 em trilha separada até evidência de vínculo |
| Corrupção estática de profundidade/MSAA | Faixas presentes desde menu/abertura, com padrão periódico e estabilidade por sessão | Não repetir a investigação como se fosse o problema novo; consultar o histórico abaixo |
| Área sem geometria e sem colisão | Kratos cai porque a área não existe fisicamente | Encaminhar ao [roadmap de cenários sem colisão](2026-10-29-roadmap-cenarios-sem-colisao.md); pressão de cache é pista compartilhada, não causa comum comprovada |

O [plano de corrupção RDNA2 de 12/10, seção 10](2026-10-12-gpu-corruption-rdna2.md#10-causa-encontrada-kalil03-2026-10-13) registra o mecanismo de profundidade multisample e a solução `SPARTAN_NO_MSAA` para faixas estáticas. Esse resultado não comprova nem descarta GC como causa da corrupção progressiva posterior. A #915 relata que a corrupção antiga deixou de ocorrer enquanto o pop-in permaneceu: os sintomas não devem ser unificados apenas por aparência.

## Inventário de evidências

“Observado no log” significa conteúdo efetivamente lido no anexo; não significa reprodução local. “Relato” preserva a afirmação do usuário sem promovê-la a causalidade comprovada.

| Fonte | Evidência e identificação | Interpretação e limite |
|---|---|---|
| [#1016](https://github.com/FelipePrado0/SpartanPort/issues/1016), [log](https://github.com/user-attachments/files/33892021/last_run.log) | Log: RTX 3080, CUSA01715 01.02, resolução patch 5120×2160, engine 120 FPS, deferred readback desligado. Pressão: 3382/3343 MiB; em outro relatório, 1148 imagens expulsas e 425 writebacks | Forte evidência de atividade intensa do GC no caso. Não há marcador sincronizado do primeiro artefato; não prova que cada writeback está errado. Desligar deferred readback já tem contraexemplo |
| [#917](https://github.com/FelipePrado0/SpartanPort/issues/917), [log](https://github.com/user-attachments/files/33795589/last_run.log) | Corpo informa RX 5500; log identifica RX 5500 XT, CUSA01623 01.00, engine 240 FPS e deferred/stale ligado. Há relatórios de 2374 expulsões/558 writebacks e 2246/467, perto do limiar de 2583 MiB | O padrão também aparece em AMD e em outro executável/atualização. A descrição é localizada antes de Hermes; progressão temporal ainda precisa ser medida |
| [#1080](https://github.com/FelipePrado0/SpartanPort/issues/1080), [log](https://github.com/user-attachments/files/33940485/last_run.log) | Log: RTX 4050 Laptop, CUSA01623 01.02, engine 60 FPS, deferred/stale ligado. Pressão: 1808/1803 MiB, 334 expulsões/15 writebacks; depois 776/372. Corpo situa o defeito no Jardim de Hera | Acrescenta ocorrência com engine 60 FPS. A intensidade do GC não demonstra a operação corruptora |
| [#863: relato temporal](https://github.com/FelipePrado0/SpartanPort/issues/863#issuecomment-6319000397), [anexo do mesmo participante](https://github.com/user-attachments/files/33791216/last_run.log), [recuperação pelo menu](https://github.com/FelipePrado0/SpartanPort/issues/863#issuecomment-6327718307) | O participante relata cerca de uma hora até a falha; sair ao menu e carregar normaliza, com recorrência posterior. Seu log identifica RX 6650 XT, CUSA01623 01.02, engine 60 FPS, deferred/stale ligado, sem mensagens de pressão | Indício de estado da sessão. Não confundir esse participante com o autor principal da issue, cujo corpo informa RTX 2060 Super. Ausência da mensagem não certifica ausência de todo descarte/readback |
| [#1059: progressão](https://github.com/FelipePrado0/SpartanPort/issues/1059#issuecomment-6395167295), [transições](https://github.com/FelipePrado0/SpartanPort/issues/1059#issuecomment-6404679209) | Relatos de degradação crescente após 15–20 minutos e recorrência após limpeza do cache; transições, efeitos e novos recursos são gatilhos relatados | Compatível com pressão e reutilização de recursos. Reiniciar e apagar cache simultaneamente não separa estado em RAM de cache persistido |
| [#1059: orçamento maior](https://github.com/FelipePrado0/SpartanPort/issues/1059#issuecomment-6458314706), [limites reconhecidos](https://github.com/FelipePrado0/SpartanPort/issues/1059#issuecomment-6479878589), [RX 6600](https://github.com/FelipePrado0/SpartanPort/issues/1059#issuecomment-6480698685), [RTX 3060 Ti](https://github.com/FelipePrado0/SpartanPort/issues/1059#issuecomment-6485631439) | Usuários relatam sessões sem recorrência com `SPARTAN_GC_BUDGET_MB=7000` | Mitigação promissora, sem logs pareados nem contagem equivalente de descarte. O número não é recomendação universal e não deve ser aplicado a GPUs menores/UMA |
| [#1103](https://github.com/FelipePrado0/SpartanPort/issues/1103), [confirmação](https://github.com/FelipePrado0/SpartanPort/issues/1103#issuecomment-6459547077) | RTX 3070, CUSA01623 01.02 segundo formulário; superfície/armas corrompem após cerca de 30 minutos com Fast Screen Effects. Outro usuário relata 30–40 minutos | Hipótese específica de aliases/views de framebuffer. Sem log nem comparação controlada, não é causalidade estabelecida |
| [#1059: contraponto de driver](https://github.com/FelipePrado0/SpartanPort/issues/1059#issuecomment-6399968128), [#1080: limpeza não resolve para o autor](https://github.com/FelipePrado0/SpartanPort/issues/1080#issuecomment-6481899587) | Hardware semelhante sem falha com outro driver; limpeza de cache ajuda um participante e não o autor de #1080 | Driver e estado inicial precisam ser controlados. Não usar limpeza de shaders como correção geral |
| [#915](https://github.com/FelipePrado0/SpartanPort/issues/915), [#1107](https://github.com/FelipePrado0/SpartanPort/issues/1107) | Flicker/pop-in por ângulo/local, sem log anexado nesses relatos. #915 distingue esse comportamento da corrupção anterior | Classificação provisória separada. A versão “v2.8.0” no formulário de #1107 é autodeclarada; falta SHA do pacote |

As quantidades de expulsões/writebacks são contadores desde o relatório anterior, não totais da sessão. Os logs analisados são de execuções diferentes, em versões e configurações diferentes; não constituem um experimento comparativo.

## O que já existe no código

As referências abaixo estão fixadas ao SHA consultado para não mudarem silenciosamente com a `main`.

| Local | Fato verificado | Consequência para o roadmap |
|---|---|---|
| [Construtor, linhas 51–71](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shadps4/video_core/texture_cache/texture_cache.cpp#L51-L71) | Os limiares iniciais dependem do orçamento reportado e de margens/constantes | Os 3343 MiB da #1016 não são uma constante universal fixa para todas as placas |
| [GC e override, linhas 1657–1678](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shadps4/video_core/texture_cache/texture_cache.cpp#L1657-L1678) | O override é real e lido uma vez; com ele ou em GPU integrada, os limiares seguem aproximadamente 70%, 85% e 95% do orçamento | Mudar a variável exige novo processo. O experimento altera quando diferentes níveis de coleta ocorrem, não apenas um limite de VRAM |
| [Descarte, linhas 1696–1721](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shadps4/video_core/texture_cache/texture_cache.cpp#L1696-L1721) | Resolve readbacks e já chama download síncrono antes de `FreeImage` no caminho aplicável | Não propor novamente “tornar o GC síncrono” como se faltasse. A lógica também evita download não linear em certas imagens tiled |
| [Download, linhas 387–428](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shadps4/video_core/texture_cache/texture_cache.cpp#L387-L428) | Copia dados GPU para backing RAM; há caminhos síncrono e diferido | Instrumentar origem/destino, tamanho, tick e geração de mapping nos dois caminhos relevantes |
| [Invalidação, linhas 649–671](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shadps4/video_core/texture_cache/texture_cache.cpp#L649-L671) | `ApplyBackingWrites` marca imagens como CPU-dirty, mas exclui `GpuModified`/`Aliased` | Verificar a passagem de autoridade para aliases; a exclusão não é bug comprovado |
| [Aliases, linhas 496–553](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shadps4/video_core/texture_cache/texture_cache.cpp#L496-L553), [remoção, linhas 1822–1848](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shadps4/video_core/texture_cache/texture_cache.cpp#L1822-L1848) | A autoridade utiliza writer/backing e UID; remoção pode resetá-la | Testar remoção do writer/backing com aliases sobreviventes e reutilização de IDs/endereço |

A `main` já inclui a série [#1005](https://github.com/FelipePrado0/SpartanPort/pull/1005), inclusive a [leitura de uploads pela backing view](https://github.com/FelipePrado0/SpartanPort/commit/63d437e373cc6d1a2183e18633fdc4b2aacdd333). Isso exige revalidar os relatos na base atual, sem declarar que essa série já os resolveu.

A [PR #1104](https://github.com/FelipePrado0/SpartanPort/pull/1104) corrige largura versus pitch em resoluções como 3440×1440. O autor relata redução de criações de imagens e melhora de FPS; esses resultados são do autor, não testes deste roadmap. A descrição exclui expressamente #1016 e #1103 da resolução. Tratar a PR como possível redução de pressão em larguras preenchidas, não como encerramento desta investigação.

## Hipóteses priorizadas e testes que podem refutá-las

| Hipótese | Confiança atual | Teste discriminante | Decisão |
|---|---|---|---|
| Descarte/writeback expõe perda de conteúdo ou autoridade | Média como mecanismo; alta como trilha de investigação | Forçar coleta controlada e comparar bytes antes/depois de remover writer/backing | Primeira divergência deve determinar a correção; só correlação temporal não basta |
| Limiar conservador provoca coleta desnecessária, mas integridade está correta | Média para pressão; baixa para explicar corrupção sozinho | Comparar limites usando os mesmos recursos e validar conteúdo após igual quantidade de descarte | Se conteúdo continuar errado após coleta, corrigir integridade antes de elevar padrão |
| Alias/view de Fast Screen Effects mantém writer antigo ou conteúdo incompatível | Média-baixa | Patch ligado/desligado com budget e rota iguais; exercício sintético de views de alturas/formato compatíveis | Se exclusivo do patch, limitar investigação a esse caminho e manter os demais casos abertos |
| Writeback pendente alcança mapping reutilizado ou região vizinha | Plausível, ainda não observado nesta rodada | Remapear a região e vizinhos enquanto há readback agendado; comparar gerações e sentinelas | Impedir escrita obsoleta sem cancelar dados válidos dos vizinhos |
| Dados do jogo/extração já estão incorretos | Possível, sem demonstração para este conjunto | Reextrair o mesmo base/update e comparar manifests/WADs, mantendo port/save/configuração | Se arquivos mudarem e o defeito desaparecer, registrar subgrupo de integridade; hashes iguais sozinhos não certificam origem correta |
| Cache persistido ou driver é causa independente | Aberta | Reiniciar mantendo cache; depois testar cópia isolada de cache limpo; driver somente em experimento separado | Se o defeito acompanhar conteúdo de cache ou driver, abrir trilha própria com reproducer |

## Fase de preparação e reprodução

**Entrada:** uma instalação preservada, save reproduzível e pacote identificado. **Saída:** reprodução confiável ou registro explícito de que ainda não há reproducer.

- Registrar SHA do port, hash do pacote, CUSA, `APP_VER`, hash do executável e manifest de nomes/tamanhos/hashes dos WADs pertinentes. Não publicar arquivos do jogo.
- Registrar SO, GPU, driver, memória dedicada/compartilhada, orçamento por heap, resolução efetiva, engine FPS, limite de apresentação, upscaler, readback efetivo, Fast Screen Effects e demais patches.
- Escolher um caso progressivo (#1059 ou #863) e um sob alta pressão (#1016 ou #1080). Não variar tudo ao mesmo tempo nem usar todos os casos como obrigação inicial.
- Copiar saves/configurações/caches para instalações de teste separadas. Restaurar o mesmo estado inicial em cada comparação; não apagar o cache original do usuário.
- Fixar a rota, incluindo transição, cinematics, menu de melhoria e efeito que costuma disparar o defeito. Capturar o primeiro artefato com timestamp e cena; registrar também se o personagem continua colidindo normalmente.
- Comparar release estável e pacote da `main` fixada, com as mesmas configurações efetivas. Cada versão usa sua cópia do cache; não compartilhar caches incompatíveis entre versões.
- Se a base atual não reproduzir, repetir a rota e duração de exposição que reproduzia na release. Classificar como “não reproduzido na base consultada”, não “corrigido por #1005” sem isolar a mudança.
- Se ambas falharem, seguir para instrumentação. Se só houver corrupção imediata, reclassificar o caso antes de executar a matriz progressiva.

## Fase de diagnóstico e matriz mínima

**Entrada:** reprodução registrada. **Saída:** correlação precisa entre primeiro artefato, coleta, transferência e autoridade da imagem, ou hipótese específica descartada.

| Experimento | Variável alterada | Controles | Evidência procurada |
|---|---|---|---|
| Baseline | Nenhuma | Mesmo save, rota, resolução, FPS, driver e cache inicial | Tempo/quantidade de transições até primeiro artefato e volume de recursos descartados |
| Orçamento | Padrão versus override calculado para a máquina | Mesma versão e rota; novo processo para cada configuração | Artefato acompanha o primeiro descarte ou apenas o tempo? Há divergência após o mesmo número de descartes? |
| Fast Screen Effects | Ligado versus desligado | Mesmo orçamento; repetir primeiro no caso #1103 | Primeiro writer/alias incorreto depende do patch? |
| Resolução | Nativa versus resolução do caso, uma por execução | Mesmo patch/orçamento e rota | Pressão chega antes ou existe erro de pitch/extent independente da pressão? |
| Readback | Estado original versus outro modo, apenas se a trilha exigir | Não misturar com mudança de orçamento/patch | Qual modo reproduz a mesma divergência de bytes? O modo desligado já falha na #1016 |

- Executar pares suficientes para demonstrar recorrência e variabilidade; definir a duração com base no tempo observado até a falha de cada caso. Não encerrar teste de uma hora após poucos minutos.
- A instrumentação proposta deve registrar endereço guest, tamanho, mapping generation, image ID e UID, formato/pitch/extent, flags, writer/backing e gerações do alias, tick GPU, motivo e resultado de cada writeback/eviction relevante.
- Preferir buffer circular e descarregamento limitado ao primeiro erro, sem conteúdo sensível nem dump indiscriminado de memória. Medir overhead separadamente; diagnóstico pesado não é benchmark de desempenho.
- Registrar imagens criadas/destruídas, evictions, writebacks e bytes por intervalo; uso e orçamento de memória por heap; tempo de GC e frame time p95/p99. O log deve permitir distinguir a RAM do guest das alocações de imagens.
- Implementar, se necessário e após spec, acionamento de GC apenas no harness de teste, com conjunto de recursos conhecido. Reduzir moderadamente o budget num processo de teste é um estímulo alternativo, mas não garante o caminho exato e pode provocar OOM.
- Não usar `7000` indiscriminadamente. Selecionar orçamento a partir do heap/budget efetivo e do consumo de buffers, swapchain, staging e outros processos; manter reserva medida. Em UMA, a VRAM “dedicada” exibida pelo Windows não representa sozinha a capacidade disponível.
- Interromper uma execução ao primeiro artefato documentado, falha de alocação, perda de dispositivo ou esgotamento de margem definida para a máquina. Preservar log antes de relançar. Nunca desabilitar GC indefinidamente ou elevar limite além do orçamento disponível para prolongar o teste.

## Fase de testes de regressão antes da correção

**Entrada:** operação suspeita reduzida a um caso controlado. **Saída:** teste que falha sem a correção e passa com ela, ou retorno ao diagnóstico se a hipótese não reproduzir o problema.

- Reutilizar a infraestrutura existente antes de criar outra. Os alvos `aliasing-test`, `readback-queue-test` e `readback-sources-test` estão declarados em [`gpu/CMakeLists.txt`](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/CMakeLists.txt). Eles não comprovam, sozinhos, integridade de uma transferência Vulkan real.
- **Autoridade de alias:** criar duas views compatíveis sobre a mesma região, escrever padrão GPU conhecido, consultar pela outra, forçar descarte do writer e depois do backing. Validar bytes e leitura pelo alias sobrevivente, inclusive após reutilização de um ID com outro UID.
- **Reutilização de mapping:** escrever padrão A na GPU, agendar transferência, liberar/remapear a região e gravar padrão B. Drenar o trabalho antigo. B e as sentinelas vizinhas devem permanecer intactos; o protocolo correto de descarte/cancelamento é o que o teste precisa verificar.
- **Vizinhos na mesma página:** duas imagens em intervalos diferentes da mesma página com padrões distintos; descartar uma não pode cancelar o readback necessário da outra nem sobrescrever seus bytes.
- **Streaming e invalidação:** carregar padrão novo no backing por caminho sem page fault e verificar atualização de imagens CPU-dirty e aliases GPU-modified, incluindo mudanças de autoridade.
- **Pitch e views:** testar largura alinhada e preenchida, alturas parciais e formatos permitidos pelo caminho real; verificar conteúdo e padding/sentinelas, além da quantidade de imagens criadas.
- Separar testes de estado/filas em CPU dos testes com imagem produzida por GPU e download real. Executar os últimos em hardware e driver identificados; skip por falta de Vulkan/hardware é pendência, não sucesso.
- Usar o target oficial C++23 do projeto. Não substituir o renderer por um harness independente em padrão de linguagem diferente. Antes de usar comandos de build, conferir a configuração atual e os alvos necessários.

Requisitos críticos verificáveis:

- Quando uma imagem modificada pela GPU for descartada, o sistema deve preservar o conteúdo ainda válido exigido pelos consumidores.
- Quando uma região for remapeada, nenhuma transferência pertencente à geração anterior deve sobrescrever dados da nova geração.
- Enquanto um alias sobreviver, o sistema deve resolver a autoridade de conteúdo sem referenciar imagem removida ou UID reutilizado.
- Se uma transferência não puder ser aplicada à geração esperada, o sistema deve seguir um caminho de cancelamento/ressincronização verificável, sem escrever em memória substituta.
- O sistema deve sempre preservar os bytes e readbacks válidos de vizinhos que compartilham uma página.

## Fase de correção mínima e decisão

| Resultado do diagnóstico | Ação permitida pela evidência | O que não concluir |
|---|---|---|
| Primeira divergência em writeback de geração antiga | Corrigir ordenação/validação de geração e cobrir o caso de remap | Aumentar o budget não corrige a perda de dados |
| Alias sobrevivente usa writer/backing inválido | Corrigir transferência/reset de autoridade e invalidação necessária | Remover aliases ou sincronizar tudo globalmente não é solução mínima demonstrada |
| Bytes corretos, mas criação excessiva em largura preenchida | Avaliar/aproveitar #1104 e medir seu efeito isolado | Não encerrar #1016/#1103 com essa otimização |
| Conteúdo correto após coleta, mas coleta gera thrashing e falha de alocação | Avaliar política de budget/heap e retenção, com limites em placas menores/UMA | Limiar único maior não é seguro para todos os dispositivos |
| Apenas instalação reextraída funciona, com WADs diferentes | Documentar diferenças, validar base/update e separar caso de dados | Não atribuir automaticamente à região CUSA |
| Artefato sem pressão/eviction observada, com bytes corretos nesse caminho | Ampliar para alias/refresh/render pass/shader/driver conforme captura | Não forçar toda issue a caber na hipótese GC |

Qualquer mudança precisa passar pelo teste de regressão primeiro, revisão dos chamadores e validação do fluxo real. Não retirar asserts nem ignorar erros de formato/alocação para fazer a sessão continuar.

## Fase de validação, pacote e confirmação

- Rodar os testes focados afetados e depois build/checks relevantes ao patch. Registrar comandos, SHA, ambiente e resultado; não copiar resultados antigos como validação nova.
- Comparar base e candidata no mesmo trecho e estado inicial. Exigir integridade também sob GC forçado, não apenas ausência de artefatos com orçamento ampliado.
- Para casos progressivos, cobrir período superior à janela que reproduziu a falha na base, com número comparável de transições, upgrades e efeitos. Registrar quantidade real de repetições e duração; uma execução sem falha não estabelece taxa zero.
- Cobrir ao menos o hardware do relator, uma GPU de outro fornecedor quando o mesmo caminho for afetado e uma configuração com memória menor. Registrar explicitamente as lacunas; ausência de hardware não bloqueia preparar o diagnóstico, mas limita o alcance da conclusão.
- Verificar ausência de novos artefatos, crashes, OOM, travamentos de carga e perda de colisão. Medir memória e frame time contra a base; os limites de regressão aceitáveis devem ser definidos com as medições e objetivo do patch antes da aceitação, não inventados depois do resultado.
- Preparar pacote do SHA exato, checksum, notas e roteiro. Classificar como diagnóstico enquanto a causa permanecer aberta; usar correção candidata quando houver regressão reproduzida e corrigida. Publicar somente após autorização explícita.
- Testar em instalação separada. O rollback consiste em voltar ao pacote/configuração/cópia de cache anteriores, preservados antes do ensaio. Não sobrescrever saves para testar uma hipótese visual.
- Solicitar comparação ao relator com logs completos antes/depois, primeiro artefato ou sucesso, tempo, trecho e configurações efetivas. Preservar `last_run.log` antes da próxima execução; não solicitar dump/PKG do jogo.
- Encerrar apenas o subconjunto confirmado. #915/#1107 e casos que não reproduzem o teste corrigido continuam independentes. Silêncio do relator, merge da PR ou aprovação sintética não confirmam gameplay.

## Critérios de aceite e registro de entrega

- A primeira divergência foi localizada ou o mecanismo foi reproduzido em teste reduzido, com relação demonstrada à correção.
- O teste de regressão falha na base e passa na candidata; valida conteúdo, não só contadores ou presença de código.
- Descarte, aliases e remaps preservam os invariantes acima nos casos afetados; os testes negativos não são omitidos.
- A rota real anteriormente defeituosa passa com exposição comparável e GC efetivamente exercitado, incluindo memória menor quando aplicável.
- Logs de base/candidata, vídeo/capturas do ponto, hashes dos pacotes, configurações e resultados são rastreáveis à mesma comparação.
- O parecer final distingue correção confirmada por cenário, mitigação, casos não reproduzidos e validações pendentes; nenhuma issue é encerrada apenas por semelhança visual.

## Pendências que impedem uma conclusão causal hoje

- Não há captura do instante do primeiro artefato correlacionada à operação exata de GC/writeback nos logs analisados.
- Os resultados com orçamento maior não têm pares controlados nem evidência de integridade após volume equivalente de descartes.
- A #1103 não anexou log, driver nem SHA do pacote; falta demonstrar dependência exclusiva de Fast Screen Effects.
- O relato de aproximadamente uma hora na #863 e os números das outras issues pertencem a execuções distintas; não combinar suas identidades/configurações.
- A base remota atual contém mudanças posteriores à release dos relatos. Falta reproduzir o conjunto nessa base antes de propor novamente correções existentes.
- Não foram certificadas as extrações dos jogos. Uma comparação de WADs deve usar a mesma região/base/update; hashes iguais entre cópias da mesma fonte não provam que ambas estejam corretas.

Próximo resultado útil: obter um par de execuções com artefato e sem artefato sob condições controladas e um teste de integridade que force o mesmo descarte. Esse par decide se a primeira correção deve atacar a autoridade dos dados, sua ordenação ou a política de memória.

## Primeiro experimento e decisão de continuidade

- **Caso principal:** selecionar um participante da #1059 que reproduza corrupção progressiva e esteja disponível para comparar as condições. Os resultados anteriores com orçamento maior orientam o ensaio, mas não substituem uma baseline nova.
- **Entradas necessárias:** pacote e hardware identificados, save/rota, cópias equivalentes dos caches e orçamento seguro definido para a máquina. Se o participante não conseguir reproduzir, usar #1016/#1080 com checkpoint e primeiro artefato documentados; não ampliar a matriz sem esse ponto de partida.
- **Par de orçamento:** padrão versus override seguro na mesma build, sem alterar resolução ou Fast Screen Effects. Reiniciar o processo para cada condição e guardar tempo até o primeiro artefato, quantidade de transições, descarte, writebacks e uso por heap. Um caso sem falha e sem descarte exercitado valida apenas a mitigação observada.
- **Par do patch:** na mesma build e orçamento, comparar Fast Screen Effects ligado/desligado, preferencialmente no trecho da #1103. Não misturar a PR #1104 neste par; sua avaliação é outra variável e exige identificação do head.
- **Saída mínima:** pares de logs e capturas sincronizadas com o primeiro artefato. Havendo correlação com descarte, o próximo trabalho é o teste reduzido de conteúdo após GC e reutilização; sem correlação, revisar refresh/aliases/driver em vez de elevar o orçamento padrão.
- **Dependências comuns:** [protocolo, referências e revisão coordenada](2026-10-29-roadmap-investigacao-issues.md).
