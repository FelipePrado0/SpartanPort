# Manifesto da instalação do jogo (`tools/game_manifest.py`)

Data: 2026-10-29. Branch: `diag/infinite-loading`. Frente: carregamento infinito
(`docs/plans/2026-10-29-roadmap-carregamento-infinito.md`, fase de integridade). Reutilizado pelas
frentes de colisão e de Alto FPS (F3) para identificar a instalação de cada execução.

## SDD

* **Objetivo:** como mantenedor ou relator, quero gerar um inventário (caminho relativo, tamanho,
  SHA-256) da pasta do jogo extraído e comparar dois inventários, para saber se uma instalação tem
  arquivos faltando, sobrando ou diferentes de outra extração, sem enviar arquivos do jogo.
* **Contrato:**
  * `python -I tools/game_manifest.py create <pasta> [-o arquivo] [--format json|text] [--quick] [--exclude PADRÃO]...`
    * Percorre `<pasta>` sem seguir links de diretório; para cada arquivo regular registra o caminho
      relativo com `/`, o tamanho em bytes e o SHA-256 (ou `null` com `--quick`).
    * Saída JSON: `{"format": "spartanport-game-manifest", "version": 1, "root": "<só o nome da pasta>",
      "hash": "sha256"|null, "files": [{"path", "size", "sha256"}...], "errors": [{"path", "error"}...],
      "totals": {"files", "bytes", "errors"}}`, ordenada por caminho. Nunca grava o caminho absoluto
      (dado pessoal).
    * Saída texto: uma linha por arquivo, `<sha256 ou -> <tamanho> <caminho>`, e linhas `# error <caminho>: <motivo>`.
    * Sem `-o`, escreve na saída padrão. Progresso em stderr só quando stderr é um terminal.
    * Erros de leitura (acesso negado, arquivo sumiu) entram em `errors` e não interrompem.
  * `python -I tools/game_manifest.py compare <A> <B> [--format text|json]`
    * `A` e `B` são manifestos (JSON ou texto deste programa) ou pastas (geradas na hora).
    * Relata `missing` (em A, não em B), `extra` (em B, não em A), `different` (tamanho ou hash
      diferentes), `case_only` (mesmo caminho a menos de maiúsculas, comum entre Windows e Linux) e
      `unhashed` (comparados só por tamanho porque um lado foi gerado com `--quick`).
  * Código de saída: 0 sem diferença; 1 com diferença (ou erros de leitura em `create`); 2 erro de uso/entrada.
* **Critérios de aceite:**
  * QUANDO `create` roda numa árvore O SISTEMA DEVE listar todo arquivo regular com tamanho e SHA-256 corretos, em ordem estável e com `/` como separador.
  * QUANDO `create` roda O SISTEMA DEVE gravar só o nome da pasta, nunca o caminho absoluto.
  * QUANDO um arquivo não puder ser lido O SISTEMA DEVE registrá-lo em `errors`, continuar e sair com 1.
  * QUANDO `--exclude` casar com um caminho relativo O SISTEMA DEVE omiti-lo.
  * QUANDO `compare` recebe duas árvores/manifestos O SISTEMA DEVE separar faltando, sobrando, divergentes e diferenças só de maiúsculas, e sair com 0 só se não houver nenhuma.
  * QUANDO um lado foi gerado com `--quick` O SISTEMA DEVE comparar só o tamanho e listar esses arquivos em `unhashed`.
  * QUANDO a entrada não é um manifesto válido O SISTEMA DEVE sair com 2 e mensagem clara.
  * O SISTEMA NÃO DEVE copiar, mover, alterar ou enviar arquivos; só lê a árvore e escreve o manifesto pedido.
* **Requisitos não funcionais:** Python 3.8+ só com biblioteca padrão, compatível com `python -I`;
  Windows (PowerShell/Git Bash, caminhos com `\`) e Linux; leitura em blocos de 1 MiB (memória
  constante para WADs de centenas de MB); sem rede.
* **Arquivos afetados:** `tools/game_manifest.py` (novo), `tests/test_game_manifest.py` (novo).
* **Plano técnico:** `os.walk(followlinks=False)` com `onerror` para registrar pastas ilegíveis;
  `hashlib.sha256` por blocos; `fnmatch` para `--exclude`; leitura do manifesto texto/JSON pelo
  primeiro caractere. Risco: hash de ~40 GB leva minutos (documentado; `--quick` para triagem).
* **Validação:** `python3 -m unittest tests.test_game_manifest` (de `tests/`: `python3 -m unittest
  test_game_manifest`) com árvore sintética: criação, exclusão, erro de leitura (POSIX, sem root),
  comparação texto e JSON, `--quick`, entrada inválida. No Windows: o mantenedor roda `create` na
  pasta do jogo (roteiro em `docs/investigations/carregamento/`).
* **Aprovação:** aprovada pelo líder por delegação do mantenedor (2026-10-29).

## TDD

* **Comportamento:** todos os critérios de aceite acima.
* **Teste:** `tests/test_game_manifest.py` (`GameManifestTests`, 8 casos: criação, saída em
  arquivo/texto, `--exclude`/`--quick`, arquivo ilegível por permissão, erro de leitura em processo,
  comparação com faltando/sobrando/divergente/maiúsculas, idêntico e `--quick`, entrada inválida).
* **Red:** `cd tests && python3 -m unittest test_game_manifest` antes da ferramenta existir:
  `FAILED (failures=6, skipped=1)`, com `can't open file '.../tools/game_manifest.py'`.
* **Green:** `tools/game_manifest.py` escrito; mesmo comando: `Ran 8 tests ... OK (skipped=1)`
  (o caso de permissão é pulado como root). Numa cópia rodando como `nobody`
  (`setpriv --reuid=65534 ... python3 -m unittest -v test_game_manifest`): `Ran 8 tests ... OK`.
* **Refactor:** nenhum além do teste em processo para o caminho de erro (vale também como root).
* **Validação final:** os mesmos comandos acima. Não testado aqui: Windows (PowerShell/Git Bash) e
  árvore real do jogo; ficam no roteiro do mantenedor.
