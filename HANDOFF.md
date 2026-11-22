# Como usar este pacote

Conteúdo:
- `SPEC.md` — especificação detalhada (aprovada).
- `PLAN.md` — marcos e tarefas testáveis (aprovado).
- `server/` — esqueleto FastAPI do servidor de licenciamento (roda em dev; ver `server/README.md`).
- `client/` — stubs e notas de integração C++ (entram no repo do solver, não aqui).

## Para entregar ao agente de IA
1. Coloque esta pasta num **repositório separado** do solver.
2. Entregue ao agente: **o prompt** (está no documento do Claude — exporte para Markdown) **+ `SPEC.md` + `PLAN.md`**.
3. Diga ao agente: *"A SPEC e o PLAN já estão aprovados. Pule a etapa de propor spec e implemente a partir
   deles, marco a marco, com testes e revisão adversarial ao fim de cada um. Comece pelo M0."*
4. O código de integração do cliente (`client/`) vai para o repo do solver (Structuradll), não para cá.

## Ordem sugerida
M0 setup → M1 auth/licença → M2 integridade → M3 equilíbrio+K → M4 integração C++ →
M5 anti-tamper → M6 abuso → M7 admin → M8 pagamentos → M9 hardening/LGPD/pentest/deploy.
