# Reconstrução do modelo de Piluso, Faella e Rizzano (2001) para T-stubs aparafusados

Fase C1. Documento de **reprodução de um modelo publicado**; não é norma, não é implementação no Structura e **não declara validação profissional**. Nenhuma equação foi inventada: cada uma traz a fonte `[P1 n]` / `[P2 n]` (equação *n* do artigo). Quando um símbolo da extração de texto estava ilegível, a equação foi conferida **na imagem da página**; quando há interpretação, ela está marcada **[INTERPRETAÇÃO]**.

Código de reprodução independente (Python puro, sem Structura): `research/tstub_piluso/piluso_tstub.py`. Saídas: `research/tstub_piluso/saida_validate_published.txt`.

## 1. Referências
- **[P1]** Piluso, V.; Faella, C.; Rizzano, G. (2001). *Ultimate behavior of bolted T-stubs. I: Theoretical model.* J. Struct. Eng. ASCE 127(6), 686–693. DOI 10.1061/(ASCE)0733-9445(2001)127:6(686). (PDF lido integralmente: `docs/references/ASCE_2001_partI.pdf`; extração de texto + leitura das imagens das 8 páginas.)
- **[P2]** Piluso, V.; Faella, C.; Rizzano, G. (2001). *Ultimate behavior of bolted T-stubs. II: Model validation.* J. Struct. Eng. ASCE 127(6), 694–704. DOI 10.1061/(ASCE)0733-9445(2001)127:6(694). (PDF lido integralmente: `docs/references/ASCE-2001-Model-Validation.pdf`; inclui o exemplo trabalhado do espécime 7.)
- Citadas por [P1]/[P2] e **não consultadas** (lacunas, ver §9): Faella, Piluso, Rizzano (1998), *Experimental analysis of bolted connections: snug versus preloaded bolts*, J. Struct. Eng. 124(7), 765–774 (origem de `b_eff`, da rigidez `K` e de `m'`); Faella, Piluso, Rizzano (1999), *Structural steel semirigid connections*, CRC (mecanismo 3); CEN (1997), Eurocode 3 Annex J (draft).
- A revisão da MDPI (*Appl. Sci.* 2021, 11(22), 10731) e as demais fontes complementares **não puderam ser obtidas** (HTTP 403); não foram usadas.

## 2. Hipóteses fundamentais do modelo (literal de [P1], p. 687)
1. Modelo **2D**; efeitos 3D não considerados.
2. **Sem não linearidade geométrica** (busca de soluções fechadas).
3. **Compatibilidade entre deslocamento do parafuso e da mesa não considerada.**
4. Influência do cortante no comportamento plástico desconsiderada (a interação momento–cortante reduziria a resistência plástica em ≈ 14 % para m/tf = 2, diminuindo com m/tf; [P1] cita Faella et al. 1999).
5. Forças de alavanca *Q* **concentradas nas bordas** da mesa.
6. **Flexão dos parafusos desprezada.**
7. Ruptura do material aproximada: ocorre quando a deformação natural última `ε_u` é atingida nas fibras extremas das mesas.
8. Rótulas plásticas conforme o Eurocode 3: na ligação mesa–alma e/ou no eixo do parafuso. A ação do parafuso é uniformemente distribuída sobre `d_w` (arruela, porca ou cabeça, conforme o caso).
9. A espessura inicial da mesa é usada (sem estricção na flexão); lei material **verdadeira (tensão verdadeira × deformação natural)**, ν = 0,5 em grandes deformações.
10. O ponto de momento nulo ao longo da mesa permanece fixo durante o carregamento e igual ao da condição última ([P1] p. 690; [P2] p. 698 observa que na prática ele migra, mais no mecanismo 2).

