# Roadmap de investigação e correção: carregamento infinito

Data da análise: 29/10/2026. Repositório: `FelipePrado0/SpartanPort`.


## Progresso (29/10/2026): rodada 1 concluída, frente pausada

**Branch:** `diag/infinite-loading` (de `main` `f593377`). **Estado:** pausada pelo mantenedor para priorizar o Alto FPS. Nada foi testado no jogo.

**Feito:**
- [x] Revalidação: nenhuma das issues do inventário mudou; linhas de código conferem. A tag `windows-v2.6.0-issue345-test1` (`397c307`) serviu de base para o trace. #1113 (Intel UHD) fica fora do escopo.
- [x] `tools/game_manifest.py`: manifesto da instalação (caminho, tamanho, SHA-256) e comparação entre dois manifestos; 8 testes. Spec: `docs/specs/2026-10-29-load-game-manifest.md`.
- [x] Trace `Load:` em `runtime_file.c` (`SPARTAN_FILE_TRACE=1` ou `all`, `SPARTAN_FILE_TRACE_LIMIT`) com início/fim, thread, offset, bytes e errno preservado.
- [x] Watchdog de carga parada (`SPARTAN_LOAD_WATCHDOG=<s>`), só registra no log: no Linux com pc/sp/syscall, no Windows com id, nome e CPU das threads. `build.sh --test` verde no Linux (15 PASS).
- [x] Ficha, roteiro do mantenedor e rascunho bilíngue aos relatores em `docs/investigations/carregamento/` (não publicado).
- Nenhum bug de runtime encontrado nesta rodada.

**Falta:**
- [ ] Compilar e testar no Windows (sem MinGW aqui); rodar o manifesto no PowerShell.
- [ ] Endereço de instrução das threads no Windows (`SuspendThread`/`GetThreadContext`): só na spec.
- [ ] Teste no jogo pelo mantenedor: manifesto, reextração em pasta vazia e 2.7.0 × branch no mesmo checkpoint.

## Objetivo e estado

Eliminar carregamentos de áreas que deixam o jogo renderizando indefinidamente sem devolver o controle ao jogador. O plano deve distinguir instalação incompleta, processamento de dados inválidos e bloqueios do runtime antes de escolher uma correção.

- **Executado nesta análise:** consulta aos corpos e comentários das issues abaixo; releitura dos logs anexados às #345, #982 e #1047; leitura adicional dos anexos #987 e #990 na revisão; inspeção dos trechos de código remoto citados.
- **Não executado:** extração de PKGs, comparação de WADs reais, execução do jogo, captura de threads de uma reprodução atual, implementação, build ou testes de uma correção.
- **Conclusão atual:** existe evidência de resolução por reextração em um relato de Helios e existe evidência de leitura completa seguida de bloqueio em outro relato, após Hades. Não há causa única comprovada.
- **Fora do escopo:** crashes em integradas Intel; tela preta antes do menu sem confirmação de transição de área; ausência de colisão sem carregamento infinito; quedas de FPS que posteriormente se recuperam. Esses casos só entram se houver reprodução e assinatura compatíveis.

## Baseline reproduzível

Estado remoto conferido em 29/10/2026:

