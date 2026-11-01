# Vento estático em edifícios (NBR 6123:2023)

Comando **Carregamentos > Vento em edifícios**. Gera casos de carga de vento a partir da planta, dos pavimentos e dos parâmetros da norma. Substitui o antigo comando "Vento NBR 6123" (removido): calcula a força de cada pavimento no edifício inteiro, com os coeficientes de barlavento e de sotavento da Tabela 6, e produz as hipóteses ±X, ±Y com excentricidade. Para pressões distribuídas em panos de barras use **Carga de área**.

## Cálculo

* Velocidade característica Vk = V0·S1·S2·S3 e pressão dinâmica q = 0,613·Vk² (N/m²). **S2 = bm·Fr·(z/10)^p** com bm e p das Tabelas 1 (categorias I–V, classes A/B/C) e Fr da Tabela 2;
  abaixo de 5 m vale o S2 de 5 m e, na categoria V, abaixo de 10 m o de 10 m; acima de zg vale o de zg. Confere com a Tabela 3 (por exemplo, categoria II classe B: 0,98 a 10 m, 1,04 a 20 m, 1,21 a 100 m).
* **Classe** (5.3.2) pela maior dimensão da superfície frontal em cada direção: A até 20 m, B até 50 m, C acima. Acima de 80 m a norma remete ao Anexo A; o programa usa a classe C e avisa.
* **Coeficientes** (Tabela 6, paredes de edificação de planta retangular a × b, b a menor dimensão): escolhidos pela relação h/b (≤ 1/2; 1/2 a 3/2; 3/2 a 6) e por a/b (1 a 3/2; 2 a 4; interpolação linear entre 3/2 e 2 — nota 2).
  Vento a α = 0° (perpendicular à fachada de largura b): barlavento C, sotavento D; a α = 90° (fachada de largura a): barlavento A, sotavento B. A força na direção do vento usa Ce(barlavento) − Ce(sotavento).
  Vento de alta turbulência (6.1.3.2): sotavento com 2/3 do valor.
* **Força do pavimento** F = q(z)·(Ce,barlavento − Ce,sotavento)·L·h_trib, com L a largura da fachada de barlavento e h_trib a altura carregada (metade do vão abaixo mais metade do vão acima; no topo, metade do último vão mais a platibanda informada).
* **Excentricidade** (6.1.4): ea = 0,075·a e eb = 0,075·b (0,15 com efeitos de vizinhança) em relação ao eixo vertical geométrico, medida na direção da largura da fachada de barlavento. São geradas as hipóteses centrada e com +e e −e
  (ou só as excêntricas, à escolha).

## Aplicação ao modelo

Cada hipótese vira um caso de carga (`VENTO +X`, `VENTO +X e+`, …; refeito a cada geração) com cargas nodais:

* se o pavimento tem **diafragma rígido** (Cálculo > Vínculos), a força e o momento da excentricidade (M = −(y_linha − y_mestre)·Fx, ou (x_linha − x_mestre)·Fy) atuam no nó mestre;
* sem diafragma, a força é repartida entre os nós do nível por largura tributária na direção transversal e a linha de ação é corrigida por um binário nos nós extremos.

As combinações com permanentes e sobrecargas são montadas em **Casos e combinações** (ou *Gerar combinações*). O vento oblíquo (45°), a ação do vento nas coberturas (Tabelas 7 a 12) e os efeitos dinâmicos
(ver *Dinâmica > Vento dinâmico*) não são gerados por este comando. Não há ação de neve.

## Verificação

`TestProjectIoAndSolver::windStaticFollowsTheStandard` confere S2 contra a Tabela 3, a classe, os coeficientes da Tabela 6 (inclusive a interpolação), a pressão dinâmica, a força por pavimento, as alturas carregadas e a excentricidade.

## Fator topográfico S1 (5.2)

Os dois diálogos de vento têm o botão **Calcular S1 topográfico…**: terreno plano (S1 = 1,0), talude ou morro no ponto B (S1(z) = 1 + (2,5 − z/dt)·tg(θt − 3°) ≥ 1 para 6° ≤ θt ≤ 17°; 1 + (2,5 − z/dt)·0,31 para θt ≥ 45°; interpolação linear entre 3° e 6° e entre 17° e 45°; 1,0 para θt ≤ 3°) e vale profundo (0,9). `WindStaticService::s1Topographic`.

## Vento em galpões (Carregamentos > Vento em galpões)

`WindShedService` + `WindShedDialog`, NBR 6123:2023. Detecta nas barras do modelo as **colunas** (barras verticais) e as **tesouras/vigas de cobertura** (barras inclinadas num plano transversal à cumeeira; o eixo da cumeeira é escolhido no diálogo e detectado quando possível) e aplica **cargas distribuídas** nos eixos globais:

* **Pressão** `p = q (Ce − Cpi)` (positiva = para dentro); a força na barra é `−p · largura de influência · normal exterior`. A largura de influência é a faixa entre os pontos médios das barras vizinhas (prolongada pelo valor informado nas extremidades); quando a faixa cruza zonas de coeficientes diferentes o `Ce` é a média ponderada pelo comprimento.
* **Paredes** (Tabela 6): barlavento A / sotavento B e oitões C1D1 / C2D2 (vento transversal, α = 90°); laterais A1B1, A2B2, A3B3 (nota 3) e oitões C / D (vento longitudinal, α = 0°); `x1 = máx(b/3; a/4) ≤ 2h`.
* **Cobertura de duas águas** (Tabela 7): barlavento EFI e sotavento GHJ (α = 90°); EG até x1, FH até a/2 e partes I e J (nota 3, interpolada em a/b) (α = 0°); interpolação linear em θ dentro da faixa de h/b.
* **Cobertura de uma água** (Tabela 8, h/b < 2): colunas H e I / L e J (α = ±90°, vento do lado alto ou do baixo), e H e L até b/2 / de b/2 a a/2 (α = 0°); 5° ≤ θ ≤ 30°.
* **Pressão interna** (6.3.2): estanque (−0,2 ou 0); quatro faces permeáveis (−0,3 ou 0); duas faces opostas permeáveis (+0,2 / −0,3); abertura dominante na face de barlavento (tabela de 6.3.2.1-c, interpolada), de sotavento (Cpi = Ce) ou em zona de alta sucção; valores informados. Cada valor de Cpi gera um caso.
* **Casos**: `VENTO T+`, `T−` (transversal), `L+`, `L−` (longitudinal) com sufixo `cpi=…`; mutuamente exclusivos nas combinações (tipo Vento). O memorial dos coeficientes aparece no diálogo.
* **S2** é calculado na cota de referência escolhida (cumeeira por padrão, beiral ou média); a classe vem da maior dimensão da superfície frontal de cada sentido.

**Não coberto**: telhados múltiplos (Tabelas 9 a 12), vento oblíquo (45°), coberturas isoladas (Tabelas 24 e 25) e o `cpe` médio dos elementos de vedação e terças (use a Tabela 7 e 8 e a carga de área). Testes: `TestProjectIoAndSolver::windShedFollowsNbr6123Tables` (tabelas e forças em um galpão de cinco pórticos conferidas à mão).
