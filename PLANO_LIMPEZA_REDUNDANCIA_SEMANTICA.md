# Plano de Limpeza de Redundancia Semantica

## Objetivo
Remover redundancias semanticas (mesmo comportamento com implementacoes parecidas) de forma incremental, sem alterar features, contratos publicos ou fluxo funcional.

## Escopo da varredura
- app: servicos, repositorio in-memory e utilitarios de IDs.
- ui: dialogs, handlers de carga e builders de painel.
- viz: mapeadores de visuais de carga/apoio.
- core: modelos com padroes repetidos (identificacao/validacao).
- tests: suites com boilerplate repetitivo.

## Redundancias confirmadas
1. Geracao de externalId repetida em servicos/utilitarios.
2. Busca linear findByExternalId repetida por tipo no repositorio.
3. Atribuicao de propriedades de barra repetida (material/secao/conjunto).
4. Utilitarios de combo duplicados em dialogs de propriedades.
5. Logica de validacao de carga nula duplicada.
6. Reconstrucao repetida de QHash/lookups em UI/Viz.
7. Padroes de identificacao/validacao semelhantes em modelos core.
8. Casos de teste repetitivos para cenarios muito parecidos.

## Estrategia incremental

### Fase 0 - Baseline de seguranca
- Executar build e testes existentes.
- Registrar comportamento esperado para fluxos sensiveis:
  - IDs externos,
  - atribuicao de propriedades,
  - aplicacao/remocao de cargas e apoios,
  - persistencia DAT,
  - chamada de analise estrutural.

### Fase 1 - Quick wins (baixo risco)
- Centralizar helper de ComboBox (populate + index by UUID) e reutilizar em dialogs.
- Consolidar verificacao de valores nulos de carga em helper unico.
- Reduzir duplicacao em BarService para atribuicoes unitarias via caminho unificado interno.

Status: **implementado (itens de baixo risco)**.

### Fase 2 - IDs externos
- Extrair helper generico para nextExternalId (max + 1).
- Consolidar ensureExternalIds para materiais/secoes/barras.
- Manter regra atual: somente IDs <= 0 recebem novo valor.

Status: **implementado**.

### Fase 3 - Lookup maps e repository
- Centralizar findByExternalId no repositorio sem alterar semantica.
- Centralizar construcao de lookup maps (QHash) em UI/Viz.
- Sem alterar regra de filtro/ordem; apenas remocao de duplicacao.

Status: **implementado**.

### Fase 4 - Core + testes
- Avaliar extracao segura de utilitarios comuns de modelo (id/externalId/validacao).
- Reduzir boilerplate de testes com helpers/data-driven.
- Adicionar testes de regressao focados nas areas refatoradas.

Status: **implementado**.

### Fase 5 - Hardening
- Rodar suite completa e smoke tests manuais de fluxo principal.
- Fazer rollout em commits pequenos por subfase.
- Reverter somente subfase com divergencia, se necessario.

Status: **implementado**.

Evidencias desta fase:
- Build release executado com sucesso apos consolidacao de todas as fases.
- Suite completa executada com sucesso (5/5):
  - TestCoreModels
  - TestRepositoryAndServices
  - TestIdGeneratorService
  - TestSceneControllerFacade
  - TestArchitectureBoundaries
- Smoke de inicializacao da aplicacao executado (launch + encerramento controlado) sem erro imediato.

Plano de rollout em commits pequenos (por subfase):
1. Commit A - UI quick wins e carregamento de propriedades
   - src/ui/DialogComboUtils.h
   - src/ui/BarPropertiesDialog.cpp
   - src/ui/BarPropertiesDialog.h
   - src/ui/AssignBarPropertiesDialog.cpp
   - src/ui/AssignBarPropertiesDialog.h
   - src/ui/MainWindowLoads.cpp
   - src/ui/MainWindowProperties.cpp
2. Commit B - App IDs, lookups e repositório
   - src/app/ExternalIdUtils.h
   - src/app/LookupUtils.h
   - src/app/NodeService.cpp
   - src/app/BarService.cpp
   - src/app/services/IdGeneratorService.cpp
   - src/app/InMemoryModelRepository.h
3. Commit C - Viz/UI mappers
   - src/viz/LoadSupportVisualMapper.cpp
   - src/ui/PropertiesPanelEntryBuilder.cpp
4. Commit D - Core model cleanup
   - src/core/model/ModelValidationUtils.h
   - src/core/model/Material.h
   - src/core/model/Section.h
   - src/core/model/Node.h
   - src/core/model/GridLine.h
