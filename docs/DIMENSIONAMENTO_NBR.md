# Dimensionamento de barras de aço

Módulo `src/design` (independente de Qt) + `app/services/DesignService` + aba **Dimensionamento** da ribbon.

## Fluxo
1. A análise (F5) gera esforços por caso/combinação. O `DesignService` reconstrói os esforços em 21 seções por barra (mesmo avaliador dos diagramas) e converte para N e mm (unidades deduzidas do módulo E do material).
2. Cada barra é verificada em todas as combinações últimas (por padrão, as que não começam por `ELS`).
3. **Verificar perfis**: apenas valida os perfis adotados (tabela de η, memória de cálculo por barra, exportável).
4. **Dimensionar melhor perfil**: para cada perfil em uso, escolhe o mais leve da mesma série que atende (η ≤ 1) para todas as barras que o usam, troca (desfazível), reanalisa e repete até convergir (máx. 8 iterações). Séries: catálogo (laminados) ou séries paramétricas (formados e tubulares). Se nenhum perfil atende, a barra é reportada.

## Normas aplicadas
| Perfil | Norma |
|---|---|
| W, I, U, L, T laminados, tubo circular, barras | ABNT NBR 8800:2024 (5.2 tração, 5.3 compressão, Anexo D/E flexão, 5.4.3 cortante, 5.5 interação, Anexo C B1/B2) |
| U, C enrijecido, Z, L, cartola formados; tubo retangular/quadrado | ABNT NBR 14762:2010 (MLE 9.2, 9.6 a 9.9, Tab. 4, Anexo E) |

Detalhes relevantes (NBR 14762): largura efetiva (AA/AL/enrijecedor de borda), χ global, flambagem por flexo-torção (monossimétricos e assimétricos), FLT com χFLT, flambagem distorcional por análise de estabilidade elástica (método das faixas finitas, `FiniteStrip`), dispensa pelas Tabelas 11/14, interação linear N+M (9.9), M+V (9.8.4), limites b/t (Tab. 4).
NBR 8800: interação 5.5.1.2 (8/9), Cb por trecho destravado, B1 por plano (Cm), torção em tubos (5.5.2).

## Convenções
N > 0 tração; Mx>0 comprime +b (topo, eixo local y); My>0 comprime +a (−z do solver). Seções assimétricas (L) são verificadas nos eixos principais (os mesmos do solver).

## Parâmetros por barra (`.dat`: `[DESIGN_SETTINGS]`, `[MEMBER_DESIGN]`)
Aço, Kx/Ky/Kz, trechos destravados ou Lb, mesa contida, Cb=1, B2, An/Ag.

## Limitações conhecidas (declaradas, não ocultas)
- Não há validação independente contra outro programa; os testes (`TestDesign`) conferem fórmulas contra cálculo à mão e os sinais dos esforços.
- Tubo retangular esbelto (λ > λr) na NBR 8800 não é coberto; cantoneira laminada em flexão usa verificação simplificada.
- Torção em perfis abertos (empenamento) não é verificada (aviso emitido).
- B2 automático usa o drift de andar da própria combinação e aplica B2 a todos os momentos da barra (não separa Mnt/Mlt): é conservador. Barras de um andar recebem o maior B2 entre X e Y.
- Imperfeições geométricas (carga nocional 0,3%) não são geradas automaticamente: lance-as como cargas se exigidas.
- Flecha: corda entre apoios; vigas inclinadas usam L/250, horizontais L/350 (ajustável por barra); balanços usam 2L.
- Limite de esbeltez em tração é apenas recomendado: vira aviso, não reprova a barra.
- Ligações e furos na mesa tracionada só entram via An/Ag.
- Distorcional: o modo é identificado como o 2º mínimo da curva de assinatura; perfis sem mínimo claro reportam a observação.

