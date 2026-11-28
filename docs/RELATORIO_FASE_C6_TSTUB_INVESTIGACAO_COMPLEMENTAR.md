# Relatório da Fase C6 — investigação complementar do T-stub

Nada foi implementado em C++; nenhum arquivo de produção, `JointComponents.cpp`, `TStubStiffness`, M–θ, golden ou verificação normativa foi alterado. Este relatório **não declara validação profissional**: distingue reprodução de equações, comparação experimental, verificação numérica por MEF e validação física, e diz qual evidência sustenta cada conclusão.

## 1. Entregáveis e classificação
| Atividade | Documento | Classificação |
|---|---|---|
| C6.1 Faella (1998, 1999) | `docs/technical/TStub_Faella_1998_1999_Investigation.md` | **NÃO CONCLUÍDA** |
| C6.2 Alternativas de parâmetros do material | `docs/technical/TStub_Material_Properties_Alternatives.md` | **PARCIALMENTE CONCLUÍDA** |
| C6.3 MEF elástico | `docs/validation/TStub_FEM_Elastic_Study_Results.md` (+ `research/tstub_fem/`, gráfico `figures/tstub_fem_stiffness.svg`) | **PARCIALMENTE CONCLUÍDA** |
| C6.4 Mecanismo 3 | `docs/technical/TStub_Failure_Mode_3_Investigation.md` | **PARCIALMENTE CONCLUÍDA** (permanece indisponível) |
| C6.5 API | `docs/architecture/TStubForceDisplacement_API_Specification.md` | **CONCLUÍDA** (especificação; sem código) |
| C6.6 Proposta constitutiva final | `docs/technical/TStub_Constitutive_Model_Final_Proposal.md` | **CONCLUÍDA** (proposta fundamentada nas evidências disponíveis; ver §3, resposta 9) |

## 2. Fontes e acessos
Lidas integralmente (acesso aberto, baixadas em `docs/references/`, **material protegido: não versionar**): Piluso et al. (2001) I e II (fornecidos), Faralli et al. (2020), Swanson (2002), Girão Coelho et al. (2004), Tartaglia & D'Aniello (2021). **Não obtidas:** Faella et al. (1998) (OpenAlex: acesso fechado, sem repositório), Faella et al. (1999) (livro), Francavilla et al. (2016) e (2025), Latour & Rizzano (2012), revisão MDPI (403). Um segundo meio de acesso a fontes com HTTP 403 foi **bloqueado pela política de permissões da sessão e não foi insistido**. Nenhuma citação secundária substituiu fonte original.

## 3. Respostas objetivas (§10 do prompt)
1. **Faella (1998) e (1999) foram obtidos e compreendidos?** **Não.** Acesso fechado/inexistente em fonte aberta. Registrei só o que Piluso afirma sobre eles.
2. **Largura efetiva e rigidez foram reconstruídas?** **Não a partir da fonte.** As expressões (`K`, `b_eff`, `m'`, `ζ`) foram reproduzidas **como impressas em [P2]** (exemplo trabalhado a ≤ 0,06 %, Fase C); a origem e o domínio de validade seguem desconhecidos.
3. **Existem alternativas publicadas que dispensem os parâmetros de cupom?** **Não verificado.** Entre as fontes lidas: Faralli et al. dão **mapas de modo de falha** com propriedades convencionais (`t_p²/A_s`, `f_y/f_ub`), não uma lei F–δ nem `δ_u`; a candidata mais provável (Francavilla 2016/2025, forma fechada) **não pôde ser lida**.
4. **Alternativa com melhor equilíbrio rigor × aplicabilidade?** Nenhuma verificada. Recomendação: **arquitetura de modelos múltiplos (Estratégia C) com conteúdo inicial Piluso estrito (A)**, sem defaults; um segundo modelo independente só após leitura de Francavilla.
5. **O MEF foi executado?** **Sim** (Mecway + CalculiX, sólidos hexaédricos, contato unilateral), verificado contra balanço analítico (+1 % do limite de placa), convergido (último passo −0,3 %).
6. **A rigidez inicial foi comparada quantitativamente?** **Sim, para geometrias assumidas** (placa sem filete, furo/arruela/`t_w` assumidos): EN(`l_eff=b_eff`)/MEF = 1,5–3,6; Piluso/MEF = 0,89–2,2; tendências robustas: saturação com a largura, crescimento bem menor que `t_f³`. **Não decide** entre as formulações: a espessura da arruela (±50 %), o furo (+80 % sem ele) e o filete dominam e não constam das publicações.
7. **Há fontes suficientes para o mecanismo 3?** **Não** entre as lidas (sem lei F–δ; só `F_u,3=2B_u`, resultados de MEF e ensaios qualitativos). A fonte promissora (Francavilla 2016) está restrita.
8. **A API está suficientemente especificada?** **Sim** para o escopo decidido: modelo explícito (`ModelId`), entradas com origem, estados, saídas com `ResultKind::ModelUltimate`, desacoplamento. Pontos abertos: fonte da rigidez (`UserProvided` até Faella) e escolha dos avisos de domínio.
9. **A proposta constitutiva está suficientemente fundamentada?** **Para os mecanismos 1 e 2, sim, como modelo publicado reproduzido e comparado com 12 ensaios dos próprios autores** (Fase C: `F_u` e `δ_u` com média 0,98/0,95; curva contínua −3,9 % de viés), com ressalvas: critério de mecanismo falhou em 1/12; a janela de aviso próxima do limite é hipótese; **a rigidez** não está fundamentada (Faella não lido, MEF inconclusivo); **o mecanismo 3** não está fundamentado.
10. **O que impede a implementação em C++?** (a) **dados do aço** (`ε_h, E_h, E_u, ε_u`) só de cupom: aplicabilidade prática limitada, sem alternativa verificada; (b) **rigidez inicial não resolvida** (Faella 1998 ausente; MEF depende de dados que faltam); (c) mecanismo 3 indisponível; (d) decisões metodológicas pendentes (janela de aviso do critério; área do parafuso bruta × resistente). Nada disso impede uma implementação **isolada e parcial**.

