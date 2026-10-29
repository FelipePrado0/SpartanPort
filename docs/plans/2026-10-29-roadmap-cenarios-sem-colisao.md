# Roadmap de investigação: cenários ausentes e sem colisão

Data da consulta: 29/10/2026. Projeto: `FelipePrado0/SpartanPort`.

Este documento organiza investigação, correção candidata e validação futura. Foram lidos corpos e comentários das issues citadas, três logs anexados e trechos do código remoto. Não foram executados gameplay, build ou testes de correção. As caixas abaixo são trabalho pendente; não representam implementação aprovada.


## Progresso (29/10/2026): rodada 1 concluída, frente pausada

**Branch:** `diag/missing-collision` (de `main` `f593377`). **Estado:** pausada pelo mantenedor para priorizar o Alto FPS. Nada foi testado no jogo.

**Feito:**
- [x] Revalidação: sem comentários novos nas issues do inventário; #899 fechada; linhas de código conferem. Caso novo candidato: #1051 (Caminho de Eos, fundo azul e queda no vazio, sem log).
- [x] Testes de memória: substituição flexível e mapeamento substituto recusado passaram na base (hipóteses enfraquecidas).
- [x] **Bug achado e corrigido com TDD:** a notificação de unmap da geração A, ainda em voo, podia gravar o readback sobre a geração B mapeada no mesmo endereço. Cada mapeamento ganhou uma geração; escritas atrasadas da thread que notifica são descartadas e contadas. `late_unmap_of_old_generation_keeps_new` falhava na base e passa. Spec: `docs/specs/2026-10-29-col-stale-unmap-writeback.md`. Cobertura parcial: readback feito por outra thread não é filtrado. Sem prova de ligação com #871/#1000.
- [x] Trace `Col:` em `runtime_memory.c` (`SPARTAN_MEM_TRACE=1`, `SPARTAN_MEM_TRACE_MIN_KB`, `SPARTAN_MEM_TRACE_MAX`), com teste. `build.sh --test` verde no Linux.
- [x] Ficha, roteiro do mantenedor (12 itens) e rascunho bilíngue aos relatores em `docs/investigations/colisao/` (não publicado).

**Falta:**
- [ ] Compilar e testar no Windows (código específico de Windows não compilado aqui).
- [ ] Alinhar a base de tempo do `Col:` com a do `Load:` (diag/infinite-loading) para correlacionar logs.
- [ ] Investigar o unmap em `sceKernelBatchMap`, que avisa a GPU depois de desmapear (sem teste).
- [ ] Teste no jogo pelo mantenedor, classificação da #1051 e da #839 (esta fica no catálogo de Alto FPS até haver prova de área sem geometria).

## Objetivo e escopo

Determinar por que uma área esperada deixa de existir visualmente e/ou fisicamente durante a progressão. O resultado deve identificar uma causa reproduzível e validar que a área permanece acessível depois da correção, sem simplesmente esconder o sintoma.

- Casos centrais: corredor após os Juízes do Submundo/Pit of Tartarus, área de Hefesto ausente e plataforma após a corrente de Hades.
- Casos a classificar separadamente: parede invisível nas Correntes do Equilíbrio e queda após a cena de Cronos.
- Fora deste agrupamento, até prova contrária: dificuldade de esgueirar na parede, input/QTE e corrupção apenas de cores/texturas quando a geometria física continua presente.
- Crashes em integradas Intel ficam fora do escopo solicitado.

## Referências e baseline

