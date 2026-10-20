# Cascas e chapas (placas, lajes, reservatórios, tanques e vasos de pressão)

Módulo de elementos finitos de casca com solver em DLL própria (`Femdll/` → `mefs.dll`), independente da DLL de pórticos (`rds.dll`).
Aba **Cascas → Cascas e chapas** do programa.


## 0.0 Chapas apoiadas em barras: quinhões de carga (padrão)

Sem **Acoplar às barras**, as cargas superficiais (pressão, peso próprio, carga de área) das **chapas planas** desenhadas sobre barras chegam às barras por **quinhões**: cada ponto da chapa
descarrega na aresta apoiada (com barra) mais próxima (envoltoria; em retângulos resultam os triângulos e trapézios a 45°). A carga de cada barra é uma carga distribuída (média do quinhão no trecho que
ela cobre), de modo que os diagramas das barras são **parabólicos**. Divida as vigas de borda (**Dividir barras nos nós**) para que o triângulo/trapézio seja representado por mais trechos.
Arestas sem barra não apoiam (a carga vai às arestas apoiadas). A carga total transferida é *p × área* (testado, 1 %). O **MEF acoplado** continua disponível (e distribui a carga pela rigidez real,
com reações nodais oscilantes e diagramas poligonais).
Para ver o modelo na interface sem usar o mouse: `STRUCTURA_AUTOSHOT=<pasta> Structura.exe modelo.dat` analisa o modelo, exibe os diagramas e grava capturas.

## 0. Modelagem direta no 3D (aba Cascas)

As cascas são desenhadas na vista 3D com as mesmas ferramentas de lançamento das barras (plano de construção ativo XY/XZ/YZ e nível, snap de malha e de pontos, caixa de coordenadas com Tab,
vértices das cascas existentes atraem o cursor):

| Ferramenta | Desenho | Resultado |
|---|---|---|
| Chapa por contorno | polilinha (clique em cada vértice; Enter, clique direito ou clique repetido termina; **C** ou clique no 1º ponto fecha; Backspace desfaz) | chapa plana poligonal |
| Chapa retangular | dois cantos opostos | chapa plana |
| Disco / anel | centro e um ponto do contorno | disco (raio interno opcional) |
| Extrudar linha | polilinha (aberta ou fechada) e depois o vetor de extrusão (dois pontos) | parede, casca prismática ou cilíndrica reglada |
| Cilindro / cone | círculo e depois o vetor (dois pontos); escala da extremidade (1 cilindro, 0 cone, outra = tronco) | cilindro, cone, tronco |
| Revolução | perfil (polilinha) e depois o eixo (dois pontos); ângulo opcional | domos, esferas, tampos de vasos, tanques, toros |

Depois de cada desenho abre-se a janela de propriedades (nome, material, espessura, dimensão da malha, inverter normal) e a casca fica selecionada. **Clique** numa casca para selecioná-la
(realce laranja), **Del** exclui, o **menu de contexto** (botão direito) oferece editor, inverter normal, mover, copiar e excluir. Tudo entra na pilha de desfazer. As cascas gravam-se no `.dat`
([SHELL_MODEL]) como geometria (pontos e parâmetros), não como malha.

Normal positiva: chapas, retângulos e discos saem com a normal no sentido do eixo normal do plano em uso (use "inverter normal" para o outro lado); cilindros e revoluções saem com a normal para fora quando
o contorno/perfil é desenhado no sentido natural (perfil do equador ao polo para domos). Pressões positivas atuam no sentido da normal.

Partes que se tocam (fundo + costado + teto) conectam-se pelos nós coincidentes (1 µm). Quando as malhas **não** coincidem (dimensões de malha diferentes, divisões circunferenciais com fases
diferentes, parede apoiada no meio de uma laje, perfis de revolução em planos inclinados) o programa **liga automaticamente** os nós de borda do lado mais fino à aresta ou à face do elemento do
lado mais grosso, por interpolação bilinear dos nós do elemento (`Mesh::ties`; nos seis graus de liberdade, imposta na DLL por penalidade proporcional à rigidez do próprio nó). Em arestas
curvas aceita-se afastamento de até 12 % do tamanho do elemento (flecha do arco). **Mover** ou **copiar** uma casca leva consigo os apoios (por plano ou ponto) e a cota do líquido das cargas
hidrostáticas que pertencem a ela.

## 0.1 Acoplamento às barras

Botão **Cascas → Acoplar às barras** (grava-se no `.dat`: `COUPLED 1`). Com o acoplamento ativo, as cascas entram na **mesma análise das barras** (mesma matriz de rigidez, mesmo solver
`rds.dll`, que passou a conter o elemento Q4): casos de carga, combinações, segunda ordem, flambagem, modal, forças nocionais e tirantes continuam valendo, e as barras recebem os
esforços de interação com as cascas.