## Segunda ordem, B2 e serviço
- **Análise de 2ª ordem** (Parâmetros > Segunda ordem): o solver (`structura_set_options`) itera K + Kg(N) (matriz geométrica consistente, P-Δ e P-δ). Como não há superposição, cada combinação é resolvida com suas cargas ponderadas; combinações ELU usam EA/EI reduzidos (padrão 0,8, NBR 8800 4.10.5). Com 2ª ordem ligada, B1/B2 não são aplicados. Validado contra a solução fechada de coluna engastada-livre (tan kL).
- **B2 automático** (1ª ordem): B2 = 1/(1 − (1/Rs)(Δh/h)(ΣN/ΣH)) por andar e combinação; avisos quando B2 > 1,4.
- **Flechas ELS**: combinações cujo nome começa por `ELS`; curvatura M/EI integrada ao longo da barra (validada em viga biapoiada 5qL⁴/384EI e balanço qL⁴/8EI). Limites da NBR 8800 Tab. B.1. Deslocamentos por andar: h/500 (vários pavimentos) e topo H/400 ou H/300 (galpão).

## Interface
Aba Dimensionamento: Parâmetros, Verificar perfis, Dimensionar perfis. Os resultados abrem num painel acoplado (cards de resumo, tabela de η com filtros, aba Serviço, aba Memória) e as barras são coloridas por aproveitamento (Visualização > cores > por aproveitamento). O dimensionamento automático mostra uma prévia (perfil atual → proposto, η e peso) com opção de excluir grupos, agrupando por perfil ou por família, com η alvo ajustável.

## Materiais
Biblioteca padrão de materiais (`MaterialLibrary`): aços NBR 7007/ASTM/ZAR e concretos NBR 6118. O aço do material atribuído à barra é o aço do dimensionamento (a menos que a barra tenha aço próprio). `[MATERIAL_PROPS]` do `.dat` guarda nome, entrada da biblioteca, fy e fu.

## Grupos, cargas de área e vento
- **Grupos de barras** (Início > Criar grupo / Grupos): perfil, material, posicionamento e rotação editados em uma barra são aplicados a todo o grupo (inclusive pelo dimensionamento e pela troca de perfil). Cada barra pertence a no máximo um grupo; ao adicionar barras a um grupo elas assumem o perfil e o posicionamento dos demais. Exibir/ocultar por grupo no painel. Persistido em `[GROUPS]`. Criar/editar grupos não entra no desfazer.
- **Carga de área**: pressão (kN/m²) distribuída nas barras selecionadas pela largura de influência (metade da distância às paralelas vizinhas, com prolongamento opcional). Gera uma carga distribuída global por barra no caso escolhido (substitui a carga anterior da barra nesse caso).
- **Vento NBR 6123**: Vk = V0·S1·S2·S3, q = 0,613·Vk²; S2 = b·Fr·(z/10)^p; pressão da face = q·(Ce − Ci). Os parâmetros b, p e Fr seguem a tabela da NBR 6123:1988 (de memória; a norma não está em `normas/`) e a edição 2023 pode diferir — confira. Ce/Ci são informados pelo usuário (há sugestões típicas, não as tabelas completas).

## Memorial de cálculo e relatório
- **Motor**: `src/report` — parser de um subconjunto de LaTeX (frações, índices, raízes, gregas), conversor das linhas de cálculo do módulo de dimensionamento (`FormulaText`), renderizador de equações para a tela (`MathRenderer`, QPainter), HTML para a prévia (`ReportHtml`) e escritor de `.docx` (`DocxWriter`) com **equações nativas do Word (OMML)**, estilos, capa, sumário, cabeçalho/rodapé e tabelas coloridas por situação.
- **Conteúdo** (`ReportService`): critérios, materiais e seções, geometria, ações e combinações, resultados da análise, resumo do dimensionamento, memória por barra (desenho e propriedades da seção, dados, verificações com fórmulas e valores) e serviço (flechas, andares, B2). Escolha de barras (todas / não atendem / selecionadas) e dos itens de cálculo.
- **Interface**: aba Dimensionamento > Memorial (relatório geral com prévia e exportação) e botão "Memorial de cálculo da barra…" no painel de propriedades (lista de itens à esquerda como no CYPE 3D: memorial completo ou um item por vez; exporta o que está na tela).
- **Limitações**: as fórmulas vêm da conversão automática das linhas de texto do dimensionamento (heurística): linhas incomuns podem sair como texto; a prévia limita o detalhamento a 6 barras (o arquivo exportado leva todas); o sumário do Word é um campo que o Word pede para atualizar ao abrir.

