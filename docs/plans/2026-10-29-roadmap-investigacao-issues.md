# Roadmaps de correção: índice e protocolo comum

Data da consulta: 29/10/2026. Repositório: `FelipePrado0/SpartanPort`.

Este documento é o índice das quatro frentes e o protocolo comum das três investigações. Os roadmaps descrevem trabalho futuro; não representam correções implementadas nem aprovação de uma release.


## Progresso (29/10/2026)

| Frente | Branch | Estado |
|---|---|---|
| Alto FPS | `feat/high-fps-f2-qte`, `docs/high-fps-catalog`, `feat/high-fps-f40-rate-readers` | **em andamento** (prioridade única) |
| Corrupção de texturas | `diag/texture-corruption` | pausada; correção de autoridade de alias feita, trace `Tex:` incompleto |
| Cenários sem colisão | `diag/missing-collision` | pausada; rodada 1 concluída (correção de geração de mapeamento e trace `Col:`) |
| Carregamento infinito | `diag/infinite-loading` | pausada; rodada 1 concluída (manifesto, trace `Load:` e watchdog) |

Os detalhes do que foi feito e do que falta estão na seção "Progresso" de cada roadmap.

## Documentos por problema

| Frente | Roadmap | Resultado esperado |
|---|---|---|
| Alto FPS | [Roadmap de Alto FPS](2026-10-29-high-fps-roadmap.md) | Jogo jogável do começo ao fim de 100 a 240 FPS: correção de causa por mecânica, com queda automática para 60 enquanto a causa não for corrigida |
| Carregamento infinito | [Roadmap de carregamento](2026-10-29-roadmap-carregamento-infinito.md) | Distinguir conteúdo incompleto de bloqueio após leitura e identificar a tarefa que impede a conclusão |
| Cenários sem colisão | [Roadmap de colisão](2026-10-29-roadmap-cenarios-sem-colisao.md) | Distinguir área não carregada, dados invalidados e interação de movimento; validar geometria e física separadamente |
| Corrupção progressiva de texturas | [Roadmap de texturas](2026-10-29-roadmap-corrupcao-texturas.md) | Identificar a primeira perda de integridade durante uso, descarte, readback ou reutilização de imagens |

### Fronteiras entre as frentes

