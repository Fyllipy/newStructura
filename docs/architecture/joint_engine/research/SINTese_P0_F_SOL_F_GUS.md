# Síntese P0 — F-SOL e F-GUS

Documental; sem alterações de código/contratos. Detalhes e rastreabilidade das fontes em `P0_F_SOL_SOLDAS.md` e `P0_F_GUS_GUSSETS_TRELICAS_TUBULARES.md`. **Limite desta pesquisa:** só duas fontes primárias foram **lidas por inteiro nas páginas citadas** ([NG04] e [CB07], artigos do AISC *Engineering Journal*); as demais (EN 1993-1-8, NBR 8800:2024, CIDECT DG1, AISC DG 29, Neis, Lesik & Kennedy, Butler & Kulak, Whitmore, Thornton) foram **apenas localizadas ou descritas por terceiros** e estão marcadas assim. Nenhuma conclusão abaixo é validação física.

## 1. O que está **documentado**
| Item | Fonte lida |
|---|---|
| Resistência do filete em função do ângulo de carga `V_θ/V_0 = 1,0 + 0,50 sen^1,5 θ` e sua base experimental (faixa 1,28–2,00 da razão transversal/longitudinal) | [NG04] |
| Largura de Whitmore `L_w = 2 L tan30° + s`; tratamento da parte que excede as bordas; pseudo-coluna com `K` entre 0,5 e 1,2 e conservadorismo consistente | [CB07] |

## 2. Comparação entre as famílias
| Aspecto | F-SOL (soldas) | F-GUS (gusset/treliça/tubular) |
|---|---|---|
| Resistência normativa | equações conhecidas (A1) e implementadas em 2 arranjos; método EN só por fonte secundária | Whitmore e flambagem simplificada documentados; tubular só descrito |
| Natureza do método | fórmulas de resistência por ângulo; ICR para grupos | métodos **simplificados** (distribuição de tensões, coluna equivalente) com conservadorismo declarado |
| Rigidez/lei constitutiva | existem na literatura (Neis 1985, Lesik–Kennedy 1990, Butler–Kulak) mas **equações não lidas** | **nenhuma** fundamentada (rigidez de nó tubular: só RHS, discordante) |
| MEF/ensaio | ICR e fora do plano exigem validação | fora do plano, distribuição entre barras e rigidez exigem MEF e ensaios |
| Dado ausente principal | processo/tenacidade/folga/βw; geometria genérica de grupo | **contorno da chapa**; N barras; excentricidades; limites β, γ, τ |

## 3. Componentes mecânicos identificados
- **Solda de filete** → mecanismo de resistência (A1/A4/A9) + interface (`Link{Welded}`); componente deformável **somente com lei fundamentada** (candidatas por ICR; **ainda sem fonte lida**).
- **Chapa de nó (gusset)** → mecanismos: escoamento/ruptura na seção de Whitmore, block shear, flambagem no plano (pseudo-coluna); componente deformável de rigidez: **sem fonte**.
- **Barra–gusset (parafusos/solda)**: resistência; compatibilidade/distribuição: **sem fonte**.
- **Nó tubular**: mecanismos por modos (face, parede, punção); rigidez: **sem fonte para CHS**.

## 4. Modelos disponíveis
**Resistência normativa:** filete (A1), Whitmore (A1 de F-GUS), flambagem de Whitmore com `K` variável. **Rigidez e constitutivos fundamentados:** **nenhum** com equação lida nesta fase. O resultado correto de P0 para F–δ/M–θ de solda e gusset é **"fundamentação disponível apenas em parte; P1 depende de leitura dos textos completos"**.

## 5. Lacunas de dados e de arquitetura
- **Dados do Structura:** processo e tenacidade da solda, abertura de raiz, número de passes, βw/classe do metal-base (EN), geometria genérica do grupo de solda, espessura efetiva de penetração parcial; contorno da chapa de nó, número e ângulos de barras, excentricidades do nó, limites β/γ/τ de tubulares.
- **Arquitetura:** (i) `NodeConnection` tem **uma** barra: gusset multi-barras exige a topologia N-ária (já em J1, derivada parcialmente); (ii) saída `GeneralizedVector` existe em contrato, sem modelo; (iii) nenhum contrato novo é necessário para P0.
- **Divergência registrada (não corrigida):** o código usa `K = 0,65` para a coluna de Whitmore e limita `L_w` à largura da chapa; a fonte lida mostra faixa 0,5–1,2 para `K` e deduz/pondera quando a seção sai da chapa — a conformidade precisa de verificação em P1.

## 6. Casos que exigem MEF ou ensaios
MEF: flambagem fora do plano e calibração de `K` em gussets; distribuição entre barras e excentricidades; ICR e fora do plano em soldas; rigidez de nó tubular. Ensaio: penetração parcial, grupos de solda excêntricos, rigidez de nós tubulares CHS, ligações híbridas (parafuso + solda).

## 7. Dependências entre as famílias
F-GUS usa a **solda** (solda do gusset ao apoio): depende de F-SOL em resistência; F-HIB depende de F-SOL (deformabilidade) e de F-PAR; nós tubulares dependem de F-SOL (comprimento efetivo/garganta de soldas de perfis tubulares); a topologia N-ária (J1) é compartilhada por F-GUS.

## 8. Recomendações para P1
1. **Aquisição e leitura prévia (bloqueante)** de: EN 1993-1-8 (§4.5, §7; edição a fixar), NBR 8800:2024 §6.2/§6.5, CIDECT DG1 (2008), AISC DG 29, Neis (1985) + discussão, Lesik & Kennedy (1990), Butler–Pal–Kulak (1972), Whitmore (1952), Thornton (1984). Se uma fonte não puder ser obtida, o item fica "indisponível".
2. P1 de F-SOL (nível resistência): wrapper `IResistanceMechanism` do filete (A1/A9) sobre `SupportWeld`, **sem** lei constitutiva; documentar lacunas (A6–A8).
3. P1 de F-GUS (nível resistência): wrapper de Whitmore/block shear/flambagem sobre `TrussConnections`, com **K e comprimentos registrados como parâmetros do modelo com origem**, e verificação da divergência acima.
4. Lei constitutiva de solda (B1/B2): **só após** a leitura completa e escolha explícita; nunca presumir solda rígida.
5. Nenhuma ação de implementação sem nova autorização.