## Tipos de barra, panos de carga e banco de dados
- **Rótulas e tipos de barra** (Início > Tipo de barra): rotular início/fim, bi-rotulada, engastar, **treliça** (biarticulada, só esforço axial) e **tirante** (biarticulado e só tração). Usam as liberações de extremidade do solver (já existentes por extremidade: Rx, Ry, Rz). O tirante marca a barra como `tensionOnly`: o solver (`structura_set_tension_only`) itera desativando a barra que encurta e reativando a que alonga (validado em teste: duas barras em V sob carga lateral, a barra comprimida sai e a outra recebe T = 5H/3). Persistido na coluna "Tipo" de `[MEMBER_PLACEMENT]`.
- **Panos das cargas de área/vento**: cada aplicação grava os panos (`[AREA_LOADS]`) com contorno, pressão, direção e barras; a opção "Panos das cargas de área e vento" (barra de exibição) desenha a superfície translucida, as setas da pressão e as setas ao longo de cada barra (a ação nas barras). As cargas distribuídas geradas continuam sendo as usadas pelo solver.
- **Excluir cargas** (Carregamentos): por tipo (nodal, distribuída, momento, área/vento), caso e abrangência (seleção ou modelo todo).
- **Banco de dados do programa**: `UserLibrary` grava `profiles.json` e `materials.json` em `<AppData>/Structura/library` (valem para todos os projetos). O editor de perfis tem "Salvar no banco de perfis do programa" (marcado por padrão) e o cadastro de material tem "Salvar no banco de materiais do programa". Os itens salvos aparecem no navegador de perfis e na biblioteca de materiais (categoria Personalizados); aços com fy e fu entram no dimensionamento.

## Produtividade

- **Checar modelo** (Ferramentas > Modelo): nós soltos/duplicados/sobre barras, barras duplicadas ou de comprimento nulo, material/seção ausentes, apoios insuficientes, trechos soltos (mecanismo), casos/combinações inexistentes e unidades suspeitas. Duplo clique no item seleciona o elemento. Executa automaticamente antes da análise (erros pedem confirmação).
- **Desfazer completo**: cargas, momentos, casos/combinações, restrições, cargas de área/vento, exclusão de cargas, grupos e novos materiais/perfis entram na pilha de desfazer (Ctrl+Z) por snapshot do estado.
- **Modelo pronto**: galpão de duas águas e edifício de pórticos paramétricos (perfis do catálogo, grupos, tirantes, cargas e combinações iniciais), gerados via `.dat`.

## Análise: combinações e flambagem

- **Gerar combinações** (Carregamentos): combinações ELU normais (NBR 8681: γg·Fg + γq·Fq1 + Σγq·ψ0·Fqj) e ELS quase permanente/frequente/rara, com tipo de ação e coeficientes editáveis (iniciais da NBR 8800:2024 tab. 1 e 2 — conferir). Ventos são mutuamente exclusivos; inclui combinações com permanentes favoráveis para vento dominante. Cargas nocionais não são geradas.
- **Flambagem elástica** (Carregamentos): autovalores de (K + λ·Kg)φ = 0 com os axiais de primeira ordem da combinação escolhida (solver: `structura_set_buckling` / `structura_get_buckling`, solução densa até 2200 GDL livres). Mostra os fatores críticos λ e a forma modal desenhada sobre o modelo. Validado contra a carga de Euler (pilar engastado-livre, erro < 1%). Análise ideal, sem imperfeições; tirantes inativos entram com a rigidez da última iteração.
- **Análise modal** (Carregamentos): vibração livre (K − ω²M)φ = 0 com matriz de massa consistente (ρA, inércia torsional) e massas concentradas vindas das cargas verticais da combinação escolhida (carga ÷ g; se a combinação não tiver o caso de peso próprio, usa a massa específica dos perfis). Mostra f, T, ω, massa efetiva X/Y/Z e a forma de cada modo no modelo. Solver: `structura_set_modal` / `structura_get_modal` (denso, até 2200 GDL livres). Validado contra a frequência da viga engastada (erro < 0,5%). Sem sismo/espectro de resposta.
- **Não implementado**: vento completo NBR 6123 (coeficientes Ce/Ci seguem informados pelo usuário; a norma não está em `normas/`).

## Projeto e documentação

