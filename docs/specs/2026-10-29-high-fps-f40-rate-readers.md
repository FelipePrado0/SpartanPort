# F4.0: ferramenta `tools/find_rate_readers.py`

Fase F4.0 do roadmap `docs/plans/2026-10-29-high-fps-roadmap.md` (branch `docs/high-fps-roadmap`).
Branch: `feat/high-fps-f40-rate-readers`.

## Spec (SDD)

* **Objetivo:** como agente da F4 (modo nuvem, sem o jogo), quero que o mantenedor rode uma
  ferramenta no próprio dump e me cole a lista de todo código que lê a taxa do motor, para escolher
  as rotinas candidatas a correção sem que o executável saia do PC dele.
* **Contrato:**
  - Uso: `python -I tools/find_rate_readers.py <eboot.elf|eboot.bin> [--no-objdump | --objdump CAMINHO]`.
  - Python 3 puro, sem dependências; `llvm-objdump`/`objdump` só se estiver no PATH (ou indicado).
  - Entrada: ELF64 (o `eboot.elf` preparado pelo port, ou um `eboot.bin` já em ELF), lido pelos
    segmentos `PT_LOAD`/`PT_SCE_RELRO` nos endereços virtuais (os offsets de imagem dos hooks).
    Arquivo que não é ELF é lido como imagem plana, com aviso na primeira linha.
  - Saída (texto, stdout): o setter e os dois globais da taxa; as cópias das constantes 59,94
    (`0x426fc28f`), 60,0 (`0x42700000`) e 1/60 (`0x3c888889`) nos dados; cada leitor com endereço
    virtual, offset no arquivo, instrução, alvo, 48 bytes em hex e, com objdump, ±16 instruções;
    os leitores agrupados por função (do `push rbp; mov rbp, rsp` anterior até o `ret` seguinte),
    com a contagem por função; e a conferência do caso conhecido.
  - Código de saída: 0 quando a análise roda (o caso conhecido ausente é um **aviso**); 2 para uso
    errado.
* **Critérios de aceite:**
  - QUANDO o executável tiver o setter de `src/probe.c` (`engine_fps_setter`) O SISTEMA DEVE
    calcular os globais como `src/probe.c` (float: site+7+disp@3; arredondado: site+30+disp@26).
  - QUANDO um padrão de assinatura ocorrer mais de uma vez O SISTEMA DEVE tratá-lo como não achado
    (mesma regra de `src/signature.h`, `??` coringa).
  - QUANDO uma instrução `[rip+disp32]` resolver para um global ou uma constante O SISTEMA DEVE
    listá-la; `lea`, `mov`, `movss`/`vmovss`, `cvtsi2ss` e as formas VEX/SSE de
    `addss`/`mulss`/`subss`/`divss` com o nome; outra forma qualquer com "a confirmar".
  - QUANDO a instrução fizer parte do próprio setter O SISTEMA NÃO DEVE contá-la como leitor.
  - QUANDO o leitor da "swing energy" (padrão do Frame Rate Patch, `0x48ff1a` no v01.02) não
    estiver na lista O SISTEMA DEVE imprimir `WARNING: ... the tool is wrong for this build`.
  - QUANDO estiver no executável sintético do teste O SISTEMA DEVE achar exatamente os 3 leitores.
* **Requisitos não funcionais:** roda com `python -I` (sem site, sem pacotes); não grava nada além
  de um arquivo temporário para o objdump; imprime só endereços e trechos curtos, nunca o arquivo;
  lê um executável de ~30 MB em tempo aceitável (varredura linear do código).
* **Arquivos afetados:** `tools/find_rate_readers.py`, `tests/test_find_rate_readers.py`, esta spec.
* **Plano técnico:** busca de assinatura igual à de `src/signature.h`; varredura de cada posição de
  código como disp32 (com imediato de 0, 1 ou 4 bytes depois), classificação pelo ModRM
  `mod=00 rm=101` e pelos prefixos/opcode antes dele; agrupamento por função pelo prólogo
  `55 48 89 e5`. Risco: falsos positivos raros quando bytes de dados no código formam um disp32
  válido (ficam "a confirmar"); funções sem prólogo `push rbp` aparecem como "function ?".
* **Validação:** `python3 -I tests/test_find_rate_readers.py` com um ELF sintético (setter falso,
  globais, constantes, três leitores, um engodo); o mantenedor roda no `eboot.elf` do v01.02 e
  confere o aviso do caso conhecido.
* **Aprovação:** aprovada pelo líder por delegação do mantenedor.

## Ciclo TDD (registro)

* **Comportamento:** os critérios acima (setter e globais, assinatura única, 3 leitores no
  sintético, setter fora da contagem, aviso do caso conhecido, `python -I` sem objdump).
* **Teste:** `tests/test_find_rate_readers.py` (`test_signature_search_matches_src_signature_h`,
  `test_three_readers_and_the_known_case`, `test_missing_known_case_is_a_warning`,
  `test_runs_isolated_without_objdump`). O ELF sintético é montado no teste; nenhum arquivo do jogo.
* **Red:** `python3 -I tests/test_find_rate_readers.py` sem a ferramenta →
  `FileNotFoundError: ... tools/find_rate_readers.py`, `FAILED (errors=4)`.
* **Green:** `tools/find_rate_readers.py` → `Ran 4 tests ... OK`.
* **Refactor (testes verdes):** objdump passa a ser só o GNU (o `llvm-objdump` não lê um bloco cru:
  a primeira versão saía "(no disassembly)"); a varredura passa a partir das posições com ModRM
  `mod=00 rm=101` (regex), de minutos para ~1 s num arquivo sintético de 30 MB.
* **Validação final:** `python3 -I tests/test_find_rate_readers.py` → OK; rodada manual no ELF
  sintético com `objdump` mostra a disassembly em volta de cada leitor; arquivo de 30 MB em 1,1 s.
* **Não testado:** o executável real (fica no PC do mantenedor). Pedido ao mantenedor:
  `python -I tools/find_rate_readers.py <dump>\eboot.bin > readers.txt` (ou o `eboot.elf` do port)
  e colar a saída; a última linha deve ser `known case: the swing energy reader at 0x48ff1a is listed`.
