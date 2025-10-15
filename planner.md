# Planejamento de Evolução — StructuraRibbon3D

Este documento descreve, em alto nível, as etapas para evoluir o aplicativo para um modelador/solucionador estrutural 3D com UI inspirada no Solid Edge.

---

## Visão Geral de Arquitetura

- Núcleo orientado a objetos
  - `Node` (nó): id, coords (x,y,z), seleções, restraints, loads aplicadas, meta (tag/notes).
  - `Bar` (membro): id, nós i/j (refs), material, seção, comprimento/linha, cargas distribuídas, estado de seleção.
  - `Grid` e `GridLine`: passo global (dx,dy,dz), linhas discretas por eixo com ids; highlight/ghost state.
  - `Material`: id (uuid + externo), E, G, nome.
  - `Section`: id (uuid + externo), A, Iz, Iy, J, nome.
  - `Load` hierarquia: `NodalLoad` (Fx,Fy,Fz,Mx,My,Mz), `DistributedLoad` (qx,qy,qz, sistema L/G). Futuro: térmica, massa, etc.
  - `SelectionModel`: mantém conjunto atual de entidades selecionadas, eventos de mudança.
  - `Project`: agrega coleções, metadados, preferências de visualização e fornece serialização (dat/ini/dxf no futuro).

- Camada de renderização (ponte para VTK)
  - `SceneController` permanece como fachada de VTK, mas passa a receber/emitir entidades do núcleo; desenha nós (esfera/quadriculada), barras (linhas/tubos), suportes (glifos), cargas (setas/setas distribuídas), grid e highlights/ghosts.
  - Mapeamento OOP↔VTK via adapters: `NodeAdapter`, `BarAdapter`, `GridAdapter`, `GlyphLibrary`.

- Persistência
  - `.dat` (já suportado): expandir para incluir restraints e exibição de cargas.
  - `.ini` de materiais/seções (banco do usuário) e `.ini` de configurações (preferências UI e tamanhos de glifos, cores, etc.).
  - Importação `.dxf`: parser em módulo próprio; suporta polylines/lines para gerar nós/barras.

- UI
  - QuickAccessBar integrada (já integrada): adicionar botão “Configurações”.
  - Ribbon com tabs: `Início | Carregamentos | Visualização`.
  - Barra lateral (não modal) de ferramentas com ícones verticais (inicialmente “Propriedades”).
  - Painel de Propriedades (dock/widget) contextual para nó/barra/grid (leitura/edição básica).
  - Barra inferior (status/visual): espelhar comandos de visualização e mover “Snap ao grid” para lá.
  - Janela lateral “Entes” (dock): lista de nós e barras, agrupamento e multiseleção.

---

## Etapas (fatiamento por entregas)

### Etapa 1 — Refatoração OOP mínima e seleção unificada
- Introduzir classes `Node`, `Bar`, `Material`, `Section`, `GridLine` no núcleo.
- Adaptar `SceneController` para aceitar/retornar ids dessas entidades, mantendo render atual.
- Implementar `SelectionModel` (nó/bar/misto), com highlight visual consistente.
- Critérios de aceite
  - Adição/remoção/seleção de nós e barras funciona como hoje.
  - Serialização `.dat` continua operando (backwards compat) com o novo modelo interno.

### Etapa 2 — Barra lateral e painel de Propriedades
- Adicionar coluna de botões fixos (dock/overlay) à direita: primeiro botão “Propriedades” (apenas ícone de “tools”).
- Clicar em “Propriedades” abre/ancora um painel não modal mostrando:
  - Nó: id, coords, restraints (check), cargas (lista), material/ seção (N/A).
  - Barra: id, nós i/j, comprimento, material, seção, cargas distrib.
  - Grid: passo, contagem, plano/linha destacada se houver highlight.
- Edição inline para campos básicos (coords do nó, material/seção da barra) com undo/redo simples.
- Critérios
  - Selecionar item atualiza o painel.
  - Multi‑seleção mostra campos em estado “variam” e permite atribuição em lote.

### Etapa 3 — Inserção de nó (duas entradas) e snap na barra inferior
- Ribbon `Início`: separar “Inserir nó (Coordenadas)” e “Inserir nó (Tela)”.
- Mover checkbox “Snap ao grid” para a barra inferior; estado global.
- Melhorar picking na view plane e snapping consistente.
- Critérios
  - Ambos os fluxos inserem corretos; snap visível/alternável no rodapé.

### Etapa 4 — Linhas individuais de grid (add/delete) com ghost e highlight
- Ações: “Adicionar linha X/Y/Z” (habilitar apenas se grid existente para modo com referência; senão, modo livre por clique).
- Lógica
  - Ao acionar, mostrar “ghost” (linha translúcida) movimentando ao longo do plano selecionado.
  - Digitável: distância em relação à linha existente mais próxima (essa linha fica em highlight).
  - Confirmar para persistir.
- “Deletar linha de grid”: destacar a linha sob cursor/seleção, exibir confirmação não modal ancorada ao lado da linha (tooltip persistente), manter o restante.
- Critérios
  - Ghost segue snapping; highlight correto; confirmação não bloqueia a UI.