- **Quantitativos** (Ferramentas > Documentação): massa, comprimento e área de pintura por perfil (A·L·γ; perímetro da seção × L), exportação CSV.
- **Exportar DXF**: eixos das barras em DXF R12 (3D, planta ou elevações X-Z / Y-Z), uma camada por perfil, rótulos de barra/perfil.
- **Dimensionar perfis** agora também respeita as flechas ELS: após convergir a resistência, os grupos com flecha acima do limite recebem um piso de inércia e o ciclo repete (até 14 iterações); a mensagem final informa a massa antes/depois. O piso usa a maior inércia do perfil como aproximação do plano da flecha.
- **Ligações** (Dimensionamento): verificação de grupo de parafusos (NBR 8800:2024 6.3.3 cisalhamento 0,45/0,56·Ab·fub/γa2, pressão de contato 1,2/2,4 ou 1,5/3,0, tração com Abe = 0,75·Ab, interação quadrática, espaçamentos e bordas) e de filetes de solda (6.2.5: 0,6·Aw·fw/γw2 com fator angular; metal-base 6.5; perna mínima/máxima e comprimento mínimo). Distribuição elástica no plano. Tabela 16 (distância mínima à borda) usa valores conservadores — conferir. **Não verificados**: efeito alavanca, cisalhamento de bloco, chapas/cantoneiras/enrijecedores, ligações por atrito, ligações de momento completas, chumbadores.

## Comprimentos de flambagem (K) e resultados no .dat

- **K na norma**: a NBR 8800:2024 **não usa K**. O esforço vem de análise de 2ª ordem com imperfeições e rigidez reduzida (ou B1/B2) e a força de flambagem Ne usa o **comprimento destravado** Lx, Ly, Lz (5.3.5.1; 4.10.6.1). A NBR 14762:2010 continua usando o comprimento efetivo KxLx, KyLy, KzLz.
- **Recurso**: em Parâmetros de dimensionamento (barras selecionadas), cada eixo tem Kx/Ky/Kz e um comprimento informado Lx/Ly/Lz (m) que substitui K·L quando > 0 — útil para travamentos intermediários (NBR 8800: K = 1 e comprimento destravado) ou para um K calculado à parte (NBR 14762). Gravado em `[MEMBER_DESIGN]`/`DEFAULTS` (3 colunas finais) e citado no memorial.
- **Resultados no .dat** (salvar grava, abrir restaura): `[ANALYSIS_RESULTS]` (deslocamentos, reações e esforços de cada caso e das combinações de 2ª ordem, com impressão digital do modelo), `[DESIGN_RESULTS]` (perfil, aproveitamento, verificação e combinação governantes por barra), `[BUCKLING_RESULTS]` e `[MODAL_RESULTS]` (última flambagem/modal calculadas). Ao abrir, os resultados só são restaurados se o modelo (geometria, perfis, cargas, vínculos e opções de 2ª ordem) for idêntico ao analisado; caso contrário são descartados e avisa-se para refazer a análise. Arquivos antigos continuam abrindo normalmente.

## Ligações do modelo (viga–apoio)

Dimensionamento > Ligações abre a lista de ligações associadas aos nós. **Nova**: selecione a viga no modelo, escolha a extremidade (nó) e o tipo; o app preenche a geometria a partir do perfil da viga (alma, altura, aço) e já propõe a ligação mais leve que atende aos esforços da análise. Cada ligação grava no `.dat` (`[CONNECTIONS]`), entra no **Quantitativos** (peças, massa, comprimento de solda) e no **memorial** (capítulo Ligações, com as fórmulas).

