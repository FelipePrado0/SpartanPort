# Catálogo dos bugs de FPS alto

Fase F1 do roadmap "Alto FPS jogável do começo ao fim" (`docs/plans/2026-10-29-high-fps-roadmap.md`,
branch `docs/high-fps-roadmap`). Busca feita em 2026-10-29 em todas as issues de
`FelipePrado0/SpartanPort` (846 issues, PRs ignoradas), filtradas pelos termos do roadmap e lidas
com os comentários. **Nada aqui foi confirmado no jogo pelo mantenedor ainda**: a coluna "Some a 60"
traz só o que os relatos dizem. O roteiro de confirmação está no fim.

**Legenda.** Some a 60: *sim (relato)* quando alguém relatou que passou ao baixar para 60; *não
(relato)* quando acontece também a 60; *não testado* quando ninguém disse. Estado: *aberto*,
*coberto pela F2* (QTE) ou *coberto pela F3* (área), *corrigido pela F4*, *fora* (não é de FPS).

## Resumo por mecânica (tabela do mantenedor, conferida contra as issues)

| Mecânica | Pessoas | O que acontece | Evidências | Conferência (2026-10-29) |
|:--|:--|:--|:--|:--|
| Travessia lateral em salientes e paredes, incluindo Hades após obter o arco | 7 | Kratos recua ou não atravessa; passa ao reduzir de 120/240 para 60; alguns usam salto junto à parede | #114, #313, #640, #535 | confere: 7 pessoas relatam que 60 resolve ou ajuda (#114: 2; #313: 1; #640: 2; #535: 2 com 60 + salto); outras 2 passaram sem baixar, só com X. Linhas A1 a A3 |
| Levantar o tronco no prólogo com Gaia | 1 | Mash de Círculo não levanta a 120; passou a 60 | #44 | confere; a #589 (árvore/tronco) é provavelmente o mesmo trecho, sem taxa informada. Linha B1 |
| QTE de levantar objetos, local não especificado | 1 | Acima de 60 o mash de Círculo falha; relato histórico ligado à aceleração | comentário na #12 | confere: comentário de marceloaugustofbr; o mantenedor ligou à aceleração corrigida na 2.1.4. Linha B2 |
| Empurrar objeto na área de Peirithous, Reino de Hades | 1 | Empurrar trava a 240; 60 resolve; chutar falha também a 60 (Auto QTE) | #1085 | confere; dois comentários dizem que desligar o Auto QTE resolve o chute. Linha B3 |
| Girar a balista na sala com portais azuis | 1 | Dispara mas não gira; passou de 120 para 60 e reiniciando o checkpoint | **#812** | achada: ricksanchez6 (120 → 60 + reinício); ShigeBOI passou só reiniciando o checkpoint. Linha B4 |

#839, #535 e #1011 ficam no catálogo enquanto não houver prova de uma área sem geometria.

## Grupo A: travessia lateral e pulo (mecânica suspeita: movimento ao longo de saliente/parede)

| ID | Issues | Área e local | Sintoma | Taxa / some a 60 | Mecânica suspeita | Save | Pessoas | Evidências | Estado |
|:--|:--|:--|:--|:--|:--|:--|:--|:--|:--|
| A1 | #114, #313 | Reino de Hades, saliente na parede logo no começo (prints na #114 e na #313) | Kratos recua ao andar de lado na saliente e não atravessa | 120 e 240; sim (relato, 3 pessoas) | velocidade/colisão do "andar na saliente" integrada por quadro | não | 4 (#114: mjhcsta, JunioJsv; #313: autor, ItsYusei99) + 2 que passaram sem baixar com X/salto (agustinb-dev, Dilshad716) | #114 "after i dropped the FPS back to 60FPS i managed to get pass"; #313 "this happens only at a higher framerate, not at 60fps" | aberto |
| A2 | #640 | Reino de Hades, logo depois de obter o arco (Bow of Apollo): passagem estreita onde se atira uma flecha | A ação de se espremer pela passagem é interrompida | não informada (alto); sim (relato) | a mesma de A1 (travessia lenta em espaço estreito) | não (vídeo no Drive) | 2 (Klonerr, springmercy) | "you need to limit to 60fps for this part, known issue"; autor: "I was able to get past this point after the 60 fps limitation" | aberto |
| A3 | #535 | Parede sobre o abismo (Kratos agarrado à parede), local exato não dito; vídeo na issue | Ao mover de lado na parede, Kratos vai para a plataforma em vez de seguir pela parede | não informada; **parcial**: 60 sozinho não bastou para um; 60 + X (salto) resolveu para dois | a mesma de A1, mais o salto na parede | não | 4 (autor + Mouadabdelgh, danielgalvan, centralizzes) | "switching to 60 FPS and pressing X at a specific time did the trick"; "60fps plus jump worked"; segurar X passa também na 2.6.0 | aberto |
| A4 | #1011 | Reino de Hades, primeira parte ("begining of the game hades parte") | Kratos não anda nas beiradas da parede | não informada; não testado | a mesma de A1 | não | 2 (autor, Jhonbarros21) | comentário: "esse bug tem no jogo original (desde o PS3)... tem um ângulo certo no analógico" | aberto (pode ser do jogo: confirmar a 60) |
| A5 | #839 | Chains of Balance, início: balançar em duas correntes até o outro lado | Depois da segunda corrente uma "parede invisível" bate em Kratos e ele cai | não informada; não testado | balanço/impulso na corrente por quadro, ou colisão no fim do passo | não | 1 | só o relato | aberto (sem prova de área sem geometria) |
| A6 | #620, #1001 | Reino de Hades, um vão específico e o vão seguinte | O segundo pulo no ar falha e Kratos cai (passa com pulo duplo infinito) | **relatado a 60** ("60 FPS render and physics"); não (relato) | pulo duplo | não (log anexado) | 1 (mesmo autor nas duas) | #1001: "I've managed to bypass this issue by enabling the infinite double jump hack" | aberto; provável **fora** (acontece a 60): confirmar |
| A7 | #965 | Luta com Poseidon, 2.ª fase, descida pelas vinhas da Gaia logo após o golpe do tridente | Desce rápido demais ou é lançado alto demais | não informada; não testado | impulso de pulo/queda por quadro | não (2 vídeos) | 1 | só o relato | aberto |

