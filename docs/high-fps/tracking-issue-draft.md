# Rascunho: issue de acompanhamento "Alto FPS" (NÃO PUBLICADA)

Rascunho para o mantenedor revisar. Só abrir no GitHub com autorização dele
(`gh issue create -R FelipePrado0/SpartanPort ...`). Título sugerido:

**[Tracking] High engine frame rates (100–240 FPS): spots that break above 60 / [Acompanhamento] Taxa do motor alta (100–240 FPS): trechos que quebram acima de 60**

---

**EN:**

This issue tracks every spot of the game that breaks when the engine runs above 60 FPS (Performance → Engine frame rate at 100, 120, 144, 180 or 240) and works at 60. The goal is to play the whole game at any of these rates.

The list lives in [`docs/high-fps/catalog.md`](../blob/main/docs/high-fps/catalog.md), grouped by the game mechanic that probably causes each problem:

- **Sideways ledge and wall traversal** (Realm of Hades and others): Kratos is pushed back or will not cross (#114, #313, #640, #535, #1011).
- **Button-mash and object prompts**: lifting the trunk in the prologue (#44, #589), pushing an object in Peirithous' room (#1085), turning the ballista in the blue portal room (#812).
- **Missing sounds** at high rates (#885).
- Reports still to confirm: the invisible wall in Chains of Balance (#839), the jump down the vines against Poseidon (#965), Kratos stuck after Hades' death (#1024), the Muses' harp (#768).

**Until each one is fixed:** press **R3 + L1 + R1** (or **F9**) until the notice shows **60 FPS**, pass the spot, then press it again to go back. A coming version also drops the engine to 60 by itself during quick-time prompts.

**How you can help:** if a spot breaks above 60 and works at 60, comment here with:
1. where in the game (chapter or area, and what Kratos is doing);
2. the engine rate where it breaks, and confirmation that it works at 60;
3. your `user\last_run.log` and, if you can, a save right before the spot.

Spots that also break at 60 are a different problem: please open a separate issue for them.

---

**PT-BR:**

Esta issue acompanha cada trecho do jogo que quebra quando o motor roda acima de 60 FPS (Desempenho → Taxa do motor em 100, 120, 144, 180 ou 240) e funciona a 60. O objetivo é jogar o jogo inteiro em qualquer uma dessas taxas.

A lista fica em [`docs/high-fps/catalog.md`](../blob/main/docs/high-fps/catalog.md), agrupada pela mecânica do jogo que provavelmente causa cada problema:

- **Travessia lateral em salientes e paredes** (Reino de Hades e outros): Kratos recua ou não atravessa (#114, #313, #640, #535, #1011).
- **Comandos de apertar repetido e de objetos**: levantar o tronco no prólogo (#44, #589), empurrar um objeto na sala do Peirithous (#1085), girar a balista na sala dos portais azuis (#812).
- **Sons faltando** em taxas altas (#885).
- Relatos ainda a confirmar: a parede invisível em Chains of Balance (#839), a descida pelas vinhas contra Poseidon (#965), Kratos travado depois da morte do Hades (#1024), a harpa das Musas (#768).

**Até cada um ser corrigido:** aperte **R3 + L1 + R1** (ou **F9**) até o aviso mostrar **60 FPS**, passe o trecho e aperte de novo para voltar. Uma próxima versão também baixa o motor para 60 sozinha durante os comandos de QTE.

**Como ajudar:** se um trecho quebra acima de 60 e funciona a 60, comente aqui com:
1. onde no jogo (capítulo ou área, e o que Kratos está fazendo);
2. a taxa do motor em que quebra, e a confirmação de que funciona a 60;
3. o seu `user\last_run.log` e, se puder, um save logo antes do trecho.

Trechos que também quebram a 60 são outro problema: abra uma issue separada para eles.