- **Esforços**: envoltória das combinações últimas na extremidade da viga — cortante resultante √(Vy²+Vz²) e axial dominante (tração/compressão), este último tratado como cortante nos parafusos e como tração/compressão na chapa e na solda. Momentos de extremidade não são transmitidos (ligação flexível).
- **Chapa de cisalhamento** (uma coluna de parafusos, chapa soldada nos dois lados): cisalhamento dos parafusos com excentricidade V·a (método elástico), pressão de contato na chapa e na alma, espaçamentos e bordas (6.3); chapa — escoamento e ruptura ao cisalhamento (6.5.5), rasgamento (6.5.6), flexão V·a e axial (6.5.3/6.5.4); alma da viga — rasgamento; solda — dois filetes verticais (método elástico, cortante direta + flexão + axial), metal-base, perna mínima/máxima e comprimento (6.2).
- **Cantoneira dupla**: parafusos da alma em corte duplo com excentricidade V·a (a = linha de furos do apoio à da alma), parafusos do apoio em corte simples (V/2 por cantoneira; tração N/2 se N > 0), contato na alma, nas cantoneiras (2t) e no apoio, escoamento/ruptura/rasgamento e flexão da cantoneira, rasgamento da alma.
- **Proposta**: varre diâmetro (16–24 mm), 2–10 parafusos, espessura da chapa/cantoneira e perna da solda e escolhe a de menor massa que atende; limita pela altura da viga.
- **Simplificações declaradas**: a distância de borda no contato usa o menor valor (conservador); a solda usa o método elástico (mais conservador que o centro instantâneo); a excentricidade da cantoneira usa a linha de furos do apoio; **não são verificados** o apoio (mesa/alma do pilar, flexão e punção), a flexão da alma da viga, efeito alavanca, ligações de momento, ligações por atrito e a Tabela 16 (distâncias de borda) usa valores conservadores — conferir com a norma.
- **Calculadora avulsa**: dentro da lista, para verificar um grupo de parafusos ou de filetes sem associar ao nó.

## Pranchas (desenhos)

Ferramentas > Documentação > **Pranchas** gera as pranchas do projeto a partir do modelo, com pré-visualização e exportação em **PDF** (uma folha por página) e **DXF** (um arquivo por folha, uma camada por tipo de elemento, em mm do papel):

- **Planta geral (projeção)** e **plantas por nível** (barras horizontais no nível + pilares como retângulos na escala real da seção), **elevações X–Z e Y–Z** (pórticos e planos que contêm barras), com eixos de referência (nomes do projeto ou A, B…/1, 2…), balões, **cotas entre eixos em cadeia + total**, cotas de nível, **marca e perfil** em cada barra, tirantes tracejados e rótulas (círculos) nas extremidades articuladas.
- **Marcas**: P (pilar), V (viga), D (diagonal), R (treliça), T (tirante); barras de mesmo perfil e comprimento recebem a mesma marca. **Lista de barras e materiais** (marca, perfil, quantidade, comprimento, kg/m, massa unitária e total) com totais e as peças das ligações.
- **Detalhes de ligações**: uma prancha por ligação cadastrada, com vista pela alma (apoio, viga, chapa ou cantoneira, furos com linhas de centro, solda de filete com símbolo, cotas de passo, bordas, excentricidade e altura), corte A–A em planta (parafuso com cabeça e porca, espessuras, solda) e notas com parafusos, chapa, solda, esforços e η.
- **Folha**: A4 a A1 paisagem, moldura, carimbo (projeto, prancha, escala, data, responsável, revisão, folha n/N), escala automática (maior escala padrão que cabe) ou fixa, rótulos e cotas opcionais. Por padrão poucas vistas ficam marcadas (planta geral, uma elevação de cada direção, plantas de nível se forem até 6, lista e ligações); marque o restante.
- **Limites**: desenho esquemático de eixos (barras em linha, sem contorno do perfil nas elevações); não há cortes de seções nem detalhamento de bases, emendas ou ligações de momento; as cotas são entre eixos/nós, não entre faces.

### Pranchas: detalhe dos perfis, rótulos, cotas entre faces e carimbo

