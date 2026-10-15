# infamous_firstlight

Pesquisa de port nativo para PC de **inFAMOUS First Light** (PlayStation 4), focada na versão
**CUSA00575**, Windows 11 e Vulkan.

Este repositório é um fork de
[FelipePrado0/gow3_pc](https://github.com/FelipePrado0/gow3_pc). A ideia é aproveitar a
arquitetura de port nativo: o código x86-64 original do jogo roda diretamente na CPU do PC,
enquanto um runtime próprio substitui serviços do PS4 e a camada gráfica GNM é traduzida para
Vulkan pela base derivada do shadPS4.

> Nenhum arquivo do jogo é incluído. Use um dump feito a partir da sua própria cópia.

## Estado atual

**Bring-up / ainda não jogável.**

A branch do First Light já possui:

- validação do CUSA00575;
- preparação e linkagem de `eboot.bin`, `libc.prx` e `libSceFios2.prx`;
- imports adicionais específicos do First Light;
- contratos offline para NP Manager, NP Score, NP TUS e NP WebApi;
- suporte existente de RTC ligado a `sceRtcGetTime_t`;
- registro headless de símbolos GNM/VideoOut para `--check-imports`;
- hooks de gameplay específicos de God of War III desativados;
- perfil GPU do First Light sem endereços ou hashes de shader inventados;
- build Windows gerando `out/firstlight-probe.exe`;
- log automático e resumo do primeiro fault;
- testes e compilação Windows pelo GitHub Actions.

Ainda não existe comprovação de boot bem-sucedido do CUSA00575.

## Alvo estático confirmado

- Title ID: **CUSA00575**
- APP_VER: **01.00**
- Entry point: `0xA19C30`
- Imports referenciados no eboot: **468**
- Relocações TLS:
  - eboot: 0
  - libSceFios2: 2
  - libc: 14

## Primeiro teste no Windows 11

Depois de instalar as dependências de [docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md):

```powershell
git clone --recursive https://github.com/xruanzin/infamous_firstlight.git
cd infamous_firstlight
git checkout port/cusa00575-stage1
Set-ExecutionPolicy -Scope Process Bypass
.\build.ps1
```

Valide seu dump:

```powershell
python .\tools\validate_firstlight.py "D:\Games\FirstLight\CUSA00575"
```

Rode o Stage 1:

```powershell
python .\run_firstlight.py "D:\Games\FirstLight\CUSA00575" --mode check
```

O objetivo é terminar com:

```text
Imports: 0 missing
```

Depois:

```powershell
python .\run_firstlight.py "D:\Games\FirstLight\CUSA00575" --mode cpu
python .\tools\summarize_firstlight_log.py
```

Quando os imports estiverem limpos, o caminho Vulkan experimental é:

```powershell
python .\run_firstlight.py "D:\Games\FirstLight\CUSA00575" --mode gpu
```

A saída completa fica em `user/last_run.log`.

## Documentação

- [Build Windows](docs/BUILD_WINDOWS.md)
- [Stage 1](docs/STAGE1_FIRSTLIGHT.md)
- [Stage 2](docs/STAGE2_FIRSTLIGHT.md)
- [Estratégia GPU](docs/FIRSTLIGHT_GPU.md)

## Regra do port

Nada de copiar offsets, shaders ou hooks de God of War III/Bloodborne para o First Light sem
evidência. Cada exceção específica do jogo deve nascer de um trace reproduzível do CUSA00575.

Licença: **GPL-2.0**. Este projeto não é afiliado à Sony Interactive Entertainment nem à Sucker
Punch Productions.


## Triage automático

Depois de compilar, o caminho recomendado para o primeiro teste real é:

```powershell
python .\tools\firstlight_triage.py "D:\Games\FirstLight\CUSA00575"
```

Ele executa a verificação de imports, tenta o boot CPU-only e cria
`user\diagnostic.json`. Esse JSON não contém os binários do jogo nem deve incluir o caminho
absoluto do seu dump; ele reúne commit, alvo, quantidade de imports faltantes e o primeiro blocker.

O Vulkan continua opt-in durante o bring-up:

```powershell
python .\tools\firstlight_triage.py "D:\Games\FirstLight\CUSA00575" --gpu
```
