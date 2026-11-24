# Fase 2 — Seleção de nós de cascas na vista principal (infraestrutura)

Base das próximas fases (apoios por clique, cargas desenhadas). Nenhuma DLL, formulação ou licenciamento foi alterado; não há formato de arquivo novo.

## Implementado
| Parte | Descrição |
|---|---|
| `RenderMesh::vertexNode` | índice do nó de `Mesh::nodes` em cada vértice desenhado. |
| `src/shell/ShellSelection.{h,cpp}` (sem Qt) | `NodeTable` (nós únicos: posição na vista, coordenadas **sem deformada em metros**, objeto); `ScreenProjector`; `pickNearestNode` (tolerância em px; empate vai ao mais próximo da câmera); `nodesInRect`; `NodeSelection` (Substituir/Acrescentar/Alternar); `NodeRef` + `makeNodeRefs`/`resolveNodeRefs` (referência **persistente por coordenadas**). |
| `ShellRenderer::setSelectedNodes` | pontos laranja sobre a malha; não participam do picking; não forçam reconstrução da geometria. |
| `MainWindow` | ação **Selecionar nós** (aba Cascas, grupo Análise): clique no nó (Ctrl alterna, Shift acrescenta), caixa arrastada, Esc limpa. Exclusiva com **Consultar resultado**. Habilitada com a malha de cascas visível (não exige resultados). |

## Decisões
1. **Seleção transitória e por coordenadas.** Guarda-se o conjunto de índices só para o desenho; o que sobrevive a nova malha são as `NodeRef`. Ao regenerar a malha (editar, remalhar, trocar de vista), `reresolveShellNodeSelection` reencontra os nós pela posição. O nó mais próximo só vale dentro de **1e-5 m** (tolerância de fusão de nós); fora disso a referência fica "não resolvida" e é **relatada na barra de status**, nunca trocada por outro nó.
2. **Objeto é só dica.** Nós são compartilhados entre objetos (nós coincidentes); a resolução não usa o objeto.
3. **Oclusão (corrigido após o primeiro uso).** A caixa e o clique ignoram nós escondidos atrás de outra parte das cascas (ex.: o fundo de um cilindro visto de frente). `buildDepthMap` rasteriza os triângulos num z-buffer em software, na resolução da vista (sem OpenGL); `isNodeVisible` compara a profundidade do nó com a da superfície no seu pixel, com tolerância proporcional à inclinação (superfícies rasantes não escondem seus próprios nós) e mínimo na vizinhança 3×3 (silhueta). Triângulos com vértice atrás da câmera são ignorados.
4. Desligar a ferramenta **limpa** a seleção (transitória). Com a ferramenta ativa, o clique é consumido (inclusive no vazio, que limpa); para selecionar barras, desligue-a.
5. A malha agora é guardada na janela principal mesmo sem resultados (legenda e probe continuam exigindo `hasValues`).

## Testes
`TestShellSelection` (14): tabela de nós (únicos, metros, deformada × sem deformada, sem associação), projeção e ponto atrás da câmera, pick (sobre o nó, tolerância, longe, empate por profundidade), caixa (ordem dos cantos), conjunto transitório, referências na mesma malha, **entre malhas de numerações diferentes** (grossa→fina resolve tudo com ids diferentes; fina→grossa relata exatamente os nós inexistentes), nunca "encaixar" em nó distante, e o destaque no renderizador sem afetar o picking.
`TestShellSelection` passou de 14 para 17 casos (oclusão: cilindro visto de lado — frente toda selecionável e nenhum nó do fundo —, superfícies em ângulo rasante sem auto-oclusão, casca na frente de outra). `ctest` (build-dev): **25/25**. Sem warnings novos.

## Limitações
- A ligação na janela principal (ação, clique, caixa, Esc, re-resolução após remalhar, status) **compila e o aplicativo sobe, mas não foi exercitada** (não há teste que construa o `MainWindow`; não houve uso manual). Roteiro sugerido: gerar malha, ativar "Selecionar nós", clicar, Ctrl+clique, caixa, Esc, mudar `meshSize` e ver a seleção seguir.
- Não há seleção de **elementos** (só nós); não é gravada no projeto. A oclusão considera só as cascas (barras e outros objetos não escondem nós); nós exatamente na silhueta podem ser aceitos ou recusados por até ~1 pixel.
- Esta fase não aplica apoios nem cargas: só prepara a seleção.
- Demo: compilada na Fase 1; esta fase não foi recompilada na configuração Demo/release.

## Integridade
DLLs, `.enc` e executáveis comerciais com SHA-256 inalterados. `dist-dev` foi remontado pelo build de desenvolvimento.