- **Detalhe dos perfis** (opção em todas as pranchas): *Linha de eixo*, *Contorno do perfil* ou *Contorno + mesas e alma*. O contorno usa a largura **visível** da seção em cada vista (projeção da caixa d × bf pelos eixos locais da barra, com o giro da seção); no último modo desenha as faces internas das mesas nas elevações e a **seção em I** dos pilares na planta (preenchida, na escala real); o detalhe de ligação também mostra as mesas da viga.
- **Rótulos sem sobreposição**: cada marca/perfil testa posições (dois lados, deslocamentos ao longo da barra) e reduz para só a marca; se não couber, omite.
- **Cotas entre faces**: vão livre de cada barra no plano da vista = comprimento − meia extensão da seção das barras que chegam em cada nó (ao longo do eixo da barra, com os eixos locais e o giro de cada uma); camada própria `COTAS_FACES`. Elevações com muitos níveis (cobertura inclinada) mostram só os extremos e os níveis de topo/base de pilares.
- **Editar carimbo** (no diálogo de pranchas): empresa, cliente, obra/local, projeto, responsável técnico + CREA/CAU, desenho, verificação, data, revisão, prefixo do nº do desenho (EST-01…), notas gerais no rodapé, **tabela de revisões** (as 6 últimas acima do carimbo) e **títulos personalizados** por prancha, com pré-visualização. Gravado no `.dat` (`[TITLE_BLOCK]`) e reaproveitado em todas as exportações.
- **Limites**: sobreposição de contornos nos nós não é resolvida (aramado, como vista de modelo); o contorno ignora alinhamentos/excentricidades da seção em relação ao nó.

### Contorno com excentricidade, câmera e seleção

- **Pranchas**: o contorno do perfil e as cotas entre faces agora seguem o alinhamento e a excentricidade da seção (ponto de referência + deslocamentos A/B em cada extremidade, com o giro): a faixa da barra é desenhada em torno do centroide deslocado (nos eixos locais y/z girados), os pilares em planta são deslocados, os eixos de referência e as cotas entre eixos continuam nos nós, e o vão livre usa a posição real das faces das barras que chegam (com a excentricidade delas).
- **Câmera** (botões do mouse inalterados: direito gira, meio faz pan, roda dá zoom): o giro é do tipo mesa giratória (horizonte sempre nivelado, polos travados) **em torno do ponto sob o cursor** quando há geometria (senão o centro do modelo); o zoom da roda vai **em direção ao cursor** (o ponto sob o cursor fica parado).
- **Seleção por retângulo** (botão esquerdo): da **esquerda para a direita = janela** (só o que está inteiro dentro; retângulo azul contínuo); da **direita para a esquerda = cruzamento** (tudo que o retângulo toca; verde tracejado). Barras parcialmente atrás da câmera entram no cruzamento (recorte no plano próximo). Shift soma e Ctrl alterna, como antes.

### Linhas de dobra e vistas 3D em linhas

- **Detalhe do perfil** (modo *Contorno + mesas e alma*): além do contorno, as elevações e plantas mostram as **arestas longitudinais e linhas de dobra** do perfil, obtidas da seção paramétrica real: cantos vivos e as duas tangentes de cada dobra arredondada (agrupadas pelos trechos curtos do arco), projetadas pelos eixos locais girados da barra; em planta, o contorno real da seção (I, C, U, cantoneira, tubo, com furos) na escala. Camada `DETALHES_PERFIL`.
- **Vistas 3D em linhas** (`+ Vista 3D…` no diálogo de pranchas): desenho só de linhas, no estilo CAD, a partir de uma câmera definida por azimute/elevação (ortogonal, com escala) ou **capturada da câmera atual do modelo** (inclusive perspectiva). Os perfis são extrudados com o contorno, as dobras e as tampas; as arestas visíveis são obtidas por **remoção de linhas ocultas** (buffer de profundidade, resolução ~1400 células) e saem como linhas vetoriais — no PDF e no DXF. Opção de mostrar as linhas ocultas tracejadas. As vistas são gravadas no projeto (`[VIEWS_3D]`).
- **Limites**: a remoção de linhas ocultas é aproximada na resolução do buffer (arestas quase coincidentes podem aparecer/sumir); tampas de seções ocas usam triangulação aproximada; as marcas das barras na vista 3D ficam sobre o maior trecho visível.

### Flambagem distorcional em todos os perfis formados a frio e espessura das linhas

- A verificação distorcional (9.7.3 e 9.8.2.3) passou a ser feita por análise de faixas finitas para **todos** os formados a frio: U, U enrijecido, Z, L, cartola e tubo retangular, na compressão e na flexão em torno dos dois eixos. Sem modo distorcional na curva de assinatura (seção fechada, L, U simples), o memorial registra a nota. A dispensa pelas Tabs. 11/14 continua só para U enrijecido.
- Pranchas: contornos em 0,18 mm, eixos/cotas 0,13 mm, detalhes 0,09 mm, moldura 0,5 mm. O diálogo tem **cor das linhas** (por camada ou pretas) e **espessura** (fina 0,7×, normal, grossa 1,5×), válidas no PDF e no DXF.

