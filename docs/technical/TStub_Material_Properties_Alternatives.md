# Alternativas para os parâmetros do material do modelo de Piluso (Fase C6.2)

O modelo de Piluso et al. (2001) exige `ε_h, E_h, E_u, ε_u` (além de `E, f_y, f_u`), que **só vêm de ensaio de cupom**. Este documento avalia se há alternativa **publicada** que dispense esses dados. **Estado: PARCIALMENTE CONCLUÍDA** — as fontes abertas foram lidas; a fonte com maior potencial (fechamento em forma fechada da capacidade de deformação, Francavilla et al.) **não pôde ser obtida**. Não foram criados valores padrão nem alternativas próprias. Sem validação profissional.

## 1. Fontes efetivamente lidas
Piluso et al. (2001) I e II; Faralli, Tan, McShane, Wrobel (2020), *J. Struct. Eng.* (versão aceita, acesso aberto); Girão Coelho et al. (2004), *J. Constr. Steel Res.* 60; Tartaglia e D'Aniello (2021); Swanson (2002). **Não lidas** (acesso restrito): Francavilla et al. (2016), Francavilla et al. (2025), Latour & Rizzano (2012), Faella et al. (1998, 1999).

## 2. Natureza das propriedades (para não misturar)
| Propriedade | Tipo | Onde vem |
|---|---|---|
| `E` | normativa | 210 000 N/mm² (EN/NBR) |
| `f_y`, `f_u` | normativa (mínimos) / declarada pelo fabricante (certificado) / ensaio | **os valores reais do material diferem dos mínimos** (em [P2]: `f_y` 265–347 contra Fe430 nominal) |
| `ε_h`, `E_h` | **ensaio** (patamar e endurecimento) ou **correlação** | não são propriedades normativas |
| `E_u` | ensaio (ou correlação) | idem |
| `ε_u = \ln(A_0/A_f)` | **ensaio** (redução de área na fratura; RILEM) | **não é** a deformação de alongamento de norma |
| Parâmetros de Faralli et al. (2020), Tab. 5 | calibração própria dos autores; para S450, `σ_y, σ_u` da EN e `E, E_h, ε_y, ε_h, ε_u` **assumidos** pelos autores | **correlação/hipótese de modelagem**, não propriedade |
**Alerta de definição:** em [P2] `ε_u` (verdadeira, por redução de área) vale 0,49–0,97; em Faralli et al. (2020) `ε_f` vale 0,17–0,25 (para S450, retirado da EN 10025-2 pelos autores). São grandezas **de definições diferentes** (a última é uma deformação nominal de alongamento convertida); substituir uma pela outra altera a capacidade de deformação prevista de forma não controlada.

## 3. Alternativas encontradas e classificação
| Alternativa | Propriedades necessárias | Disponibilidade em projeto | Fundamentação | Validação | Aplicabilidade | Limitações | Implementação |
|---|---|---|---|---|---|---|---|
| **A. Piluso et al. 2001** (mecanismos 1 e 2) | `E,f_y,f_u,ε_h,E_h,E_u,ε_u`; `f_ub, A_b`; geometria | **baixa** (4 parâmetros só de cupom) | [P1], [P2] | 12 ensaios (autores); reproduzido aqui: `F_u` média 0,98, `δ_u` média 0,95 | perfis laminados HEA/HEB, Fe430, M20–M27, monotônico, parafusos *snug* | sem mecanismo 3, sem pós-pico, sem rigidez com pré-tensão | moderada (função fechada + inversão numérica) |
| **B1. Mapas de deformação (Faralli et al. 2020)** | `f_y, f_ub, t_p, A_s, m, n, l_eff` (propriedades convencionais) para o **modo de falha**; deslocamento último por MEF | **alta** para o modo | MEF 3D (ABAQUS), plastificação com dano; comparado com ensaios de Piluso, Girão Coelho, Bursi–Jaspart, Ribeiro | demonstrada para o **regime de falha** (não para a curva F–δ) | T-stub de uma fileira, linhas de plastificação "não circulares" no estudo paramétrico; aços S235–S450 | **não fornece lei F–δ nem `δ_u` em forma fechada**; parâmetros do aço da Tab. 5 são calibrações, parte assumida | não aplicável como lei (só classificação de modo) |
| **B2. Francavilla et al. (2016) e (2025)** (forma fechada da capacidade de deformação) | **desconhecidas** (não lidas) | desconhecida | existem publicações | desconhecida | desconhecida | desconhecidas | desconhecida |
| **B3. EN 1993-1-8 (descrição qualitativa)** | `—` | alta | norma: ductilidade "ilimitada" se a flexão da mesa governa; limitada se o parafuso governa (segundo a descrição de Girão Coelho et al. 2004); critérios de rotação em 6.4 | — | só **classificação** | **não é lei local**; não fornece `δ_u` | trivial |
| **B4. Dados experimentais de terceiros** (Girão Coelho 2004: S355/S690 de chapas soldadas; Tartaglia 2021: HV/HR) | — | — | ensaios | servem como **base de validação** de qualquer modelo | tipos de T-stub distintos do de Piluso | não são modelos | — |
Não foi encontrada, **entre as fontes lidas**, formulação publicada que estime `δ_u` a partir apenas de propriedades convencionais. **A existência de B2 não está excluída; apenas não foi verificada.**

## 4. Estratégias
**A — Piluso estrito:** exigir os parâmetros originais com `Origin=Test`. Rigor científico: máximo (modelo reproduzido e comparado a 12 ensaios). Usabilidade: **baixa** (cupom). Aplicabilidade profissional: restrita a projetos/estudos com ensaio de cupom e a pesquisa.
**B — Modelo alternativo publicado:** **sem candidato verificado**. B1 só dá o modo; B2 não foi lido. Rigor: desconhecido. Não se pode propor sem leitura.
**C — Modelos múltiplos com seleção explícita:** estrutura de módulo permite `ModelId` distinto por modelo, com requisitos e limites próprios. Rigor: o de cada modelo. Usabilidade: média (o usuário escolhe, vê requisitos e limitações). É a arquitetura correta **qualquer que seja** o conjunto de modelos futuros.

## 5. Recomendação
**Arquitetura C com conteúdo inicial A:** implementar, quando autorizado, o contêiner com seleção explícita de modelo (`ModelId`) e **apenas** `Piluso2001` com parâmetros `Origin=Test` (ou declarados com aviso). **Nenhum valor padrão e nenhuma substituição de parâmetros por tabelas de grau de aço** (isso seria transformar correlações em propriedades determinísticas e misturar modelos — regras 10 e 11 do prompt). Para que a usabilidade melhore, o caminho é **obter e ler B2** (Francavilla 2016/2025): se fornecerem forma fechada com entradas convencionais e validação, entram como um segundo `ModelId` **independente**, não como estimador dos parâmetros de Piluso. Enquanto B2 não for lido, a lacuna de aplicabilidade prática permanece (**Cenário B** do prompt: implementação parcial).
