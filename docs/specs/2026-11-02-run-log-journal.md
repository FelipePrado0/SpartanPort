# Diário do log de execução: `last_run.jsonl`

Branch: `pr/run-log-timestamps`. Código em `launcher-rust/src/core/log.rs` e
`launcher-rust/src/core/launch.rs`.

## Spec (SDD)

* **Objetivo:** como mantenedor, testador ou ferramenta (incluindo assistentes de IA) que lê o log de
  uma execução, quero saber QUANDO cada linha saiu e qual veio primeiro numa cascata de problemas
  (por exemplo, remoções do cache de texturas minutos antes de um defeito de textura), sem mudar o
  `last_run.log`.
* **Problema hoje:** o `last_run.log` guarda só o texto do jogo. Não há hora nas linhas, nem nível,
  nem o código de saída do jogo. Quem recebe um relato não consegue ordenar eventos no tempo.
* **Contrato:**
  - Entrada: os mesmos pedaços de saída que a thread leitora de `spawn_game` já grava no
    `RunLog`, mais o aviso de saída (o conselho de `exit_hint`) e o código de saída.
  - Saída: o arquivo `last_run.jsonl`, na mesma pasta do `last_run.log`, em JSON Lines (um objeto
    JSON por linha, UTF-8, terminador `\n`).
  - API pública: `RunLog::open(path)` e `RunLog::write(text)` não mudam. `RunLog` ganha
    `finish(self, code, advice)`, que grava o aviso (como `write` o fazia) e o registro de saída.
    Quem chama (`ui/play.rs`, `runner/mod.rs`) não muda.
  - O `last_run.log`, a cópia no console, o texto de `GameEvent::Output` e o log da interface
    continuam byte a byte iguais. O `last_run.jsonl` é novo no launcher Rust; o launcher Python não
    o grava.
* **Esquema dos registros** (um por linha do arquivo; a ordem das chaves é a da tabela):

  | `type` | Campos | Quando |
  |---|---|---|
  | `start` | `schema` (1), `time`, `launcher` (versão), `os` | ao abrir o diário |
  | `line` | `n`, `t`, `time`, `source`, `level`, `area`, `continuation` (só quando `true`), `text` | uma por linha do `last_run.log` |
  | `exit` | `t`, `time`, `code`, `advice` | depois que o jogo termina |

  - `time`: hora local em RFC 3339 com milissegundos e deslocamento (`2026-11-02T14:03:21.482-03:00`).
  - `t`: segundos desde a abertura do diário, arredondados a milissegundos (número JSON).
  - `n`: número da linha no `last_run.log`, a partir de 1, contando linhas vazias e as linhas do
    aviso de saída.
  - `source`: `game` para o que o jogo escreveu, `launcher` para o que o launcher escreveu na saída
    (o aviso).
  - `level`: `trace`, `debug`, `info`, `warning`, `error` ou `critical`.
  - `area`: texto ou `null`.
  - `text`: a linha sem `\r\n`.
  - `code`: inteiro, ou `null` quando `wait` do processo falhou. `advice`: texto, ou `null` quando o
    launcher não escreveu aviso.
