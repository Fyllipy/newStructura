# Validação da metodologia de efeito de alavanca na mesa do pilar

Scripts Python (somente biblioteca padrão) que reproduzem referências publicadas. **Não usam resultados do Structura como referência.**

| Arquivo | O que faz |
|---|---|
| `tstub_spec.py` | Especificação executável: Camada B (EN 1993-1-8, Tabelas 6.2/6.4/6.6, 6.2.4.2) e Camada A (NBR 8800:2024, 6.3.5.2–6.3.5.4) |
| `validate_ref.py`, `validate_ref2.py` | Camada B contra SCI P398 Exemplo C.1 e a verificação de T-stub da IDEA StatiCa (método dos componentes e MEF sólido publicados) |
| `validate_nbr.py` | Camada A contra as mesmas referências e varreduras de espessura |
| `heb200.py` | Exemplo HEB200 da investigação do A2, com as duas camadas |
| `saida_*.txt` | Saídas geradas por estes scripts |

Referências (consultadas em 27/11/2026): SCI P398, *Joints in steel construction: Moment-resisting joints to Eurocode 3*, Apêndice C, Exemplo C.1 (https://steelconstruction.info/images/5/5d/SCI_P398.pdf); G. Gödrich, *Verification of T-stub* (IDEA StatiCa, 2015) (https://people.fsv.cvut.cz/sokol/sts3/Verification_T-stub_c.pdf); EN 1993-1-8:2005 (cópia consultada: https://www.phd.eng.br/wp-content/uploads/2015/12/en.1993.1.8.2005-1.pdf; texto extraído com ruído de OCR — conferir equações na norma oficial); ABNT NBR 8800:2024, 6.3.5 (cópia em `normas/`).
