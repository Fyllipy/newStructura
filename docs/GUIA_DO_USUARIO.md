# Guia do usuário — roteiro de trabalho no Structura

Este guia segue as abas da faixa de opções (**Início, Ferramentas, Carregamentos, Visualização, Cálculo, Cascas**) na ordem em que um projeto de estrutura de aço costuma ser feito. Os nomes dos botões são os da interface. Para o detalhe das fórmulas e normas, veja `DIMENSIONAMENTO_NBR.md`, `AUDITORIA_NORMAS.md` e os livros em `docs/LIVRO_*.md`.

## 1. Roteiro rápido (pórtico de galpão)

1. **Início › Propriedades › Gerenciador de perfis**: crie o material (aço) e os perfis (família, dimensões ou catálogo). Perfis armados ("I Soldado", "Caixão Soldado"), formados a frio, tubulares e "Quatro Cantoneiras" estão no gerenciador.
2. **Início › Modelagem**: *Inserir barra* (clique no início e no fim; nós são criados), *Inserir nó*, *Restrições nodais* (apoios), *Vínculos e ligações* e *Extremidades e rótulas* (articulações, molas, rótulas plásticas). *Ferramentas › Modelo › Modelo pronto* e *Ferramentas › Geração › Gerar treliça* montam geometrias comuns.
3. **Início › Propriedades › Atribuir a barras**: associe perfil e material às barras selecionadas; *Posicionar seção* ajusta giro e excentricidade.
4. **Início › Tipo de barra**: viga, pilar, treliça (rotulada nas pontas), tirante (só tração), etc. O tipo vale também para o comprimento destravado e as verificações.
5. **Carregamentos › Carregamentos**: força nos nós, carga distribuída, momento, carga geral em barra, **carga de área** (distribui para as barras do pano), **vento em edifícios** e **vento em galpões** (NBR 6123).
6. **Carregamentos › Casos e combinações**: defina casos (peso próprio, sobrecarga, vento…) e use **Gerar combinações** (NBR 8800:2024, 4.8.7 e NBR 8681: normais, de construção e excepcionais).
7. **Ferramentas › Modelo › Checar modelo**: confere nós soltos, barras de comprimento nulo, apoios insuficientes.
8. **Cálculo › Análise › Processar**: análise linear (ou de 2ª ordem, em *Parâmetros*). *Flambagem elástica* e *Análise modal* completam o estudo de estabilidade; *Dinâmica* faz o espectro de resposta e a resposta no tempo.
9. **Cálculo › Dimensionamento**:
   * *Parâmetros*: comprimentos de flambagem K, travamentos laterais, flechas (Tabela B.1), amplificação B1/B2, imperfeições, empenamento, von Mises, enrijecedores (5.7.9, 14762 9.5), pilar misto, barra composta, etc.
   * *Verificar perfis*: verifica todas as barras e colore o modelo pela razão η.
   * *Dimensionar perfis*: escolhe o perfil mais leve da série que atende (por grupo ou por família), reanalisando até convergir; mostra uma prévia antes de aplicar.
10. **Cálculo › Ligações e mistas**: *Ligações* (catálogo de ligações viga–pilar, emendas, bases, treliças, tubulares), *Copiar ligações*, *Vigas e lajes mistas*.
11. **Cálculo › Resultados**: diagramas de esforços e deslocamentos; *Comparar combinações*.
12. **Cálculo › Documentação › Memorial**: monta o memorial de cálculo (capa, sumário, **controle de revisões**, critérios, dados, resultados, dimensionamento por barra com fórmulas) e exporta para **Word (.docx)** ou **PDF**.
13. **Ferramentas › Documentação**: quantitativos, **Exportar DXF**, **Exportar NC (DSTV)**, **Exportar IFC**, **Importar DXF/IFC** e **Pranchas** (plantas, elevações, peças, ligações, quadro de conjuntos, plano de corte, lista de peças das ligações).

## 2. Importação e exportação (DXF e IFC)

* **Exportar DXF**: eixos das barras em 3D, planta ou elevações, uma camada por perfil (metros).
* **Exportar IFC**: peças (pilares, vigas, diagonais) com o comprimento de corte e a seção, mais as chapas das ligações (IFC4). O arquivo ainda não foi validado em outro programa BIM.
* **Importar DXF/IFC** (*Ferramentas › Documentação*): escolha o perfil e o material padrão, a tolerância de fusão de nós e, no DXF, a unidade. Linhas (`LINE`), polilinhas (`LWPOLYLINE`, `POLYLINE`) viram barras; vigas, pilares e barras (`IFCBEAM`, `IFCCOLUMN`, `IFCMEMBER`, `IFCSTRUCTURALCURVEMEMBER`) do IFC viram barras pelo eixo ou pelo sólido extrudado. Nomes de camada (DXF) ou de perfil (IFC) iguais aos de um perfil do projeto atribuem esse perfil. Arcos, círculos, blocos, chapas e parafusos não são importados; os avisos listam o que foi ignorado. A importação desfaz com Ctrl+Z.

## 3. Boas práticas

* Rode **Checar modelo** antes de processar e depois de importar.
* Confira os **comprimentos de flambagem K** e o **travamento lateral** (segmentos de Lb) em *Parâmetros*; eles governam pilares e vigas esbeltas.
* Para estruturas deslocáveis, ative 2ª ordem ou o B2 automático e as imperfeições (desaprumo ou forças nocionais); o memorial registra o que foi adotado.
* Salve o projeto (.dat) — os parâmetros de dimensionamento por barra ficam gravados ao final da linha de cada barra.
* Compare com um cálculo independente: `VALIDACAO_CRUZADA.md` traz casos de referência e a validação contra os exemplos do livro.

## 4. Limites conhecidos

O programa **não** cobre (veja `AUDITORIA_NORMAS.md` para a lista completa): sismo conforme a NBR 15421 (o espectro é informado pelo usuário), dimensionamento em situação de incêndio (NBR 14323), vigas mistas contínuas (4.11.6/4.11.7), coberturas isoladas, telhados de várias águas e vento a 45° pela NBR 6123, e a verificação da ligação de diagonais e presilhas de pilares treliçados. O acoplamento flexo-torcional vale para a rigidez, a 2ª ordem e a flambagem elástica, mas não para a análise modal.
