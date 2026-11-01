# Detalhamento de ligações — peças, listas, símbolos e exportação CNC

Primeira etapa do plano para dispensar softwares externos (Tekla etc.) no detalhamento. Origem de tudo: os sólidos 3D da ligação (`ConnectionSolids`), para que **qualquer tipologia do catálogo** ganhe peças, furos e listas sem desenho próprio.

## 1. Modelo de fabricação (`FabricationModel`)

`FabricationModel::fromConnection(NodeConnection)` devolve uma `FabAssembly` com:

- **Chapas** (`FabPart::Kind::Plate`): contorno no plano (relativo ao canto mínimo), espessura, caixa envolvente, furos (centro e diâmetro), área, massa líquida (descontados os furos, 7850 kg/m³), aço (`fy` da ligação) e marca `C1, C2…`. Chapas idênticas (espessura, contorno e furação) dividem a marca, e a quantidade soma.
- **Furação nos perfis ligados** (`MemberPlate`): mesas e almas dos perfis da ligação que recebem parafusos, com as coordenadas dos furos.
- **Parafusos**: agrupados por diâmetro e aperto (grip); comprimento sob a cabeça sugerido = grip + porca + 2 arruelas + 3 passos de rosca, arredondado para múltiplos de 5 mm; classe da ligação; massa com porca e 2 arruelas.
- **Soldas de filete**: perna e comprimento de cada cordão, agrupados.

Furos: diâmetro padrão da NBR 8800 (d + 1,5 mm até 24 mm; d + 3 mm acima), no ponto onde o eixo do parafuso atravessa a peça (teste de ponto no contorno).

`FabricationModel::bom(conexões)` consolida o projeto: chapas por espessura e dimensão (somadas entre ligações, com a lista das ligações que as usam), parafusos por Ø/classe/comprimento e solda por perna.

## 2. Pranchas

Duas vistas novas em **Pranchas** (`SheetBuilder`):

- **Peças da ligação — nó N, viga M** (`ViewKind::Fabrication`, desligada por padrão): cada chapa marcada com contorno, furos, eixos, cotas encadeadas (bordas e linhas de furos), massa e quantidade; tabelas de parafusos, soldas e furação nos perfis; símbolo de solda normalizado.
- **Lista de peças das ligações** (`ViewKind::ConnectionBom`, ligada por padrão): chapas, parafusos e solda do projeto inteiro com massas.

## 3. Símbolos normalizados (`drawing/DrawingSymbols`)

`drawWeldSymbol` (seta, linha de referência, filete/chanfro, dimensão, comprimento, lados, bandeira de campo e círculo de contorno), `drawBoltHole`, `drawMarkCallout` e `drawSectionCut` (corte com setas e letras). O detalhe da chapa de cisalhamento e da chapa de nó passou a usar o símbolo de solda em vez do texto "FILETE n mm".

## 4. Exportação CNC (DSTV, `.nc1`)

`DstvExporter` grava um arquivo por chapa marcada (`lig<id>_<marca>.nc1`): bloco ST (cabeçalho, código B = chapa), BO (furos, face v) e AK (contorno de chapas não retangulares). Botão **Ferramentas > Documentação > Exportar NC (DSTV)**.

**Limite importante:** o formato segue a estrutura pública do DSTV, mas as variantes de máquina e de importador diferem (colunas, sufixos de referência, campos opcionais). O arquivo **não foi validado em uma máquina nem em um importador real**; valide antes de produzir.

## 5. Peças de perfil fabricadas (`MemberFabrication`)

`MemberFabrication::build(Input)` transforma cada barra em uma peça de fabricação:

- **Comprimento de corte** = comprimento entre nós − recuos até a face dos apoios − afastamento de cada extremidade em relação à face (folga/setback da ligação).
- **Furação** dos perfis ligados (mesa e alma) em coordenadas da peça: x desde a extremidade de corte (I ou J) e y desde a linha de centro; vem dos parafusos das ligações das duas pontas.
- **Marca única** (P pilar, V viga, D diagonal, R treliça, T tirante) por perfil + comprimento de corte + furação. Com ligações definidas, `SheetBuilder::assignMarks` passa a usar essas marcas e a lista de barras mostra o comprimento de corte.
- **Prancha "Peça <marca>"** (desligada por padrão): elevação pela alma com furos e cotas encadeadas, seção transversal e tabelas de furação e das chapas das ligações das pontas.

Limites: o perfil ligado nas vistas das ligações é um modelo de contexto (as mesas têm largura sintética), então a furação **das mesas** pode ser aproximada; a da alma vem da geometria real da ligação. Emendas (perfil dos dois lados do nó) e bases de pilar não geram recortes. As chapas das ligações aparecem como informativo (não se sabe, em cada tipologia, se estão soldadas ao apoio ou ao perfil).

## 6. Recortes, cortes inclinados e conjuntos soldados

- **Recortes das mesas (cope):** detectados dos sólidos (mesa que começa depois da alma); comprimento desde a extremidade e profundidade = espessura da mesa + folga (`copeC`). Desenhados no contorno da peça, cotados e anotados; fazem parte da marca.
- **Corte inclinado:** o ângulo `skew` da ligação vira o ângulo de corte da extremidade (anotação na prancha da peça e parte da marca). O contorno inclinado não é desenhado.
- **Conjuntos:** uma chapa é "soldada ao perfil" quando uma solda a toca e toca uma peça do perfil (chapa de extremidade, por exemplo); chapas só soldadas ao apoio ficam como informação. A massa da marca é a do conjunto (perfil + chapas soldadas ao perfil), e a prancha mostra `[P]` (perfil) ou `[A]` (apoio) em cada chapa.
- **Detalhe das ligações do catálogo:** as notas passam a listar as marcas de chapas, parafusos (quantidade, diâmetro, comprimento, classe) e soldas iguais às da prancha de peças e das listas.

## 7. Plano de corte (`CutListService`)

Prancha "Plano de corte (perfis e chapas)" (ligada por padrão com ligações):

- **Perfis:** primeiro encaixe decrescente por perfil, barra de estoque configurável (`Options::stockLengthMm`, padrão 12 000 mm) e serra de 3 mm; mostra barras, comprimento usado, comprado, perda, aproveitamento, massa comprada e o padrão de corte de cada barra (marca: comprimento). Peças maiores que a barra geram aviso de emenda.
- **Chapas:** prateleiras por espessura (primeiro encaixe decrescente em altura, giro de 90°, serra de 4 mm) em chapa padrão configurável (3000 × 1200 mm); mostra chapas, área útil e aproveitamento, e avisa das peças maiores que a chapa.
- Heurísticas, **não ótimas**: servem para estimar a compra, não para o aninhamento final de uma máquina.

## 8. Exportação IFC (`IfcExporter`)

Botão **Ferramentas > Documentação > Exportar IFC** (IFC4, texto STEP, unidade mm). Estrutura: `IfcProject > IfcSite > IfcBuilding > IfcBuildingStorey`, com os elementos contidos no pavimento.

- **Peças de perfil:** cada barra vira `IfcColumn` (vertical), `IfcBeam` (horizontal) ou `IfcMember` (diagonais, treliças, tirantes), com nome = marca, descrição = perfil, posição no início do corte real, comprimento de corte e giro, extrudada de `IfcIShapeProfileDef` (perfis I/H) ou `IfcRectangleProfileDef` (os demais, largura × altura).
- **Chapas das ligações:** cada chapa física vira `IfcPlate`, com o contorno e os furos (`IfcArbitraryProfileDefWithVoids` com círculos), posicionada pelo referencial da ligação (s ao longo do perfil a partir da face do apoio, t na altura do perfil, u = s × t).
- **Material e propriedades:** `IfcMaterial` (aço) associado a todos os elementos; conjuntos de propriedades `Structura_Peca` (marca, perfil, comprimento de corte, massa do conjunto) e `Structura_Chapa` (marca, espessura, furos, massa, soldada ao perfil).
- GlobalIds de 22 caracteres (codificação IFC de 128 bits); textos com aspas e acentos são escapados.