## 4. Recomendação técnica — **Cenário B (implementação parcial possível)**
Existe fundamentação suficiente para implementar, **de forma isolada**, o `TStubForceDisplacement` apenas para:
- **modelo `Piluso2001`, mecanismos 1 e 2**, com curva de 4 pontos + tabela de ≥ 128 amostras, parâmetros do material **informados com origem** e `Status` explícito;
- rigidez inicial **somente como entrada `UserProvided`** (a fórmula `0,5E b_eff t_f³/m'³` só como "transcrita de [P1]/[P2], fonte original não verificada", se autorizada);
- **mecanismo 3, pós-pico, pré-tensão, dados de aço ausentes e carregamento cíclico: estados "indisponível/fora do domínio"**, sem curva e sem defaults.
**Não** é Cenário A: a rigidez e a aplicabilidade prática (dados do aço) não estão resolvidas. **Não** é Cenário C: o modelo de Piluso para 1 e 2 está reproduzido e comparado a ensaios, e a API/estados que impedem uso indevido estão especificados. A escolha de A/B/C **segue as evidências**, não a conveniência.

## 5. Testes e regressão
- Protótipo Python `research/tstub_piluso`: **14/14**.
- `ctest -C Release`: **35/35**.
- **Nenhum arquivo C++/CMake/golden alterado** nesta fase (`find src CMakeLists.txt -newermt` desde o início da fase: lista vazia). Resultados da Fase B inalterados (`TestTStubStiffness`, `TestTStub`, `TestMomentRotationRegression` passam).
- MEF: verificação de fluxo (`research/tstub_fem/verify_pipeline.py`, balanço analítico) e equilíbrio (`B_q=F_q` sem contato) como controles; **não** são testes automatizados do repositório.

## 6. Limitações e pendências
Limitações: Faella não lido; MEF com geometria assumida, elástico, sem filete e sem pré-tensão; contato por penalidade; ensaios usados só os de [P2] (12). Pendências, em ordem de impacto: (1) obter Faella et al. (1998) e (1999) por via legítima; (2) obter e ler Francavilla et al. (2016) e (2025); (3) repetir o MEF com o espécime real (dimensões completas, filete, arruela, furo) e comparar com rigidez **medida**; (4) decidir (autorização metodológica) a janela de aviso do critério de mecanismo e a área do parafuso; (5) só então autorizar a implementação isolada do Cenário B.

**Arquivos criados nesta fase:** os seis documentos acima, `research/tstub_fem/` (scripts, saídas, 7 modelos `.liml` convergência/geometrias; os demais são regeneráveis por `run_study.py`) e `docs/validation/figures/tstub_fem_stiffness.{svg,png}`. **Alterados:** `TStub_Stiffness_Compatibility.md` (§8) e `TStub_FEM_Elastic_Study.md` (nota de atualização).