- `main` consultada: [`f593377bbaf1934bf606171973d7a58210510991`](https://github.com/FelipePrado0/SpartanPort/commit/f593377bbaf1934bf606171973d7a58210510991).
- Release estável consultada: [`windows-v2.7.0`](https://github.com/FelipePrado0/SpartanPort/releases/tag/windows-v2.7.0), publicada em 22/10/2026, referência declarada `003e42b6e004ed686aae58c053d2301fe9c8c68e`.
- A `main` contém mudanças posteriores à release, incluindo a integração da [PR #1005](https://github.com/FelipePrado0/SpartanPort/pull/1005). Não tratar o estado de um relato em 2.7.0 como reprodução na `main`.
- Ao iniciar a execução deste plano, reconfirmar release, SHA e eventuais novos comentários. Registrar a referência efetivamente compilada, não apenas o nome da branch.

## Inventário de evidências

“Log” indica informação observada no anexo; “relato” indica resultado descrito pelo usuário; “hipótese” não significa causa demonstrada. O estado aberto/fechado é o da consulta.

| Caso e estado | Evidência e configuração disponível | Interpretação e limites |
|---|---|---|
| [#279, fechada](https://github.com/FelipePrado0/SpartanPort/issues/279) | Relato original em 2.2.0, RX 580; autor informa [CUSA01715 1.02](https://github.com/FelipePrado0/SpartanPort/issues/279#issuecomment-6186035803). Consolidação registra AMD e NVIDIA no mesmo corredor. | O sintoma não está restrito a uma fabricante. A consolidação histórica não substitui a reprodução individual de cada duplicata. |
| [#279, save anterior](https://github.com/FelipePrado0/SpartanPort/issues/279#issuecomment-6209055581) | Usuário relata que, em 2.4.0, carregar save manual anterior e percorrer cerca de oito minutos fez a área carregar. [Outro usuário confirma o contorno](https://github.com/FelipePrado0/SpartanPort/issues/279#issuecomment-6218494457). | Indício de influência do estado restaurado, ordem de carga ou tempo de streaming. Não prova corrupção do autosave nem solução geral. O encerramento posterior pediu atualização, sem comprovar correção deste trecho. |
| [#405, fechada como duplicata](https://github.com/FelipePrado0/SpartanPort/issues/405) | Linux, RX 9070 XT, port 2.3.0; área de Hefesto vira vazio. [Autor relata sucesso ao carregar save manual anterior](https://github.com/FelipePrado0/SpartanPort/issues/405#issuecomment-6210158479). CUSA/APP_VER não confirmados nesta análise. | Contorno semelhante ao da #279, mas ainda sem prova de causa comum com carregamento infinito. Foi encerrada como duplicata, não como fix confirmado. |
| [#871, aberta](https://github.com/FelipePrado0/SpartanPort/issues/871) | Relator afirma falha com CUSA01623 01.00/01.02 e CUSA01715 01.02, inclusive em jogo novo. [Log atualizado](https://github.com/user-attachments/files/33748229/Log.txt): CUSA01715 01.02, RTX 2050, engine 60 FPS, save `SD-261021002258`, readback deferred/stale ativo. | O contorno “60 FPS e save manual” não é suficiente neste caso. Relatos de múltiplas instalações enfraquecem explicação exclusiva por região, mas não certificam a integridade delas. |
| [#871, WAD/cache](https://github.com/FelipePrado0/SpartanPort/issues/871#issuecomment-6306886363) | No mesmo log, `Hds450` registra `fSize` de 216.229.824 bytes. Há relatórios de pressão de cache com 794 expulsões/379 writebacks e depois 621/137. | `fSize` não certifica processamento ou registro da colisão. Pressão de cache é correlação a investigar; não prova que causou a área ausente. O WAD da área que deveria aparecer precisa ser identificado, sem assumir que é o último impresso. |
| [#899, fechada com confirmação](https://github.com/FelipePrado0/SpartanPort/issues/899) | Port 2.6, RTX 4060. [Trocar a instalação resolveu](https://github.com/FelipePrado0/SpartanPort/issues/899#issuecomment-6336758922); autor informa que saiu da versão USA 1.02 para [CUSA01715 EUR](https://github.com/FelipePrado0/SpartanPort/issues/899#issuecomment-6343406643). | Evidência de influência dos dados/instalação naquele caso. A versão da atualização da instalação funcional e hashes dos WADs não foram comparados. O próprio [encerramento](https://github.com/FelipePrado0/SpartanPort/issues/899#issuecomment-6496549116) mantém essa incerteza. Não concluir que EUR é universalmente correta. |
| [#1000, aberta](https://github.com/FelipePrado0/SpartanPort/issues/1000) | Port 2.7.0, RTX 3080 Ti; plataforma visível à distância desaparece ao se aproximar, sem colisão. [Log anexado](https://github.com/user-attachments/files/33877039/last_run.md): CUSA01715 **01.00**, engine 120 FPS, save `SD-261022201452`, deferred readback desligado; `Hds100 fSize=268.956.800`. Sem mensagens de pressão de cache no anexo. | Contraponto a atribuir todos os casos à atualização 01.02, autosave ou deferred readback. Ausência de mensagem de pressão não prova ausência de qualquer problema de cache. |
| [#839, aberta](https://github.com/FelipePrado0/SpartanPort/issues/839) | Port 2.6.0, Vega 7; parede invisível ao saltar da segunda corrente. Sem log e sem CUSA/APP_VER confirmados. | Classificar como colisão excedente ou trajetória/input/timing até verificar geometria. Não é automaticamente “chão ausente”. |
| [#975, aberta](https://github.com/FelipePrado0/SpartanPort/issues/975) | Port 2.7, RX 5700 XT; mistura morte do inimigo montável com cenário que some após a cena de Cronos. [Log após atualização](https://github.com/user-attachments/files/33914148/last_run.log): CUSA01623 01.02, engine 120 FPS, `AUTOSAVE`, deferred/stale ativo; sem mensagens de pressão de cache no anexo. | Separar falha do combate/montaria da falha da área. O anexo comprova configuração, não isola a causa da queda nem demonstra que a atualização resolveu. |

### Casos próximos que não devem ser fundidos automaticamente

- [#535](https://github.com/FelipePrado0/SpartanPort/issues/535): Kratos não avança lateralmente na parede; comentários relatam contorno com 60 FPS e botão de salto. Isso descreve interação/traversal com superfície presente, sem prova de área descarregada.
- [#1011](https://github.com/FelipePrado0/SpartanPort/issues/1011): dificuldade de esgueirar em Hades; não há log. A alegação de que ocorre no jogo original é comentário de usuário, não comparação verificada neste trabalho.
- [#701](https://github.com/FelipePrado0/SpartanPort/issues/701#issuecomment-6392035040): em carregamento infinito, o autor principal relata ter resolvido ao extrair novamente a instalação, pois faltavam arquivos. É evidência de que extração pode produzir sintomas de carga, mas não demonstra a causa das #871/#1000. O comentário de outro participante sobre `Bas400` deve permanecer separado da resolução do autor.

## Hipóteses refutáveis

| Hipótese | Evidência favorável | Contraponto | Teste que decide o próximo passo |
|---|---|---|---|
| Arquivos ausentes, truncados ou montagem incorreta do jogo base com atualização | Troca da instalação resolveu #899; reextração resolveu um caso de loading em #701. | #871 relata falha em múltiplas combinações. Não foram fornecidos manifestos íntegros dessas instalações. | Reextrair o mesmo jogo base e a mesma atualização compatível em pasta vazia; comparar caminhos, tamanhos e hashes; repetir o trecho sem trocar port, save ou configuração. |
| Estado restaurado não solicita ou não registra corretamente a próxima área | Save manual anterior ajudou #279/#405. | #871 e #1000 já carregam saves manuais; #871 relata reprodução em jogo novo. | Comparar carga direta perto do defeito com percurso desde checkpoint anterior na mesma instalação. Registrar a primeira divergência no ciclo do WAD. |
| Dados são carregados, mas invalidados ou sobrescritos na transição | #871 tem `fSize` e muita atividade de cache; #1000 vê a plataforma desaparecer ao se aproximar. | Não há rastreamento de endereço/geração dos dados da área nem prova de ligação com colisão física. | Correlacionar leituras, mapeamentos, unmaps e writebacks com a região de memória da área esperada. Verificar se os bytes mudam após a carga sem escrita legítima identificada. |
| Condição de corrida entre streaming, GPU e host | Sintomas variam com caminho de carga; runtime recebeu correções de memória após a release. | FPS baixo e desativação de opções não resolveram #871. O mecanismo permanece desconhecido. | Reproduzir na release e na main atual, depois isolar somente a mudança relacionada que altera o resultado. Capturar threads e ordem dos eventos, sem depender de um teste que altera vários fatores. |
| Região/APP_VER específica | #899 deixou de falhar ao mudar instalação/região. | #871 relata duas regiões; #1000 usa 01.00 e outros casos usam 01.02. | Só comparar regiões depois de validar instalações completas e controlar saves compatíveis. Se houver diferença estável, localizar o arquivo ou tratamento específico responsável. |
| Problema apenas visual/culling | Plataforma some com aproximação em #1000. | A queda física é relatada; culling sozinho não explica ausência real de colisão. | Exercitar visualização e contato físico separadamente, no mesmo ponto. Se o piso continua sustentando Kratos, encaminhar como problema visual distinto. |

Nenhuma hipótese acima está confirmada como causa geral. Não usar warnings de extensões Vulkan, a palavra `fSize`, o nome `AUTOSAVE` ou a GPU isoladamente como diagnóstico.

## Fase de preparação: tornar cada execução comparável

- [ ] Selecionar um caso reproduzível de área ausente: preferir #871 ou #1000, mantendo #279/#405 como comparação histórica.
- [ ] Preservar instalação funcional, saves, configurações e logs originais em cópias separadas. Nunca sobrescrever o único save do relator nem limpar caches originais sem guardar a condição inicial.
- [ ] Registrar port/tag/commit, SO, CPU, GPU/driver, CUSA, APP_VER, hash do executável, mods, patches, resolução, engine FPS, limite FPS e modo de readback efetivamente escrito no log.
- [ ] Identificar o ponto de observação por vídeo/screenshot, caminho de aproximação e ação que evidencia a colisão: caminhar, saltar ou aterrissar.
- [ ] Guardar log completo por execução antes de abrir o port novamente. Usar identificadores de execução com instalação, build, save e configuração.
- [ ] Medir separadamente: área visível, piso sustentando Kratos, interação necessária disponível e passagem ao checkpoint seguinte. Uma screenshot não comprova colisão.

Entrega desta fase: ficha de reprodução com uma condição defeituosa repetível. Se o comportamento não repetir, coletar novas execuções e contexto antes de escolher um fix.

## Fase de integridade: verificar a hipótese do PKG/extração

- [ ] Criar nova extração em diretório vazio a partir dos mesmos arquivos de origem disponíveis ao usuário. Registrar ferramenta/versão, opções e erros. Não mesclar sobre a pasta antiga.
- [ ] Conferir os metadados do jogo base e da atualização e a aplicação da atualização compatível com aquele jogo. Registrar de onde veio cada arquivo substituído; não misturar atualizações de outra região para “testar”.
- [ ] Gerar manifesto local dos caminhos relativos, tamanhos e SHA-256, começando pelos WADs realmente solicitados na transição; expandir para a instalação se houver diferenças.
- [ ] Comparar extração original e nova. Hashes iguais mostram equivalência entre as cópias, não autenticidade/integridade do conteúdo de origem. Reextrair o mesmo PKG defeituoso pode produzir o mesmo arquivo defeituoso.
- [ ] Usar uma cópia independentemente verificada, da mesma região/base/atualização, quando disponível. Não pedir upload de PKG ou WAD; compartilhar apenas metadados e hashes necessários.
- [ ] Repetir o trecho alterando apenas a instalação. Manter a cópia do save, port, GPU, driver e configurações.

Decisões:

- Nova extração funciona e antiga falha: comparar arquivos divergentes; repetir a diferença antes de concluir. Se origem e versão são iguais, extração/montagem ganha força como causa daquele caso.
- Ambas falham e diferem: localizar diferenças relevantes, mas não atribuir causa sem teste controlado.
- Ambas falham e são iguais: extração inconsistente entre as duas cópias perde força; conteúdo de origem defeituoso ainda não está excluído.
- Instalação independentemente verificada também falha: avançar para estado/streaming/runtime.
- Outra região funciona, mas arquivos/versão/saves também mudaram: registrar como resultado misto e repetir com controles. Não publicar “use CUSA01715” como correção geral.

O hash de `eboot.bin` mostrado por [`scripts/patches.py`](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/scripts/patches.py#L202-L208) cobre somente o executável. Não é verificador da instalação completa.

## Fase de isolamento: matriz mínima de reprodução

Executar primeiro um caso central; expandir para os demais apenas após obter uma diferença informativa. Cada comparação parte da mesma cópia inicial dos saves e da condição de cache documentada. Não trocar driver, região, port e patches simultaneamente.

| Comparação | Variável alterada | O que registrar | Decisão esperada |
|---|---|---|---|
| Instalação original versus reextração controlada | Somente arquivos do jogo | Manifestos, primeiro WAD divergente, resultado físico e visual | Separar dados da instalação de runtime. |
| Save próximo versus save anterior com percurso completo | Somente caminho de entrada no trecho | WADs solicitados, conclusão e ordem de carga, instante da falha | Identificar dependência de restauração/streaming; ambos os saves podem ser manuais. |
| Release 2.7.0 versus main fixada ao SHA | Somente build do port | Mesma instalação/save/configuração e logs completos | Verificar se mudanças já existentes eliminam o caso, sem atribuir o resultado a um commit específico ainda. |
| Engine original versus engine 60 FPS | Somente engine FPS; preservar o limite de apresentação quando a comparação for tecnicamente possível | Resultado, FPS efetivo e ordem dos eventos | Avaliar timing da engine. #871 já contradiz um contorno universal a 60 FPS. Se a combinação tornar o resultado ambíguo, definir outro controle antes de concluir. |
| Limite de apresentação original versus alternativo | Somente limitador; preservar engine e VSync | Cadência efetiva e resultado físico/visual | Separar ritmo da apresentação de ritmo da lógica. Não somar esta mudança à comparação de engine. |
| Readback atual versus modo alternativo suportado | Somente modo de readback | Eventos, dados restaurados e ocorrência da queda | Localizar dependência de sincronização. #1000 já falha com deferred desligado. |
| Orçamento padrão versus controlado, se houver pressão observada | Somente orçamento do cache adequado ao hardware | Tempo, evictions, writebacks e primeiro erro físico/visual | Testar ponte com texturas; melhora apenas visual não prova recuperação da colisão. |

Para um defeito determinístico, propor inicialmente três tentativas por condição, com processo reiniciado, e registrar todas. Se for intermitente, definir o número de repetições após medir a frequência na base; ausência em três tentativas não comprova eliminação.

## Fase de diagnóstico: localizar a primeira divergência

### Observabilidade necessária

- [ ] Correlacionar abertura/leitura/fechamento do WAD com timestamp monotônico, thread, caminho, offset, bytes solicitados/retornados e erro. Confirmar que a build dispõe do rastreamento antes de orientar o relator; não presumir que `SPARTAN_FILE_TRACE` da build diagnóstica da #345 esteja na release/main.
- [ ] Registrar alocações, mapeamentos, substituições e unmaps envolvidos, com endereço guest, tamanho e geração da região. Não usar somente endereço, pois ele pode ser reutilizado.
- [ ] Correlacionar writebacks/expulsões do cache que tocam essas regiões. O objetivo é determinar se a área nunca fica pronta ou se seus dados são perdidos depois.
- [ ] Capturar duas amostras das threads quando houver transição parada, antes de encerrar o processo, para distinguir espera de tarefa ativa. Dumps locais podem conter dados do processo; publicar apenas a evidência mínima necessária.
- [ ] Identificar os marcos reais de processamento/registro da área no binário guest, caso os logs de fronteira não bastem. Não inventar nomes de funções de colisão nem assumir que `fSize` significa “colisão registrada”.

### Pontos concretos do código remoto para inspeção

| Ponto atual | Pergunta de investigação | Limite da conclusão |
|---|---|---|
| [`do_read`/`do_pread`, runtime_file.c](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/src/runtime_file.c#L286-L317) | O WAD esperado foi solicitado e retornou todos os bytes? | Leitura completa não valida conteúdo ou processamento posterior. |
| [`retire_fixed`/`announce_mapped_locked`, runtime_memory.c](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/src/runtime_memory.c#L552-L581) | Remapeamentos aposentam a geração antiga e preservam a região quando o novo mapeamento falha? | O código atual já inclui tratamento de ordem/retorno; não propor reaplicar um fix existente. |
| [`CopySparseSerial`, spartanportgpu.cpp](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shim/spartanportgpu.cpp#L111-L126) | Uploads copiam a geração esperada da memória? | A leitura pela backing view já existe; não é prova de correção destes checkpoints. |
| [`ApplyBackingWrites`, texture_cache.cpp](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shadps4/video_core/texture_cache/texture_cache.cpp#L649-L671) | Dados novos de streaming invalidam as imagens certas, inclusive relações de alias? | Renderização atualizada não demonstra que os dados de colisão estão corretos. |
| [`UnmapMemory`, texture_cache.cpp](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shadps4/video_core/texture_cache/texture_cache.cpp#L690-L700) | Cancelamento de readback/liberação de imagens corresponde à geração que realmente saiu de uso? | O TODO de download não prova defeito: estabelecer primeiro o contrato do unmap e a propriedade dos dados. |

Se o WAD correto não é solicitado, investigar seleção/restauração/progressão. Se é lido e a tarefa para antes de disponibilizar a área, conectar ao roadmap de carregamento infinito. Se a área funciona e depois perde dados após unmap/writeback, conectar ao roadmap de texturas/memória, preservando a evidência específica de colisão.

## Fase de correção: escolher somente após a evidência

- [ ] Apresentar uma spec curta com entrada/saída, invariantes, arquivos e critérios antes de implementar mudanças que exigem o gate SDD. Este roadmap não aprova uma implementação futura.
- [ ] Se os arquivos estiverem ausentes/inconsistentes, corrigir o procedimento de extração/montagem e, se necessário e autorizado, melhorar diagnóstico de arquivo inválido. Não alterar física ou shaders para contornar dados faltantes.
- [ ] Se o estado restaurado omitir uma carga, localizar a transição exata e corrigir o requisito de carga/ordem. Não apagar autosaves nem forçar avanço de checkpoint.
- [ ] Se houver erro de lifetime/remapeamento, corrigir no ponto compartilhado responsável pela geração/ordem. Não remover sincronização sem demonstrar a dependência correta.
- [ ] Se houver dado antigo sobrescrevendo uma geração nova, impedir a escrita na geração errada. Aumentar orçamento de VRAM pode mascarar esse caminho e não é prova de integridade.
- [ ] Se o defeito for exclusivamente visual ou de input, separar o caso e aplicar o critério de aceite daquele problema. Não marcar colisão como corrigida apenas porque o chão voltou a aparecer.

## Testes e requisitos verificáveis

Os testes seguintes são propostos, não executados. Selecionar o teste que representa o mecanismo demonstrado, em vez de criar uma suíte especulativa para todas as hipóteses.

- Quando a execução chega ao ponto de transição reproduzível, a área esperada deve permitir completar o percurso físico e alcançar o checkpoint seguinte.
- Enquanto a área permanece ativa, seu piso deve sustentar Kratos nas ações de reprodução, e seus dados não devem ser invalidados por recursos de uma geração anterior.
- Se um mapeamento substituto falhar, a memória e os recursos ainda válidos devem permanecer utilizáveis conforme o contrato do runtime.
- Se faltar um arquivo obrigatório ou houver erro comprovado de leitura, a execução deve produzir diagnóstico identificável; não se deve apresentar silêncio como carregamento concluído.
- O sistema deve sempre preservar os saves originais durante a validação e manter separado resultado visual de resultado físico.

Regressões possíveis após isolar a causa:

- Fixture de instalação com arquivo necessário ausente/truncado: demonstrar erro de leitura/diagnóstico, sem distribuir arquivos do jogo. Não prometer validar formato WAD por tamanho apenas.
- Fixture de memória com conteúdo conhecido: carregar geração A, substituí-la por B e disparar callback/writeback atrasado de A; B deve continuar intacta.
- Mapeamento substituto recusado: conferir que o mapeamento anterior e sua visibilidade para GPU permanecem consistentes.
- Restauração/carga: usar fixture sintética somente se a lógica correspondente existir no código host. Para a lógica interna do jogo, usar o checkpoint real e uma build de diagnóstico; não simular como se fosse teste da engine.

Todo teste de regressão da correção deve falhar na base sem o fix e passar com ele. Build e testes sintéticos aprovados validam o mecanismo coberto, não a campanha completa.

## Validação e encerramento por caso

- [ ] Executar testes/lint/build pertinentes ao código efetivamente alterado, em branch/worktree isolada. Começar por testes focados; não compilar o projeto inteiro apenas para validar este documento.
- [ ] Repetir a condição defeituosa e a candidata no mesmo hardware, instalação, save e configuração, registrando tentativas, logs e resultado visual/físico.
- [ ] Validar aproximação, contato com o chão, travessia e checkpoint seguinte. Recarregar também o checkpoint criado depois da passagem e confirmar que o defeito não reaparece imediatamente.
- [ ] Conferir pelo menos o caso que motivou a correção e um caminho que já funcionava. Para mudança compartilhada de memória, incluir o outro sistema operacional e uma GPU de outra fabricante quando disponíveis; declarar o que não foi exercitado.
- [ ] Preparar prerelease diagnóstica ou candidata com commit exato, checksum e roteiro. Publicar, comentar externamente e encerrar issues somente mediante autorização explícita.
- [ ] Solicitar confirmação do relator no trecho original. Sem resposta, manter “aguardando teste”; correção parcial não encerra automaticamente casos diferentes.

Critério de saída: causa demonstrada ou limitação dos dados claramente isolada; regressão pertinente aprovada; checkpoint original atravessado com colisão e imagem corretas; resultado da candidata documentado; limitações restantes explícitas.

## Dados faltantes e condições para parar

- Ainda faltam manifestos comparáveis de instalações defeituosas/funcionais, WAD alvo de cada área ausente, rastreamento de geração da memória e reprodução na main atual.
- A alegação de múltiplas regiões da #871 não inclui comparação completa dos arquivos; a resolução da #899 não identifica a atualização final. Não preencher essas lacunas por inferência.
- A #839 precisa de log, CUSA/APP_VER e classificação física/visual antes de entrar na matriz central. Na #975, separar combate/montaria de carga do cenário antes de escolher um fix.
- Se a base não reproduzir, não anunciar validação da candidata. Registrar a limitação e pedir o contexto que falta.
- Se houver corrupção de save, falha nova de memória ou perda de dados, interromper aquela variante, preservar artefatos e retornar à instalação/cópia anterior.
- Se um teste depender de outro hardware ou de arquivo não disponível, registrar a pendência. Não alterar CORS, infraestrutura, dados ou configurações globais para forçar a validação.

## Resultado desta análise

O melhor primeiro caminho é validar a instalação com a mesma região/base/atualização e reproduzir o trecho na main atual. Depois, comparar caminhos de entrada e instrumentar o ciclo do WAD e as gerações de memória. Há evidência para hipóteses de arquivos, estado e lifetime, mas nenhuma explica de forma confirmada todos os relatos.

Foram conferidos estados das issues, main/release, comentários e logs #871, #1000 e #975. Não foram modificados código, saves, configurações ou serviços externos. Nenhuma correção nem validação de gameplay foi executada.

## Primeiro experimento e decisão de continuidade

- **Caso principal:** #871, com cópia do save manual `SD-261021002258` e engine 60 FPS. Esse controle já evita repetir como novidade os contornos de save manual e engine 60.
- **Entradas necessárias:** instalação original preservada, nova extração da mesma CUSA/base/update, manifesto e vídeo da aproximação. Não usar save de outra região sem confirmar compatibilidade; um save incompatível invalida a comparação.
- **Par de conteúdo:** manter port e estado inicial fixos. Observar se o chão aparece, sustenta Kratos e permite chegar ao próximo checkpoint em ambas as instalações. Repetir a diferença antes de responsabilizar um arquivo.
- **Par de runtime:** com o conteúdo fixo, comparar release e main no mesmo save. Se o resultado mudar, isolar commits; se não mudar, registrar a primeira diferença na solicitação, processamento ou permanência da área.
- **Contraprova:** aplicar a hipótese delimitada à #1000, cuja atualização é 01.00 e cujo log não tem deferred readback ativo. Se o mecanismo não reproduzir ali, manter esse caso separado.
- **Saída mínima:** resultado visual e físico separados por execução, ponto exato da falha, logs e manifestos. Só abrir trabalho de instrumentação de registro de colisão quando o WAD esperado e a primeira divergência estiverem identificados, ou quando os dados disponíveis demonstrarem a necessidade dessa observabilidade.
- **Dependências comuns:** [protocolo, referências e revisão coordenada](2026-10-29-roadmap-investigacao-issues.md).