## 3. Nomenclatura (de [P1]/[P2])
| Símbolo | Significado | Unidade |
|---|---|---|
| `t_f`, `b_eff` | espessura e largura efetiva da mesa | mm |
| `m` | distância entre as seções críticas onde se formam as rótulas (eixo do parafuso à ligação mesa–alma, já descontado `0,8 r`) | mm |
| `n` | distância entre a força de alavanca *Q* (borda) e o eixo do parafuso | mm |
| `λ = n/m` | — | — |
| `d_w` | diâmetro da arruela/porca/cabeça | mm |
| `B_u` | resistência última axial de **um** parafuso | N |
| `β_u = 2M_u/(B_u m)` | parâmetro que governa o mecanismo | — |
| `ψ` | razão entre o momento no eixo do parafuso e `M_u` quando a ligação mesa–alma atinge `M_u` (mecanismo 2) | — |
| `ε, ε_n` | deformação verdadeira (natural), nominal | — |
| `f_y,f_u,E,E_h,E_u` | tensão de escoamento/última e módulos da lei multilinear | N/mm² |
| `ε_y,ε_h,ε_m,ε_u` | deformações em escoamento, início do endurecimento, carga máxima (`f_u`) e última | — |
| `χ=2ε/t_f` | curvatura; `χ̄=χ/χ_y` normalizada | mm⁻¹ |
| `M_y,M_h,M_m,M_u` | momentos fletores da mesa nos 4 pontos | N·mm |
| `ξ=M/M_u` | momento adimensional | — |
| `θ_p`, `δ_p` | rotação e deslocamento plásticos | rad, mm |
| `K`, `K_i` | rigidez axial de um elemento T; rigidez inicial do T-stub | N/mm |
| `L_b`, `A_b` | comprimento convencional e área do parafuso | mm, mm² |

## 4. Lei do material e relação momento–curvatura
Lei multilinear verdadeira (Fig. 2 de [P1]): elástica `E` até `f_y`; patamar até `ε_h`; trecho `E_h` até `f_u` em `ε_m`; trecho `E_u` até `ε_u` (fratura). `ε_y=f_y/E`; `ε_m=ε_h+(f_u−f_y)/E_h` **[P2, apêndice, p. 702]**. Relações de [P1 (1)–(3)] entre grandezas verdadeiras e nominais: `σ=σ_n(1+ε_n)`, `ε=\ln(1+ε_n)`, `ε_u=\ln(A_0/A_f)` (RILEM).

Seção retangular `b_eff × t_f` ([P1 (4)–(8)]), com `M_y = b_eff t_f² f_y/6`, `χ_y=2ε_y/t_f`:

$$\frac{M}{M_y}=\bar\chi \quad(\bar\chi\le1)\qquad [P1\,(5)]$$
$$\frac{M}{M_y}=\tfrac12\Big[3-\bar\chi^{-2}\Big]\quad(1<\bar\chi\le\bar\chi_h)\qquad [P1\,(6)]$$
$$\frac{M}{M_y}=\tfrac12\Big[3-\bar\chi^{-2}\Big]+\tfrac12\frac{E_h}{E}(\bar\chi-\bar\chi_h)\Big(1-\frac{\bar\chi_h}{\bar\chi}\Big)\Big(2+\frac{\bar\chi_h}{\bar\chi}\Big)\quad(\bar\chi_h<\bar\chi\le\bar\chi_m)\qquad [P1\,(7)]$$
$$\frac{M}{M_y}=[\text{(7)}]-\tfrac12\frac{E_h-E_u}{E}(\bar\chi-\bar\chi_m)\Big(1-\frac{\bar\chi_m}{\bar\chi}\Big)\Big(2+\frac{\bar\chi_m}{\bar\chi}\Big)\quad(\bar\chi_m<\bar\chi\le\bar\chi_u)\qquad [P1\,(8)]$$

`M_h, M_m, M_u` resultam de (6), (7), (8) com `χ̄=χ̄_h, χ̄_m, χ̄_u`. As razões `M_h/M_y, M_m/M_y, M_u/M_y` dependem só do material ([P1] p. 688; `M_u` também em [P2 (12)]).

*Nota de leitura:* no apêndice de [P2] (p. 702) a Eq. de `M_u/M_y` mostra o símbolo `E_m` no último termo; [P1 (8)] e [P2 (12)] trazem `E_u`. Adotou-se `E_u` (a reprodução numérica dá `M_u/M_y = 3,7466`, igual ao publicado).

