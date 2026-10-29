# Roteiro do mantenedor: cenários ausentes e sem colisão

Build: Windows zip da branch `diag/missing-collision` (de preferência junto com a branch
`diag/infinite-loading`, que traz as linhas `Load:` e `tools/game_manifest.py`). Seu ambiente:
Windows, RX 6700 XT, 240 Hz, CUSA01623 v01.02. Talvez o defeito não reproduza aí: o resultado
"não reproduziu" com log também serve como controle.

## Variáveis de ambiente

| Variável | Valor | Efeito |
|---|---|---|
| `SPARTAN_MEM_TRACE` | `1` | Liga as linhas `Col:` (memória guest). Desligado por padrão |
| `SPARTAN_MEM_TRACE_MIN_KB` | `64` (padrão) | Omite mudanças menores. Use `0` só num trecho curto |
| `SPARTAN_MEM_TRACE_MAX` | `20000` (padrão) | Máximo de linhas `Col:`; depois, `Col: memory trace limit reached` |
| trace de arquivos | da branch `diag/infinite-loading` | Liga as linhas `Load:` (ver o roteiro daquela branch) |

`teste.bat` ao lado do `SpartanPort.exe`:

```bat
@echo off
set SPARTAN_MEM_TRACE=1
rem acrescente aqui a variável das linhas Load: (branch diag/infinite-loading)
start "" "SpartanPort.exe"
```

## Passos (cada item: o que fazer / o que esperar e enviar)

1. **Backup:** copie `user\` (saves e configurações) para fora da pasta do port antes de tudo.
   Nada desta build deveria escrever em saves de forma diferente, mas o backup é obrigatório.
2. **Manifesto da instalação:** rode `tools/game_manifest.py` (branch `diag/infinite-loading`) na pasta do jogo.
   Guarde a saída; envie só caminhos, tamanhos e SHA-256, nunca os arquivos.
3. **Sem trace, controle de FPS:** abra pelo atalho normal, pare num ponto fixo e anote o FPS.
   Repita pelo `teste.bat`; esperado: mesmo FPS. Diferença visível = me avise antes de seguir.
4. **Trecho dos Juízes → Pit of Tartarus (#871/#279):** pelo `teste.bat`, vá do checkpoint anterior até o corredor.
   Envie as linhas `Col:` e `Load:` de ~30 s antes de entrar até a queda (ou até passar).
5. **Físico separado do visual:** no ponto, ande no piso, salte e aterrisse; grave vídeo curto.
   Responda três coisas: a área aparece? o piso sustenta Kratos? chegou ao checkpoint seguinte?
6. **Recarregar o checkpoint novo:** se passou, recarregue o checkpoint criado depois do corredor.
   Esperado: piso presente. Se cair, envie o log dessa carga também.
7. **Mesmo trecho por carga direta:** carregue um save perto do defeito (cópia), sem percorrer antes.
   Compare com o passo 4: a diferença de resultado e as primeiras linhas `Load:`/`Col:` que divergem.
8. **Engine 60 vs original:** repita o passo 4 mudando só o engine FPS (resto igual).
   Mesma rodada serve ao Alto FPS; envie o resultado das duas condições.
9. **Hefesto (#405) e plataforma que some (#1000):** se tiver save, repita os passos 4-5 lá.
   Na #1000, anote a distância em que some e se o piso some junto (físico) ou só a imagem.
10. **Linhas que importam no log:** procure `Col: stale-write`, `Col: map-failed` e, no fim,
    `late GPU writes of unmapped memory dropped=`. Qualquer uma delas: envie o log inteiro.
11. **Reextração (só se o defeito reproduzir):** extraia de novo o mesmo PKG base + mesma
    atualização numa pasta vazia; rode o manifesto; repita o passo 4 trocando só a instalação.
12. **Comparar manifestos:** envie o diff dos dois manifestos (só nomes de arquivo e hashes).
    Igual e ainda falha = extração inconsistente perde força; diferente = indique o primeiro WAD.

## O que não fazer

- Não use save de outra região para "testar"; não troque driver, região e build ao mesmo tempo.
- Não apague caches nem saves originais; não anexe WADs, PKG, `eboot.bin` nem saves em issues.
- Uma screenshot não prova colisão: sempre o passo 5.

## Como ler as linhas `Col:`

`Col: t=<ms> tid=<thread> <op> ...`: `alloc`/`release` (memória física), `map` (geração nova;
`replaced=` e `oldgen=` quando substitui outra), `retire` (GPU esquece a geração antiga antes de um
mapa fixo), `unmap ... src=munmap|batch|release`, `map-failed`, `stale-write` (escrita atrasada de
uma geração retirada, descartada). Para a área ausente, procure o endereço da área nas linhas
`map`/`unmap` próximas da carga do WAD (`Load:`) e veja se ele é desmapeado ou substituído
logo depois de carregado.
