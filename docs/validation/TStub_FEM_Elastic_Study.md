# Estudo MEF elástico do T-stub (Fase C4)

> **Atualização (Fase C6.3):** o estudo foi executado depois; ver `TStub_FEM_Elastic_Study_Results.md` (PARCIALMENTE CONCLUÍDA). O texto abaixo é o registro histórico da Fase C.

**Estado (Fase C): NÃO CONCLUÍDA.** Nenhum modelo MEF foi construído nem executado nesta execução. Este documento registra a razão e fixa o protocolo para execução posterior, para que nenhum resultado MEF seja inferido ou presumido. Sem validação profissional.

## 1. Por que não foi executada
1. **Dados do espécime publicado insuficientes para um modelo sólido fiel:** as Tabelas de [P2] não trazem a espessura da alma `t_w` do T-stub (nem a altura/comprimento da alma até a garra), a forma/raio do filete além de `r`, nem as dimensões das arruelas além de `d_w` (diâmetro). Um MEF de "um espécime publicado" exigiria **hipóteses sobre `t_w` e a alma**, que não podem entrar como dado.
2. **Contato (alavanca):** o comportamento do T-stub depende de contato entre as bordas das mesas. Um modelo **linear** sem contato ignora a alavanca; com contato exige solver não linear. O fluxo Structura × Mecway já documentado (`docs/validacao_mecway/`) valida **cascas lineares** e declara **não validados** contato e não linearidade; nenhum modelo de sólidos foi construído com esse fluxo. Atribuir "validação" a um modelo de contato não verificado seria enganoso.
3. **Escopo da evidência:** mesmo um bom MEF elástico só avaliaria a rigidez inicial (flexão da mesa + parafuso). Não valida a capacidade de deformação, as rótulas plásticas nem a ruptura (§7.4 do prompt).

## 2. Protocolo proposto (não executado)
**Modelo A — flexão da mesa sem alavanca (verificação elástica do `0,9 l_eff t_f³/m³` e de `0,5 E b_eff t_f³/m'³`):** meia simetria em `x` (largura) e em `y` (plano da alma); sólidos hexaédricos de 8 nós com modos incompatíveis (`<incompatiblehex8>`, já usado pelo gerador existente) ou 20 nós; ≥ 3 elementos na espessura da mesa; parafuso como mola axial equivalente à `E A_b/L_b` (ou barra) acoplada à região da arruela (`d_w`); sem contato (alavanca fixada por restrição de deslocamento vertical nas bordas, **hipótese idêntica à de [P1]**: reação de compressão positiva conferida a posteriori). Geometria **representativa**, com `t_w` assumido e **declarado como hipótese**; não "o espécime 7".
**Modelo B — com contato** (somente após validar o solver não linear num problema com solução conhecida, p. ex. viga sobre apoio unilateral).
**Convergência:** ≥ 3 malhas (razão 2); comparar deslocamento no ponto de carga, reações (incluindo a reação de alavanca), tensões na linha de rótula e rigidez global `F/δ`; critério: variação < 2 % entre as duas malhas finas.
**Comparação:** rigidez MEF × Piluso/Faella × EN (Fase B), para 3 espessuras de mesa; não ajustar parâmetros para forçar concordância.
**Entregáveis:** arquivos `.liml`, scripts, imagens, tabela de convergência, e este documento atualizado.

## 3. O que falta para executar
(i) definir a geometria do T-stub representativo (com `t_w` explícito como hipótese); (ii) um gerador de malha de sólidos (o existente só gera cascas Q4); (iii) validar o tratamento de contato; (iv) autorização para a extensão não linear, se o Modelo B for desejado.

**Pergunta do §12.7 (o MEF confirmou ou questionou a coerência da rigidez?): sem evidência — NÃO CONCLUÍDA.**