* **Critérios de aceite:**
  - QUANDO o launcher abrir o `last_run.log` O SISTEMA DEVE criar (ou truncar) o `last_run.jsonl` na
    mesma pasta e gravar o registro `start` como primeira linha.
  - QUANDO o jogo imprimir uma linha O SISTEMA DEVE gravar um registro `line` com o `n` dessa linha
    no `last_run.log`, na mesma ordem, com `text` sem `\r\n`.
  - QUANDO a linha for do logger da GPU (`GPU [Classe] <Nivel> arquivo:linha funcao: msg`) O
    SISTEMA DEVE usar `Nivel` em minúsculas como `level` e `Classe` como `area`.
  - QUANDO a linha começar com `STOP:` ou `Fault:` O SISTEMA DEVE gravar `level` `critical`.
  - QUANDO a linha tiver um prefixo curto antes do primeiro `: ` (até 32 caracteres, começando por
    letra, sem espaço à esquerda, sem `:` nem `\`) O SISTEMA DEVE usá-lo como `area`; senão, `null`.
  - QUANDO a linha começar com espaço ou tabulação e vier depois de outra linha O SISTEMA DEVE gravar
    `"continuation":true` e herdar `level` e `area` da linha anterior.
  - QUANDO uma linha chegar em vários pedaços de 64 KiB O SISTEMA DEVE gravar um só registro, com a
    hora em que o primeiro pedaço chegou.
  - QUANDO a saída terminar sem `\n` final O SISTEMA DEVE gravar a última linha como registro ao
    encerrar.
  - QUANDO o jogo falhar com um aviso conhecido O SISTEMA DEVE gravar as linhas do aviso (a linha
    vazia e o aviso) como registros com `source` `launcher` e o `n` do `last_run.log`.
  - QUANDO o jogo terminar, por qualquer caminho de `spawn_game` O SISTEMA DEVE gravar o registro
    `exit` como última linha, com o código de saída (`null` se `wait` falhar) e o aviso (`null` se
    não houve).
  - QUANDO o `last_run.jsonl` não puder ser criado ou uma gravação nele falhar O SISTEMA DEVE
    desligar só o diário, em silêncio; o `last_run.log`, a cópia no console e o jogo NÃO DEVEM ser
    afetados.
  - QUANDO o `last_run.log` falhar O SISTEMA DEVE continuar gravando o diário (são independentes).
* **Requisitos não funcionais:** sem crate novo (`chrono` e `serde_json` já são dependências); sem
  `unwrap`/`expect` fora dos testes; uma única chamada de escrita por registro; o diário nunca
  bloqueia nem atrasa a leitura do pipe além de uma escrita pequena por linha; multiplataforma.
* **Arquivos afetados:**
  - `launcher-rust/src/core/log.rs`, `launcher-rust/src/core/launch.rs`
  - `launcher-rust/src/core/update.rs` (`USER_FILES`: uma atualização não copia o diário)
  - `launcher-rust/tests/launch.rs`, `launcher-rust/tests/runner.rs`
  - `.gitignore`, `packaging/windows/package.sh`, `packaging/windows/smoke-test.ps1` (o diário nunca
    entra no pacote)
  - `README.md`, `README.pt-BR.md`, `README.es.md`, `packaging/windows/README-Windows.txt`
  - `docs/specs/2026-10-21-rust-launcher-design.md`, `docs/specs/2026-10-21-rust-launcher-parity.md`
* **Plano técnico:**
  - `RunLog` passa a ter o arquivo e um `RunJournal` opcional. O caminho do diário vem do caminho
    do log com a extensão trocada por `jsonl`.
  - `RunJournal` recebe o texto na mesma divisão por `\n` do arquivo, então o `n` bate sem contar
    à parte: um pedaço sem `\n` fica pendente, com a hora do primeiro pedaço, até o `\n` chegar ou
    o jogo terminar. O aviso entra pelo mesmo caminho, como texto do launcher.
  - `level` e `area` vêm de uma função pura sobre o texto da linha; linhas de continuação herdam o
    resultado da anterior.
  - `finish` substitui o `write` do aviso e o `drop` no fim da thread leitora, para que o registro
    `exit` saia em todos os caminhos (saída limpa, saída com falha, erro de `wait`).
  - A classificação é uma heurística sobre o texto (o jogo usa `printf` com prefixo de área); um
    prefixo que parece rótulo, mas não é, vira `area` mesmo assim. Quem lê o diário deve tratar
    `area` como dica, não como contrato.
* **Validação:** testes automáticos abaixo (TDD); `cargo fmt --check`, `cargo clippy --all-targets
  -- -D warnings`, `cargo test` e `cargo build --release`. Não testado: Linux e uma partida real.
* **Aprovação:** a definir pelo mantenedor.

## Ciclo TDD (plano dos testes)

### 1. Classificação e registros (`src/core/log.rs`)

* **Comportamento:** registro `start`; registros `line` com `n` e `t` crescentes e CRLF removido;
  `level` e `area` do logger da GPU, de `STOP:`/`Fault:`, de `Texture cache: memory pressure, ...`
  e `null` em texto livre; continuação herda `level` e `area`; linha longa em vários pedaços vira um
  registro; último pedaço sem `\n` é gravado ao encerrar; linhas do aviso com `source` `launcher` e
  `n` igual ao do `last_run.log`; registro `exit`; erro de escrita no diário deixa o `last_run.log`
  intacto.
* **Testes:** `src/core/log.rs`, módulo `tests` (nomes pelo comportamento).

### 2. Integração com o jogo (`tests/launch.rs`, `tests/runner.rs`)

* **Comportamento:** depois de uma execução do `fake-game`, o `last_run.jsonl` existe, seus
  registros `line` reproduzem o `last_run.log` linha a linha (`n` e `text`) e o registro `exit`
  traz o código; uma execução com falha e dica conhecida traz o aviso no registro `exit`; `--play`
  também grava o diário.
* **Red:** os testes não compilam enquanto `RunLog::finish` não existir, e falham enquanto o
  arquivo não for criado.
* **Green:** `RunJournal`, `RunLog::finish` e a thread leitora chamando `finish`.

### 3. Atualização e pacote

* **Comportamento:** `USER_FILES` lista `last_run.jsonl` ao lado de `last_run.log`; o pacote do
  Windows recusa `*.jsonl`.
* **Teste:** `src/core/update.rs::tests::user_files_include_settings_and_icon`; as verificações dos
  scripts de empacotamento rodam no CI.