## 5. Mecanismos de falha, resistências e limites
$$\beta_u=\frac{2M_u}{B_u\,m}\qquad [P1\,(9)]$$
- **Tipo 1** (plastificação da mesa; 4 rótulas: 2 no eixo do parafuso, 2 na ligação mesa–alma; ação do parafuso espalhada em `d_w`) — ocorre para `β_u ≤ β_{u,lim}`:
$$F_{u,1}=\frac{(32n-2d_w)M_u}{8mn-(m+n)d_w}\qquad[P1\,(10);\ P2\,(1)\text{–}(5)\text{ dá a dedução}]$$
$$\beta_{u,lim}=\frac{2\lambda}{1+2\lambda}\Big[1-(1+\lambda)\frac{d_w}{8n}\Big]\qquad[P1\,(11)]$$
- **Tipo 2** (plastificação da mesa com ruptura do parafuso; 2 rótulas na ligação mesa–alma) — `β_{u,lim}<β_u\le2`:
$$F_{u,2}=\frac{2M_u+2B_u n}{m+n}\qquad[P1\,(12);\ P2\,(8)]$$
- **Tipo 3** (ruptura do parafuso) — `β_u>2`: `F_{u,3}=2B_u`  **[P1 (13); P2 (9)]**.

As Eqs. (10), (12), (13) são formalmente as do Eurocode 3 (Tab. 6.2, método 2) aplicadas às condições *últimas* e não às de cálculo ([P1] p. 688). Equilíbrio de [P2 (1)–(7)]: tipo 1: `M_1=Qn−q d_w²/8=M_u`, `M_2=q d_w m+Q(m+n)=M_u`, `F_{u,1}=2B+2Q`; tipo 2: `M_2=B_u m−Q(m+n)=M_u`, `F_{u,2}=2B_u−2Q`.

**Mecanismo 3:** [P1] diz que a curva poderia ser obtida por procedimento análogo (Faella et al. 1999), mas **não fornece as equações**; adverte que a precisão é afetada pela flexão dos parafusos (não considerada) e que o modo é frágil e a evitar. [P2] repete que `F_{u,3}=2B_u` é aproximação grosseira. **A curva do mecanismo 3 não pode ser reconstruída com estas fontes.**

## 6. Cinemática e rotações plásticas
Deslocamento plástico de **um** elemento T ([P1] Fig. 3):
$$\delta_p=\theta_{p}\,m\quad(\text{tipos 1 e 3})\qquad[P1\,(14)]$$
$$\delta_p=\theta_{p1}\,m+(\theta_{p1}-\theta_{p2})\,n\quad(\text{tipo 2, }\theta_{p1}>\theta_{p2}\ \text{e}\ \theta_{p2}\ge0;\ \text{senão }\delta_p=\theta_{p1}m)\qquad[P1\,(15)]$$
*[INTERPRETAÇÃO]*: na condição de (15) o glifo impresso é "θ_p2 ↑ 0"; foi lido como `θ_p2 ≥ 0`. A reprodução dos valores publicados de `δ_u` dos espécimes 1, 2, 3 e 6 (tipo 2) a < 0,01 % sustenta a leitura.

