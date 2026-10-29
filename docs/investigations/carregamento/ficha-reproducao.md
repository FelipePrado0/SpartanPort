# Ficha de reprodução: carregamento infinito

Uma ficha por execução. Copie este bloco, preencha e guarde junto do `last_run.log` daquela execução
(renomeado, por exemplo `2026-10-30_345_r1_last_run.log`). Não anexe saves nem arquivos do jogo a
issues sem autorização. Protocolo: `docs/plans/2026-10-29-roadmap-carregamento-infinito.md` e
`docs/plans/2026-10-29-roadmap-investigacao-issues.md`.

```text
Execução
  Issue / caso ............: #345 Hades→HDS690 | #982 Helios→BAS400 | #987 Forja→HDS500 | #1047 Cav500 | #990 Pos700? | outro
  Id local da execução ....: <data>_<issue>_r<n>
  Data e hora .............:
  Quem executou ...........: mantenedor | relator (@...)

Port
  Pacote ..................: windows-v2.7.0 | build da branch diag/infinite-loading @ <sha> | outro
  SHA-256 spartanport-probe.exe / SpartanPort.exe:
  Variáveis de ambiente ...: SPARTAN_FILE_TRACE=  SPARTAN_LOAD_WATCHDOG=  SPARTAN_FILE_TRACE_LIMIT=  outras:
  Linhas de ativação no log: [ ] Load: trace on   [ ] Load: watchdog on

Máquina
  Sistema / versão ........:
  CPU / RAM ...............:
  GPU / driver ............:
  Disco do jogo (SSD/HDD) .:

Jogo
  CUSA / APP_VER (do log) .:
  Base + update usados ....:
  Ferramenta de extração / versão / avisos:
  Mods / patches extras ...:
  Manifesto (arquivo) .....: <nome>.json   totals: files=  bytes=  errors=
  Comparação com ..........: <outro manifesto>  resultado: idêntico | missing= extra= different= case-only=

Configuração efetiva (conferida no log, não no launcher)
  Engine FPS / limitador ..:
  Readback / deferred / stale:
  Resolução / VSync .......:
  Shader cache ............: frio | aquecido (cópia de <origem>)

Save
  Cópia usada (nunca o original):
  Tipo ....................: manual | autosave
  Ponto de entrada ........: carregar checkpoint | chegar por save anterior | pular cena? sim/não

Resultado
  Transição ...............: concluiu | travou | crash
  Tempo até o último progresso (último "Load: #... end"):
  Tempo de observação após o último progresso:
  Janela responsiva? FPS? .:
  Load: STALL presente? ...: sim | não  (N do watchdog = )
  Último WAD aberto e read=X/Y:
  Operação em andamento (begin sem end): #id op path
  Threads com cpu_ms subindo entre sample=1 e 2:
  Repetições / quantas falharam:
  Desvios do protocolo ....:
  Arquivos guardados ......: log, manifesto, vídeo, capturas de pilha
```

## Decisão por resultado (resumo do roadmap)

| Observação | Próximo passo |
| --- | --- |
| Nova extração passa, antiga falha, `compare` mostra diferenças | Caso de conteúdo/extração: achar a menor diferença e repetir antes de concluir |
| Ambas falham, manifestos idênticos | Comparar 2.7.0 × branch; depois capturas de threads (`Load: STALL`) |
| `read=Y/Y`, nada em andamento, CPU parada | Espera após I/O: identificar o produtor/fence esperado |
| `read=Y/Y`, CPU subindo na mesma thread | Laço ativo após I/O: conferir dados e limites |
| `begin` sem `end` | I/O do host parado: anotar a thread, o disco e o antivírus |
| Não reproduz | Registrar as condições; caso inconclusivo, sem declarar correção |
