# Formulação matemática do solver (`Structuradll`)

Documento de referência para engenheiros estruturais: descreve, com as equações efetivamente implementadas,
como o solver monta e resolve o pórtico espacial linear, e como entram o **giro da seção**, o **ponto de
referência / excentricidade** e as **liberações de extremidade (articulações)**. Todos os números citados
na seção 13 são reproduzidos por testes automatizados (`src/tests/TestProjectIoAndSolver.cpp`).

Sumário

1. Escopo e hipóteses
2. Convenções, unidades e graus de liberdade
3. Eixos locais da barra e giro da seção
4. Propriedades da seção, eixos principais e transformação de Mohr
5. Matriz de rigidez local (12 × 12)
6. Transformação para o sistema global
7. Excentricidade: braço rígido entre o nó e o eixo da barra
8. Posicionamento da seção: ponto de referência e deslocamento adicional
9. Liberações de extremidade: condensação estática
10. Carregamentos e forças nodais equivalentes
11. Montagem, condições de contorno e solução
12. Esforços nas barras e reações
13. Verificações analíticas automatizadas
14. Limitações conhecidas
15. Referências

---

## 1. Escopo e hipóteses

- Análise **estática linear de primeira ordem** (pequenos deslocamentos, material elástico linear) de pórticos
  espaciais de barras prismáticas.
- Elemento de viga de **Euler–Bernoulli** (seções planas permanecem planas e normais ao eixo): não há deformação
  por cisalhamento nem empenamento (torção de Saint-Venant uniforme).
- A barra é conectada aos nós por **braços rígidos** opcionais (excentricidade) e pode ter
  **liberações de extremidade** em qualquer um dos 12 graus de liberdade.
- Cargas: nodais, distribuídas uniformes em toda a barra (eixos locais ou globais), momentos distribuídos
  uniformes e peso próprio (como carga distribuída global).
- Dimensionamento (verificações normativas) **não** faz parte deste solver.

## 2. Convenções, unidades e graus de liberdade

- Sistema de unidades **coerente** à escolha do usuário (ex.: N, m, Pa ou kN, m, kN/m²). O solver não converte:
  $E, G$ em força/área, $A$ em área, $I, J$ em comprimento⁴, forças e momentos na mesma base de $E$.
- Eixos globais $X, Y, Z$ destrógiros. (O aplicativo desenha $Z$ para cima; o solver é indiferente.)
- Cada nó tem 6 graus de liberdade, nesta ordem: $\mathbf{u}_n = [u_x, u_y, u_z, \theta_x, \theta_y, \theta_z]^T$.
  O vetor de uma barra é $\mathbf{u}_e = [\mathbf{u}_I;\ \mathbf{u}_J]$ (12 componentes), nos eixos locais.
- Rotações positivas seguem a regra da mão direita.
- Esforços de extremidade são as forças que o nó exerce **sobre a barra**, nos eixos locais:
  $\mathbf{f} = [N_I, V_{y,I}, V_{z,I}, T_I, M_{y,I}, M_{z,I},\ N_J, V_{y,J}, V_{z,J}, T_J, M_{y,J}, M_{z,J}]^T$.

## 3. Eixos locais da barra e giro da seção

### 3.1 Triedro de referência (sem giro)

Para uma barra do nó $I$ ($\mathbf{p}_I$) ao nó $J$ ($\mathbf{p}_J$), com comprimento $L = \lVert\mathbf{p}_J-\mathbf{p}_I\rVert$:

$$\mathbf{x}' = \frac{\mathbf{p}_J - \mathbf{p}_I}{L}$$

Escolhe-se o primeiro vetor auxiliar $\mathbf{v}$ da lista $(0,0,1),\ (0,1,0),\ (1,0,0)$ tal que
$\lVert\mathbf{x}'\times\mathbf{v}\rVert > 10^{-10}$ (portanto $\mathbf{v}=Z$ salvo barras verticais, que usam $Y$):