5. Commit E - Test hardening
   - src/tests/CMakeLists.txt
   - src/tests/TestCoreModels.cpp
   - src/tests/TestRepositoryAndServices.cpp
   - src/tests/TestIdGeneratorService.cpp
6. Commit F - Documentacao
   - PLANO_LIMPEZA_REDUNDANCIA_SEMANTICA.md

## Criterios de nao regressao
- Nao alterar assinatura publica de APIs usadas externamente.
- Nao alterar sinais Qt emitidos nem ordem de efeitos colaterais.
- Nao alterar convencoes de externalId para export/import DAT.
- Nao alterar regras de mapeamento de cargas/apoios para visualizacao.

## Arquivos principais monitorados
- src/app/BarService.cpp
- src/app/NodeService.cpp
- src/app/InMemoryModelRepository.h
- src/app/services/IdGeneratorService.cpp
- src/ui/BarPropertiesDialog.cpp
- src/ui/AssignBarPropertiesDialog.cpp
- src/ui/MainWindowLoads.cpp
- src/ui/MainWindowProperties.cpp
- src/ui/PropertiesPanelEntryBuilder.cpp
- src/viz/LoadSupportVisualMapper.cpp
- src/core/model/Node.h
- src/core/model/Bar.h
- src/core/model/Material.h
- src/core/model/Section.h
- src/tests/TestCoreModels.cpp
- src/tests/TestRepositoryAndServices.cpp

## Implementacao iniciada nesta iteracao
1. Extraido helper comum de combos:
   - src/ui/DialogComboUtils.h
2. Dialogs atualizados para reutilizar helper:
   - src/ui/BarPropertiesDialog.cpp
   - src/ui/AssignBarPropertiesDialog.cpp
3. Duplicacao reduzida em atribuicao de propriedades de barra:
   - src/app/BarService.cpp
4. Verificacao de carga nula consolidada em helper local:
   - src/ui/MainWindowLoads.cpp
5. Helper compartilhado de externalId criado e aplicado:
   - src/app/ExternalIdUtils.h
   - src/app/NodeService.cpp
   - src/app/BarService.cpp
   - src/app/services/IdGeneratorService.cpp
6. Busca por externalId consolidada no repositorio in-memory:
   - src/app/InMemoryModelRepository.h
7. Construcao de lookup maps consolidada no mapper de visuais:
   - src/viz/LoadSupportVisualMapper.cpp
8. Construcao de lookups e contagens consolidada no builder do painel:
   - src/ui/PropertiesPanelEntryBuilder.cpp
9. Builders de opcoes e infos de dialogo consolidados em helper generico:
   - src/ui/MainWindowProperties.cpp
10. Padrao repetido de erase/remove_if consolidado em helper local de cargas/apoios:
   - src/ui/MainWindowLoads.cpp
11. Metodos repetidos de rebuild de indice consolidados em helper template:
   - src/app/InMemoryModelRepository.h
12. Validacoes de modelos core consolidadas em utilitario compartilhado:
   - src/core/model/ModelValidationUtils.h
   - src/core/model/Material.h
   - src/core/model/Section.h
13. Fechamento de pendencia de Fase 2 para barras via callback de atribuicao:
   - src/app/ExternalIdUtils.h
   - src/app/services/IdGeneratorService.cpp
14. Fechamento de pendencia de Fase 3 com helper compartilhado de lookup/count em camada permitida:
   - src/app/LookupUtils.h
   - src/viz/LoadSupportVisualMapper.cpp
   - src/ui/PropertiesPanelEntryBuilder.cpp
15. Conversao Vector3 -> array desduplicada via API central de Vector3:
   - src/core/model/Node.h
   - src/core/model/GridLine.h
   - src/core/model/Vector3.h
16. Boilerplate de setup em testes de servicos consolidado com fixtures locais reutilizaveis:
   - src/tests/TestRepositoryAndServices.cpp
17. Boilerplate de asserts de coordenadas consolidado em helpers locais:
   - src/tests/TestCoreModels.cpp
18. Execucao de todas as classes de teste garantida em arquivos com multiplas suites:
   - src/tests/TestCoreModels.cpp
   - src/tests/TestRepositoryAndServices.cpp
19. Testes de regressao adicionados para IDs externos e delegacoes de BarService:
   - src/tests/TestRepositoryAndServices.cpp
   - src/tests/TestIdGeneratorService.cpp
20. Nova suite registrada no CTest para cobranca explicita de IdGeneratorService:
   - src/tests/CMakeLists.txt
