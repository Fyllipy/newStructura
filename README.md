# Structura

Aplicativo C++ com Qt e VTK para renderização 3D. O projeto usa Widgets (Qt) e módulos VTK incluindo suporte a Qt e OpenGL.

## Requisitos
- Windows 10/11 64-bit
- Visual Studio 2022 (MSVC) com Desktop development with C++
- CMake >= 3.21
- Qt 6.x (kit `msvc2022_64`) ou Qt 5.x compatível
- VTK 9.x compilado com suporte a Qt (`GUISupportQt`) e os módulos usados no projeto

## Instalação manual

### Qt (Online Installer)
1. Baixe e instale o Qt pelo Qt Online Installer.
2. Selecione o kit correspondente ao seu MSVC, por exemplo: `msvc2022_64`.
3. Anote o caminho do CMake do Qt, algo como:
   - `C:/Qt/6.x.y/msvc2022_64/lib/cmake`

### VTK (a partir do código-fonte)
1. Baixe/clonar o VTK (GitHub/website) para `C:/src/VTK` (exemplo).
2. Configure com CMake (Gerador “Visual Studio 17 2022”, x64) e habilite Qt:
  ```
   cmake -S C:/src/VTK -B C:/src/VTK/build -G "Visual Studio 17 2022" -A x64 -DVTK_BUILD_TESTING=OFF -DVTK_BUILD_EXAMPLES=OFF -DVTK_GROUP_ENABLE_Qt=YES -DVTK_MODULE_ENABLE_VTK_GUISupportQt=YES -DVTK_MODULE_ENABLE_VTK_RenderingOpenGL2=YES -DVTK_MODULE_ENABLE_VTK_InteractionStyle=YES -DVTK_MODULE_ENABLE_VTK_RenderingAnnotation=YES -DVTK_MODULE_ENABLE_VTK_FiltersSources=YES -DQt6_DIR="C:/Qt/6.x.y/msvc2022_64/lib/cmake/Qt6"

   cmake --build C:/src/VTK/build --config Release --target INSTALL
  ```
   - Para Qt6, defina `Qt6_DIR` para o caminho do CMake do Qt (ex.: `C:/Qt/6.x.y/msvc2022_64/lib/cmake/Qt6`).
3. Compile e instale (target `INSTALL`). O `VTK_DIR` típico será:
   - `C:/src/VTK/install/lib/cmake/vtk-9.3`