$$\mathbf{z}' = \frac{\mathbf{x}'\times\mathbf{v}}{\lVert\mathbf{x}'\times\mathbf{v}\rVert},\qquad
\mathbf{y}' = \mathbf{z}'\times\mathbf{x}'.$$

Exemplos: barra ao longo de $+X$: $\mathbf{y}'=+Z$, $\mathbf{z}'=-Y$. Barra ao longo de $+Y$: $\mathbf{y}'=+Z$, $\mathbf{z}'=+X$.
Barra vertical ($+Z$): $\mathbf{y}'=+Y$, $\mathbf{z}'=-X$.

### 3.2 Giro da seção $\beta$

O giro $\beta$ é a rotação da seção em torno de $\mathbf{x}'$ (regra da mão direita, positivo = sentido horário
para quem olha do nó $I$ para o nó $J$). Os eixos de flexão passam a ser

$$\mathbf{y}'' = \cos\beta\,\mathbf{y}' + \sin\beta\,\mathbf{z}',\qquad
\mathbf{z}'' = -\sin\beta\,\mathbf{y}' + \cos\beta\,\mathbf{z}'.$$

A matriz de rotação $3\times3$ (linhas = eixos locais expressos no global) é
$\mathbf{R}=[\mathbf{x}';\ \mathbf{y}'';\ \mathbf{z}'']$, com $\mathbf{R}\mathbf{R}^T=\mathbf{I}$.
Daqui em diante $y, z$ denotam os eixos **já girados**. Para o aplicativo, o vetor `auxK` devolvido pelo solver
é $\mathbf{y}''$ (de modo que $\mathbf{z}''=\mathbf{x}'\times\mathrm{auxK}$ e $\mathbf{y}''=\mathbf{z}''\times\mathbf{x}'$).

> **Efeito físico do giro:** como a rigidez $EI_z$ atua no plano $x$–$y$ e $EI_y$ no plano $x$–$z$ locais, girar a
> barra $\beta$ equivale a girar as inércias principais da seção em relação aos eixos de referência
> (seção 4.3). Em $\beta=90^\circ$ as inércias trocam de plano.

## 4. Propriedades da seção, eixos principais e transformação de Mohr

### 4.1 Definições

Para uma seção de área $A$ com origem no **centroide**, eixos $y$ (altura) e $z$ (largura):

$$A=\int dA,\quad I_z=\int y^2\,dA,\quad I_y=\int z^2\,dA,\quad I_{yz}=\int yz\,dA .$$

$I_z$ governa a flexão no plano $x$–$y$ e $I_y$ no plano $x$–$z$. $J$ é a constante de torção de Saint-Venant
(no aplicativo, calculada por fórmulas por família de perfis: tubulares fechados por Bredt, perfis abertos por
$\tfrac13\sum bt^3$; é uma aproximação de pré-dimensionamento).

### 4.2 Integração sobre o contorno (teorema de Green)

Para um contorno poligonal com vértices $(a_k,b_k)$ (relativos ao centroide, orientado anti-horário; furos com
sinal oposto) e $c_k = a_kb_{k+1}-a_{k+1}b_k$:

$$A=\tfrac12\sum c_k,\qquad
I_z=\tfrac1{12}\sum c_k\,(b_k^2+b_kb_{k+1}+b_{k+1}^2),\qquad
I_y=\tfrac1{12}\sum c_k\,(a_k^2+a_ka_{k+1}+a_{k+1}^2),$$

$$I_{yz}=\tfrac1{24}\sum c_k\,(a_kb_{k+1}+2a_kb_k+2a_{k+1}b_{k+1}+a_{k+1}b_k).$$

(No código: $a \equiv z$, $b \equiv y$; unidades convertidas de mm para m.)

### 4.3 Rotação dos eixos (Mohr)

Para eixos $y'=\cos\theta\,y+\sin\theta\,z$, $z'=-\sin\theta\,y+\cos\theta\,z$:

$$\begin{aligned}
I_{z'} &= \cos^2\theta\,I_z+\sin^2\theta\,I_y+2\sin\theta\cos\theta\,I_{yz},\\
I_{y'} &= \sin^2\theta\,I_z+\cos^2\theta\,I_y-2\sin\theta\cos\theta\,I_{yz},\\
I_{y'z'} &= \sin\theta\cos\theta\,(I_y-I_z)+(\cos^2\theta-\sin^2\theta)\,I_{yz}.
\end{aligned}$$

**Eixos principais** ($I_{y'z'}=0$):

$$\tan 2\theta_p=\frac{2I_{yz}}{I_z-I_y}\quad\Rightarrow\quad
\theta_p=\tfrac12\,\mathrm{atan2}\,(2I_{yz},\,I_z-I_y),\qquad I_{1,2}=\frac{I_y+I_z}{2}\pm\sqrt{\Big(\frac{I_z-I_y}{2}\Big)^2+I_{yz}^2}.$$

### 4.4 Como o solver trata o giro e a assimetria

O solver monta a rigidez de flexão **sempre nos eixos principais** (sem acoplamento $I_{yz}$), que são os eixos
locais girados. Para uma barra com giro de usuário $\beta$ de uma seção cujos eixos principais estão a $\theta_p$
dos eixos do desenho (cantoneiras, perfis U inclinados, etc.), o aplicativo envia ao solver

$$I_z \leftarrow I_{z'}(\theta_p),\quad I_y \leftarrow I_{y'}(\theta_p),\quad \text{giro da barra}=\beta+\theta_p.$$

Seções duplamente simétricas têm $\theta_p=0$ e nada muda. Em eixos de referência fixos (sem giro) a seção girada
parece ter, inversamente ($I_z,I_y$ principais, giro total $\gamma$):

$$I_{z}^{\mathrm{ref}}=\cos^2\gamma\,I_z+\sin^2\gamma\,I_y,\quad
I_{y}^{\mathrm{ref}}=\sin^2\gamma\,I_z+\cos^2\gamma\,I_y,\quad
I_{yz}^{\mathrm{ref}}=\sin\gamma\cos\gamma\,(I_z-I_y),$$

valores exibidos no painel de posicionamento (em cm⁴).

## 5. Matriz de rigidez local (12 × 12)

Ordem dos graus de liberdade: $[u_x,u_y,u_z,\theta_x,\theta_y,\theta_z]_I,\ [u_x,u_y,u_z,\theta_x,\theta_y,\theta_z]_J$.
A matriz é simétrica e a soma de quatro blocos desacoplados (axial, torção e dois planos de flexão).

**Axial** $(u_{x,I},u_{x,J})$ e **torção** $(\theta_{x,I},\theta_{x,J})$:

$$\mathbf{k}_a=\frac{EA}{L}\begin{bmatrix}1&-1\\-1&1\end{bmatrix},\qquad
\mathbf{k}_t=\frac{GJ}{L}\begin{bmatrix}1&-1\\-1&1\end{bmatrix}.$$

**Flexão no plano $x$–$y$** $(u_{y,I},\theta_{z,I},u_{y,J},\theta_{z,J})$, com $I_z$:

$$\mathbf{k}_{xy}=\frac{EI_z}{L^3}\begin{bmatrix}
12&6L&-12&6L\\ 6L&4L^2&-6L&2L^2\\ -12&-6L&12&-6L\\ 6L&2L^2&-6L&4L^2\end{bmatrix}.$$

**Flexão no plano $x$–$z$** $(u_{z,I},\theta_{y,I},u_{z,J},\theta_{y,J})$, com $I_y$ (sinais trocados nos termos
cruzados porque $\theta_y$ gira $z$ em sentido oposto a $\theta_z$ em relação a $y$):

$$\mathbf{k}_{xz}=\frac{EI_y}{L^3}\begin{bmatrix}
12&-6L&-12&-6L\\ -6L&4L^2&6L&2L^2\\ -12&6L&12&6L\\ -6L&2L^2&6L&4L^2\end{bmatrix}.$$

## 6. Transformação para o sistema global

Com $\mathbf{R}$ da seção 3, a matriz de transformação da barra é

$$\mathbf{T}=\mathrm{diag}(\mathbf{R},\mathbf{R},\mathbf{R},\mathbf{R})\ (12\times12),\qquad
\mathbf{u}_e=\mathbf{T}\,\mathbf{U}_e,\qquad \mathbf{K}_e^{G}=\mathbf{T}^T\mathbf{k}\,\mathbf{T}.$$

(Com excentricidade e liberações, $\mathbf{T}$ é substituída por $\boldsymbol\Lambda$ e $\mathbf{k}$ por $\mathbf{k}_c$;
seções 7 e 9.)

## 7. Excentricidade: braço rígido entre o nó e o eixo da barra

O eixo da barra (linha dos centroides) pode ficar afastado da linha dos nós por um vetor **transversal**
$\mathbf{e}=(0,e_y,e_z)$ nos eixos locais (girados), diferente em cada extremidade
($\mathbf{e}_I,\mathbf{e}_J$). O comprimento $L$ não muda (não há excentricidade axial).

### 7.1 Cinemática de corpo rígido

O ponto do eixo da barra acompanha o nó como um corpo rígido:

$$\mathbf{u}_{\mathrm{eixo}}=\mathbf{u}_{\mathrm{nó}}+\boldsymbol\theta_{\mathrm{nó}}\times\mathbf{e},\qquad
\boldsymbol\theta_{\mathrm{eixo}}=\boldsymbol\theta_{\mathrm{nó}}.$$

Com $\mathbf{e}=(0,e_y,e_z)$ e $\boldsymbol\theta=(\theta_x,\theta_y,\theta_z)$:

$$\boldsymbol\theta\times\mathbf{e}=\big(\theta_y e_z-\theta_z e_y,\ -\theta_x e_z,\ \theta_x e_y\big),$$

isto é, em cada extremidade (bloco $6\times6$)

$$\mathbf{T}_{\mathrm{off}}^{(end)}=\begin{bmatrix}\mathbf{I}&\mathbf{S}\\ \mathbf{0}&\mathbf{I}\end{bmatrix},\qquad
\mathbf{S}=\begin{bmatrix}0&e_z&-e_y\\ -e_z&0&0\\ e_y&0&0\end{bmatrix}.$$

O mapa completo dos deslocamentos globais do nó para os deslocamentos do eixo, em eixos locais, é

$$\boldsymbol\Lambda=\mathbf{T}_{\mathrm{off}}\,\mathbf{T},\qquad
\mathbf{u}_{\mathrm{eixo}}=\boldsymbol\Lambda\,\mathbf{U}_e .$$

### 7.2 Rigidez e forças

Por trabalho virtual (os esforços são os do eixo, $\mathbf{f}=\mathbf{k}\,\mathbf{u}_{\mathrm{eixo}}$):

$$\boxed{\ \mathbf{K}_e^{G}=\boldsymbol\Lambda^T\,\mathbf{k}\,\boldsymbol\Lambda,\qquad
\mathbf{F}_{\mathrm{nó}}=\boldsymbol\Lambda^T\,\mathbf{f}_{\mathrm{eixo}}\ }$$

Interpretação: a força de eixo $\mathbf{F}$ vira, no nó, a mesma força $\mathbf{F}$ mais o **momento de transporte**
$\mathbf{M}_{\mathrm{nó}}=\mathbf{M}_{\mathrm{eixo}}+\mathbf{e}\times\mathbf{F}$. Por isso um carregamento axial aplicado
no nó de uma barra excêntrica gera flexão (ver caso de verificação 13.3), e **os esforços reportados são os do
eixo da barra (centroide)**, que é onde o dimensionamento deve ser feito.

## 8. Posicionamento da seção: ponto de referência e deslocamento adicional

### 8.1 Geometria da seção

No desenho da seção (visto do nó $I$ para o nó $J$): $a$ = largura, no sentido de $+z$; $b$ = altura, no sentido de
$+y$. O **retângulo envolvente** em relação ao centroide é $[a_{min},a_{max}]\times[b_{min},b_{max}]$.
Os 9 pontos de referência $P_r=(a_r,b_r)$ são:

| $r$ | ponto | $(a_r,\,b_r)$ | $r$ | ponto | $(a_r,\,b_r)$ |
|---|---|---|---|---|---|
| 0 | centro (centroide) | $(0,0)$ | 5 | meio inferior | $(0,\,b_{min})$ |
| 1 | inferior esquerdo | $(a_{min},b_{min})$ | 6 | meio direito | $(a_{max},\,0)$ |
| 2 | inferior direito | $(a_{max},b_{min})$ | 7 | meio superior | $(0,\,b_{max})$ |
| 3 | superior esquerdo | $(a_{min},b_{max})$ | 8 | meio esquerdo | $(a_{min},\,0)$ |
| 4 | superior direito | $(a_{max},b_{max})$ | | | |

### 8.2 Excentricidade resultante

O ponto de referência escolhido fica sobre o nó, deslocado do **deslocamento adicional** $\mathbf{d}=(d_a,d_b)$
(largura $z$, altura $y$). O centroide então fica em relação ao nó (eixos da seção girada):

$$e_z = d_a - a_r,\qquad e_y = d_b - b_r .$$

(Em $r=0$ e $\mathbf{d}=\mathbf{0}$ a barra é concêntrica e nenhum braço rígido é gerado.) Pode-se informar $\mathbf{d}$
distinto em cada extremidade ($\mathbf{d}_I,\mathbf{d}_J$), o que dá $\mathbf{e}_I\neq\mathbf{e}_J$ (alinhamentos que variam ao longo
da barra, como topo inclinado).

### 8.3 Passagem aos eixos principais

Se a seção é assimétrica ($\theta_p\neq 0$, seção 4.4), o solver trabalha nos eixos principais e o vetor
$\mathbf{e}$ é reexpresso por rotação de $-\theta_p$:

$$e_y^{p}=\cos\theta_p\,e_y+\sin\theta_p\,e_z,\qquad e_z^{p}=-\sin\theta_p\,e_y+\cos\theta_p\,e_z .$$

Os valores $(e_y^{p},e_z^{p})$ de cada extremidade são os `dy1, dz1, dy2, dz2` da DLL (`structura_solve_v3/v4`).

## 9. Liberações de extremidade: condensação estática

Seja $\mathcal{R}$ o conjunto de graus de liberdade **liberados** (articulação, deslizamento, torção livre) e
$\mathcal{A}$ o restante, nos 12 graus locais do eixo. Particionando

$$\begin{bmatrix}\mathbf{f}_A\\ \mathbf{f}_R\end{bmatrix}=
\begin{bmatrix}\mathbf{k}_{AA}&\mathbf{k}_{AR}\\ \mathbf{k}_{RA}&\mathbf{k}_{RR}\end{bmatrix}
\begin{bmatrix}\mathbf{u}_A\\ \mathbf{u}_R\end{bmatrix}+\begin{bmatrix}\mathbf{f}^0_A\\ \mathbf{f}^0_R\end{bmatrix},\qquad \mathbf{f}_R=\mathbf{0},$$

elimina-se $\mathbf{u}_R=-\mathbf{k}_{RR}^{-1}(\mathbf{k}_{RA}\mathbf{u}_A+\mathbf{f}^0_R)$, resultando

$$\boxed{\ \mathbf{k}_c=\mathbf{k}_{AA}-\mathbf{k}_{AR}\,\mathbf{k}_{RR}^{-1}\,\mathbf{k}_{RA},\qquad
\mathbf{f}^{0\prime}_A=\mathbf{f}^0_A-\mathbf{k}_{AR}\,\mathbf{k}_{RR}^{-1}\,\mathbf{f}^0_R\ }$$

com linhas e colunas de $\mathcal{R}$ nulas em $\mathbf{k}_c$ e $\mathbf{f}^{0\prime}$ (esforço nulo no grau liberado).
Se $\mathbf{k}_{RR}$ for singular (por exemplo, torção liberada nas duas pontas) a liberação forma um mecanismo da
própria barra e a análise é recusada com mensagem clara.

Máscara usada no aplicativo (`.dat`, bloco `[MEMBER_PLACEMENT]`): bit $i$ ($0\le i\le5$) = grau $i$ no nó $I$
na ordem $u_x,u_y,u_z,\theta_x,\theta_y,\theta_z$; bit $6+i$ = o mesmo no nó $J$. A interface expõe as três rotações
($M_x$ torção, $M_y$, $M_z$): “biarticulada” $=$ bits de $\theta_y,\theta_z$ em $I$ e $J$.

**Graus de liberdade sem rigidez.** Se todas as barras de um nó liberam a mesma rotação, esse grau de liberdade
não tem rigidez alguma (matriz global singular). O solver o **fixa automaticamente** quando não há carga aplicada
nele (reação nula); se houver carga, retorna “estrutura instável”.

## 10. Carregamentos e forças nodais equivalentes

### 10.1 Cargas distribuídas uniformes

Para $\mathbf{q}=(q_x,q_y,q_z)$ em eixos **locais** (cargas globais são projetadas: $\mathbf{q}_{loc}=\mathbf{R}\,\mathbf{q}_{glob}$),
as **forças nodais equivalentes** (aplicadas aos nós, carga de engastamento perfeito com sinal trocado) são

$$\begin{aligned}
&F_{x,I}=F_{x,J}=\tfrac{q_xL}{2},\qquad F_{y,I}=F_{y,J}=\tfrac{q_yL}{2},\qquad F_{z,I}=F_{z,J}=\tfrac{q_zL}{2},\\
&M_{y,I}=-\tfrac{q_zL^2}{12},\quad M_{y,J}=+\tfrac{q_zL^2}{12},\qquad
M_{z,I}=+\tfrac{q_yL^2}{12},\quad M_{z,J}=-\tfrac{q_yL^2}{12}.
\end{aligned}$$

### 10.2 Momentos distribuídos uniformes

Para $\mathbf{m}=(m_x,m_y,m_z)$ por unidade de comprimento (integrando as funções de forma de Hermite):

$$T_{I}=T_{J}=\tfrac{m_xL}{2},\qquad F_{z,I}=+m_y,\ F_{z,J}=-m_y,\qquad F_{y,I}=-m_z,\ F_{y,J}=+m_z .$$

### 10.3 Ordem de aplicação (liberações e excentricidade)

As forças equivalentes locais $\mathbf{F}^{eq}$ passam por (a) condensação das liberações (seção 9) e (b) transporte
ao nó pelo braço rígido e rotação ao global:

$$\mathbf{F}^{eq\prime}=\mathrm{cond}(\mathbf{F}^{eq}),\qquad \mathbf{F}_{\mathrm{nó}}=\boldsymbol\Lambda^T\mathbf{F}^{eq\prime}.$$

O peso próprio é uma carga distribuída global $q_Z=-\gamma A$ (com $\gamma$ o peso específico do material).
Somente cargas uniformes em todo o comprimento são suportadas.

## 11. Montagem, condições de contorno e solução

1. Cada $\mathbf{K}_e^{G}$ $(12\times12)$ é somada na matriz global esparsa $\mathbf{K}$ ($6\,n_{nós}$ graus), nos índices
   $6\,i_{nó}+\{0..5\}$ dos nós $I$ e $J$.
2. O vetor de cargas $\mathbf{F}$ soma as cargas nodais e as forças equivalentes da seção 10.
3. Graus de liberdade sem rigidez e sem carga são fixados automaticamente (seção 9).
4. **Apoios:** para cada grau restrito $r$, zeram-se a linha e a coluna de $\mathbf{K}$, coloca-se $1$ na diagonal e
   $F_r=0$ (deslocamento nulo). Recusa-se o modelo se todos os graus estiverem restritos.
5. Resolve-se $\mathbf{K}\mathbf{U}=\mathbf{F}$ por fatoração esparsa $LDL^T$ (Eigen `SimplicialLDLT`); o resíduo
   $\lVert\mathbf{K}\mathbf{U}-\mathbf{F}\rVert$ é conferido (limite $10^{-6}$).
6. **Reações:** com a matriz **completa** (sem condições de contorno), $\mathbf{R}=\mathbf{K}\mathbf{U}-\mathbf{F}$ nos graus
   restritos.

## 12. Esforços nas barras e reações

Os esforços de extremidade (forças do nó sobre a barra, no eixo da barra, eixos locais) são

$$\boxed{\ \mathbf{f}=\mathbf{k}_c\,\boldsymbol\Lambda\,\mathbf{U}_e+\mathbf{f}^{0\prime},\qquad
\mathbf{f}^{0\prime}=-\mathrm{cond}(\mathbf{F}^{eq})\ }$$

onde $\mathbf{f}^{0\prime}$ são as reações de engastamento perfeito (condensadas). Consequências verificáveis:
(i) em uma extremidade livre e sem carga concentrada, $\mathbf{f}=\mathbf{0}$; (ii) o equilíbrio da barra fecha,
$\sum \mathbf{f}+\int\mathbf{q}\,dx=\mathbf{0}$; (iii) nas extremidades articuladas o momento liberado é nulo.

O aplicativo converte para a convenção estrutural (nó $I$ = face negativa, esforço $=-f_I$; nó $J$ = face positiva,
esforço $=+f_J$) e interpola ao longo da barra por equilíbrio, com $V_z(x)=V_{z,1}-q_zx$ e
$M_y(x)=M_{y,1}+V_{z,1}x-\tfrac12 q_zx^2$ (análogo em $y$), conferido em compilações Debug contra os esforços do nó $J$.

## 13. Verificações analíticas automatizadas

Os casos 13.1–13.5 usam $E=2\times10^{11}$, $A=0{,}01$, $I_z=4\times10^{-6}$, $I_y=8\times10^{-6}$, $L=5$ (balanço
ao longo de $X$, engastado em $I$) (todos os casos estão em `TestProjectIoAndSolver.cpp`).

| # | caso | resultado analítico | verificado |
|---|---|---|---|
| 13.1 | $P=1000$ em $-Y$ na ponta, $\beta=0$ | $v=PL^3/(3EI_y)=2{,}604\times10^{-2}$ | ✓ (também $\beta=90^\circ$: $PL^3/3EI_z=5{,}208\times10^{-2}$) |
| 13.2 | idem, $\beta=45^\circ$ | média das flexibilidades $=3{,}906\times10^{-2}$ | ✓ |
| 13.3 | $N=1000$ no nó, eixo $0{,}1$ m acima ($e_y=0{,}1$) | $M=-100$; $\theta=ML/EI_z=6{,}25\times10^{-4}$; $w=ML^2/2EI_z=1{,}5625\times10^{-3}$ | ✓ |
| 13.4 | balanço, $q_z=-1000$ local | $V_I=5000$, $M_I=-12500$, ponta livre $\mathbf{f}=\mathbf{0}$ | ✓ |
| 13.5 | engastada–apoiada ($\theta_z$ liberado em $J$), $q=-1000$ | $R_I=5qL/8=3125$, $M_I=qL^2/8=3125$, $R_J=3qL/8=1875$, $M_J=0$ | ✓ |
| 13.6 | momento distribuído $m_y=100$, $L=2$ | reação $M_y=-200$ | ✓ |
| 13.7 | galpão de 3 andares (`Examples/galpao_3_andares.dat`), 4 casos | $\sum\mathbf{R}+\sum\mathbf{F}=\mathbf{0}$ em $X$ e $Z$ | ✓ |

## 14. Limitações conhecidas

- Sem deformação por cisalhamento (Timoshenko) e sem empenamento (torção não uniforme/$C_w$); inadequado para
  vãos muito curtos relativos à altura da seção e para perfis abertos de paredes finas sob torção dominante.
- Análise de 1ª ordem: sem $P$–$\Delta$, sem $P$–$\delta$, sem flambagem; imperfeições globais e efeitos de 2ª ordem
  devem ser tratados na etapa de dimensionamento.
- Seção prismática por barra; a excentricidade é puramente transversal e rígida; zonas rígidas axiais e nós
  de dimensões finitas não são modelados.
- Cargas distribuídas apenas uniformes e em toda a barra; sem cargas trapezoidais, parciais, térmicas ou recalque de apoio.
- O centro de cisalhamento é suposto coincidente com o centroide (relevante para torção de seções assimétricas).
- $J$ é aproximado por famílias de perfis; valores de catálogo podem diferir.

## 15. Referências

- Przemieniecki, J. S. *Theory of Matrix Structural Analysis*. McGraw-Hill, 1968.
- Weaver Jr., W.; Gere, J. M. *Matrix Analysis of Framed Structures*. 3ª ed., Van Nostrand Reinhold, 1990.
- McGuire, W.; Gallagher, R. H.; Ziemian, R. D. *Matrix Structural Analysis*. 2ª ed., Wiley, 2000.
- Cook, R. D. et al. *Concepts and Applications of Finite Element Analysis*. 4ª ed., Wiley, 2002 (condensação estática).
- Timoshenko, S. P.; Gere, J. M. *Mecânica dos Sólidos*. LTC (propriedades de seção, Mohr, torção).
- A modelagem de posicionamento de seção (ponto de referência de 9 posições, braço rígido, rigidez nos eixos
  principais e esforços no eixo da barra) segue o comportamento observado no CYPE 3D, reimplementado de forma
  independente a partir das equações acima.