Limites: perfis que não são I/H saem como retângulos; parafusos, soldas, recortes e furos dos perfis não entram na geometria; o lado (u) das chapas na alma é inferido do referencial da ligação e pode sair espelhado. O arquivo foi verificado só internamente (referências `#n` todas definidas, contagens de entidades, GlobalIds, escape de texto), **não** foi aberto em outro programa.

A montagem da entrada (`SheetBuilder::Input`) saiu de `onGenerateDrawings` para `MainWindow::buildSheetInput`, compartilhada com a exportação.

## 9. Chamadas de marca nos detalhes das ligações

Nos detalhes das tipologias do catálogo (vista frontal), cada chapa recebe uma chamada de marca (círculo com C1, C2… ligado ao centro da chapa), cada grupo de parafusos recebe B1, B2… (ligado a um parafuso do grupo) e cada grupo de soldas recebe o símbolo de solda normalizado com a perna; as marcas são as mesmas da prancha de peças, da lista de peças e das notas. A posição vem do modelo de fabricação (`FabricationModel::centre`, `FabBolt::position`, `FabWeld::position`).

Limite: a chamada aponta o centro da chapa (não o contorno) e a posição da solda é o centro de um cordão do grupo; em ligações densas as chamadas podem se sobrepor a cotas e precisam de ajuste manual.

## 10. Quadro de conjuntos e marcas nas vistas gerais

- **Quadro de conjuntos** (prancha ligada por padrão): marcas de perfil com quantidade, comprimento de corte e entre faces, número de furos e recortes, chapas soldadas ao perfil, massa por peça e total (perfil + chapas soldadas ao perfil).
- **Vista de montagem:** nas plantas e elevações, cada nó com ligação mostra "L<ligação>: C1 C2…" (marcas das chapas), junto às marcas únicas de cada barra.
- **Cotagem:** as vistas gerais já tinham cotas automáticas entre eixos e níveis e o vão livre entre faces; as pranchas de peças têm cotas encadeadas nos furos. Não há cotagem automática de detalhes (ainda manual por família no catálogo antigo).
- **Corte inclinado:** o contorno da peça na prancha agora é desenhado inclinado nas extremidades com `skew` > 0 (cortes em espelho nas duas pontas; o sentido real depende da ligação). Chanfros de preparação de solda não existem porque o modelo só tem soldas de filete.

## 11. O que ainda não existe

- Parafusos, soldas, recortes e furos dos perfis na geometria do IFC; perfis não I/H com contorno real; validação do IFC e do NC1 em outros programas e em máquina.
- Cotagem automática dos detalhes de ligação; símbolo de parafuso nas vistas de montagem do pórtico inteiro.
- Aninhamento 2D real das chapas; chanfros e soldas de penetração (exigiria modelar soldas de entalhe).

## 12. Posição 3D das ligações sem barra de apoio escolhida

Quando a ligação não tinha `supMember` (barra de apoio) nem `supOffset`, a origem da ligação ficava no eixo do nó: as chapas apareciam do outro lado do pilar e a viga atravessava o pilar. Agora o modelo escolhe sozinho o apoio entre as barras do mesmo nó (`pickSupportAutomatically`): a de maior altura de seção que não seja quase paralela à viga (|cos| ≤ 0,9). A face de apoio, o recuo da viga e as marcas/cortes das peças passam a usar essa face. Quem escolhe a barra de apoio no diálogo da ligação continua tendo prioridade. Limite: em nós com vários pilares ou vigas principais a escolha automática pode não ser a desejada; nesse caso informe a barra de apoio.

