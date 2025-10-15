<<<<<<< HEAD
# StructuraRibbon3D

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
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVTK_DIR="C:/src/VTK/install/lib/cmake/vtk-9.3" -DCMAKE_PREFIX_PATH="C:/Qt/6.x.y/msvc2022_64/lib/cmake"
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


## Uso (Grid, Inserção na tela e Snap)

- Gerar grid: na guia "Início" → grupo "Modelagem" → "Gerar grid". Informe espaçamentos Dx, Dy, Dz e número de linhas Nx, Ny, Nz. O grid é desenhado como linhas nas três direções a partir da origem (0,0,0).
- Inserir nó por coordenadas: "Inserir ponto" → preencha X, Y, Z → OK.
- Inserir nó na tela: "Inserir ponto" → botão "Inserir na tela". O checkbox global "Snap ao grid" fica na guia "Início" → grupo "Modelagem" e pode ser alternado a qualquer momento durante a inserção:
  - Clique esquerdo na viewport: cria nó na posição clicada; se o snap estiver marcado e houver grid, a posição é ajustada ao vértice de grid mais próximo.
  - Clique direito ou tecla Esc: encerra o modo de inserção na tela.

Notas:
- O snap considera o grid no intervalo [0, (N-1)*D] em cada eixo e faz clamp ao limite.
- A inserção na tela calcula a interseção do raio de visão com o plano Z=0; se a câmera estiver paralela a esse plano, gire levemente a vista para permitir a interseção.
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
=======
# newStructura
>>>>>>> d8f125f3101c09cdb483c5c2bc623972e84b98e2
