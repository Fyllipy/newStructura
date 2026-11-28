# Investigação do mecanismo 3 (ruptura dos parafusos) — Fase C6.4

**Conclusão: o mecanismo 3 permanece explicitamente INDISPONÍVEL.** Nenhuma fonte *lida integralmente* fornece uma lei força-deslocamento para ele. **Estado da atividade: PARCIALMENTE CONCLUÍDA** (fontes abertas lidas; a fonte mais promissora não pôde ser obtida). Sem validação profissional.

## 1. Fontes lidas (texto integral, acesso aberto) e o que dizem sobre o mecanismo 3
| Fonte | Conteúdo relevante (somente o que foi lido) | Dá lei F–δ para o mec. 3? |
|---|---|---|
| Piluso, Faella, Rizzano (2001) I e II (arquivos fornecidos) | `F_{u,3}=2B_u` ([P1 (13)]); a curva "poderia ser avaliada por procedimento análogo (Faella et al. 1999)", mas **a precisão é afetada pela flexão dos parafusos, que não é considerada**; modo frágil, a evitar. [P2]: `F_{u,3}=2B_u` é "aproximação grosseira"; a flexão dos parafusos aumenta com a carga e pode causar ruptura prematura, afetando `β_{u,lim}`. | **Não** |
| Faralli, Tan, McShane, Wrobel (2020), *Deformation maps for bolted T-stubs*, J. Struct. Eng. (versão aceita, UCL Discovery) | MEF 3D (ABAQUS) com dano nos parafusos; mapas de regimes de falha em `t_p²/A_s` × `f_y/f_ub`; estudo paramétrico: a capacidade de deformação `Δ_u/t_p` **diminui** quando o modo passa de 1 para 3; "só uma combinação limitada de `t_p²/A_s` e `f_y/f_ub` causa modo 3". Avalia Piluso (2001) e Francavilla (2016) contra o MEF: ambos **subestimam** `Δ_u` no modo 1 (hipótese de linhas de plastificação de viga) e são precisos só para pequenos deslocamentos. | **Não** (resultados de MEF e mapas, não uma lei F–δ do modo 3) |
| Tartaglia e D'Aniello (2021), COMPDYN, *Behaviour of T-stub under mode 2 and 3 with HV and HR bolts* | 4 ensaios; parafusos HV (ruptura por arrancamento da porca) e HR (estricção da haste) têm comportamento de tração diferente; **no modo 2 o tipo de parafuso não alterou a resposta**; para mesas muito espessas (resistência do modo 3 ≈ 3× a do modo 1 ou mais) o tipo de parafuso influencia a resistência residual. | **Não** (ensaios; sem modelo) |
| Swanson (2002), *Ultimate strength prying models for bolted T-stub connections*, Eng. J. AISC | modelos de resistência última com alavanca (Struik–de Back e outros), 21 ensaios de componente do SAC; **só resistência**. | **Não** |
| Girão Coelho et al. (2004), *J. Constr. Steel Res.* 60, 269–311 | ensaios de T-stubs de chapas soldadas, quase todos projetados para o modo 1; descreve que a EN dá apenas princípios qualitativos: ductilidade ilimitada se a flexão governa; limitada se o parafuso governa. Inclui rigidez inicial experimental × EN (Tabelas 8–9). | **Não** |

## 2. Fontes localizadas e NÃO obtidas
- Francavilla, Latour, Piluso, Rizzano (2016), *Bolted T-stubs: a refined model for flange and bolt fracture modes*, Steel Compos. Struct. 20(2), 267–293, DOI 10.12989/scs.2016.20.2.267: o **título indica** tratamento explícito de fratura dos parafusos; a página aberta redireciona para acesso restrito (HTTP 302 → "access required"). **Não lido.**
- Francavilla et al. (2025), *Closed form prediction of the deformation capacity of bolted T-stubs*, ce/papers 8(6), DOI 10.1002/cepa.70183 (CC-BY): HTTP 403 ao acesso automatizado; um segundo meio de acesso foi bloqueado pela política de permissões desta sessão e **não foi insistido**. **Não lido.**
- Faella, Piluso, Rizzano (1999), *Structural Steel Semirigid Connections*, CRC (citado por [P1] para o modo 3): livro, fora das fontes abertas. **Não obtido.**
- Latour & Rizzano (2012), J. Struct. Eng. 138(2), 170–182; Zhu et al. (modos adicionais por arrancamento da porca, citado em Tartaglia 2021); Dinu et al. (2025); Sebbagh et al. (2020): **não lidos**.
Nenhuma citação secundária foi usada no lugar delas.

## 3. O mecanismo pode ser representado por formulação independente?
Pelo que foi lido, o comportamento de um T-stub governado pelos parafusos depende de **três elementos que o modelo de Piluso não contém**: (i) a lei força-alongamento do parafuso até a ruptura (HV: arrancamento da porca; HR: estricção da haste — comportamentos de tração distintos segundo Tartaglia & D'Aniello); (ii) a flexão do parafuso e a interação tração–flexão (reconhecida por [P1]/[P2] como não considerada); (iii) a distribuição da alavanca com a rigidez da mesa. Portanto a curva do modo 3 **não pode ser obtida de `F_{u,3}=2B_u`** (resistência apenas) e exigiria **uma abordagem diferente** (modelo de parafuso + mesa), ainda não identificada em fonte lida. Não foi criada nenhuma formulação.

## 4. Representação adotada para o futuro
`Mechanism::Three` → `Status::MechanismUnsupported`, sem curva nem pontos; `diagnostics` cita as lacunas acima; o resultado indica `F_{u,3}=2B_u` apenas como **referência de resistência do modelo** (aproximação grosseira conforme [P2]), nunca como ponto de uma curva.

## 5. Como resolver
1. Obter e ler Francavilla et al. (2016) e (2025) por via legítima (biblioteca/assinatura ou envio dos PDFs). 2. Obter dados de tração de parafusos HV/HR (D'Aniello et al., citado por Tartaglia 2021) para a lei do parafuso. 3. Só então reavaliar o cenário. Até lá, o mecanismo 3 continua indisponível.