## Grupo B: QTE e mash (mecânica suspeita: barra do mash e prompts por quadro; a F2 cobre)

| ID | Issues | Área e local | Sintoma | Taxa / some a 60 | Mecânica suspeita | Save | Pessoas | Evidências | Estado |
|:--|:--|:--|:--|:--|:--|:--|:--|:--|:--|
| B1 | #44, #589 | Prólogo, Gaia: levantar o tronco depois que a Gaia apaga as chamas (R1 + mash de Círculo) | O mash de Círculo não levanta o tronco | 120; sim (relato) | dreno do mash por quadro (`SCR_RepeatPress`, ver #1092) | não | 2 (#44 autor; #589 autor, sem taxa informada) | #44: "I left it on the 120... It's working now" (a 60); #589: resposta do mantenedor aponta o mesmo caso | coberto pela F2 (a validar) |
| B2 | comentário na #12 | Não especificado: "animações de levantar algum objeto que demande ficar apertando o O" | Acima de 60 o mash de Círculo falha | acima de 60; histórico: na época o patch do motor não aplicava e o jogo acelerava (corrigido na 2.1.4) | mash, ou a aceleração antiga | não | 1 (marceloaugustofbr) | resposta do mantenedor na #12 liga os QTEs falhando à aceleração | provável **fora** (causa antiga); manter só como referência |
| B3 | #1085 | Reino de Hades, sala do Peirithous (oferece o arco), objeto ao lado do braseiro, tutorial "Kicking objects" | **Empurrar** trava Kratos a 240; a 60 empurra. **Chutar** trava também a 60 | 240; empurrar sim (relato); chutar não | empurrar: QTE/ação por quadro; chutar: Automatic QTE (dois comentários: desligar o Auto QTE resolve) | não | 1 (+2 sobre o chute) | "Pushing the same object also froze Kratos at 240 FPS unlimited. Locking to 60 FPS fixed pushing." | empurrar: aberto (F2 se for prompt, senão F3); chutar: **fora** (Auto QTE) |
| B4 | #812 | Sala com portais azuis: girar a balista para atirar nos outros portais | A balista dispara mas não gira | 120; parcial: um passou com 60 **e** reinício do checkpoint; outro passou só reiniciando o checkpoint | giro da balista lido por quadro, ou estado preso até o checkpoint | não (log anexado) | 3 (autor carrilhorod, ShigeBOI, ricksanchez6) | ricksanchez6: "I was running 120 fps, encountered the bug, dropped to 60 fps, restarted checkpoint, and it worked"; ShigeBOI: "restarting checkpoint fixed that bug" | aberto (confirmar se 60 é necessário) |
| B5 | #1092 (PR) | Todos os prompts de mash | A barra drena mais rápido em taxa alta | 100 a 240; teste do stub | dreno do mash (`SCR_RepeatPress`) | — | — | tabela de casos no PR | decisão F2.6 |

## Grupo C: áudio (mecânica suspeita: temporização de áudio por quadro)

| ID | Issues | Área e local | Sintoma | Taxa / some a 60 | Mecânica suspeita | Save | Pessoas | Evidências | Estado |
|:--|:--|:--|:--|:--|:--|:--|:--|:--|:--|
| C1 | #885 | Coração da Gaia, ao empurrar o pilar de pedra para trás | Som do pilar falta | acima de 60; sim (relato, vídeo) | disparo de som ligado a contagem de quadros | não | 1 | "when I activate the 60 fps mode, it starts sounding as it should" | aberto |
| C2 | #652, #931, #1002 (acompanhada na #897) | Forja do Hefesto, conversa ao voltar da câmara de Afrodite | Fala do Hefesto duplicada | relatada a 120; **também a 60** (juanme555 na #652) | — | não | 6+ | "the audio is also bugged on 60fps, its not the framerate" | **fora** (acontece a 60) |
| C3 | #988 | Perseguição a Hermes (escadaria alta) | Passos, falas e efeitos atrasados | ocorre a 60 e acima; some a 30 (relato) | sincronia de áudio com o limite de quadros, não com a taxa do motor | não | 1 | "Ao limitar o jogo a 30 FPS, o áudio volta a ficar sincronizado" | **fora** deste catálogo (acontece a 60); acompanhar à parte |