### Etapa 5 — Aba “Carregamentos” e aplicação de forças
- Ribbon `Carregamentos`: botões “Força concentrada (nós)” e “Distribuída (barras)”.
- Fluxo
  - Selecionar 1..N nós/barras; abrir popover/dock para valores (com unidades); aplicar e desenhar glifos.
- Glifos VTK: setas para forças; par de setas/círculos para momentos; rótulo com valor/unidade.
- Critérios
  - Visual em tela, transformação com zoom/manter tamanho em pixels (VTK glyph billboard/actors escalados por câmera).

### Etapa 6 — Restrições nodais (Início)
- Ação “Restrições”: diálogo/dock para escolher DOFs fixos (UX, UY, UZ, RX, RY, RZ) e aplicar a multi‑seleção.
- Desenhar ícones/triângulos/rolamentos conforme padrão de estruturas (glifos distintos por DOF fixo/solto).
- Critérios
  - Persistência no projeto; visual muda conforme DOFs marcados.

### Etapa 7 — Aba “Visualização” e espelhamento no rodapé
- Mover comandos de visualização para tab `Visualização` (e criar botões espelhados no rodapé: reset, extents, projeção, eixos, tema).
- Rodapé com slider de escala dos glifos (nos/cargas/suportes) e controle de snap.

### Etapa 8 — Listas de Materiais/Seções e banco do usuário `.ini`
- Botões na `Início`: “Materiais” e “Seções” listam/filtram/duplicam/remoção.
- Exportar/atualizar banco do usuário: `.ini` (pasta Documents/Structura). Atualizar sempre que criar/editar/remover.
- Importar do `.ini` para preencher combos.

### Etapa 9 — Configurações (QuickAccessBar)
- Botão “Configurações” na quick bar abre preferências não modais:
  - Tamanho base dos glifos; escala de texto; paleta (clara/escura/azulada SE); tolerâncias de snap/pick.
  - Persistir em `.ini` de configurações; botão “Restaurar padrão”.

### Etapa 10 — Janela lateral “Entes”
- Dock à esquerda listando Nós e Barras (árvore): grupos por tipo; filtros/ordenação; busca.
- Seleção na árvore espelha na cena e vice‑versa (SelectionModel como fonte única de verdade).
- Suporta multiseleção e operações em lote (atribuir material/seção/carga/restrição).

### Etapa 11 — Importação DXF
- Parser básico de DXF (LINETYPE/LINE/POLYLINE) para geometria 3D simplificada:
  - Gera nós em vértices; barras entre pontos consecutivos; opção para fundir nós coincidentes por tolerância.
- Mapear unidades do DXF; diálogo de pré‑visualização com contagem.

### Etapa 12 — Qualidade, Undo/Redo e preparação para solver
- Introduzir Command pattern para ações (inserção/edição/remover), integrando Undo/Redo (pilha em memória).
- Validadores (comprimento mínimo de barra, duplicatas, materiais/seções atribuídos, etc.).
- API clara para futura integração do solver (montagem de K, BCs e cargas) sem tocar na UI.

---

## Detalhes de UI (resumo executivo)

- QuickAccessBar (já integrada ao frame):
  - Ícones menores, título centralizado, botões de janela; adicionar “Configurações”.
- Ribbon
  - Tabs: `Início | Carregamentos | Visualização`.
  - “Inserir nó (Tela)” e “Inserir nó (Coords)” separados; “Snap ao grid” no rodapé.
- Barra lateral direita:
  - Ícones verticais (primeiro: “Propriedades”). Não modal.
- Rodapé
  - Controles de visualização + snap + escala de glifos.

---

## Persistência e formatos

- `.dat`: manter compatível; ampliar para restraints/cargas exportáveis e ids estáveis.
- `.ini` banco usuário (Materiais/Seções):
  - Seções por nome/id; sempre sincronizado após alterações.
- `.ini` de Configurações: preferências de UI/visualização e defaults de snapping/picking.
- `.dxf` import: módulo isolado, com tolerância de fusão e preview.

---

## Riscos e mitigação

- Desalinhamento VTK↔modelo: introduzir camada adapter e testes manuais por feature.
- Complexidade de seleção: centralizar em `SelectionModel` e eventos.
- Performance com muitos glifos: usar VTK Glyph3D/Impostors e Level‑of‑Detail.
- Undo/Redo: começar com granulação de comandos por ação do usuário.

---

## Entregas e critérios de aceite

Cada etapa terá:
- Escopo fechado + checklist visual/funcional.
- Persistência validada (quando aplicável).
- Sem regressão das funções já entregues.

---

## Roadmap sugerido (ordem e esforço relativo)

1) Refatoração OOP mínima + seleção (M)
2) Propriedades (M)
3) Inserção e snap (P)
4) Grid (linhas, ghost, delete) (M)
5) Carregamentos (M/G)
6) Restrições (M)
7) Visualização + rodapé (P)
8) Listas materiais/seções + banco `.ini` (P)
9) Configurações (P)
10) Janela “Entes” (M/G)
11) Importação DXF (M/G)
12) Undo/Redo + preparação solver (M)

(P = pequeno, M = médio, G = grande)

---

## Observações finais

- Manteremos a compatibilidade com os arquivos `.dat` já gerados.
- O design orientado a objetos facilitará a etapa futura de solver estático e dinamização dos BCs/cargas.
- A UI seguirá o modelo do Solid Edge, priorizando fluxo de trabalho e respostas visuais claras.