| Referência | Identidade verificada | Uso no plano |
| --- | --- | --- |
| `main` | [`f593377bbaf1934bf606171973d7a58210510991`](https://github.com/FelipePrado0/SpartanPort/commit/f593377bbaf1934bf606171973d7a58210510991) | Base atual de investigação; não equivale ao binário estável instalado pelo relator. |
| Release estável Windows | [`windows-v2.7.0`](https://github.com/FelipePrado0/SpartanPort/releases/tag/windows-v2.7.0), alvo `003e42b6e004ed686aae58c053d2301fe9c8c68e` | Controle de versão em que há relatos recentes. Registrar também SHA-256 do pacote e dos executáveis efetivamente usados. |
| Build histórica de diagnóstico | [`windows-v2.6.0-issue345-test1`](https://github.com/FelipePrado0/SpartanPort/releases/tag/windows-v2.6.0-issue345-test1) | Origem do rastreamento detalhado de WAD da #345; não é uma correção confirmada nem substitui testar a base atual. |

Antes de executar o roadmap, atualizar esse quadro, conferir o commit exato dos artefatos e repetir a análise de diferenças relevantes. Não usar o checkout local atrasado como prova do estado da `main` remota.

## Evidências e limites por caso

As classificações abaixo distinguem dados conferidos no log, relato do usuário e hipótese. Uma issue fechada não é prova de correção.

| Caso e estado consultado | Identificação e configuração | Evidência disponível | O que permite concluir; limite |
| --- | --- | --- | --- |
| [#345, aberta](https://github.com/FelipePrado0/SpartanPort/issues/345) | CUSA01623 01.02; Windows; RTX 4060 Laptop; log de diagnóstico com engine 120 FPS e deferred/stale readback ligados. | No [log de diagnóstico](https://github.com/user-attachments/files/33722598/last_run.log), 188 pares de `pread`, offsets contínuos, total solicitado e retornado de **98.197.264 bytes**, todos com `errno=0`, para HDS690.WAD. Sem conclusão `fSize` nem fechamento posterior desse WAD. O [relator confirma o mesmo bloqueio](https://github.com/FelipePrado0/SpartanPort/issues/345#issuecomment-6292975169). | Evidência forte de bloqueio após a leitura do arquivo nessa execução. Não comprova integridade semântica dos bytes nem identifica a tarefa bloqueada. |
| [#428, fechada](https://github.com/FelipePrado0/SpartanPort/issues/428) e [#987, aberta](https://github.com/FelipePrado0/SpartanPort/issues/987) | Forja após luta com estátuas; #987 relata port 2.7.0. Seu [log](https://github.com/user-attachments/files/33862071/last_run.log) identifica CUSA01623 01.02, GTX 1060 3 GB, engine 120 FPS, deferred/stale ligado e carga do save manual `SD-261023000218`. | O anexo da #987 termina com Hds500 `alloc/ax8`, sem `fSize`, e contém pressão de cache com até 2022 expulsões/532 writebacks em um relatório. Há [confirmação de outro usuário](https://github.com/FelipePrado0/SpartanPort/issues/987#issuecomment-6370565419). | Sustenta recorrência do sintoma na Forja e fornece configuração real. O [encerramento antigo](https://github.com/FelipePrado0/SpartanPort/issues/428#issuecomment-6356789421) pediu atualização; não demonstrou correção. Não prova leitura completa, causalidade do GC ou causa igual à #345. |
| [#701, fechada](https://github.com/FelipePrado0/SpartanPort/issues/701) | Autor original relata port 2.5.0, RTX 3060 Ti/Ryzen 7 5700X; CUSA do autor não foi estabelecida. | O autor [informou que resolveu ao baixar/extrair novamente porque faltavam arquivos](https://github.com/FelipePrado0/SpartanPort/issues/701#issuecomment-6392035040). | **Relato direto favorável à instalação incompleta como causa de pelo menos um caso de carregamento infinito.** Não há manifesto antes/depois, ferramenta de extração nem lista dos arquivos faltantes. Não atribuir ao autor os dados de hardware/CUSA de outro comentarista. |
| [#982, aberta](https://github.com/FelipePrado0/SpartanPort/issues/982), relacionada a comentário na #701 | CUSA01715 01.02; port 2.7.0; RX 9070 XT; engine 60 FPS no log. | [Log de Helios](https://github.com/user-attachments/files/33854487/last_run_helios_bas400_v2.7.0.log) termina com Bas400 `alloc/ax8`, sem `fSize`. O corpo relata testes sem deferred/stale, mas **o log anexado tem ambos ligados**. O [comentário anterior](https://github.com/FelipePrado0/SpartanPort/issues/701#issuecomment-6256911950) fornece contexto. | Candidato para comparação com #345. O log não contém o rastreamento completo necessário para provar que BAS400 foi integralmente lido. Repetir o teste de readback com estado efetivo registrado. |
| [#636, fechada](https://github.com/FelipePrado0/SpartanPort/issues/636) | CUSA01623 01.02; Radeon 660M/Ryzen 5 6600H; versões até 2.5.0; **mod de idioma sobre EXEC** declarado. | Relata Cav600 sem `fSize`, janela responsiva por mais de dez minutos, reproduzível em saves diferentes. | Evidência histórica de transição incompleta; o mod é variável adicional. Não concluir gargalo de memória ou ausência dele apenas por percentuais do Gerenciador de Tarefas. |
| [#1047, aberta](https://github.com/FelipePrado0/SpartanPort/issues/1047) | CUSA01623 01.02; Linux/RADV; RX 580 2048SP; engine 120 FPS; deferred/stale ligados no log. | [Log das cavernas](https://github.com/user-attachments/files/33911192/last_run.log) termina com **Cav500**, não Cav600, iniciado sem `fSize`. Há pressão do cache em momentos anteriores. | Afasta exclusividade de Windows para a família de sintomas. Não tratar Cav500 e Cav600 como o mesmo WAD. O [comentário sobre crash ao acionar alavanca](https://github.com/FelipePrado0/SpartanPort/issues/1047#issuecomment-6391159506) deve ser reproduzido e classificado separadamente. |
| [#990, aberta](https://github.com/FelipePrado0/SpartanPort/issues/990) | Formulário: port 2.7.0; RTX 4060 Ti 16 GB/Ryzen 7 5700X3D. O [anexo](https://github.com/user-attachments/files/33865976/last_run.log) identifica CUSA01623 01.00, engine 144 FPS e deferred/stale ligado. | Há divergência entre as evidências: o trecho colado no corpo mostra seleção de `AUTOSAVE` e `Pos700 fSize`; o anexo conferido não contém essa seleção nem esse `fSize` e termina com relatório de saída. | A identidade da execução realmente afetada continua pendente. Solicitar log e save correspondentes à falha; não atribuir automaticamente ao trecho colado a CUSA/configuração de outro anexo. Manter como caso independente até obter o par correto. |

O `eboot.bin` dos logs examinados de #345, #982 e #1047 tem SHA-256 `6e72330e9092e5164049b865e5568f2d450904001a73fe0a2f4b39194c195b3c`, apesar de CUSAs diferentes. Isso identifica esses executáveis; não certifica WADs, atualização aplicada, arquivos auxiliares ou a instalação inteira.

## Hipóteses e testes que as distinguem

| Hipótese | Evidência favorável | Limite ou contraponto | Teste decisivo e encaminhamento |
| --- | --- | --- | --- |
| Extração incompleta, fonte defeituosa, atualização incompatível ou instalação misturada | Resolução relatada pelo autor da #701; transições podem precisar de arquivos não usados no começo da campanha. | A #345 leu integralmente HDS690, mas isso não valida conteúdo nem dependências. Não há manifesto independente de instalação correta. | Reextrair o mesmo base/update compatível em pasta vazia e comparar manifestos. Se só a nova instalação funcionar, localizar diferenças; se forem idênticas, a nova extração não foi variável causal suficiente. |
| Tratamento específico de CUSA/APP_VER | Exemplares de jogo podem ter diferenças de arquivos e patches aplicados. | Há carregamentos infinitos em CUSA01623 e CUSA01715, ambos 01.02, em trechos diferentes. Isso não prova igualdade de causa nem inocenta todos os caminhos por região. | Primeiro reproduzir o mesmo checkpoint com instalação controlada da mesma CUSA. Comparar outra CUSA só depois, com saves compatíveis e diferenças registradas. Não recomendar região universalmente. |
| Bloqueio de sincronização entre cópias, GPU e processamento da área | Leitura completa na #345 seguida de ausência de progresso; há correção recente dessa categoria no código atual. | O log da #345 usa deferred/stale, enquanto a correção conhecida descreve especialmente páginas protegidas em modos mais precisos. Não há pilhas atuais que estabeleçam a cadeia de espera desse checkpoint. | Comparar release e `main` com o mesmo save/configuração. Se persistir, obter duas capturas de todas as threads e os recursos aguardados; localizar a dependência circular ou o produtor que não termina. |
| Processamento de dados inválidos, laço sem progresso ou conclusão Fios não propagada após I/O | HDS690 termina leitura, mas não completa o caminho observado de `fSize/close`. | O processamento do jogo não é identificado por esse marcador; o último log não prova que a CPU está naquele código. | Correlacionar identificador da requisição, tarefa, estado e pilhas. Mesmo endereço de instrução com CPU ativa sugere laço; espera estável exige identificar dono/produtor. Conferir bytes com extração de referência controlada. |
| Estado do save ou caminho de restauração | O hang pode ser reproduzido diretamente de um checkpoint; outras famílias de streaming variam por caminho de entrada. | Não há comparação pareada suficiente para responsabilizar o save nos casos centrais. | Chegar à transição por save anterior e por carregamento direto, em processos novos e na mesma instalação. Se apenas uma entrada falhar, instrumentar a restauração e o estado de streaming correspondente. |
| Pressão de cache ou reutilização de memória durante troca de área | #1047 registra muitas expulsões/writebacks antes da falha. | O log da #345 não apresenta mensagens de pressão. Isso não exclui toda interação com memória, mas não sustenta GC como explicação universal. | Correlacionar endereços/graus de ocupação/eventos com o primeiro bloqueio. Uma alteração de orçamento só é teste causal se as outras variáveis ficarem fixas e o efeito se repetir. |

## Código atual e capacidade de diagnóstico

- A [PR #1005](https://github.com/FelipePrado0/SpartanPort/pull/1005) já foi integrada. O [commit `63d437e`](https://github.com/FelipePrado0/SpartanPort/commit/63d437e373cc6d1a2183e18633fdc4b2aacdd333) faz cópias de upload lerem pela segunda visão da memória para evitar que threads de cópia esperem por uma GPU que espera por elas. É um motivo para retestar antes de escrever outro fix; não é confirmação de resolução das issues deste roadmap.
- A chamada atual pode ser conferida em [`CopySparseSerial`](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/gpu/shim/spartanportgpu.cpp#L110-L126); a implementação de [`runtime_memory_read_backing`](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/src/runtime_memory.c#L1164-L1188) estabiliza mapeamentos durante a cópia e preenche lacunas com zeros. A validade do mapeamento ainda precisa ser observada no cenário real.
- [`SPARTAN_SLOW_WAITS`](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/src/runtime_thread.c#L66-L90) é lido pelo runtime atual. O registro ocorre em `runtime_wait_end`: uma espera que nunca retorna pode não produzir registro. Ausência de mensagens não prova ausência de deadlock.
- A instrumentação de [`read/pread`](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/src/runtime_file.c#L297-L317) mede duração depois do retorno. Não equivale ao rastreamento completo de início/fim da build histórica.
- O [watchdog com captura de threads](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/src/probe.c#L302-L347) existe no caminho Linux. No [caminho Windows](https://github.com/FelipePrado0/SpartanPort/blob/f593377bbaf1934bf606171973d7a58210510991/src/probe.c#L1115), `--timeout` não implementa esse watchdog. Não orientar o relator como se essa capacidade existisse em todas as plataformas.
- `SPARTAN_FILE_TRACE` foi comprovado na build `issue345-test1`; não aparece no `src/runtime_file.c` da baseline atual inspecionada. Uma build nova precisa incluir e verificar explicitamente a instrumentação desejada, sem pressupor que exportar a variável a ativa.

## Fase de preparação: preservar e identificar

**Responsável proposto:** mantenedor, com dados locais do relator. **Dependência:** acesso autorizado ao checkpoint e ao ambiente de reprodução.

- Preservar instalação funcional, saves originais, configurações e logs. Usar cópias dos saves, pois o jogo pode sobrescrever o autosave durante a comparação.
- Registrar pacote/commit do port, SHA-256 dos executáveis, sistema, CPU/GPU/driver, dispositivo de armazenamento, CUSA, `APP_VER`, ferramenta/versão de extração, base e atualização utilizados, mods e patches.
- Registrar configurações efetivas no log: engine FPS, limitador, readback/deferred/stale, resolução, shader cache e variáveis de ambiente. Não confiar só na intenção descrita no formulário.
- Definir cenário principal: #345 após HADESDEATH, sem pular a cena. Definir cenário secundário AMD/Windows: #982 em Helios. Reservar #1047 para verificar Linux e evitar uma conclusão exclusiva de Windows.
- Definir antes do teste a janela de observação a partir de um controle que carrega normalmente e do histórico do relator. Registrar tempo exato, atividade do processo e último progresso; não transformar um timeout arbitrário em prova de deadlock.
- Copiar o log de cada execução antes de abrir o jogo novamente. Identificar cada resultado por caso, artefato, configuração, instalação e save.

**Saída verificável:** ficha de reprodução completa e artefatos preservados. Sem identidade confiável da build ou do save, corrigir a preparação antes de comparar resultados.

## Fase de integridade: testar a instalação sem trocar a pergunta

**Responsável proposto:** relator na máquina afetada; mantenedor compara os manifestos. **Dependência:** preparação completa.

- Inventariar a instalação atual: caminhos relativos, tamanhos e SHA-256 dos arquivos relevantes; ampliar para toda a árvore quando não se conhecer a dependência faltante. Guardar erros do inventário, inclusive acesso negado.
- Reextrair o mesmo PKG base e a atualização compatível, do próprio dump do usuário, em diretório vazio e separado. Aplicar a atualização correspondente ao mesmo title ID; não mesclar arquivos de regiões diferentes nem aplicar mods nessa cópia de controle.
- Registrar os logs e avisos da ferramenta de extração e a forma de aplicar a atualização. Uma execução da ferramenta sem erro visível não certifica todos os arquivos extraídos.
- Comparar manifestos e reproduzir o mesmo checkpoint com a mesma build do port. Se precisar trocar a fonte do dump, registrar isso como outra variável; reextrair a mesma fonte defeituosa pode produzir o mesmo erro novamente.
- Usar hashes como identidade/comparação. Hash igual entre duas extrações prova igualdade dos bytes comparados, não correção do conteúdo de origem. Hash diferente identifica uma divergência, mas arquivos de regiões/updates diferentes podem divergir legitimamente.
- Não solicitar publicação de PKGs ou WADs. Para comparação pública, bastam metadados, hashes, logs da ferramenta e resultado observado, após revisão de dados pessoais.

**Decisões:**

- Nova instalação funciona, antiga falha e há arquivos faltantes/divergentes: tratar o caso como provável problema de conteúdo/extração. Identificar a diferença mínima e repetir a reprodução antes de atribuir a uma CUSA.
- Ambas falham com manifestos iguais: prosseguir para comparação de versões e diagnóstico de threads. A igualdade não inocenta a fonte; reservar uma referência independente controlada se o processamento indicar dados inválidos.
- Ambas funcionam ou a instalação antiga deixa de reproduzir: controlar cache, save e variáveis; não declarar que a reextração corrigiu.
- Apenas trocar região funciona: abrir uma comparação específica de conteúdo/patches por title ID. Não generalizar para os demais relatos.

## Fase de reprodução: comparar release e código atual

**Responsável proposto:** mantenedor prepara artefatos; relator executa o checkpoint. **Dependência:** identidade e integridade mínima registradas.

Evitar matriz cartesiana de todas as opções. Executar os pares abaixo em ordem, avançando conforme o resultado:

| Comparação | Variável alterada | Variáveis preservadas | Resultado e decisão |
| --- | --- | --- | --- |
| Instalação original versus reextraída | Conteúdo instalado | Port, configuração, driver, checkpoint e cópia do save | Resolve a pergunta de extração antes de mudar o renderer. |
| Release 2.7.0 versus build da `main` identificada | Versão do port | Instalação controlada, save, driver e opções comuns | Se só a main funcionar, bissectar mudanças relevantes ou testar commits isolados em branch antes de atribuir o efeito à #1005. |
| Carregar checkpoint próximo versus chegar por save anterior | Caminho de restauração | Mesma build, instalação e configuração | Se divergir, instrumentar estado de streaming/restauração; preservar os dois saves. |
| Readback efetivo original versus modo alternativo | Um modo de leitura de GPU por vez | Mesmo artefato e checkpoint | Só executar se houver hipótese de sincronização. Conferir o modo no log e repetir; não usar modo alternativo como prova de correção geral. |
| Engine original versus 60 FPS | Engine FPS | Mesma build e restante da configuração | Diagnóstico secundário de timing; distinguir engine e limitador. Se apenas reduzir FPS ajudar, investigar a condição de corrida em vez de encerrar o caso como resolvido. |

Para runs frias e aquecidas de shader cache, usar cópias controladas ou diretórios separados e registrar a condição. Não limpar cache entre runs de forma não documentada. A instrumentação detalhada pode alterar timing e não deve ser usada para comparar desempenho de produção.

**Gate de saída:** pelo menos um cenário de falha repetível e seu controle, ou confirmação de que o caso deixou de reproduzir na base atual sob condições documentadas. Um único sucesso não estabelece resolução de falha intermitente.

## Fase de diagnóstico: localizar a primeira tarefa que não progride

**Responsável proposto:** mantenedor. **Dependência:** cenário reproduzível atual ou colaborador disponível para executar build de diagnóstico.

- Antes de alterar instrumentação ou correção, verificar os gates SDD do projeto: mais de um arquivo, módulo novo ou mudança de contrato público exigem spec curta aprovada, com entradas/saídas, limites, critérios de aceite e validação. Este documento é um roadmap, não autorização automática para implementar.
- Capturar duas amostras de pilhas de todas as threads durante o bloqueio, com intervalo registrado. Em Windows, usar ferramenta de depuração local adequada ou preparar capacidade própria após aprovação da spec; não presumir que o watchdog Linux funciona ali.
- Identificar produtor e consumidor da requisição do WAD: abertura, intervalo de leitura, tarefa de processamento, conclusão e recurso aguardado. Incluir identificador estável de tarefa/requisição e relógio monotônico, com limite de volume do log.
- Para espera de GPU/cópia, correlacionar thread, lock/fence/tick, dono e estado do trabalho pendente. Duas pilhas iguais são pista, não prova suficiente sem a relação de dependência.
- Para laço ativo, correlacionar amostras de instrução e contadores de progresso. Verificar bounds, tamanho esperado e consumo do fluxo antes de escolher uma correção no tratamento de dados.
- Para possível reutilização de memória, registrar endereço e geração do mapeamento antes/depois da troca de área. Não inferir corrupção apenas de uma mensagem genérica de extensão Vulkan indisponível.
- Reutilizar a instrumentação histórica de WAD quando útil, adaptando-a à base atual e verificando que o pacote contém as strings e o log de ativação. Proteger custo, integridade dos dados e preservação de `errno`.

**Encaminhamento por resultado:**

- Leitura incompleta/erro: tratar I/O, propagação do erro e conteúdo instalado; criar regressão de leitura parcial/EOF/erro antes do fix.
- Leitura completa e thread esperando conclusão inexistente: corrigir o produtor ou a ordem de sinalização, com regressão do ciclo de dependência.
- Leitura completa e processamento ativo sem progresso: confirmar qual dado dispara o laço e validar limites/erro; não forçar sucesso nem ignorar o arquivo.
- Área aguarda memória antiga nunca liberada: identificar referência/dono/fence que mantém a região viva e provar a ordem correta antes de alterar alocação.
- A instrumentação muda a reprodução: registrar efeito de timing e coletar amostras menos intrusivas; não tratar ausência do hang como resolução.

## Fase de correção e testes de regressão

**Responsável proposto:** desenvolvedor em branch/worktree isolada. **Dependência:** hipótese reduzida a um mecanismo verificável, spec aprovada quando aplicável e reprodução antes do fix.

- Escrever primeiro o teste que falha sem a correção quando houver branch/condicional não trivial, regra de negócio ou segurança; manter o teste focado no mecanismo identificado.
- Para I/O: simular leitura completa, parcial, EOF e erro; conferir bytes, offset e código propagado. Invariante: uma leitura incompleta não pode ser reportada como carga concluída.
- Para sincronização: reproduzir a dependência entre tarefa de cópia, espera do consumidor e sinalização; usar timeout finito no teste. Demonstrar que o teste falha ou expira na base defeituosa e termina na candidata sem descartar trabalho.
- Para memória: testar leitura de regiões protegidas, divisão/remapeamento e dados conhecidos após troca de área. Reutilizar testes existentes da #1005 antes de criar cobertura duplicada.
- Para parser/estado de conclusão: usar fixtures mínimas não derivadas de arquivos distribuíveis do jogo; entradas truncadas ou tamanhos inválidos devem terminar com erro diagnosticável, sem laço infinito nem acesso fora do buffer.
- Rodar suíte e build oficiais relevantes, incluindo Windows e Linux se o caminho compartilhado mudar. Conferir sanitizadores quando o mecanismo for uso após liberação, bounds ou concorrência e houver suporte na configuração do projeto.
- Não remover asserts, aumentar timeouts, forçar fences concluídas ou pular processamento para fazer o teste passar.

## Critérios de aceite e validação no jogo

Os requisitos abaixo são metas futuras verificáveis, não resultados já obtidos:

- Quando o checkpoint original da issue for executado com arquivos íntegros e configuração identificada, a candidata deve concluir a transição e devolver controle em condições comparáveis ao controle funcional.
- Enquanto houver trabalho legítimo de leitura/processamento, o diagnóstico deve permitir identificar a tarefa ativa e seu progresso; a ausência de log não pode ser a única medida de progresso.
- Se uma leitura ou conteúdo necessário falhar, o caminho corrigido deve preservar o erro e produzir diagnóstico útil, sem anunciar conclusão fictícia nem permanecer em espera impossível.
- O runtime deve sempre preservar a ordem e a integridade dos dados exigidas pelo mecanismo corrigido; a eliminação do hang não pode vir de descartar trabalho pendente.
- A regressão automatizada deve falhar na base sem o fix e passar na candidata. A build deve corresponder ao commit declarado e passar os checks relevantes.
- No jogo, repetir a transição original e uma transição próxima já funcional; verificar controle, colisão, áudio, dados salvos e avanço da campanha após a carga. Separar resultados por issue, GPU, sistema, CUSA e APP_VER.
- Definir repetições pela frequência da falha observada na baseline. Para bloqueio historicamente determinístico, repetir a mesma sequência em processos novos; para falha intermitente, observar além da exposição típica e registrar todas as tentativas, sem esconder falhas.
- Confirmar pelo menos o cenário principal e o caso secundário antes de afirmar que a família foi resolvida. Um caso Windows validado não comprova o caso Linux, nem Helios comprova Hades.

## Entrega de diagnóstico ou candidata

- Se a causa permanecer incerta, preparar pacote identificado como **diagnóstico**; se houver regressão e correção sustentada, identificar como **correção candidata**.
- Incluir commit, plataforma, checksum, hipótese, roteiro do checkpoint, opções efetivas e arquivos de diagnóstico necessários. Não incluir arquivos do jogo nem saves pessoais sem necessidade e autorização.
- Fazer o relator comparar baseline e candidata sob a mesma instalação e checkpoint, com cópias dos saves. Confirmar o resultado e coletar o log completo de cada execução antes de reiniciar.
- Publicar prerelease, comentar em issue, criar PR, fazer merge ou encerrar relato somente quando essas ações estiverem explicitamente autorizadas. Antes de merge, conferir o head exato, checks, conflitos e regras efetivas.
- Encerrar apenas o escopo confirmado pelo teste. Se #701 for resolvida por extração e #345 continuar bloqueada, registrar as duas conclusões separadamente.

## Pendências e condições para parar uma hipótese

- Falta uma referência independente de integridade dos WADs da #345, embora tamanho/hash tenham sido informados pelo usuário.
- Falta manifesto antes/depois e CUSA do autor que resolveu a #701 por reextração. O comentário é forte para orientar o teste, mas não demonstra quais arquivos faltaram.
- Falta reprodução pareada na `main` atual, depois das correções de memória integradas.
- Faltam pilhas durante o hang e correlação com a tarefa de processamento/conclusão de HDS690.
- A #982 precisa de novo log para os modos de readback que o autor afirma ter desligado; o anexo atual registra o contrário.
- A #990 precisa reconciliar o trecho colado no formulário com o anexo, que não registra a mesma sequência. Não usar esse anexo isoladamente para concluir qual atualização reproduz o checkpoint.
- Não unificar #636/Cav600 e #1047/Cav500 só por serem cavernas. Não atribuir automaticamente o crash do comentário à causa do carregamento infinito.
- Se não houver reprodução, registrar exatamente as condições testadas e manter o caso como não reproduzido; não inventar correção.
- Se a instalação controlada resolver e as diferenças de conteúdo forem demonstradas, parar a hipótese de runtime para aquele caso até surgir evidência contrária.
- Se houver erro de extração, falta de espaço ou erro de acesso durante a preparação, corrigir a preparação antes de comparar versões do port.
- Não há prazo estimado de resolução: a próxima decisão depende da obtenção de uma reprodução identificada e de evidência que diferencie conteúdo de sincronização.

## Primeiro experimento e decisão de continuidade

- **Caso principal:** #345, porque já há um checkpoint e uma leitura integral documentados. Para verificar especificamente extração, reservar também o caso de Helios com identidade completa; não reutilizar a resolução do autor da #701 como resultado da #982.
- **Entradas necessárias:** cópias dos saves originais, instalação preservada, manifesto dos arquivos e pacotes identificados. O estado atual do plano é “preparação pendente”: nenhum checkpoint foi executado nesta entrega.
- **Par de conteúdo:** mesma build, save e configuração, comparando instalação original e nova extração da mesma origem/base/update. Se a nova funcionar, comparar diferenças e repetir antes de atribuir o resultado à extração. Sem diferenças nos bytes, investigar configuração, caminho e estado inicial do experimento.
- **Par de runtime:** com a instalação fixada, comparar 2.7.0 e a main identificada. Se só a main funcionar, reduzir a diferença entre commits; não anunciar que a #1005 resolveu o checkpoint por associação.
- **Saída mínima:** ficha por execução com tempo até o último progresso, conclusão ou bloqueio, log completo, manifesto, identidade do pacote e configuração. Se ambas as builds falharem na instalação controlada, o próximo trabalho é captura de threads e correlação pós-leitura, não outro patch genérico de I/O.
- **Dependências comuns:** [protocolo, referências e revisão coordenada](2026-10-29-roadmap-investigacao-issues.md).