## 13. Ligações "fora do nó" (viga–pilar com talas e flangeada fora do nó)

As tipologias 147 (flangeada) e 148 (talas) agora aceitam **barra de apoio** (o pilar ao qual a viga se solda) e têm o campo **"Distância da face do apoio à junta"** (`outDist`, padrão 220 mm na flangeada e 260 mm nas talas). A origem da ligação em 3D passa a ser a face do apoio deslocada de `outDist` ao longo da viga, de modo que as talas/chapas de topo ficam afastadas do pilar (antes ficavam rentes ao nó e, sem apoio escolhido, do outro lado do pilar). O pilar de contexto dos desenhos e a posição das chapas no IFC usam o mesmo valor. Limite: a barra continua modelada inteira entre o apoio e a junta (o corte físico da viga na junta não está na geometria da barra), e a peça de fabricação trata só o lado da viga depois da junta.

## 14. Solda da viga ao apoio (ligações fora do nó) e barra de apoio nos detalhes

- **Cálculo (`design/SupportWeld`):** nas ligações 147 e 148 o trecho de viga entre a face do apoio e a junta é soldado ao apoio. As mesas resistem a M e N: F = M/(d − tf) + |N|/2; a solda das mesas é de **penetração total** (resistência do metal-base, 6.2.4: bf·tf·fy/γa1) ou de **filete** (filetes transversais na face externa e nas duas internas, Tabela 9 com θ = 90°, e metal-base). A alma tem 2 filetes longitudinais ao cortante (comprimento d − 2tf), com verificação do metal-base da viga e, se a barra de apoio foi escolhida, da parte do apoio. Pernas mínimas (Tabela 11) e máximas (6.2.5.2) entram como verificações.
- **Dimensionamento:** "Propor" escolhe a menor perna que atende (alma e, se a solda das mesas for de filete, mesas). Campos novos: tipo da solda das mesas, pernas e fw.
- **Modelo 3D e detalhamento:** as soldas aparecem na face do apoio (filetes ao redor das mesas e da alma; na penetração total, a seção da solda), recebem marca (S1, S2…) e símbolo de solda, entram na lista de peças (comprimento total) e no IFC pelo modelo de fabricação.
- **Detalhamento por sólidos:** as ligações fora do nó passaram a usar o detalhe por sólidos (antes usavam o desenho antigo de emenda, sem solda e sem apoio), e **todo detalhe do catálogo mostra um pedaço da barra de apoio** em cinza (camada "APOIO"): o próprio da tipologia, ou um pedaço gerado das dimensões do apoio (`supD`, `supBf`, `supTf`, `supTw`; padrão 240 × 170 × 12 × 8 mm) conforme a face escolhida (mesa ou alma do pilar, alma de viga).
- **Limites:** a solda é verificada com M, V e N dominantes da ligação (não por combinação); o sentido do cortante e a distribuição entre mesas e alma são os do modelo simplificado acima; sem verificação de solda de penetração parcial.

## 15. Detalhe das ligações do catálogo com cortes e tabelas na mesma prancha

Cada ligação do catálogo (e as fora do nó) é detalhada em uma prancha única, no nível de um projeto de oficina:

- **Chapas** (no alto): contorno, furos (Ø e posição), cotas encadeadas, quantidade e massa de cada chapa marcada.
- **Vista frontal**, com chamadas de marca (C, B), símbolo de solda e **linhas de corte** A-A, C-C e D-D com setas no sentido de observação (B-B aparece sobre o corte C-C).
- **Cortes** gerados do modelo por planos axiais: **A-A** perpendicular à viga pelo grupo de parafusos (olhando para o apoio), **B-B** longitudinal pelo plano médio da alma, **C-C** em planta a meia altura e **D-D** junto da solda viga-apoio (ligações fora do nó) ou da extremidade das chapas. Os planos atravessam chapas, perfis, parafusos (círculo ou haste), soldas e a barra de apoio; as seções cortadas são hachuradas (apoio em cinza, soldas em vermelho) com contorno forte, e o que está atrás do plano aparece sem linhas ocultas. Chamadas com nome da peça ("Viga …", "C1 PL …", parafuso, solda, barra de apoio).
- **Tabelas de materiais da ligação** (chapas, parafusos, soldas, massas) e notas no canto da mesma prancha. A prancha separada "Peças da ligação" deixou de ser listada; o **quadro total de quantitativos** continua em "Lista de peças das ligações" (e o quadro de conjuntos e o plano de corte).
- **Correção:** nas ligações fora do nó o trecho de viga de contexto do lado do apoio termina na face do apoio (antes entrava no pilar no detalhe).

Limites: os cortes são axiais (planos s, t ou u); barras tubulares inclinadas não são cortadas; os números dos cortes (A-A…D-D) são fixos; as chamadas dos cortes podem se sobrepor em ligações densas e a legenda dos parafusos usa o primeiro grupo.

### 15.1 Cotas nos cortes e símbolos de parafuso normalizados

- **Cotas encadeadas em todas as vistas do detalhe:** horizontais abaixo e verticais à esquerda, geradas dos valores-chave de cada vista (bordas das chapas e do perfil cortados, eixos dos parafusos; na elevação, também a face do apoio e a junta nas ligações fora do nó), com cota total quando há mais de dois valores. Chaves a menos de 3 mm de papel da anterior são fundidas para não empilhar cotas.
- **Parafusos:** na elevação (vista pelo eixo) cada parafuso é um símbolo normalizado: hexágono da cabeça (1,5 d entre faces), círculo da haste e cruz de eixo; no lugar dos polígonos de cilindro do modelo. Nos cortes, parafuso cortado transversalmente leva cruz de eixo, e o cortado no sentido da haste leva eixo tracejado. Os eixos também entram como chaves das cotas (espaçamentos e distâncias à borda).
- **Limite:** as cadeias são automáticas (sem anti-sobreposição entre cotas e chamadas); em ligações com muitos parafusos a cadeia de uma vista pequena pode ficar densa e pedir ajuste manual da escala ou do papel.

### 15.2 Edição das anotações da prancha

- **Grupos:** cadeias de cotas (`secA/cotaH`, `elev/cotaV`…), cotas da família, chamadas de marca, símbolos de solda, linhas de corte, títulos, chamadas dos cortes, painéis de chapa, tabelas e notas são *grupos* da prancha (`Sheet::groups`, chave estável prefixada por nó/barra da ligação). Cada entidade guarda o id do seu grupo.
- **Edição na tela de pranchas:** clique numa anotação para selecioná-la (moldura laranja); arraste para mover; `Delete` ou menu de contexto exclui; o menu de contexto também restaura as anotações da prancha ao posicionamento automático. O arraste vale no preview, no PDF e no DXF.
- **Persistência:** os ajustes (deslocamento em mm do papel ou exclusão) ficam no projeto, na seção `[SHEET_LAYOUTS]` (linhas `EDIT prancha chave dx dy excluida`), e são reaplicados a cada geração (`SheetBuilder::applyEdits`). Se a chave deixar de existir (ligação alterada), o ajuste é ignorado.
- **Sem sobreposição automática:** cada cadeia, cota, chamada e símbolo de solda é desenhado numa prancha auxiliar e testado contra as áreas já ocupadas (`Sheet::occupied`); a cadeia é afastada de 5 em 5 mm (até 8 tentativas), chamadas e símbolos tentam posições vizinhas, e vale a de menor sobreposição.
- **Limites:** a verificação usa retângulos envolventes (texto estimado pelo nº de caracteres); em vistas muito pequenas pode restar sobreposição, que se resolve arrastando. Vistas (hachuras, geometria) e carimbo não são editáveis.