Balanço de comprimento `L` com momento `ξM_u` no engaste e zero na ponta (Fig. 4): `θ=∫χ dz` [P1 (18)]; `θ_p = θ−θ_y`, `θ_y=χ_yL/2` [P1 (22)]. Quatro casos, com
$$\xi_1=\frac{M_y}{M_u},\quad \xi_2=\frac{M_h}{M_u}\ [P1\,(17)],\quad \xi_3=\frac{M_m}{M_u}\ [P1\,(25)]:$$
Caso 1 (`ξ≤ξ_1`): `θ_p=0` [(16)]. Casos 2–4: `θ_p=(L/t_f)\,D(\xi)`, `(L/t_f)F(\xi)`, `(L/t_f)G(\xi)` para `ξ_1<ξ≤ξ_2`, `ξ_2<ξ≤ξ_3`, `ξ_3<ξ≤1`:
$$D(\xi)=\varepsilon_y\Big[2\bar\chi_\xi-\frac{1}{\xi}\frac{M_y}{M_u}\Big(3\bar\chi_\xi+\frac1{\bar\chi_\xi}-3\Big)-1\Big]\qquad[P1\,(23),(24)]$$
$$F(\xi)=\varepsilon_y\Big[2\bar\chi_\xi-\frac{1}{\xi}\frac{M_y}{M_u}\Big(3\bar\chi_\xi+\frac1{\bar\chi_\xi}-3+\frac{E_h}{E}\frac{(\bar\chi_\xi-\bar\chi_h)^3}{\bar\chi_\xi}\Big)-1\Big]\qquad[P1\,(28),(29)]$$
$$G(\xi)=\varepsilon_y\Big[2\bar\chi_\xi-\frac{1}{\xi}\frac{M_y}{M_u}\Big(3\bar\chi_\xi+\frac1{\bar\chi_\xi}-3+\frac{E_h}{E}G_h+\frac{E_u}{E}G_u\Big)-1\Big]\qquad[P1\,(33),(34)]$$
$$G_h=\frac{\bar\chi_m^3}{\bar\chi_\xi}+3\bar\chi_m\bar\chi_\xi-3\bar\chi_m^2+3\bar\chi_h^2-3\bar\chi_h\bar\chi_\xi-\frac{\bar\chi_h^3}{\bar\chi_\xi},\qquad G_u=\bar\chi_\xi^2+3\bar\chi_m^2-3\bar\chi_m\bar\chi_\xi-\frac{\bar\chi_m^3}{\bar\chi_\xi}\qquad[P1\,(35),(36)]$$
`χ̄_ξ` é a curvatura normalizada correspondente a `M=ξM_u`: obtida por **inversão numérica** de (5)–(8) — "a única parte numérica da abordagem" ([P1] p. 690). Constantes dependentes só do material: `D(ξ_2)`, `F(ξ_3)`, `C=G(1)`.

*Observação:* na extração de texto o símbolo `ε_y` que multiplica as chaves de (24), (29), (34) estava ilegível (aparece como "y"); identificado por `θ_p=χ_yL/2[\dots]=(L/t_f)\,ε_y[\dots]` e confirmado numericamente (valores do exemplo trabalhado reproduzidos a < 0,02 %).

## 7. Pontos característicos e curva (`F`–`δ` do T-stub completo: dois elementos T)
Quatro pontos ([P1] Fig. 5): `(F_y,δ_y)` primeiro escoamento (`M=M_y`); `(F_h,δ_h)` início do endurecimento (`M_h`); `(F_m,δ_m)` carga máxima do material, `f_u` (`M_m`); `(F_u,δ_u)` fratura (`M_u`). `M` é o momento na ligação mesa–alma.