## Configurar e compilar o projeto
Na raiz do projeto, execute:

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVTK_DIR="D:/src/VTK/build/lib/cmake/vtk-9.5" -DCMAKE_PREFIX_PATH="D:/Qt/6.9.3/msvc2022_64/lib/cmake"
cmake --build build --config Release
```

- Para Qt5, ajuste `CMAKE_PREFIX_PATH` para o caminho do CMake do Qt5.
- Para Debug, use `--config Debug`.

## Execução
- Após compilar, o executável estará em `build/Release` (ou `build/Debug`).

## Dicas e problemas comuns
- Erro: “Could not find a package configuration file provided by "VTK" … `VTKConfig.cmake`”
  - Solução: verifique `-DVTK_DIR` apontando para `.../lib/cmake/vtk-9.x`.
- Erro: Qt não encontrado pelos `find_package(Qt...)`
  - Solução: garanta `-DCMAKE_PREFIX_PATH=".../Qt/6.x.y/msvc2022_64/lib/cmake"` (ou Qt5 equivalente).
- Faltando `GUISupportQt` no VTK
  - Solução: reconfigure/recompile o VTK com `VTK_GROUP_ENABLE_Qt=YES` e `VTK_MODULE_ENABLE_VTK_GUISupportQt=YES`.

## Observação
Para evitar hardcode de caminhos no comando, considere usar `CMakePresets.json`/`CMakeUserPresets.json` ou variáveis de ambiente (`VTK_DIR`, `CMAKE_PREFIX_PATH`).


## Uso (lançamento de barras e snap)

- Os nós são criados ao lançar barras ("Inserir barra"): clique no ponto inicial e no final. O lançamento usa o plano de construção, a malha de referência ("Malha" no rodapé), os eixos nomeados (Ferramentas > Eixos de referência) e os snaps de extremidade, centro de barra, malha e guias ortogonal/perpendicular. Com a barra alinhada a uma guia ortogonal, o ponto também é atraído pela malha.
- Tab edita o valor digitado; Esc ou clique direito encerra o comando.

## Uso (Materiais, Secoes e Barras)

- Criar material: In�cio -> Propriedades -> ""Novo material"". Informe nome, m�dulo de elasticidade (Pa) e densidade (kg/m3).
- Criar secao: In�cio -> Propriedades -> ""Nova secao"". Informe nome, area (m2) e inercias Iy/Iz (m4).
- Inserir barra: In�cio -> Modelagem -> ""Inserir barra"". O comando ativo aparece na barra de status.
  - Passe o mouse sobre os nos para ver highlight.
  - Clique esquerdo no primeiro no, depois no segundo no. Escolha material/secao na janela exibida.
  - Clique direito rotaciona a camera sem sair do comando; ESC cancela.
- Atribuir propriedades em lote: In�cio -> Propriedades -> ""Atribuir a barras"". Selecione barras na lista, escolha material/secao e confirme.

Notas adicionais:
- Materiais e secoes podem ser deixados em branco (""Sem material""/""Sem secao"").
- O destaque de nos usa cor azul e segue o cursor enquanto o comando estiver ativo.
## Arquivos .dat
- Use ""Abrir .dat"" na guia In�cio para carregar arquivos no template indicado (materiais com E/G, se��es com A/Iz/Iy/J, n�s, barras e cargas).
- ""Salvar .dat"" gera um arquivo com o mesmo template usando os dados atuais (resist�ncias e cargas continuam armazenadas para uso futuro).

## Novidades de modelagem e visualizacao

- **Lancamento grafico**: *Inserir no (Tela)* e *Inserir barra* usam o plano de construcao com snaps
  (malha/eixos, extremidade, centro, perpendicular, ortogonal). `Tab` edita X/Y/Z/angulo (prefixo `@` =
  relativo ao ponto inicial), `Enter` confirma, `Esc` sai. Barras novas usam o ultimo material/secao.
- **Eixos de referencia** (aba *Ferramentas*): organizados por **niveis/pavimentos** (nome + altura Z), cada um com seus eixos X/Y nomeados, rotulos e snap. Da para criar niveis, copiar os eixos para outros niveis ou novas alturas, e navegar pelos niveis no rodape.
- **Barra de exibicao** (rodape): grid, eixos, identificadores, eixos locais, nos, secoes extrudadas,
  apoios, cargas, snap (menu por tipo), plano XY/XZ/YZ + nivel e tamanho da malha.
- **Secoes extrudadas**: o dialogo *Nova secao* aceita um perfil (aco ou concreto), inclui o **catalogo de 173 perfis de aco laminados** (W, I, U, L, T) e calcula A, Iz, Iy e J.
- **Cargas**: forca nodal, distribuida e **momento distribuido** em barras, com **casos de carga**
  (opcao de peso proprio usando o peso especifico do material, kN/m3) e **combinacoes** lineares.
  Cada caso e resolvido separadamente pela DLL e as combinacoes somam os resultados.
- **.dat**: blocos novos e opcionais `[MATERIAL_PROPS]`, `[SECTION_GEOMETRY]`, `[LOAD_CASES]`,
  `[COMBINATIONS]`, `[MEMBER_MOMENTS]`, `[AXES]`, `[VIEW_OPTIONS]` e coluna final de caso nas cargas.
  Arquivos antigos continuam abrindo. O solver (`Structuradll`) ignora os blocos que nao conhece.
- **Solver/DLL**: `structura_solve_v4` adiciona articulacoes de extremidade; `structura_solve_v3` adiciona giro e excentricidade da secao; `structura_solve_v2` adiciona momentos
  distribuidos; `structura_solve` continua exportada. Recompile em `Structuradll` e copie `solver_dll.dll` para a raiz.
- **Posicionamento da secao** (aba *Inicio* > *Propriedades* > *Posicionar secao*, ou o atalho no painel de propriedades da barra), modelado como no CYPE 3D:
  - *Giro da secao* (graus, regra da mao direita em torno do eixo da barra; botoes -90/+90). As inercias principais
    passam a atuar nos eixos girados (equivale a transformar Iy/Iz/Iyz por Mohr; o painel mostra Iy/Iz/Iyz nos eixos de referencia).
  - *Ponto de referencia* (9 posicoes do retangulo envolvente: centro, 4 cantos, 4 meios de lado) = ponto da secao que fica
    sobre o no, mais *deslocamento adicional* em largura (z) e altura (y). O centroide (eixo da barra) fica a
    `deslocamento - ponto_de_referencia` do no, tratado no solver como braco rigido no/eixo (`K = T^T K_barra T`).
    Esforcos e diagramas sao reportados no eixo da barra. Sem geometria parametrica so vale o deslocamento adicional.
  - Persistido em `[MEMBER_PLACEMENT]` (`Member_ID giro_graus ponto_ref(0-8) desloc_z desloc_y`); desfazer/refazer e copia suportados.
  - A CLI `solver_cli` aceita colunas opcionais em `[MEMBERS]`: `... giro [dy1 dz1 dy2 dz2]`.
  - A janela flutuante (mesmo estilo do painel de eixos) mostra a secao girada com o no, o centroide e os 9 pontos de
    referencia; clique num ponto para escolhe-lo e arraste a secao para deslocar (resolucao de 1 mm). Tem giro (slider,
    campo e atalhos -90/+90/180), deslocamento adicional por extremidade, **articulacoes** (torcao e flexao em I e J,
    com atalhos Continua / Articulada I / Articulada J / Bi-articulada) e o resultado (centroide em relacao ao no e inercias).
    O painel de propriedades da barra mostra a miniatura da disposicao e o atalho para a janela. Edicoes seguidas
    na mesma barra viram um unico passo de desfazer.
  - Secoes assimetricas (ex.: cantoneiras) usam os eixos principais automaticamente (giro adicional + inercias principais).
  - O `.dat` guarda tambem a extremidade J diferente e a mascara de liberacoes em `[MEMBER_PLACEMENT]`.
- **Exemplo**: `Examples/galpao_3_andares.dat` (5 porticos, 2 vaos de 6 m, 3 andares de 4 m; pilares W 310 girados 90 graus com faces
  externas alinhadas, vigas com topo da mesa no nivel do piso, contraventamentos em L biarticulados; casos PP/CP/SC/VT e 4 combinacoes;
  sistema kN-m com E em kN/m2).
- **Unidades**: o solver e coerente com as unidades dos dados. O aplicativo rotula cargas em kN, mas `E`/`G` sao pedidos em Pa e
  nenhuma conversao e feita: use um sistema coerente (N-m-Pa ou kN-m-kN/m2).
- O executavel aceita um `.dat` como argumento. Ver `NOTICE_OPENSA.md` sobre origem/licenca.
- **Niveis na barra de exibicao**: lista de niveis para saltar direto a um pavimento, setas para navegar e o botao
  *Mostrar todos os niveis* (plano XY), que exibe a malha e os eixos de todos os niveis ao mesmo tempo; o lancamento
  usa o nivel sob o cursor.
- **Copiar/Mover elementos** (aba Inicio, grupo Modelagem): selecione nos/barras, clique no ponto base e no destino
  (snaps, `Tab` para digitar distancia, previa deslocada). Copiar repete ate `Esc` e leva apoios, cargas, momentos,
  material, secao e ponto K; Mover tem desfazer (Ctrl+Z) e as barras conectadas acompanham os nos.
