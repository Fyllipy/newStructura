# Fase 4 — Cargas de cascas dadas por clique (força nodal, linear e de superfície)

Item E do plano. Nenhuma DLL, formulação ou licenciamento foi alterado.

## Implementado
| Parte | Descrição |
|---|---|
| Tipos de carga (`Load::NodalPoints`, `LinePoints`, `RegionPoints`) | gravados por **coordenadas** (`Load::points`, m), como os apoios por nós. Força em cada nó (N, global); carga por comprimento (N/m, global) sobre as arestas cujos **dois** nós estão no conjunto; carga de superfície sobre os elementos cujos **quatro** nós estão no conjunto (força por área global, por área projetada em planta, ou pressão normal). |
| `src/shell/ShellLoads.{h,cpp}` | `resolvePointLoad` (nós, arestas sem repetição, elementos; pontos ausentes são contados, nunca trocados por nó distante, tolerância 1e-5 m), `pointLoadEffect` (forças nodais equivalentes), `loadArrows`. |
| `buildJob` | aplica as cargas por pontos ao caso pelo nome; forças nodais `q·L/2` nos dois nós de cada aresta e `q·A/4` por nó do elemento (como a carga de área existente); modo normal vira pressão por elemento. Avisos: pontos que sumiram, linha sem arestas, região sem elementos. |
| Projeto (.dat) | linhas `LOADNODE x y z` após o `LOAD`; arquivos antigos abrem sem mudança; `fingerprint` (desfazer) inclui os pontos. |
| Vista | setas (corpo + cabeça) com comprimento proporcional ao maior valor de cada tipo: roxo = força nodal, laranja = carga linear, verde = carga de superfície. |
| Faixa Cascas, grupo "Cargas das cascas" | **Aplicar carga** (diálogo: caso, tipo, componentes em kN, kN/m, kN/m² ou kPa; tipos sem alvo ficam desabilitados e o diálogo mostra "n nós, n arestas, n elementos"), **Remover carga**, **Mostrar cargas**. |
| Editor de cascas | a tabela de cargas lista os novos tipos. |

## Decisões
1. Alvos pelas coordenadas, como nos apoios: a carga acompanha a malha quando ela é refeita e **avisa** quando um ponto não existe mais.
2. Linha = arestas de elemento entre nós selecionados (vale para linhas internas, compartilhadas por dois elementos: contadas uma vez). Região = elementos **completamente** selecionados (um canto isolado não vira carga).
3. Mesma carga (caso, tipo, valores) em outros nós **junta os pontos** na mesma entrada; senão, entrada nova (cargas somam).
4. Não há momento distribuído nem carga em ponto qualquer do elemento (só nós, arestas e elementos), conforme a limitação da API do solver (nodal 6 GDL + pressão por elemento).

## Testes
`TestShellLoads` (12): resolução (nós, ausentes sem encaixe, filtro por objeto), **ΣF = q·L** em linha interna (4 arestas compartilhadas) e linha sem arestas, **ΣF = q·A** em região, os três modos de superfície em placa inclinada (global: q·A; projetada: q·A·|nz| = q·área em planta; normal: Σp·A), casos separados, `fingerprint`, setas e renderizador, diálogo, e **pelo solver real** a soma das reações verticais e horizontais = −(forças aplicadas) nos quatro casos (linha, nodal, superfície, pressão normal). `TestProjectIoAndSolver` ampliado: ida e volta do `.dat` com duas cargas por pontos seguidas.
`ctest` (build-dev): **27/27**. Sem warnings novos.

## Limitações
- **Ligação na janela principal não exercitada** (ações, diálogo, substituição, remoção, setas): compila e os testes cobrem modelo, resolução, equilíbrio, persistência e componentes, mas não há teste que construa o `MainWindow` nem houve uso manual. Roteiro: gerar malha → Selecionar nós → Aplicar carga (linha de nós / região de nós) → ver setas → reanalisar e conferir reações → refazer malha → salvar e reabrir.
- As cargas por clique entram **só na análise MEF das cascas** (isolada ou acoplada às barras). No cálculo de quinhões para barras (modo não acoplado) são ignoradas, como qualquer tipo não previsto lá.
- Setas só do tipo/caso do modelo atual (todos os casos juntos, sem filtro por caso); a pressão normal usa a normal do elemento.
- Região com malha mista (elementos de objetos diferentes) funciona por nós compartilhados; não há seleção de "faces" independente dos nós.
- Demo e release sem `DEV_BUILD` não foram recompilados nesta fase.

## Integridade
DLLs, `.enc` e executáveis comerciais inalterados. `dist-dev` remontado.
