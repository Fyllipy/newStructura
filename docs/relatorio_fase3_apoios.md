# Fase 3 — Apoios de cascas por clique e visualização dos tipos

Itens C e D do plano. Nenhuma DLL, formulação ou licenciamento foi alterado. Resolve a lacuna da auditoria "apoios por clique em nós; `resolveSupports()` extraído; apoios persistentes por coordenadas".

## Implementado
| Parte | Descrição |
|---|---|
| `Support::Nodes` (seletor novo) | lista de pontos (m) com tolerância; cada ponto procura o nó da malha atual **dentro de `tol`**. Os pontos que sumiram (malha refeita) são **relatados e nunca trocados por outro nó**; "Ponto" (nó mais próximo) continua como estava. |
| `src/shell/ShellSupports.{h,cpp}` | `resolveSupports` (extraído de `ShellJob`, comportamento de Plano/Borda livre/Ponto preservado), `classifySupport`, `supportMarks`. O **solver e a vista usam a mesma resolução**. |
| Projeto (.dat) | linhas `SUPPORTNODE x y z` após o `SUPPORT` correspondente; arquivos antigos abrem sem mudança; versões antigas preservam as linhas desconhecidas. `fingerprint` (desfazer) inclui os pontos. |
| Desenho | `ShellRenderer::setSupports`: glifos 3D por tipo com cores distintas — engaste (cubo, vermelho), articulado (pirâmide, azul), móvel (octaedro, verde), restrição parcial (tetraedro, roxo), mola (zigue-zague, laranja), só compressão (zigue-zague + placa, verde-água). `SupportLegendOverlay` lista os tipos presentes com a quantidade de nós. |
| Faixa Cascas, grupo "Apoios das cascas" | **Aplicar apoio** (diálogo: engaste, articulado, móvel em Z/X/Y, mola com rigidez por área e só compressão com normal, ou graus de liberdade livres), **Remover apoio**, **Mostrar apoios**. |
| Editor de cascas | a tabela de apoios mostra o seletor "Nós (n por clique)". |

## Decisões
1. **Persistência por coordenadas**, com `tol` = 1e-5 m, e não por índice nem por "mais próximo": em malha nova um apoio nunca migra em silêncio para outro nó; o aviso aparece na barra de status e no resultado do `buildJob`.
2. **Aplicar substitui**: os nós selecionados saem de outros apoios por clique antes de receber o novo (um nó não fica com duas definições por clique). Apoios de Plano/Borda/Ponto não são tocados (união das restrições, como já era).
3. **Mesma definição = mesmo apoio**: aplicar o mesmo tipo duas vezes acrescenta pontos ao mesmo `Support`, mantendo a lista curta.
4. A exibição une apoios no mesmo nó (união das restrições; mola prevalece).
5. Mola por clique usa rigidez **por área** × área tributária do nó (o que a API do solver já suporta); não há mola nodal em N/m.
6. As coordenadas gravadas vêm da malha (double), não dos vértices float da vista; a tolerância da re-resolução da seleção acompanha a extensão do modelo.

## Testes
`TestShellSupports` (14): seletores antigos sem mudança (plano, borda, ponto, plano vazio), `buildJob` com plano e mola só-compressão, seletor `Nodes` (nós exatos e aplicação no job), ausentes relatados sem encaixar em nó distante, **remalhagem** (grossa→fina resolve; muito grossa relata), filtro de objeto, `fingerprint`, classificação dos 6 tipos, fusão de apoios no mesmo nó, glifos no renderizador, legenda e diálogo (presets). `TestProjectIoAndSolver` ampliado: ida e volta do `.dat` com dois apoios por nós seguidos.
`ctest` (build-dev): **26/26**. Sem warnings novos.

## Limitações
- **Ligação na janela principal não exercitada** (ações, diálogo, substituição/remoção, redesenho e legenda): compila, os testes cobrem modelo, resolução, persistência, classificação e componentes, mas não há teste que construa o `MainWindow` nem houve uso manual. Roteiro: gerar malha → Selecionar nós → Aplicar apoio → ver glifos e legenda → refazer malha (mudar `meshSize`) → conferir aviso e apoios → salvar e reabrir → reanalisar.
- Os glifos têm tamanho proporcional à diagonal do modelo e orientação fixa (descendo em Z); não giram com a normal (exceto a mola só-compressão).
- Apoios por Plano/Borda/Ponto aparecem nos glifos, mas só os por clique são removidos pelo botão; os demais continuam no Editor de cascas.
- A seleção por clique ainda é só de nós (sem arestas/faces). Deslocamento prescrito e apoio inclinado continuam fora (a API só aceita `fixed[6]`).
- Resultados antigos ficam inválidos ao aplicar/remover apoios (a vista volta à malha sem resultados).
- Demo/release sem `DEV_BUILD`: esta fase não foi recompilada nessas configurações.

## Integridade
DLLs, `.enc` e executáveis comerciais: SHA-256 inalterados. `dist-dev` remontado (16:12).
