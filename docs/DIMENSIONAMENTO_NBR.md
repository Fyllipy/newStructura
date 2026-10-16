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

