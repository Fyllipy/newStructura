# Modos de casca na janela, comparação com o BucklingPlate do Mecway e torre de telecomunicações (Structura x Mecway 28)

Data: 2026-11-24. DLLs, `.enc` e executáveis comerciais intactos (hashes `f40c09a9d032`, `09ffc08a6001`, `c3a9b8b2c3b6`, `ff6c9550e2d2`). O Mecway só foi usado em linha de comando (modelos `.liml` e scripts), sem automação de tela.

## 1. O programa passa a desenhar os modos de casca

- `BucklingService::shellShapeMesh` monta a malha das cascas deformada pela forma (nós de casca fundidos com nós de barra usam o deslocamento do nó de barra), colorida pelo deslocamento normalizado com a escala em faixas; é a função que a janela chama.
- `SceneController::setShellModeMesh` / `clearShellModeMesh`: a malha original das cascas sai da vista enquanto o modo está ativo e volta ao fechar a janela (respeitando o "mostrar cascas" do usuário).
- Diálogos de **flambagem** e **modal** (`MainWindowBuckling.cpp`, `MainWindowModal.cpp`): barras e cascas na mesma escala (a maior translação entre os dois conjuntos vale a amplitude do controle), e a extensão do modelo agora inclui os nós de casca.
- `SolverBridge::Result` ganhou `bucklingShellShapes` e `modalShellShapes` (as formas dos nós de casca eram descartadas).
- Testes: `TestShellCoupling` (14/14): formas devolvidas, malha deformada com a flecha pedida, modelo sem cascas devolve vazio. **A ligação na janela não foi exercitada com a interface aberta** (teste manual pendente).

## 2. BucklingPlate.liml (exemplo do Mecway) x Structura

O exemplo é uma placa 1 x 2 x 0,0125 m em **sólidos hex20** (1084 nós), E = 1e6 Pa, ν = 0,3, apoiada nas quatro bordas no plano médio, compressão em y por pressão de 80 Pa na borda (= 1 N/m). Equivalente em casca no Structura: malha de 1/14 m, mesmo apoio e 1 N/m de compressão (`bp_run.cpp`). Arquivo original intacto (hash conferido).

| Modo | Mecway (sólidos) | Structura (casca) | Structura/Mecway |
|---|---|---|---|
| 1 | 7,1480 | 7,0936 | −0,76 % |
| 2 | 8,6005 | 8,3658 | −2,73 % |
| 3 | 11,1446 | 11,1168 | −0,25 % |
| 4 | 11,9740 | 11,2666 | −5,9 % (os modos 3 e 4 do Structura estão próximos: 11,12 / 11,27; a forma do modo 4 não foi comparada, pode haver troca de ordem) |

Analítico do modo 1 (placa apoiada, a/b = 2, k = 4): 7,061; Structura +0,46 %, Mecway +1,2 % (os sólidos enrijecem). Prints: `docs/validacao_mecway/curvas/prints/bucklingplate_modo{1,2}_{structura,mecway,comparacao}.png`. A forma do Mecway é reamostrada (interpolação serendipity do hex20) na malha de cascas e renderizada com a escala do Structura; o sinal de cada modo é normalizado.

## 3. Torre de telecomunicações (perfis L e C), Structura x Mecway

Modelo (`gen_torre.py`): torre autoportante de 4 pernas, treliça em X, 30 m (10 módulos de 3 m), lado de 3,2 m na base afunilando a 1,2 m, apoios articulados; 84 nós, 244 barras. Perfis: L 101,6x9,525 e L 76,2x6,35 (pernas), L 63,5x6,35 e L 50,8x4,76 (diagonais e horizontais), **C** 102x50 (U laminado) nos anéis da plataforma (níveis 27 e 30 m) e nas pernas do último módulo. Casos: peso próprio, antenas (4 x 2 kN no topo) e vento (forças por nível + arrasto das antenas). O arquivo `.dat` é carregado pelo caminho do aplicativo (`TestTowerModel`, ctest): leitura, montagem por caso, combinação, `rds.dll`.

**Equivalência dos modelos (importante).** O aplicativo gira os perfis L em 45° por padrão (abas alinhadas aos eixos locais); o Mecway não tem produto de inércia nem orientação da seção de barras (conferi por experimento: a regra dele é fixa, e atributos de orientação não têm efeito). Para os dois programas verem a mesma rigidez, os L foram girados +45° no `.dat` (`[MEMBER_PLACEMENT]`, eixos principais alinhados aos eixos locais) e no Mecway Iyy/Izz foram trocados (regra do Mecway: deslocamento no plano vertical da barra usa iyy; no Structura usa Iz). As propriedades vêm do solver do Structura (`sections_effective.csv`), não do `.dat`: o aplicativo recalcula as inércias a partir da geometria dos perfis L.

| Análise | Structura | Mecway | diferença |
|---|---|---|---|
| Estática (peso+antenas+vento): deslocamento no topo (x, z) | 49,082 mm, −2,0749 mm | 49,082 mm, −2,0749 mm | 3e-6 do máximo (maior diferença nodal) |
| Reação total (Fx, Fz) | −23,9 kN, 41,112 kN | −23,9 kN, 41,112 kN | iguais |
| Flambagem linear, modos 1 a 4 (fator) | 3,0214 / 3,7224 / 3,9181 / 5,5215 | 3,0220 / 3,7246 / 3,9186 / 5,5221 | −0,02 % / −0,06 % / −0,01 % / −0,01 %; MAC = 1,000 |
| Frequências, modos 1 a 6 (Hz) | 3,4473 (x2) / 7,3603 / 9,0322 / 10,2296 (x2) | 3,4473 (x2) / 7,3602 / 9,0321 / 10,2295 (x2) | < 0,001 % |

Os modos 3 e 4 de frequência têm MAC = 1,000; nos pares degenerados (1–2 e 5–6) a orientação dentro do subespaço é arbitrária, por isso o MAC individual é baixo e a frequência coincide. O modo 1 de flambagem é local (uma diagonal da face).

Prints: `docs/validacao_mecway/torre/prints/torre_{estatica,modal1,modal3,flambagem1}_{structura,mecway,comparacao}.png` (barras em tubos, escala em faixas, estrutura indeformada em cinza). Como nos demais, o do Mecway é o resultado dele renderizado pelo Structura.

Antes da equivalência de orientação, a mesma comparação dava −9 % a −14 % na flambagem e −17 % em modos superiores: o desvio era só a convenção de eixos, não de solver.

## 4. Arquivos salvos

- Structura: `docs/validacao_mecway/torre/torre_telecom.dat` (e `torre_geom.csv`, gerador `gen_torre.py`).
- Mecway: `docs/validacao_mecway/torre/modelos_mecway/torre_{estatica,flambagem,modal}.liml` (gerador `gen_torre_mw.py`); BucklingPlate original e resultados em `docs/validacao_mecway/curvas/modelos_bucklingplate/`.
- Resultados: `docs/validacao_mecway/torre/resultados/`.

## 5. Limites

- O `.dat` da torre usa L com eixos principais alinhados (modelagem de equivalência); em projeto real o L fica a 45°, que o Structura calcula e o Mecway não consegue representar.
- A comparação de esforços internos das barras não foi feita (o Mecway não expõe cortante, momento e axial de barra por script).
- Os prints do Mecway são renderizações dos resultados dele, não capturas da janela do Mecway.
- Segunda ordem e não linear da torre não foram comparados (o Structura não tem não linear de casca; a segunda ordem de barras existe e não entrou aqui).
- Sem a janela aberta, a visualização dos modos de casca no aplicativo não foi conferida a olho.
