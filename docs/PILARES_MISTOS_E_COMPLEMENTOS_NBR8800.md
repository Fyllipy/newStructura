# Pilares mistos (Anexo M) e complementos da NBR 8800:2024

Documento complementar a `LIVRO_DIMENSIONAMENTO_PERFIS_LAMINADOS.md`. Unidades: mm, N, MPa. Código: `src/design/CompositeColumn.*`, `src/design/BeamExtras.*`,
`src/design/PinConnection.*`, `src/design/TaperedMember.*`, `src/app/services/IntegrityService.*`.

## 1. Pilares mistos de aço e concreto (Anexo M, método simplificado)

### 1.1 Seções e parâmetros
Seção do pilar escolhida na caixa "Pilar misto (Anexo M)" dos parâmetros da barra; o perfil de aço é o da própria seção da barra:

| `mixType` | Seção | Perfil de aço exigido |
|---|---|---|
| 1 | I/H totalmente revestido (M.1-a) | I/H (família W) |
| 2 | I/H parcialmente revestido (M.1-b) | I/H |
| 3 | tubular preenchido (M.1-c, M.1-d) | tubular retangular/quadrado ou circular |

Dados: `hc`, `bc` (concreto; em branco = dimensões do perfil), `fck`, `fyk` da armadura, área e número de barras, distância do centro das barras à face, `αE` do agregado,
modelo de cálculo (1, 2 ou 3). Nos revestidos a armadura forma um anel retangular (`mixBarsH` barras em cada face horizontal, com os cantos, e `mixBarsV` intermediárias
em cada face lateral); no tubo circular são `mixBarsH` barras numa circunferência. No tubo retangular o raio interno adotado é `ri = t` (a biblioteca não informa o raio de dobra).

### 1.2 Propriedades (M.3.4, M.4)
* `Ec = (0,8 + 0,2 fck/80) Eci` (≤ Eci), `Eci = αE 5600 √fck` (fck ≤ 50 MPa) ou `21,5·10³ αE (fck/10 + 1,25)^(1/3)`.
* `(EI)e = Ea Ia + αc Ec Ic + Es Is`, `(EA)e = Ea Aa + Ec Ac + Es As`; `αc = 0,45 + 3 (Aa+As)/(Ac+Aa+As) ≤ 0,9` (preenchidos), `0,60` (parcialmente revestidos),
  `0,25 + 3 (Aa+As)/(Ac+Aa+As) ≤ 0,7` (totalmente revestidos).
* `Npℓ,Rd = Aa fyd + Ac fcd1 + As fsd` (revestidos) ou `... + As fcd1 Es/Ec` (preenchidos); `fcd1 = α ηc fcd`, `α = 0,95` (tubo circular) ou `0,85`; `ηc = (40/fck)^(1/3) ≤ 1` (revestidos), 1,0 (preenchidos).
* `δ = Aa fyd/Npℓ,Rd` entre 0,1 e 0,9 (M.1.3-d).
* Tubulares: classificação pela Tabela M.1 (retangular `b/t`: 2,26 √(E/fy), 3,00 √(E/fy), limite 5,00 √(E/fy); circular `D/t`: 0,15, 0,19 e 0,31 E/fy). Semicompacta:
  `Np,Rd = Npℓ,Rd − (Npℓ,Rd − Ny,Rd)(λ−λp)²/(λr−λp)²`; esbelta: `Np,Rd = σcr,Rd Aa + fcd2 (Ac + As Es/Ec)`, `fcd2 = 0,7 fcd`.

### 1.3 Resistência axial (M.3)
`NRd = χ Np,Rd`, χ da curva de 5.3.3 com `λ0,m = √(Np,R/Ne)`, `Ne = π² (EI)e/L²` (um por plano, com Lx e Ly), `Np,R` com resistências características. Para `λ0,m > (90/π)√(Np,R/(EA)e)`
somam-se a retração e a fluência: `ecc = (MSd/NSd + L/300)(e^m − 1)`, `m = 1,08/(Ne/NSd − 0,45)` (revestidos) ou `0,27/(Ne/NSd − 0,45)` (preenchidos), `Mcc = NSd ecc`.

### 1.4 Momento resistente (M.5.5)
`Mpℓ,Rd = fyd (Za − Zan) + 0,5 fcd1 (Zc − Zcn) + fsd (Zs − Zsn)`, `Mmax,pℓ,Rd = fyd Za + 0,5 fcd1 Zc + fsd Zs`. A linha neutra plástica `hn` vem de M.5.5.3 (I/H revestidos, eixos x e y, três
posições: na alma, na mesa, fora do perfil) ou M.5.5.4 (tubulares; circular com a formulação do retangular, `b → D`, `r → D/2 − t`). `Asn` e `Zsn` (barras na faixa `2 hn`) são obtidos por
iteração porque dependem de `hn`. Tubulares semicompactos: `MRd = Mpℓ,Rd − (Mpℓ,Rd − Me,Rd)(λ−λp)/(λr−λp)`; esbeltos: `Mcr,Rd`, com as distribuições de tensões das figuras M.4 e M.5
integradas por fibras (600 faixas) com o concreto triangular `fcd2`.