- Travessia lateral em paredes e salientes (#535, #1011) e a parede invisível nas Correntes do Equilíbrio (#839) pertencem ao catálogo de Alto FPS (F1) enquanto não houver evidência de área sem geometria. Se o sintoma persistir a 60 FPS, voltam ao roadmap de colisão.
- O par "engine original versus 60 FPS" das matrizes de colisão e carregamento também alimenta a confirmação da F1 do Alto FPS. Uma mesma rodada de testes do mantenedor pode servir às duas frentes.
- O protocolo comum abaixo vale para as três investigações. O Alto FPS segue o próprio fluxo de entrega (§6 do seu roadmap), mas usa as mesmas regras de identificação de build, save e log.

## Referências de versão

| Referência | Identidade conferida nesta consulta | Uso |
|---|---|---|
| `main` remota | [`f593377bbaf1934bf606171973d7a58210510991`](https://github.com/FelipePrado0/SpartanPort/commit/f593377bbaf1934bf606171973d7a58210510991) | Base de leitura do código e candidata à primeira reprodução |
| Última release estável retornada pela API | [`windows-v2.7.0`](https://github.com/FelipePrado0/SpartanPort/releases/tag/windows-v2.7.0), referência `003e42b6e004ed686aae58c053d2301fe9c8c68e` | Comparação com os relatos da versão publicada |
| Mudanças de memória integradas | [PR #1005](https://github.com/FelipePrado0/SpartanPort/pull/1005), integrada pela [PR #1111](https://github.com/FelipePrado0/SpartanPort/pull/1111) | Verificar antes de desenvolver outra correção para um mecanismo já alterado |
| Correção de cópias por memória protegida | [`63d437e373cc6d1a2183e18633fdc4b2aacdd333`](https://github.com/FelipePrado0/SpartanPort/commit/63d437e373cc6d1a2183e18633fdc4b2aacdd333) | Corrige um impasse conhecido; não confirma resolução dos checkpoints destas issues |
| Otimização ultrawide em análise | [PR #1104](https://github.com/FelipePrado0/SpartanPort/pull/1104), aberta, head `d41dc689914cb7b1568dda9374650f34858e82a8` | Avaliação separada: o autor declara que a corrupção progressiva não foi resolvida por essa PR |

Estas identidades são um retrato da consulta. Antes de executar a matriz, conferir novamente os SHAs e registrar a identidade do pacote efetivamente usado. O nome de uma tag, de uma pasta ou de um executável não substitui essa identificação.

## Evidências que mudam a decisão

### Extração incompleta é uma hipótese sustentada para parte dos relatos

Na [#701, comentário do autor `exmof`](https://github.com/FelipePrado0/SpartanPort/issues/701#issuecomment-6392035040), o autor informou que resolveu o carregamento ao obter e extrair novamente o jogo e relatou arquivos ausentes na primeira extração. Não há inventário ou hashes antes/depois; a resolução é uma confirmação do usuário, não uma auditoria independente da extração.

O [comentário sobre `Bas400`](https://github.com/FelipePrado0/SpartanPort/issues/701#issuecomment-6256911950) é de outro usuário, `jacopolucchini35-boop`. A resolução do autor não confirma a resolução desse participante nem da [#982](https://github.com/FelipePrado0/SpartanPort/issues/982).

Na [#899](https://github.com/FelipePrado0/SpartanPort/issues/899#issuecomment-6496549116), outra instalação permitiu passar pelo cenário defeituoso. Como também mudou a região e não houve comparação dos arquivos, isso não estabelece qual variável resolveu o caso.

**Consequência para o planejamento:** checar integridade e montagem base/atualização é uma etapa inicial dos roadmaps de carregamento e colisão, não um último recurso. A avaliação anterior da conversa foi incompleta ao não destacar a confirmação existente na #701.

### Leitura completa não demonstra conteúdo íntegro

O [log de diagnóstico da #345](https://github.com/user-attachments/files/33722598/last_run.log) contém 188 leituras consecutivas do `HDS690.WAD`, cobrindo 98.197.264 bytes, com retorno integral e `errno=0`. Isso descarta uma leitura curta nessa execução; não prova a correção dos bytes nem identifica o estágio interno que não terminou. O [relator descreve o mesmo resultado](https://github.com/FelipePrado0/SpartanPort/issues/345#issuecomment-6292975169).

- Hash idêntico do `eboot.bin` não valida os WADs ou demais arquivos do jogo.
- Hashes iguais entre duas extrações do mesmo conteúdo incorreto não certificam uma instalação correta.
- Comparar regiões diferentes mistura conteúdo, metadados e possíveis tratamentos específicos do port. A primeira comparação deve manter CUSA, atualização, save e configuração.
- Nem o marcador `fSize` nem a ausência de erros Vulkan provam que uma área está utilizável ou que sua colisão foi registrada.

### Os três grupos não têm causa comum demonstrada

Há pressão de cache nos logs de colisão da #871 e de carregamento da #1047. Porém, o log da #345 não contém o mesmo marcador de pressão, e o log da #1000 também não. A ausência desse marcador não prova ausência de atividade no cache; impede apenas afirmar que essa correlação já foi observada em todos os casos.

Se um mesmo defeito de memória vier a explicar mais de uma frente, exigir uma reprodução controlada em cada uma antes de consolidar as issues. Compartilhar infraestrutura de diagnóstico não significa compartilhar conclusão causal.

## Protocolo comum de comparação

### Dossiê mínimo por execução

- Identificador da issue e identificador local da execução.
- Data, sistema, CPU, GPU selecionada, driver, RAM e memória disponível por heap quando aplicável.
- Tag e SHA do port, origem do pacote, checksum do pacote e parâmetros de execução.
- CUSA e atualização declarados pelo jogo, hash do executável e manifesto dos arquivos pertinentes.
- Origem e versão do jogo base e da atualização compatível; ferramenta de extração, versão e avisos emitidos. Não publicar chaves, arquivos do jogo nem caminhos pessoais completos.
- Save usado, tipo manual/automático, cópia preservada e ponto exato de entrada no trecho.
- Resolução, engine FPS, limite de FPS, VSync, patches e configurações relevantes efetivas, conferidas no log.
- Estado inicial dos caches. Restaurar cópias equivalentes para comparações; não apagar evidências nem fazer uma execução partir de um cache limpo e a outra de um cache alterado sem registrar isso como variável.
- Log completo preservado antes da próxima inicialização, vídeo quando necessário e pilhas das threads se houver travamento.
- Resultado observado, duração, repetição, desvios do protocolo e arquivos de evidência gerados.

### Primeira comparação

- Preservar a instalação e os saves existentes. Reextração deve usar outra pasta, sem mesclar resíduos de instalações anteriores.
- Fixar o caso reproduzível e registrar o comportamento na versão original do relato.
- Comparar a mesma instalação na `main` identificada, com configurações equivalentes. Se o formato do save ou cache não for compatível, usar cópias separadas e registrar a limitação, sem sobrescrever o original.
- Na comparação de extração, manter a versão do port fixa. Na comparação do runtime, manter os arquivos do jogo fixos.
- Não executar um produto cartesiano de todas as opções. Começar pelo menor conjunto que distingue as hipóteses; expandir quando o resultado justificar.

### Como interpretar o resultado

| Observação | Conclusão permitida | Próxima ação |
|---|---|---|
| Mesma versão do port e mesmo trecho funcionam após reextração | O conjunto de conteúdo mudou de maneira relevante, se as demais variáveis foram preservadas | Comparar manifestos, compatibilidade base/update e primeiro arquivo divergente |
| Arquivos idênticos funcionam na `main`, mas falham na release | Há evidência de diferença de comportamento entre builds | Identificar qual alteração remove o mecanismo e repetir a comparação |
| Orçamento maior apenas adia o primeiro artefato | Há evidência de influência da pressão/cache; não de correção da integridade | Forçar a operação suspeita em teste controlado e observar o primeiro byte divergente |
| Área é desenhada, mas não sustenta Kratos | A validação visual passou; a validação física falhou | Investigar registro/estado/dados de colisão; não encerrar por screenshot |
| Sintético passa, checkpoint real continua falhando | O teste cobriu uma condição insuficiente ou outro mecanismo | Refinar a hipótese e o teste; manter a issue pendente |
| Caso não é reproduzido na base | Não é possível atribuir o sucesso da candidata à mudança | Melhorar o protocolo ou registrar o caso como inconclusivo |

## Ordem de execução sugerida

- **Alto FPS, em paralelo:** F1 (catálogo) e F2 (queda para 60 durante QTEs) não dependem das investigações; no máximo uma worktree de código para esta frente e outra para a investigação em curso.
- **Preparação compartilhada:** consolidar dossiês, fixar versões, preservar saves, identificar instalações e casos reproduzíveis.
- **Carregamento e colisão:** fazer a comparação de conteúdo/extração e de release/main antes de desenvolver novos caminhos de carga.
- **Texturas, em paralelo:** montar uma reprodução cumulativa e medir cache/descarte/writeback. Esse trabalho não precisa aguardar a conclusão dos outros grupos.
- **Diagnóstico dirigido:** capturar o primeiro estado incorreto ou a dependência bloqueada, usando ferramentas existentes quando suficientes.
- **Correção por mecanismo:** escrever spec e teste de regressão após a hipótese estar suficientemente delimitada. Um teste deve falhar na base e passar com a correção.
- **Confirmação:** comparar base e candidata no jogo, no trecho original e nas condições que produziam a falha; registrar resultados negativos também.

## Critérios da revisão técnica

- Toda conclusão relevante deve apontar a issue, comentário, log ou código que a sustenta.
- Distinguir dado observado no log, relato do usuário, interpretação técnica e proposta ainda não executada.
- Conferir contradições entre formulário e log; não escolher silenciosamente o dado mais favorável à hipótese.
- Cada fase deve produzir um artefato verificável e indicar a decisão permitida por seu resultado.
- Cada hipótese principal deve ter um teste capaz de enfraquecê-la, e não apenas uma tentativa de contorno.
- Não transformar comparação entre regiões em prova de extração correta ou de defeito exclusivo de CUSA.
- Não apresentar como inédita uma correção que já existe na `main`.
- Não confundir corrupção progressiva com as antigas faixas estáticas de profundidade/MSAA ou com pop-in dependente da câmera.
- Prever limites de memória e parar em OOM, falha de driver ou risco de sobrescrever saves/evidências. Não remover asserts para prolongar o teste.
- Exigir validação física para colisão, progressão real para carregamento e integridade mais duração de sessão para texturas.

## Entrega, publicação e encerramento futuros

- Sem causa comprovada, qualquer pacote preparado deve ser chamado de diagnóstico.
- Com correção sustentada e testes aprovados, preparar candidata vinculada ao SHA exato, com checksum e roteiro de comparação.
- Publicação, comentários externos e fechamento de issues dependem de autorização específica. A redação deste roadmap não os autoriza.
- Encerrar apenas o escopo confirmado pelo relator ou por reprodução suficiente. Um caso resolvido por reextração não encerra os demais relatos de carregamento; uma mitigação de cache não encerra corrupção de dados.
- No rollback, retornar ao pacote e às cópias de configuração/save previamente preservadas. Não reutilizar automaticamente um save alterado pela candidata em uma versão anterior.