**Tipo 1** — forças: `F=(32n−2d_w)M/[8mn−(m+n)d_w]` com `M=M_y,M_h,M_m,M_u` **[P1 (37)]**. Rigidez do elemento T para parafusos apenas ajustados (*snug*): 
$$K=0{,}5\,E\,\frac{b_{eff}t_f^3}{m^3}\qquad[P1\,(38)]$$
e, no apêndice de [P2] (p. 702), com `m'` no lugar de `m`: `ζ=0{,}16\,(m+0{,}8r)/t_f-0{,}08`, `m'=m+0{,}8r-ζ r`, `K=0{,}5E\,b_{eff}t_f^3/m'^3` (referido a Faella et al. 1998; *[INTERPRETAÇÃO]*: `r` = raio de concordância do perfil laminado, ou a garganta da solda no espécime 12). `b_eff=\min[d_h+2m;\,b]` (apêndice de [P2]). Deslocamento no escoamento:
$$\delta_y=\frac{2F_y}{K}+\delta_{b,y}\ [P1\,(39)],\qquad \delta_{b,y}=\frac{B_y}{E A_b/L_b}=\frac{F_y/2+M_y/n}{E A_b/L_b}\ [P1\,(40)],\qquad K_i=\frac{F_y}{\delta_y}\ [P1\,(41)]$$
`L_b` = soma das espessuras das chapas ligadas + metade da soma das espessuras de cabeça e porca; no espécime 7: `L_b=2t_f+(t_h+t_n)/2=49,98 mm` e `A_b=π d_b²/4=572,5553 mm²` (valor do exemplo; ver §9). Demais pontos:
$$\delta_h=\delta_{eh}+2\delta_{ph},\ \delta_{eh}=F_h/K_i,\ \delta_{ph}=\frac{m^2}{2t_f}D(\xi_2)\qquad[P1\,(42),(43)]$$
$$\delta_m=\delta_{em}+2\delta_{pm},\ \delta_{pm}=\frac{m^2}{2t_f}F(\xi_3)\qquad[P1\,(44),(45)]$$
$$\delta_u=\delta_{eu}+2\delta_{pu},\ \delta_{pu}=\frac{m^2}{2t_f}C,\ C=G(1)\qquad[P1\,(46),(47)]$$
(`L=m/2` no balanço, pois o momento nulo está a meio de `m`.)

**Tipo 2** — `ψ=(2−β_u)λ/[β_u(1+λ)]` **[P1 (49)]**; `F_{u,2}=2M_u(1+ψ)/m` [(48)]; forças `F=2M(1+ψ)/m` com `M=M_y,M_h,M_m,M_u` [(50)]; `δ_y` por (39) com `δ_{b,y}=(F_y/2+ψM_y/n)/(EA_b/L_b)` [(51)]. Rotações (hinge 1 = ligação mesa–alma; hinge 2 = eixo do parafuso):
$$\theta_{p1h}=\frac{m}{t_f(1+\psi)}D(\xi_2)\ [(52)],\qquad \theta_{p1m}=\frac{m}{t_f(1+\psi)}F(\xi_3)\ [(54)],\qquad \theta_{p1u}=C\frac{m}{t_f(1+\psi)}\ [(58)]$$
$$\theta_{p2}=\frac{m}{t_f}\Big(\frac{\psi}{1+\psi}+\lambda\Big)\times\begin{cases}0&\xi\le\xi_1\\ D(\xi)&\xi_1<\xi\le\xi_2\\ F(\xi)&\xi_2<\xi\le\xi_3\\ G(\xi)&\xi_3<\xi\le1\end{cases}\quad[(53),(55)\text{–}(57),(59)\text{–}(62)]$$
com `ξ=ψM_h/M_u` (em `h`), `ψM_m/M_u` (em `m`) e `ψ` (em `u`). `δ_p` por (15); `δ=F/K_i+2δ_p` (42), (44), (46).

**Curva completa ("simulation" de [P2] Fig. 8):** as mesmas fórmulas aplicadas a qualquer `M` entre `M_y` e `M_u` (afirmado em [P1] "Generality" e [P2] "Theoretical prediction"); os pontos intermediários **não têm outra hipótese**. [P2] também compara o modelo multilinear de 4 pontos.

## 8. Algoritmo (implementado em `piluso_tstub.py`)
1. Validar entradas (todas obrigatórias e positivas; `f_u>f_y`; `ε_y<ε_h<ε_m<ε_u`).
2. `ε_y,ε_m`; `χ̄_h,χ̄_m,χ̄_u`; `M_h/M_y, M_m/M_y, M_u/M_y` por (6)–(8).
3. `b_eff`; `M_y`, `M_u`; `B_u=A_s f_ub`; `β_u`, `β_{u,lim}`; escolher mecanismo (1, 2 ou 3; o 3 retorna sem curva).
4. Forças dos 4 pontos por (37) ou (50).
5. `K`, `m'`; `L_b`; `δ_{b,y}`; `δ_y` e `K_i`.
6. Para `h, m, u`: `D(ξ_2)`, `F(ξ_3)`, `G(1)` (inversão numérica de `χ̄_ξ`); `δ_p` por (43)/(45)/(47) ou (15); `δ=F/K_i+2δ_p`.
7. Curva contínua: repetir 6 para `M∈[M_y,M_u]`.