* **Nós coincidentes** (1·10⁻⁵ m) de barra e malha de casca são o mesmo nó (continuidade total, 6 graus de liberdade).
* **Casca apoiada x monolítica:** por padrão a casca **apoia** nas barras: cada nó da malha sobre uma barra é um nó próprio da casca ligado ao da barra só nas **translações** (rotações livres). A laje transmite às vigas as reações — os quinhões de carga, como forças nodais — e não momentos (diagramas das vigas sem saltos de momento concentrado). O botão **Ligação monolítica** compartilha também as rotações (a viga passa a receber torção e momento da laje; necessário para ação composta e para laje apoiada em um único ponto, que de outro modo seria mecanismo).
* **Nó de barra sobre uma casca** (borda ou face de elemento, afastamento até 1 mm) fica **ligado** ao elemento por interpolação.
* **Barra ao longo de uma borda ou linha de apoio da casca:** os nós da malha que caem sobre o eixo da barra só se conectam a ela se a barra for dividida neles. O botão **Dividir barras nos nós**
  faz isso (com desfazer) e a análise oferece fazê-lo quando detecta nós pendentes. Dica: escolha a malha da casca de modo que os nós caiam nos pontos desejados das vigas.
* **Apoios, molas e cargas** das cascas (peso próprio, pressão, líquido, temperatura, carga de área) entram nos casos de carga **pelo nome do caso**; apoios das cascas sobre nós de barras somam-se
  aos apoios das barras. Molas só-compressão (fundação) são resolvidas por iteração dentro do solver.
* **Resultados:** botão **Analisar** do editor de cascas (ou a análise normal do projeto) resolve tudo de uma vez; o editor mostra os resultados das cascas (deslocamentos, `n`, `m`, `q`, tensões,
  combinações do projeto e verificação) a partir da análise do projeto.
* **Excentricidade casca-viga:** a casca fica no plano dos nós. Para viga abaixo da laje (ou mista), use o **posicionamento/deslocamento de seção** da barra (a barra é ligada ao nó por vínculo rígido): a seção composta resulta correta (teste: viga com eixo 10 cm abaixo da faixa, flecha dentro de 0,2 % da teoria de seção composta).
* **Segunda ordem, flambagem e modal** incluem a casca: rigidez geométrica de membrana (efeito das forças de membrana sobre a flexão; chapa quadrada simplesmente apoiada, `Ncr` dentro de 12 %) e massa concentrada nos nós.
* **Tirantes (só tração)** com combinações: as combinações passam a ser resolvidas uma a uma (não por superposição de casos), para que um tirante nunca fique comprimido.
* **Modelo só de cascas** (sem barras) também é analisado com o acoplamento ativo. Os resultados das cascas **são gravados** no `.dat` junto com os das barras.
* **Verificação das barras:** usa os esforços da análise acoplada (que já incluem a interação com a laje); a resistência não credita colaboração da laje (a viga mista continua no módulo de viga mista) — é a favor da segurança.

## 1. Solver (`Femdll/`)

* **Elemento Q4** de 6 graus de liberdade por nó (u v w θx θy θz), em sistema local do elemento (e₃ pela normal média das diagonais, e₁ pela bissetriz das arestas 1‑2 e 4‑3):
  * **Membrana:** bilinear com **modos incompatíveis de Wilson** e correção de Taylor (jacobiana do centro) — passa o *patch test* em precisão de máquina em malha distorcida;
  * **Flexão:** Reissner‑Mindlin, `κs = 5/6`, com **interpolação MITC4** das deformações de cisalhamento (sem travamento em placas finas);
  * **Rotação no plano (drilling):** penalidade de Hughes‑Brezzi, `γ G t (θz − ½(∂v/∂x − ∂u/∂y))²`, `γ = 1`.
* **Barra espacial** de Euler‑Bernoulli (12×12), para reforços e anéis (disponível na DLL; sem interface — as barras do projeto são resolvidas pelo `rds.dll`, ver 0.1).
* **Ligações** `fem_add_tie`: nó escravo preso a até 8 nós mestres por interpolação (nós pendentes em malhas não conformes), por penalidade escalada pela rigidez do nó.
* **Cargas:** nós; pressão nodal na direção da normal (hidrostática e pressão de gás); força de corpo (peso próprio); temperatura (uniforme e gradiente na espessura); carga distribuída em barras.
* **Apoios:** restrições nodais e **molas** (apoio elástico por área tributária); mola **só‑compressão** (fundação) por iteração.
* **Solução:** matriz esparsa, `SimplicialLDLT` (Eigen), uma fatoração para todos os casos; resultantes nos 4 cantos e no centro de cada elemento.
* **Convenções:** `m > 0` traciona a face de normal positiva (para fora em tanques e vasos); esforços locais `nxx nyy nxy`, `mxx myy mxy`, `qx qy`.

