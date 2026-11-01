# Verificação em situação de incêndio (ABNT NBR 14323:2013)

Implementada em `src/design/Nbr14323.{h,cpp}`; integrada ao dimensionamento por `DesignService::verifyMember`.
Método simplificado da seção 8 (aço, temperatura uniforme na seção, incêndio-padrão).

## Como usar

1. **Parâmetros de cálculo** (Dimensionamento > Parâmetros): grupo *Situação de incêndio*, caixa
   *Verificar a estrutura em situação de incêndio* e o TRRF (min, NBR 14432). Persiste em `[DESIGN_SETTINGS]`
   (`FIRE_CHECK`, `FIRE_TRRF`).
2. **Esforços**: em *Casos e combinações > Gerar*, marque *Incêndio (NBR 14323, 6.3)*. Geram-se `FOGO1…`:
   - `FOGO1`: γg·G + ψ·Q, com γg da Tabela 3 (aço 1,10; pré-moldado 1,15; demais 1,15–1,30) e ψ = 0,21 (uso geral),
     0,28 (comercial/acesso público) ou 0,42 (bibliotecas, depósitos, garagens) conforme o tipo da sobrecarga;
   - uma `FOGO*` por vento: G + 0,20·W (6.3.2); permanentes favoráveis com 1,0.
   - Sem combinações `FOGO*`, a verificação usa **70 % dos esforços a frio** (8.2.3) das combinações selecionadas
     (conservador: inclui as combinações com vento, que a norma exclui).
   - `FOGO*` não entram na verificação a frio nem recebem variantes de forças nocionais.
3. **Proteção térmica por barra** (parâmetros da barra): sem revestimento, tipo contorno ou tipo caixa
   (espessura, condutividade, massa específica e calor específico), e exposição (4 lados ou 3 lados com laje).
4. O aproveitamento de incêndio (`fire_*`) entra em `maxRatio`/η, portanto também no **dimensionamento automático**
   (`chooseBest`): perfis mais massivos (menor u/Ag) ou com revestimento passam a ser escolhidos quando o incêndio governa.

## O que é calculado

| Item | Norma |
|---|---|
| ky, kE, kσ(θ), interpolação linear | 5.1.1.2, Tabelas 1 e 2 |
| Curva do incêndio-padrão θg = 20 + 345 log10(8t + 1) | NBR 14432 / ISO 834 |
| Temperatura do aço sem revestimento (ksh, u/Ag ≥ 10 m⁻¹, Δt = 5 s) | 8.5.1.1, Anexo E.3 (ca) |
| Temperatura do aço com revestimento (contorno/caixa) | 8.5.1.2 |
| Tração (escoamento da seção bruta; ruptura da seção líquida dispensada) | 8.3.1, 8.4.1 |
| Compressão: χfi, λ0,fi = λ0/0,85, α = 0,022 √(E/fy); flambagem local (b/t > 0,85 do limite) com kσ·Q·A | 8.4.2 |
| Flexão: Mpl·ky·κ, My·ky, My·kσ (λp,fi = 0,85 λp, λr,fi = 0,85 λr); FLT com χfi; alma esbelta (4,84√(E/fy)) com kσ fy e kE E | 8.4.3.2 |
| Cortante: limites × 0,85, E × kE, fy × ky (ou kσ na flambagem elástica) | 8.4.3.4 |
| Interação N + M: 8/9 (N/NRd ≥ 0,2) ou N/2NRd; formados a frio: soma linear | 8.4.4.1, 8.4.4.2 |
| κ = 1,40 (sem revestimento) ou 1,15 (com), viga com laje no 4º lado | 8.4.3.2.4 |

Efeito P-δ local: B1 com a rigidez a frio e B2 da combinação (8.2.1). Resistências com coeficiente de ponderação unitário (6.4).

## Aproximações e limites (declarados)

- Formados a frio (NBR 14762): a resistência de incêndio parte da resistência a frio com γ = 1 (seção efetiva) multiplicada por
  ky ou kσ (conforme λp ≤ 0,85 λp,lim); na FLT usa-se min(ky|kσ, kE) no lugar do χfi (Me não é exposto) — conservador.
- Flexão de seções que não são I/U com FLT: mesma redução min(kσ, kE) sobre o valor a frio.
- Cortante: a região (plástica/elástica) é detectada derivando a resistência a frio em relação a E.
- Não cobertos: torção (aviso), ligações (8.4.5, 8.5.4), barras mistas, de seção variável, treliçadas e pilares mistos,
  curva de incêndio natural, análise avançada (seção 10), distribuição não uniforme de temperatura.
- A massividade usa o contorno externo da seção; seções tubulares e caixão são tratadas como totalmente expostas pelo exterior.
- Tinta intumescente (8.5.1.2.6) não se aplica ao cálculo de revestimento: informe propriedades equivalentes de ensaio.

## Testes

`TestDesign`: `fireSteelPropertiesAndTemperatureFollowNbr14323`, `fireBarChecksFollowNbr14323`,
`fireCheckEntersDesignAndProfileChoice`, `fireCombinationsAndPersistence`; `TestProjectIoAndSolver` (round-trip do `.dat`).