## 9. Limitações e informações NÃO identificadas
- **Mecanismo 3:** sem equações em [P1]/[P2] → não reproduzido.
- **Origem de `b_eff`, `K`, `m'`, `ζ`:** Faella et al. (1998), não consultado. As expressões foram tomadas de [P1 (38)] e do apêndice de [P2]; seu domínio de validade (perfis laminados, parafusos *snug*, T-stub com dois parafusos) não é demonstrado aqui.
- **Rigidez inicial de parafusos pré-tensionados** não é coberta (`K` vale para *snug-tightened*).
- **`E` não consta das tabelas de [P2];** `210 000 N/mm²` vem do exemplo trabalhado.
- **Área do parafuso — inconsistência interna de [P2]:** o texto de [P1] chama `A_b` de "área líquida"; o exemplo trabalhado usa `A_b=459 mm²` (resistente) em `β_u=0,47` e `A_b=572,5553 mm²` (bruta) em `δ_{b,y}`; a **Tabela 3** de [P2] reproduz-se com a área **bruta** `π d_b²/4` também na resistência (`β_u`, `F_{u,2}`): o `β_u` do espécime 7 é 0,3794 na Tabela 3 e 0,47 no exemplo (ver `validate_published.py`). Aqui se adotou a área bruta em ambos, por reproduzir a tabela publicada.
- **Erros tipográficos identificados em [P2] (sem alterar equações):** (i) p. 702, substituição de `F_y`: "My = 1.126.283" deve ser 1.216.283 (valor da própria página e necessário para `F_y=114.746,3 N`); (ii) Tabela 3: colunas `F_{u,exp}` e `F_{u,th}` **trocadas** nos espécimes 6 e 7 (confirmado pelo exemplo trabalhado, pela Fig. 8 e pela curva experimental digitalizada); (iii) Tabela 3, `β_u` do espécime 11: 0,3651 publicado × 0,3749 reproduzido (+2,7 %); `δ_u,th` e `F_{u,th}` do 11 coincidem, logo é divergência só de impressão; (iv) Tabela 1, espécime 8, `d_w=35,38 mm` (menor que a cabeça, `d_h=39,98`) é incomum (não alterado).
- **Espécime 8:** a Tabela 3 usa o **mecanismo 1** (`F_{u,th}=385,99=F_{u,1}`, `δ_{u,th}=45,19`), mas o critério `β_u=0,405>β_{u,lim}=0,310` indica o **mecanismo 2** (`F_{u,2}=319,4`). Observou-se tipo 1 (Fig. 7). O critério do modelo **não prevê** o mecanismo observado nesse espécime.
- Dados de espessura da alma (`t_w`), distância entre parafusos na largura e pré-tensão não constam das tabelas (necessários para um MEF, ver estudo MEF).
- A definição exata do deslocamento experimental `δ` (entre garras da máquina; inclui deformação das almas?) não é detalhada em [P2].
- Não consultadas as figuras dos mecanismos em alta resolução além do exame das imagens das páginas 1, 4 e 6 de [P1].

## 10. Diagramas dos mecanismos (esquemas; o desenho original é a Fig. 1/Fig. 3 de [P1])
```
Tipo 1 (4 rótulas)                 Tipo 2 (2 rótulas)               Tipo 3
  F↑                                  F↑                              F↑
 ──┴──  rótula na mesa-alma          ──┴──  rótula na mesa-alma       ──┴──  mesas elásticas
 ╱   ╲  ● (M_u)                     ╱   ╲  ●                          ╱   ╲
Q●────●B  rótula no eixo do          Q●─────B_u (ruptura do parafuso)  B_u (ruptura)
 n   m    parafuso (M_u)             n    m
δ_p = θ_p m                          δ_p = θ_p1 m + (θ_p1 − θ_p2) n   δ_p = θ_p m
```