### 1.5 Interação (M.5)
* **Modelo I (M.5.2):** interação de 5.5.1.2 com `NRd` de M.3 e `Mx,Rd`, `My,Rd` de M.5.5.1; momentos de primeira ordem multiplicados por `B1 = Cm/(1 − NSd/(0,64 Ne)) ≥ 1` e por `B2`.
* **Modelo II (M.5.3):** só seções compactas. `Mc = 0,9 Mpℓ,Rd` (fy ≤ 350 MPa) ou `0,8` (350 < fy ≤ 450); `Md = 0,8 Mmax,pℓ,Rd` (nunca menor que `Mc`); `μ` pelos três intervalos de `NSd/Npℓ,c,Rd`;
  momentos totais `Mtot = MSd + NSd L/[300 (1 − NSd/Ne2)]`, imperfeição em um eixo por vez (a mais desfavorável); `Mx,tot/(μx Mc,x) + My,tot/(μy Mc,y) ≤ 1`.
* **Modelo III (M.5.4):** tubulares preenchidos; `csr = (Aa fy + As fys)/(Ac fck)`, `cp` e `cm` das expressões da norma (retangular e circular).

### 1.6 Demais verificações
* Limites M.1.3 e M.1.4 (id `cc_limits`): 1 % de aço, `δ`, `λ0,m ≤ 2,0`, armadura mínima 0,4 % nos revestidos, relação altura/largura 0,2 a 5,0, cobrimentos `max(bf/6; 40 mm)`, compacidade.
* Cortante: só o perfil de aço, `0,60 Aw fyd`.
* M.2 (id `cc_interface`): tubulares com `τRd` da Tabela M.2 (ou da alternativa M.2.1.8, a maior); revestidos têm `τRd = 0` e exigem conectores. `Vℓ,Sd = VSd (1 − Npℓ,a,Rd/Npℓ,Rd)`, comprimento de introdução = 2 × menor dimensão.

### 1.7 Limitações declaradas
Conectores de cisalhamento (número, L.4.2.1) e estribos não são dimensionados; M.1.3-i (concretagem com o pilar montado) é só lembrete; `τSd` usa tensão média no perímetro de contato (a norma pede propriedades
elásticas com o concreto não fissurado); `γc = 1,4` e `γs = 1,15` fixos; tração só com o perfil de aço. Não há validação cruzada com outro programa; o teste `compositeColumnFollowsAnnexM` confere Npℓ,Rd, `αc`, `(EI)e`, `hn` e `Mpℓ,Rd`
do tubo circular por cálculo à mão e as relações de consistência do pilar revestido.

## 2. Itens complementares implementados

| Item | Resumo |
|---|---|
| 5.2.6, 6.4, 6.6.2 | Chapa com furo e pino: escoamento `Ag fy/γa1`, pressão de contato `1,8 dp t fy/γa2`, ruptura `2 t bef fu/γa2` (`bef = mín(2t+16; (b−dh)/2)`), cisalhamento `0,60 Asf fu/γa2`; `b ≥ 2 bef + dp`, `a ≥ 1,33 bef`; pino: `MRd = 1,2 W fy/γa1`, `0,60·0,75 Ag fy/γa1`, pressão `1,5 t d fy/γa1`. |
| 5.2.7 | Barra redonda rosqueada: ruptura `0,75 Ag fu/γa2`. |
| 5.4.4 | Lamela: comprimento adicional `aℓ` (b, 1,5 b ou 2 b) e solda de ligação. |
| 5.4.5 | Solda mesa–alma: `Vs Qs/Ix` (+ força na mesa) contra `0,60 fw Aw/γw2`; perna mínima (Tabela 10). |
| 5.7.6 | Alma sob par de forças: `FRd = 24 tw³ √(E fy)/(γa1 h)`, `h = d − 2k`, metade na extremidade do pilar. |
| 4.11.5 | `Lpd = [0,12 + 0,076 M1/M2] (E/fy) ry` em seções I. 4.11.6 e 4.11.7 **não** implementados. |
| 4.13 | Integridade: força de tração de amarração `máx(1 % NSd,pilar; 75 kN)` (45 kN em pilares de borda sem laje), reação vertical em vigas e emendas de pilares; 4.13.4 informativo. |
| H.5 e H.6 | Fadiga: parafuso (categoria G, `Abe = π/4 (db − 0,9382 p)²`), material (Tabela H.1) e requisitos de fabricação. |
| Anexo G | Barras de seção I de alma variável: solver subdivide em trechos prismáticos; verificação seção a seção; inércia equivalente por Rayleigh–Ritz; Cb = 1,0. |
| Inclinação de ligação | Altura, mesa e raio da viga projetados na face do apoio por `1/cos α` (α em 0,5° a 85°); desenhos e sólidos 3D continuam ortogonais. |
