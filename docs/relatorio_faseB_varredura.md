# Fase B: varredura de perfil, espessura por segmento, casca curva e escala de cores em faixas

Data: 2026-11-23. Só aplicativo; DLLs, `.enc` e executáveis comerciais intactos (hashes iguais: `f40c09a9d032`, `09ffc08a6001`, `c3a9b8b2c3b6`, `ff6c9550e2d2`, `04298a1a0bff`, `61c4812938f6`). `ctest` completo em `build-dev`: 27/27.

## 1. Escala de cores em faixas (pedido adicional)

Antes: cor por vértice interpolada pela GPU (transição suave). Agora, nas faces coloridas por valor, a faixa de valores é dividida em **10 degraus** (`kColorBands`) de cor constante, estilo Mecway, e a fronteira entre degraus segue a curva de nível dentro de cada elemento (textura 1D de 250 cores, 25 por faixa, sobre o valor interpolado).
- `ShellVisual`: `colorBand`, `bandColor` (degraus sobre a escala arco-íris, do azul ao vermelho); `RenderMesh::colors` continua contínua (usada por quem não desenha por textura).
- `ShellRenderer`: tabela de cores em faixas e `InterpolateScalarsBeforeMapping`; valor inválido (NaN) mantém o cinza. Sem valores, cores RGB diretas como antes. A atualização só de cores (sem reconstruir a geometria) continua valendo.
- Legenda (`ColorLegendOverlay`, `ShellDialog`): os mesmos degraus e **11 rótulos**, um por fronteira de faixa. A barra é preenchida por linha de pixels (o `drawLine` com antialiasing borrava a fronteira).
- Testes: `colorBandsAreHardSteps` (limites das faixas, extremos azul e vermelho, as 10 cores aparecem na legenda sem cor intermediária) e a contagem de rótulos do teste da legenda.
- Não alterei o desenho das barras (esforços/deformada das barras): só as cascas.

## 2. Varredura de perfil por caminho (`Kind::Sweep`)

- Dados: `Object::path` (caminho 3D) além de `polygon` (perfil 3D, posicionado onde foi desenhado). Parâmetros: `t`, `closed` (perfil), `pathClosed` (caminho), `twist` (graus), `flip`, `tSeg<n>`. Gravado no `.dat` (`PATHPT`), na impressão de geometria (`fingerprint`) e deslocado por `translateObject`.
- Malha: referencial de transporte paralelo (sem torção própria), junta em meia-esquadria nos vértices do caminho, torção distribuída linearmente (em caminho fechado a torção é ajustada para o referencial fechar), nós fundidos nas bordas e espessura por segmento do perfil. Normal coerente em toda a superfície (mesma do equivalente em extrusão).
- Interface: botão **Varredura** na faixa de cascas (desenha o perfil, depois o caminho; Enter termina, C fecha), linhas de espessura por segmento e tabelas de perfil e caminho no Editor de cascas, atração (snap) nos pontos do caminho.
- Diagnóstico de empenamento: vale a verificação de qualidade já existente (empenamento, razão de aspecto, ângulos, normais). Uma faixa torcida 90° em 4 m é acusada com elementos grandes e a fração de elementos empenados cai ao refinar (teste); a junta muito fechada (> 120° entre segmentos) gera aviso na malha.
- Testes novos (`TestShell`): varredura reta = extrusão (mesma malha e normais), duto circular num arco de 90° (área de Pappus a 1%, bocas livres exatas, sem erro de qualidade), toro de caminho fechado (nenhuma aresta livre, área a 1%), torção diagnosticada e melhorando com o refino, espessura por segmento em revolução, extrusão e varredura, deslocamento do caminho junto com o objeto; e a ida e volta do `.dat` com a varredura (`TestProjectIoAndSolver`).

## 3. Espessura por segmento (`tSeg<n>`)

Vale em Revolution, Extrusion (polilinha) e Sweep: cada segmento do perfil/linha com a sua espessura, senão `t`. Na revolução, um perfil por segmento (nós das juntas fundidos).

## 4. Casca curva: Scordelis-Lo por revolução parcial (80°) contra o Mecway e a literatura

Cilindro R = 25, L = 50, t = 0,25, E = 4,32e8, ν = 0, peso 90 N/m², diafragmas rígidos nas extremidades curvas (uy = uz = 0), bordas retas livres. Flecha vertical no meio da borda livre (`resultado_scordelis.json`):

| Divisões axiais | Structura | Mecway 28 | dif. Structura x Mecway | Structura x 0,3024 |
|---|---|---|---|---|
| 4 | 0,3954 | 0,3571 | 11% | +30,7% |
| 8 | 0,3262 | 0,3070 | 6,3% | +7,9% |
| 16 | 0,3042 | 0,2980 | 2,1% | +0,6% |
| 32 | 0,3015 | 0,3028 | 0,4% | −0,3% |

Taxa observada 3,06 e extrapolado de Richardson 0,3012. **A referência 0,3024 é valor de memória, não conferido nesta sessão.**

Leitura: os dois programas convergem para o mesmo valor, mas **não coincidem nas malhas grossas** (nas placas planas coincidiam a 1e-13). A diferença vem da formulação do elemento em casca curva e facetada (o Structura usa modos incompatíveis de Wilson na membrana; o Mecway não os usa), não de geometria nem de carga: o Structura chega mais perto da referência nas malhas de 4 a 16. A geometria da revolução parcial está correta (ambos reproduzem a flecha e o valor máximo de deslocamento, 0,3408 contra 0,3424).

Prints: `docs/validacao_mecway/curvas/prints/` (`scordelis_32_*`, `duto_varredura_structura.png`).

## 5. Não feito / não validado

- Furos em contornos, refino local e sweep por perfil armado (Fase A) não fazem parte desta fase.
- Varredura não foi comparada com o Mecway (só geometria: área, fechamento, normais e diagnóstico); o benchmark de casca curva usa revolução.
- O botão e o fluxo de desenho da varredura estão compilados e ligados, mas **não foram exercitados na janela**.
- Pinched cylinder e hemisfério com furo (também previstos) ficam para depois.

Reprodução: `docs/validacao_mecway/curvas/` (`sl_dump.cpp`, `gen_sl.py`, `compare_sl.py`, `sl_shots.cpp`, modelos `.liml`).
