# Rascunho: mensagem ao cuesta4 (W5)

Rascunho para o mantenedor revisar. Não foi enviado. O mantenedor decide se envia, por onde e com
que texto. Não é aconselhamento jurídico.

Fatos que a mensagem usa (todos conferidos em 2026-11-18):

- O SpartanPort só leu o código do rehooked (`a8caa54`, v0.1.2-alpha) para comparar mecanismos.
  Nenhum código dele foi copiado nem executado. A análise está em
  `docs/plans/2026-11-rehooked-gap-analysis.md`.
- A comparação de desempenho não rodou: o rehooked aceita só o eboot da 1.02 real, e a máquina de
  teste só tem um dump com o código da 1.00.
- O crédito atual do cuesta4 no README (patches e a branch `eltutz` do shadPS4) continua.

---

## EN-US

Hi cuesta4,

I'm Felipe, from SpartanPort. First, thank you again: the game renders correctly in SpartanPort
because of the fixes in your `eltutz` branch, and you are credited for them and for the patches.

I read the source of your rehooked port to understand how it handles shaders and memory. I
compared it with what SpartanPort does today: preparing shaders from the game data before play,
the shader cache layout, keeping hot pages open with per-piece hashing, lazy barriers, and thread
planning by CPU topology. I did not copy any of your code. If one of these ideas ends up in
SpartanPort, it will be written from scratch from a plain description, and the commit and the
README will credit the idea to you.

I could not compare performance yet, because my only dump has the 1.00 eboot and your port needs
the 1.02 one.

Two questions, if you have a moment:

- Is there a kind of credit you would prefer, beyond the README line ("ideas studied from
  rehooked by cuesta4")?
- Would you mind if I contacted you with questions about the per-piece hashing, in case we try it?

Thanks for the work you share with everyone.

Felipe

---

## PT-BR

Olá, cuesta4,

Sou o Felipe, do SpartanPort. Antes de tudo, obrigado de novo: o jogo é renderizado corretamente
no SpartanPort graças às correções da sua branch `eltutz`, e você é creditado por elas e pelos
patches.

Li o código do seu port rehooked para entender como ele trata shaders e memória. Comparei com o que
o SpartanPort faz hoje: preparo de shaders a partir dos dados do jogo antes de jogar, o formato do
cache de shaders, páginas quentes abertas com hash por pedaço, barreiras preguiçosas e planejamento
de threads pela topologia da CPU. Não copiei nada do seu código. Se uma dessas ideias entrar no
SpartanPort, ela será escrita do zero a partir de uma descrição simples, e o commit e o README vão
dar o crédito da ideia a você.

Ainda não consegui comparar desempenho, porque meu único dump tem o eboot da 1.00 e o seu port
precisa do da 1.02.

Duas perguntas, se tiver um tempo:

- Você prefere algum tipo de crédito além da linha no README ("ideias estudadas do rehooked, do
  cuesta4")?
- Posso te procurar com dúvidas sobre o hash por pedaço, caso a gente tente essa ideia?

Obrigado pelo trabalho que você compartilha com todo mundo.

Felipe
