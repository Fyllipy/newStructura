# Cargas de barra com posição, vínculos, ligações e análise não linear

Resumo de uso dos recursos do solver incluídos além do pórtico linear. A formulação está em `Structuradll/docs/FORMULACAO_MATEMATICA.md` (seções 10.4, 10.5 e 15 a 18),
a interface da DLL em `consumeDll.md` e os testes em `Structuradll/tests/solver_tests.cpp` (121 verificações analíticas) e `src/tests/TestProjectIoAndSolver.cpp`.

## 1. Cargas de barra com posição

Comando **Carregamentos > Carga geral em barra** (ou botão direito > *Carga geral em barra…*; sem seleção, escolha as barras na viewport e conclua com Enter).

| Tipo | Dados | Observações |
|---|---|---|
| Trapezoidal / parcial | posições *a* e *b* (m, a partir do nó I, ou % do comprimento), forças por comprimento (qx, qy, qz) em *a* e em *b*, sistema local ou global | *a* = 0 e *b* = L: carga em toda a barra; valores iguais: parcial uniforme; valores diferentes: triangular/trapezoidal |
| Força concentrada | posição *a*, (Fx, Fy, Fz), local ou global | em *a* = 0 ou *a* = L vai direto ao nó |
| Momento concentrado | posição *a*, (Mx, My, Mz) | torque (Mx) não combina com torção com empenamento |
| Temperatura | ΔT uniforme (°C), gradiente em y e em z local (°C/m) | usa o coeficiente de dilatação α do material (aço 12, concreto 10, em 10⁻⁶/°C; editável no cadastro de materiais) |
| Retração | deformação livre ε (‰, negativa encurta) | barra impedida de encurtar fica tracionada: N = E A ε |

Cada carga pertence a um caso de carga e aparece na viewport (faixas trapezoidais com os valores nas pontas, setas e arcos nos pontos de aplicação, linhas vermelhas/azuis para temperatura/retração),
na lista **Cargas da seleção** (editar com duplo clique, excluir), em **Excluir cargas**, no memorial e no relatório de propriedades da barra. Dividir a barra reparte as cargas entre os trechos;
copiar leva as cargas junto; desfazer/refazer vale. Os diagramas de esforços e as flechas usam o equilíbrio do corpo livre com as cargas na posição correta (inclusive o salto de cortante sob a carga concentrada).

Persistência: seção `[MEMBER_ACTIONS]` do `.dat` (e `[MATERIAL_THERMAL]` para α). Fluência não é modelada; recalque de apoio e protensão estão fora do escopo.

## 2. Vínculos e ligações entre nós

**Cálculo > Vínculos e ligações** (selecione os nós antes de abrir): diafragma rígido (mestre, eixo normal, escravos), ligação rígida (corpo rígido de 6 graus, 3 translações, 3 rotações ou 1 grau),
apoio inclinado (direções e1, e2, e3 e graus restringidos), ligação por mola ou elemento de interface (6 leis, ver abaixo) e vínculo linear geral Σ c·u = 0.
Vínculos entram por penalidade (erro relativo da ordem de 10⁻⁷); a força do vínculo de apoio inclinado entra nas reações. Excluir nós remove os vínculos que os citam.

Leis das ligações: linear, só tração, só compressão, folga de tração/compressão, elastoplástica (escoamento e enrijecimento), multilinear (pontos deformação:força) e rígida.
Apoio unilateral (só compressão) e fundação com folga são casos típicos.

## 3. Ligações de extremidade e rótulas plásticas

**Cálculo > Extremidades e rótulas plásticas** (barras selecionadas): rigidez rotacional (semirrígida) em rx, ry e rz de cada extremidade e rótulas plásticas bilineares em ry e rz
(Mp, k elástico, enrijecimento, Ny e expoente para a interação N–M). Os valores sugeridos vêm do perfil e do aço: Mp = fy·Z, k = 1000·EI/L, Ny = fy·A. A articulação das propriedades da barra prevalece sobre a mola.
Divisão de barras mantém as ligações de I no primeiro trecho e as de J no último.

## 4. Análise não linear incremental

**Cálculo > Opções não lineares** define passos, tolerância, critérios (forças, deslocamentos, energia), reduções de passo e o controle: de carga (até um fator) ou de deslocamento (grau do nó até um alvo).
A análise incremental liga-se sozinha quando há rótulas plásticas ou ligações com lei não linear; a opção "Executar sempre" a aplica também ao modelo elástico (2ª ordem passo a passo).
As combinações passam a ser resolvidas uma a uma (cargas ponderadas) — sem superposição. **Cálculo > Curva de capacidade** mostra fator de carga × deslocamento, o estado das rótulas (momento, rotação plástica) e as forças das ligações.
Se a estrutura vira mecanismo antes do fator pedido, o resultado é o do último passo convergido, com aviso.

Restrições: flambagem e modal não se combinam com a análise não linear; barras com torção com empenamento não admitem rótulas; o ramo de amolecimento pós-pico não é traçado.
Cabos, protensão e pré-tensão não foram implementados (fora do escopo pedido).

## 5. Verificação

```
cmake -S Structuradll -B Structuradll/build/vs
cmake --build Structuradll/build/vs --config Release --target solver_tests
Structuradll/build/bin/Release/solver_tests.exe
```

Cobertura analítica: carga pontual, triangular, parcial, momento, torque, temperatura e retração; molas de extremidade; rótula plástica (resposta, controle de deslocamento, mecanismo de colapso 8Mp/L, N–M);
ligações; diafragma, ligação rígida e apoio inclinado; 2ª ordem pelo motor igual à análise padrão. No aplicativo: `TestProjectIoAndSolver` (persistência `.dat`, ponte com a DLL e vento estático).