## Grupo D: progressão e lógica (a classificar)

| ID | Issues | Área e local | Sintoma | Taxa / some a 60 | Mecânica suspeita | Save | Pessoas | Evidências | Estado |
|:--|:--|:--|:--|:--|:--|:--|:--|:--|:--|
| D1 | #1024 | Reino de Hades, depois da morte do Hades (cinemática) | Kratos não responde mais; o mundo segue animando | 100 (log); não testado | trava de estado após cinemática; pode ser a carga do `Hds690` (#345) | não (log) | 1 | log: "engine reports a frame rate of 100 FPS" | aberto (confirmar a 60) |
| D2 | #1083, #1027, #1069, #1074, #804 | Jogo todo (#1083); Três Juízes, primeiro teste (#1027/#1069); vários (#1074) | Lógica acelerada **com o motor a 60** | 60 | taxa real e taxa do motor divergem (inverso) | não (logs) | 5+ | #1074 "capped both frame rate limit - 60 and engine frame rate - 60" | **fora** da rede (não é FPS alto); acompanhar à parte |
| D3 | #910 | Gaia, depois da cinemática com Zeus | Morre em loop ao entrar na Gaia | não informada | — | não (log) | 1 | autor: reiniciando do último save funciona | **fora** até haver relato de FPS |
| D4 | #768 | Câmara das Musas (harpa), WADs `Oly150`/`Oly160` | Minigame da harpa não julga nenhum toque | não informada; também na 2.4.1 | contador de tempo do minigame (hipótese) | não | 2 | comentário de ooiac descreve o minigame sem julgamento | aberto (confirmar a 60) |

**Descartados na triagem** (não são de FPS pelos relatos): #110 (QTE de Círculo em loop no Legion
Go, resolvido com drivers novos), #314 (toques duplos com controle a 500/1000 Hz, acontece a 60),
#377 (botão remapeado por engano), #811/#874/#912 (QTE fechava o jogo em CPUs sem BMI1, corrigido
na 2.7.0), #911 (pedido de opção que já existia), #736 (versão antiga, sem dados de FPS).

## Roteiro de confirmação (mantenedor)

Ambiente: Windows, RX 6700 XT, monitor 240 Hz, CUSA01623 v01.02, build atual. Para cada item: motor
a 240, tentar 3 vezes; depois R3 + L1 + R1 (ou F9) até o aviso "60 FPS" e tentar de novo. Anotar
"quebra a 240 / passa a 60" e guardar o `user\last_run.log` e um save perto do local.

| Item | Onde e como chegar | Teste |
|:--|:--|:--|
| A1 | Reino de Hades, a primeira saliente lateral depois de sair do Estige (print da #114). | Andar de lado sem pular a 240; depois a 60. Linha `Engine FPS: 60` no log marca a troca. |
| A2 | Hades, logo depois de pegar o arco do Peirithous: a passagem estreita com o alvo da flecha. | Atravessar a 240 e a 60; ver se a ação para no meio. |
| A3 | A parede sobre o abismo do vídeo da #535 (provavelmente Hades). | Mover de lado a 240, a 60 sem pular e a 60 com X. |
| A4 | Hades, primeira parte, beiradas da parede (#1011). | Mesmo teste de A1; se falhar a 60, é do jogo (fora). |
| A5 | Chains of Balance, primeiro par de correntes. | Balançar e soltar na 2.ª corrente a 240 e a 60. |
| A6 | Hades, o vão do vídeo da #620 e o seguinte. | Pulo duplo a 240 e a 60; se falhar a 60, sai do catálogo. |
| A7 | Poseidon, 2.ª fase, vinhas da Gaia. | Subir e descer pulando a 240 e a 60; comparar a altura. |
| B1 | Prólogo, tronco depois que a Gaia apaga as chamas. | R1 + mash de Círculo a 240 e a 60 (Auto QTE desligado). |
| B3 | Hades, sala do Peirithous, objeto ao lado do braseiro. | Empurrar a 240 e a 60 com Auto QTE desligado; chutar com Auto QTE ligado e desligado. |
| B4 | Sala dos portais azuis com a balista. | Girar a 240; se travar, a 60 sem reiniciar; só então reiniciar o checkpoint. |
| C1 | Coração da Gaia, pilar de pedra. | Empurrar o pilar para trás a 240 e a 60 e ouvir o som. |
| D1 | Depois da morte do Hades. | Ver a cinemática a 240 e a 60 (save antes da luta). |
| D4 | Câmara das Musas, harpa. | Tocar a 240 e a 60 e ver se o minigame julga. |