### Verificação (`Femdll/tests/fem_tests.cpp`, `src/tests/TestShell.cpp`)

| Caso | Referência | Resultado |
|---|---|---|
| Patch test de membrana (malha distorcida) | tensão constante | 6·10⁻¹⁵ |
| Viga em balanço no plano (40×4) | Timoshenko | 0,9 % |
| Placa apoiada, q uniforme, t/a = 0,01 | `0,00406 q a⁴/D` e `0,0479 q a²` | 0,13 % e 0,8 % |
| Placa engastada | `0,00126 q a⁴/D` | 0,3 % |
| Cilindro com pressão interna | `n = pR`, `u = pR²/(Et)` | 0,5 % |
| Vaso com tampos hemisféricos, 1 MPa | `pR` (circ.), `pR/2` (axial e esfera) | 2 %, 4 %, 4 % |
| Tanque com líquido sobre fundação só‑compressão | `n = γ(h − z)R`; reação = `γ h A` | 6 % (malha de 0,5 m), 1 % |
| Barra em balanço | `PL³/3EI` | exato |
| Viga sobre molas só‑compressão (gangorra) | equilíbrio, nenhuma mola traciona | ok |

## 2. Modelo e malha (`src/shell/`, independente de Qt)

* **Objetos paramétricos:** chapa poligonal plana (malha estruturada se tiver 4 vértices; senão triangulação por orelhas, 3 quadriláteros por triângulo e subdivisão até a dimensão pedida), reservatório retangular (paredes, fundo, tampa), tanque cilíndrico (fundo, virolas de espessuras diferentes, teto plano/cônico/domo) e vaso de pressão (cilindro com tampos hemisféricos, elipsoidais 2:1 ou planos, vertical com saia ou horizontal).
* Superfícies de revolução têm polos tratados por leque de quadriláteros; nós coincidentes (1 µm) são unidos, de modo que partes que se tocam ficam conectadas.
* **Cargas:** peso próprio, pressão uniforme, líquido (peso específico, cota da superfície, pressão de gás), temperatura, carga de área global (opção por área projetada).
* **Apoios:** plano (com limite e pernas), bordas livres, ponto; fixos ou elásticos.

## 3. Resultados e interface

Casos de carga e combinações são os do projeto (superposição linear). Grandezas: deslocamentos, `n`, `m`, `q`, tensões de von Mises e principais nas duas faces. Malha colorida na vista 3D, escala da deformada, faixa fixa, exportação CSV. Parâmetros por elemento (utilização, armadura) também colorem a malha.

## 4. Verificação (`ShellDesign`)

* **Aço:** `σvm ≤ fy/1,10` (NBR 8800:2024).
* **Vasos de pressão:** `Pm ≤ S` e `PL + Pb + Q ≤ 3 S`, com `S = min(fu/3,5 ; fy/1,5)` — critério tipo ASME VIII; **o código de vasos não consta em `normas/`**, o critério é indicativo e a classificação de tensões é simplificada (a membrana é tomada por elemento, sem linearização ao longo da espessura).
* **Concreto (NBR 6118:2023):** momentos de projeto de **Wood‑Armer**, armadura de flexão pelo bloco retangular (`x/d ≤ 0,45`), armadura mínima (Tabela 17.3 e Tabela 19.1: `0,67 ρmin` na face positiva) e cisalhamento sem armadura transversal (19.4.1: `VRd1 = [τRd k (1,2 + 40ρ1)] d`). Flecha máxima de serviço apenas informada (estádio I).
* **Não verificados:** puncionamento, fissuração, flechas diferidas, flambagem de cascas cilíndricas (vento, vácuo), fadiga, bocais e selas.

## 5. Limitações conhecidas

* Só elementos quadrilaterais; elementos empenados são projetados no plano médio (o programa avisa quando o empenamento é grande).
* Os apoios por plano/borda/ponto são definidos por coordenada (acompanham *mover* e *copiar* feitos pelos comandos do programa, mas não edições manuais da posição); flambagem de cascas só pela rigidez geométrica de membrana (aproximação com w bilinear); sem aberturas em chapas.
* Pressão positiva atua no sentido da normal: confira o sentido da normal em chapas (botão "Inverter normal").