### Novas tipologias de ligação

A janela **Ligações** (aba Dimensionamento) tem agora um seletor de tipologia (*Nova ligação…*). Cada tipologia usa os esforços das combinações últimas na extremidade da barra escolhida (extremidade perguntada ao criar), tem proposta automática (menor massa que atende; exceto a tubular), memorial, peças, quantitativo, prancha de detalhe (vista + corte, com cotas e notas) e persistência no `.dat`. Emenda, base e ligação tubular são verificadas **combinação a combinação** e prevalece a mais desfavorável (a combinação governante aparece no relatório).

- **Chapa de nó (gusset)** – diagonal/montante/contraventamento (peça simples ou dupla) parafusado em chapa soldada ao banzo/pilar (`TrussConnections`). Verifica: parafusos (cisalhamento 6.3.3.2, contato 6.3.3.3, espaçamentos e bordas, na chapa e na diagonal); diagonal em tração (escoamento e ruptura da seção líquida efetiva, Ct = 1 − ec/lc, 5.2.2/5.2.5) e cisalhamento de bloco (6.5.6); chapa na **seção de Whitmore** (30°): tração (escoamento e ruptura com Ae ≤ 0,85 Ag), compressão (coluna de Whitmore, K = 0,65, χ da 5.3.3) e cisalhamento de bloco; solda de filete da chapa ao apoio (6.2) para a força total da diagonal. Limites: método da força uniforme simplificado (sem decomposição entre banzo e montantes) e apoio não verificado.
- **Emenda de viga** – talas de mesa (externas e, opcionalmente, internas) e duas talas de alma parafusadas. As mesas resistem ao momento (Ff = M/(d − tf) ± N Af/A) e a alma ao cortante com a excentricidade do grupo de parafusos. Verifica parafusos, talas (tração, compressão, cisalhamento de bloco, flexão e cisalhamento das talas de alma) e a mesa da viga (seção líquida e bloco). Limite: a alma não colabora no momento.
- **Base de pilar** (NBR 8800:2024, 6.7, base tipo 1, perfil I/H com chumbadores fora das mesas) – casos C1 a C3 e T1 a T3 (Figuras 22 e 23), tp,min, db,min, pressão de contato σc,Rd (6.6.5), atrito (6.7.2.2) e solda pilar–placa. As disposições construtivas (a1, a2, a3, h1, arruelas, dimensões do bloco, fck,min) vêm da Tabela 18; o bloco é controlado também pela NBR 6118:2023, 21.2.1 (F_Rd = Ac0 fcd √(Ac1/Ac0) ≤ 3,3 fcd Ac0). Quando V,Sd > V,Rd a ligação indica a necessidade de placa de cisalhamento ou arruelas especiais (6.7.2.3 a 6.7.2.5, não dimensionadas). Não cobre a base tipo 2 nem o dimensionamento da armadura do bloco (NBR 6118).
- **Ligação tubular (CHS)** – CIDECT Design Guide 1 (2ª ed., 2008): T/Y, X, K/N com afastamento e K/N com sobreposição, diagonal circular ou chapa transversal (T/X). Verifica plastificação da face do banzo (eq. 4.1 com Qu e Qf), punção (4.2), cisalhamento do banzo (6.2), escoamento local da diagonal sobreposta (4.8/4.9), eficiência da diagonal coberta, cisalhamento das diagonais (4.11/4.12) e a faixa de validade (β, d0/t0, d/t, θ ≥ 30°, g ≥ t1 + t2, e/d0 ≤ 0,25, fy). As resistências N* do guia já são de cálculo. Os esforços do banzo e das diagonais vêm das barras que chegam ao nó (a geometria e o tipo são detectados do modelo e podem ser editados). Limites: sem proposta automática, banzo e diagonais circulares (RHS e I como diagonais só como chapa), ligações multiplanares e momentos nas diagonais não cobertos; a solda é suposta de resistência total.

### Biblioteca de perfis formados a frio e tubulares

