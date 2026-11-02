# Edições, compilação e distribuição — Structura::Metalicas 0.8 beta

Uma única base de código gera duas edições:

| | **Structura::Metalicas** (Comercial) | **Structura::Metalicas Demo** (Demonstrativa) |
|---|---|---|
| Executável | `Structura.exe` | `StructuraDemo.exe` |
| Pasta de distribuição | `dist/Structura-Metalicas` | `dist/Structura-Metalicas-Demo` |
| Instalação (padrão) | `Program Files\Structura Metalicas` | `Program Files\Structura Metalicas Demo` |
| Chave de registro / dados do usuário | `Structura-Metalicas` / `%APPDATA%\Structura-Metalicas` | `Structura-Metalicas-Demo` / `%APPDATA%\Structura-Metalicas-Demo` |
| Instalador | `dist/installers/Structura-Metalicas-Setup-0.8beta.exe` | `dist/installers/Structura-Metalicas-Demo-Setup-0.8beta.exe` |

As duas edições coexistem no mesmo computador (pastas, atalhos, chaves de registro e dados do usuário distintos); desinstalar uma não afeta a outra e
nenhuma remove os dados do usuário. Nenhuma associa a extensão `.dat`.

## O que cada edição oferece

| Recurso | Comercial | Demo |
|---|---|---|
| Modelagem, visualização 3D, pós-processamento, diagramas | sim | sim |
| Análise estrutural (RDS, 1ª e 2ª ordem, não linear, vento, cascas) | sim | sim |
| Cascas (MEFS) | sim | **até 100 nós** na malha |
| Flambagem elástica, análise modal, dinâmica | sim | não |
| Dimensionamento, verificação de perfis, pilares/lajes mistas, integridade estrutural | sim | não |
| Ligações (catálogo, verificação, visualização 3D) | sim | não |
| Detalhamento: pranchas, lista de corte, DSTV, IFC | sim | não |

Na Demo, os botões dos recursos comerciais aparecem **desabilitados e sem lógica**, com dica "Disponível na versão paga" (`DemoStubs.cpp`).

| Importação CAD (DXF/IFC) e exportação DXF (eixos das barras) | sim | sim |
| Relatório | memorial de cálculo completo | relatório de análise (sem dimensionamento) |
| Abrir/salvar `.dat` | tudo | tudo — dados comerciais são **preservados** (ver abaixo) |

## Como compilar

```
# Comercial
cmake -S . -B build
cmake --build build --config Release -j 10
# Demonstrativa (pasta de build própria)
cmake -S . -B build-demo -DSTRUCTURA_EDITION=DEMO
cmake --build build-demo --config Release -j 10
# Testes de cada edição
ctest --test-dir build -C Release
ctest --test-dir build-demo -C Release
# Instalador NSIS (requer o NSIS; um por edição)
cmake --build build --config Release --target installer
cmake --build build-demo --config Release --target installer
```

Em uma pasta de build nova informe também onde está o VTK (`-DVTK_DIR=...`) se ele não estiver no caminho padrão.
`STRUCTURA_DIST_DIR` é um cache do CMake: em um build já configurado, passe `-DSTRUCTURA_DIST_DIR=...` para mudar a pasta.

## Identidade do produto

`cmake/ProductInfo.cmake` é a fonte única: nome, edição, versão, empresa, nome do executável, identificador de dados, pasta de instalação e do instalador.
Dele são gerados em `<build>/generated`: `app/ProductInfo.h`, `app.rc` (VERSIONINFO e ícone) e `product.nsh` (instalador). Para mudar versão ou nome,
edite apenas esse arquivo. `STRUCTURA_COMPANY` ainda é provisório.

Na Comercial, a primeira execução **copia** (sem apagar) `%APPDATA%\Structura` — dados da identidade anterior — para a pasta nova.

## Arquitetura da separação

- `StructuraCore` (estática): tudo o que é compartilhado — modelo, análise, visualização, interface, importação/exportação, geometria de seção
  (`src/core/section`), `SectionPropertiesService`, `MaterialLibrary`.
- `StructuraDesign` (estática, **só Comercial**): `STRUCTURA_DESIGN_SOURCES` em `CMakeLists.txt` — `src/design`, `src/drawing`, `DesignService`,
  ligações, detalhamento, flambagem/modal/dinâmica e as telas correspondentes.
- `StructuraDemo` (estática, **só Demo**): `src/demo/DemoStubs.cpp`, definições vazias dos pontos de entrada comerciais que a interface compartilhada
  declara. Não contém código de cálculo.
- `StructuraLib` (INTERFACE): liga o executável e os testes à edição escolhida.
- `src/app/Edition.h`: ponto único de consulta (`kHasDesign`, `kHasFabrication`, `kHasStabilityAndDynamics`, `kMaxShellNodes`). Não se usam diretivas de
  pré-processador de edição no código do produto (verificado por `TestEditionBoundaries`).

Os solvers `rds.dll` e `mefs.dll` são compartilhados e não foram alterados.

## Compatibilidade de arquivos `.dat`

O leitor/gravador (`DatModelIoService`) preserva o que a edição não interpreta:

- seções e chaves desconhecidas (de outras versões) voltam ao arquivo como foram lidas;
- na Demo, as seções de dimensionamento, ligações, mistas, pranchas e resultados de dimensionamento/flambagem/modal, e as chaves de dimensionamento de
  `[DESIGN_SETTINGS]`, são guardadas e regravadas intactas; a Demo só altera as opções da análise;
- resultados derivados (dimensionamento, flambagem, modal) só são regravados se a análise não foi refeita (senão ficariam desatualizados).

Limitação: colunas extras no fim de linhas posicionais de seções que a edição interpreta não são preservadas.
Testes: `TestDatPreservation` (Comercial → Demo → Comercial, edição só das opções de análise, resultados derivados).

## Verificações

- `TestEditionBoundaries`: fontes compartilhados sem código comercial; sem diretivas de edição espalhadas; o executável da Comercial contém e o da Demo **não**
  contém textos exclusivos do código de dimensionamento, ligações e detalhamento.
- `TestGoldenNumerics` (Comercial): rede de segurança numérica de ligações, barras e vigas mistas — a refatoração não alterou nenhum número.
- `TestShell::demoLimitsShellMesh` (Demo): limite de 100 nós.
- Demo: 14 testes; Comercial: 19 testes.

## Pendências conhecidas

- Textos de ajuda/mensagens em telas compartilhadas ainda citam dimensionamento (não há código por trás); os nomes dos slots comerciais aparecem nos
  metadados do Qt do executável da Demo.
- Arte: `images/splash.png` é a mesma nas duas edições (diz "cálculo e dimensionamento" e "Beta 0.8.1"); ícones ainda são da marca anterior.
- Instalação/desinstalação dos dois instaladores ainda não foi exercitada (exigem elevação).
- Proteção dos solvers (ofuscação/criptografia/licenciamento) está fora do escopo desta etapa e será planejada à parte.
