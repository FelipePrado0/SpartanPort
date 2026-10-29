# Ficha de reprodução: corrupção progressiva de texturas

Uma ficha por execução. Não anexe arquivos do jogo, saves nem caminhos pessoais completos.

## Identificação
- Issue / ID local da execução:
- Data e hora de início:
- Port: tag ______ SHA ______ origem do pacote ______ checksum ______
- Jogo: CUSA ______ `APP_VER` ______ hash do executável ______
- Instalação: extraída com ______ (versão ______); avisos: ______

## Máquina
- SO e versão:
- CPU / RAM:
- GPU / driver (versão exata):
- VRAM dedicada / compartilhada:
- Monitor (resolução, Hz):

## Configuração efetiva (conferir no `last_run.log`)
- Resolução interna / patch de resolução:
- Engine FPS / limite de apresentação / VSync:
- Upscaler:
- Fast Screen Effects (ligado/desligado):
- Readback (`SPARTAN_DEFERRED_READBACK`, `SPARTAN_STALE_READBACK`) efetivo:
- `SPARTAN_GC_BUDGET_MB` (vazio = padrão):
- `SPARTAN_TEX_TRACE` (1 = ligado):
- Outros patches/opções alterados:
- Estado do cache de shaders/pipelines no início (cópia preservada? limpo?):

## Rota
- Save usado (manual/automático), cópia preservada em:
- Ponto de entrada:
- Trechos percorridos, em ordem (transições, cinematics, menu de melhorias, Fúria de Esparta, troca de arma):

## Resultado
- Primeiro artefato: hora ______ (minutos desde o início ______), cena ______
- Descrição (superfície, arma, cor, padrão):
- Piora com o tempo? Some ao sair ao menu e recarregar? Some ao reiniciar o port?
- Kratos continua colidindo normalmente no trecho?
- Captura/vídeo:
- Sem artefato: duração total ______, número de transições ______
- Última linha `Tex: summary` antes do artefato (copiar):
- Linhas `Tex: evict`/`download`/`alias-forget` nos 10 s antes do artefato (copiar):
- Linhas `Texture cache: memory pressure` (quantas, primeira hora):
- Crash, OOM, perda de dispositivo, travamento de carga:
- `last_run.log` preservado em (antes de relançar):

## Desvios do protocolo
-