`data/catalog_steel.json` passou a ter as famílias **Ue (C Formado)**, **U**, **Z**, **L formado**, **Cartola** e os tubulares **circular, quadrado e retangular** (espessuras comerciais 1,20 a 4,75 mm nos perfis dobrados; 1,2 a 12,5 mm nos tubulares). As dimensões seguem as séries usuais da NBR 6355 e NBR 8261/NBR 16239 e foram compiladas das séries comerciais correntes; confirme a série e a espessura do fabricante antes de comprar. O otimizador e a biblioteca usam o catálogo no lugar das séries geradas anteriormente.


### Forças nocionais automáticas

*Parâmetros > Forças nocionais* (NBR 8800:2024, 4.9.7.2). Para cada combinação última criam-se quatro variantes (`+NX`, `−NX`, `+NY`, `−NY`) com forças horizontais iguais a 0,3 % (editável) do peso aplicado em cada nó, calculadas como **casos auxiliares** (`NOC:X:<caso>`, `NOC:Y:<caso>`) para manter a superposição linear e o fluxo de 2ª ordem. As variantes aparecem como combinações nos resultados, no dimensionamento e nas ligações. Resultados armazenados são descartados ao ativar a opção, porque exigem nova análise.

### Vigas mistas e lajes mistas (aba Dimensionamento > Vigas e lajes mistas)

`src/design/CompositeFloor.{h,cpp}`, `ui/CompositeDialog`, `ui/MainWindowComposite.cpp`. Entradas gravadas no `.dat` (`[COMPOSITE_FLOOR]`).

- **Viga mista (NBR 8800:2024, Anexo L)**, perfil I/W biapoiado com laje maciça ou com forma de aço (nervuras perpendiculares ou paralelas): largura efetiva (L/8 por lado, limitada a metade da distância à viga vizinha); E_cs e η_c do concreto; resistência do conector Q_Rd = min(0,5 A_cs √(f_ck E_c)/γ; R_g R_p A_cs f_u/γ); força F_hd = min(A_a f_yd; 0,85 η_c f_cd b t_c); momento resistente positivo com interação completa ou parcial pela posição da linha neutra plástica; grau de interação mínimo η_min = max(c_p c_r² η_0; 0,4 c_r); fase de construção (perfil isolado, só se não escorada); cortante no perfil; flecha por fases com I_ef e módulo reduzido para a longa duração; limites de espaçamento (8 h, 915 mm, 4d/6d), diâmetro, projeção do conector e dimensões da forma (L.2.6). “Propor espaçamento” procura o maior passo (múltiplo de 25 mm) que atende o momento e o grau mínimo. Esforços: da análise (maior M⁺ e |V| das combinações últimas), das cargas informadas (biapoiada) ou digitados.
- **Laje mista com forma de aço incorporada (Anexo N)**, por metro de largura: fase inicial (flexão e flecha da forma sob concreto fresco + construção; 1, 2 ou ≥ 3 vãos), momento positivo da fase final (linha neutra acima ou dentro da forma), cisalhamento longitudinal pelo método m-k, cisalhamento vertical e flecha de serviço (L/350) com inércia média entre as seções fissurada e não fissurada. **As propriedades da forma (A_F,ef, I, W, e, m, k, V_v,F,Rd) são dados de fabricante/ensaio e devem ser informadas.**
- **Não coberto**: armadura de costura e cisalhamento longitudinal da laje (L.1.3.4), fissuração (L.5), pontos de carga concentrada, punção e cargas lineares sobre a laje, viga contínua e perfis não duplamente simétricos.

### Ligações em 3D

Botão *Ligações em 3D* na barra de exibição. Para cada ligação do modelo desenha, no referencial da barra (s ao longo da barra a partir do nó, t na altura da seção, u na normal da alma), chapas, cantoneiras, talas, parafusos (haste, cabeça e porca), soldas, chumbadores e bloco de concreto (`app/services/ConnectionSolids`, `viz/structure/ConnectionRenderer`). As chapas ficam verdes (η ≤ 0,7), amarelas (≤ 1,0) ou vermelhas (> 1,0) quando a ligação foi verificada; a ligação tubular é só um marcador no banzo. A geometria é esquemática (dimensões das entradas da ligação); a orientação supõe a alma no plano x'–y' local da barra.
