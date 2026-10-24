# Como Consumir a `rds` (rds.dll)

Este documento descreve como chamar a DLL gerada pelo projeto (`rds.dll`) a partir de outro programa, em memoria, sem uso de arquivo `.txt`.

## Assinatura Exportada

Header: `src/bridge_dll/interface_dll.h`

```cpp
extern "C" int structura_solve(
    int materialCount,
    const int* materialIds,
    const double* materialE,
    const double* materialG,
    int sectionCount,
    const int* sectionIds,
    const double* sectionA,
    const double* sectionIz,
    const double* sectionIy,
    const double* sectionJ,
    int nodeCount,
    const int* nodeIds,
    const double* nodeX,
    const double* nodeY,
    const double* nodeZ,
    const int* nodeRestraints6,
    int memberCount,
    const int* memberIds,
    const int* memberNodeI,
    const int* memberNodeJ,
    const int* memberMaterialId,
    const int* memberSectionId,
    int nodalLoadCount,
    const int* nodalLoadNodeIds,
    const double* nodalLoadValues6,
    int memberLoadCount,
    const int* memberLoadMemberIds,
    const int* memberLoadCoordSystem,
    const double* memberLoadQx,
    const double* memberLoadQy,
    const double* memberLoadQz,
    double* outNodeDisplacements6,
    double* outNodeReactions6,
    double* outMemberLocalForces12,
    double* outMemberAuxK3,
    int errorBufferSize,
    char* errorBuffer);
```

Retorno:
- `0`: sucesso
- `!= 0`: erro (mensagem em `errorBuffer`)

## Giro e excentricidade da secao (`structura_solve_v3`)

`structura_solve_v3` e igual a `structura_solve_v2` e recebe, logo antes dos arrays de saida:

- `const double* memberSectionRotationDeg` (`memberCount`): giro da secao em torno do eixo x local, em graus
  (regra da mao direita: `y' = cos*y + sin*z`, `z' = -sin*y + cos*z`). Pode ser `nullptr` (= 0).
- `const double* memberOffsets4` (`memberCount * 4`): `dy1, dz1, dy2, dz2` em metros = posicao do eixo da barra
  (centroide) em relacao ao no I / no J, nos eixos locais **ja girados**. Pode ser `nullptr` (= 0).

O solver monta `K_no = T^T * K_barra * T` com `T = T_offset * R` (`u_eixo = u_no + theta x (0, dy, dz)`); as forcas
equivalentes de cargas distribuidas usam a mesma transformacao e os esforcos de saida (`outMemberLocalForces12`) sao
do eixo da barra. Com giro diferente de zero, `outMemberAuxK3` devolve o eixo local `y` girado
(`z = x cross aux`, `y = z cross x` reproduz os eixos).

## Articulacoes de extremidade e graus de liberdade sem rigidez (`structura_solve_v4`)

`structura_solve_v4` e igual a `structura_solve_v3` e recebe, apos `memberOffsets4`:

- `const int* memberReleases12` (`memberCount * 12`): nao zero = grau de liberdade liberado, na ordem
  `ux uy uz rx ry rz` no no I e depois no no J (eixos locais). Pode ser `nullptr`. Os graus liberados sao
  condensados da rigidez e das cargas equivalentes da barra (ver `docs/FORMULACAO_MATEMATICA.md`, secao 9).

Graus de liberdade nodais sem rigidez alguma (ex.: rotacao de um no onde todas as barras chegam articuladas)
sao fixados automaticamente; se houver carga aplicada neles a analise falha com "Unstable structure".

## Convencao dos Arrays

- `nodeRestraints6`: vetor achatado com `nodeCount * 6` inteiros (`1` restringido, `0` livre).
- `nodalLoadValues6`: vetor achatado com `nodalLoadCount * 6` doubles na ordem `Fx Fy Fz Mx My Mz`.
- `memberLoadCoordSystem`: `0 = LOCAL`, `1 = GLOBAL`.
- `outNodeDisplacements6`: tamanho `nodeCount * 6`.
- `outNodeReactions6`: tamanho `nodeCount * 6`.
- `outMemberLocalForces12`: tamanho `memberCount * 12`.
- `outMemberAuxK3`: tamanho `memberCount * 3`.

A ordem de saida segue a ordem de entrada dos arrays de nos e membros.

## Exemplo Minimo em C++

```cpp
#include <iostream>
#include <vector>
#include "interface_dll.h"

int main() {
    std::vector<int> materialIds{1};
    std::vector<double> materialE{2.1e11};
    std::vector<double> materialG{8.07e10};

    std::vector<int> sectionIds{1};
    std::vector<double> sectionA{0.005};
    std::vector<double> sectionIz{4.0e-6};
    std::vector<double> sectionIy{8.0e-6};
    std::vector<double> sectionJ{1.2e-5};

    std::vector<int> nodeIds{1, 2};
    std::vector<double> nodeX{0.0, 5.0};
    std::vector<double> nodeY{0.0, 0.0};
    std::vector<double> nodeZ{0.0, 0.0};
    std::vector<int> nodeRestraints6{
        1,1,1,1,1,1,
        0,0,0,0,0,0
    };

    std::vector<int> memberIds{1};
    std::vector<int> memberNodeI{1};
    std::vector<int> memberNodeJ{2};
    std::vector<int> memberMaterialId{1};
    std::vector<int> memberSectionId{1};

    std::vector<int> nodalLoadNodeIds{2};
    std::vector<double> nodalLoadValues6{0.0, -10000.0, 0.0, 0.0, 0.0, 5000.0};

    std::vector<int> memberLoadMemberIds{1};
    std::vector<int> memberLoadCoordSystem{0}; // LOCAL
    std::vector<double> memberLoadQx{0.0};
    std::vector<double> memberLoadQy{0.0};
    std::vector<double> memberLoadQz{-5000.0};

    std::vector<double> outNodeDisplacements6(nodeIds.size() * 6, 0.0);
    std::vector<double> outNodeReactions6(nodeIds.size() * 6, 0.0);
    std::vector<double> outMemberLocalForces12(memberIds.size() * 12, 0.0);
    std::vector<double> outMemberAuxK3(memberIds.size() * 3, 0.0);

    char errorBuffer[1024] = {0};

    const int rc = structura_solve(
        static_cast<int>(materialIds.size()), materialIds.data(), materialE.data(), materialG.data(),
        static_cast<int>(sectionIds.size()), sectionIds.data(), sectionA.data(), sectionIz.data(), sectionIy.data(), sectionJ.data(),
        static_cast<int>(nodeIds.size()), nodeIds.data(), nodeX.data(), nodeY.data(), nodeZ.data(), nodeRestraints6.data(),
        static_cast<int>(memberIds.size()), memberIds.data(), memberNodeI.data(), memberNodeJ.data(), memberMaterialId.data(), memberSectionId.data(),
        static_cast<int>(nodalLoadNodeIds.size()), nodalLoadNodeIds.data(), nodalLoadValues6.data(),
        static_cast<int>(memberLoadMemberIds.size()), memberLoadMemberIds.data(), memberLoadCoordSystem.data(),
        memberLoadQx.data(), memberLoadQy.data(), memberLoadQz.data(),
        outNodeDisplacements6.data(), outNodeReactions6.data(), outMemberLocalForces12.data(), outMemberAuxK3.data(),
        static_cast<int>(sizeof(errorBuffer)), errorBuffer);

    if (rc != 0) {
        std::cerr << "Erro da DLL: " << errorBuffer << "\n";
        return rc;
    }

    std::cout << "Deslocamento UX do no 2: " << outNodeDisplacements6[6] << "\n";
    return 0;
}
```

## Dicas de Integracao

- Garanta que o chamador use o mesmo runtime/arquitetura da DLL (`x64` com `x64`, `Debug` com `Debug` durante desenvolvimento).
- No Windows, mantenha `rds.dll` acessivel no `PATH` ou na mesma pasta do executavel consumidor.
- Para C#, Python ou outras linguagens, use FFI e respeite os mesmos buffers lineares descritos acima.

## Cascas acopladas (structura_set_shells)

Antes de `structura_solve_v4`, `structura_set_shells(...)` registra elementos de casca Q4 (ids, 4 nos, E, nu, massa especifica, espessura), cargas por elemento (pressao nodal,
forca de corpo, temperatura), ligacoes no-escravo (ate 8 mestres, pesos que somam 1), molas nodais (inclusive so-compressao). Os nos das cascas sao nos comuns da chamada (ids, coordenadas e
restricoes nos mesmos arrays dos nos de barras). `structura_get_shell_results` devolve 40 valores por elemento (5 pontos x nxx nyy nxy mxx myy mxy qx qy, eixos locais) e os eixos
(9). `structura_clear_shells()` limpa o estado (o estado e global, como em `structura_set_options`).

## Opcoes avancadas de analise (estado global, como `structura_set_options`; padrao = tudo desligado, resultados inalterados)

- `structura_set_analysis_options(shearDeformation, stabilityFunctions, loadSteps)`
  - `shearDeformation`: barras de Timoshenko (phi = 12EI/(G As L^2)) com as areas de `structura_set_shear_areas`; area 0 = rigida ao cortante naquela direcao.
  - `stabilityFunctions`: segunda ordem pelas funcoes de estabilidade de Livesley (P-delta e P-delta local exatos por barra; serie para |x| < 0,5);
    com deformacao por cortante vale a rigidez geometrica consistente (aviso emitido).
  - `loadSteps`: 0 = iteracao direta e, se divergir, incremento de carga automatico (10 passos + bisecao ate o fator critico); n > 1 = sempre em n passos.
    Se nao converge, a mensagem traz o fator de carga critico estimado ("between a and b").
- `structura_set_shear_areas(count, sectionIds, Ay, Az)`: areas de cisalhamento (unidade de comprimento ao quadrado) por secao; Ay = cortante na direcao local y (flexao em torno de z).
- `structura_set_rigid_zones(count, memberIds, lengthI, lengthJ)`: zonas rigidas (m sobre o eixo da barra a partir de cada no). Somente o trecho restante e flexivel; as cargas
  distribuidas das zonas rigidas vao direto aos nos e os esforcos de extremidade sao os dos nos. A massa (modal) usa o comprimento total.
- `structura_get_warnings(buffer, size)`: avisos do ultimo calculo (matriz mal condicionada, grau de liberdade sem rigidez fixado, tirantes/molas que oscilam, 2a ordem so por incrementos).

## Torcao com empenamento (Vlasov, secoes abertas de paredes finas)

Cada extremidade de uma barra com empenamento ganha um setimo grau de liberdade, theta' (taxa de torcao), alem dos seis do no; esses graus ficam depois dos 6 x nos (so existem nos nos
de barras que pedem empenamento). A matriz local da barra passa a 14 x 14 com o bloco de torcao exato (solucao fechada da equacao E Cw theta'''' - G J theta'' = m), nao a G J / L.

- `structura_set_warping_sections(count, sectionIds, Cw)`: constante de empenamento (unidade de comprimento a sexta) por secao; Cw = 0 ou ausente = so Saint-Venant. Substitui a lista anterior.
- `structura_set_warping_members(count, memberIds, modeStart, modeEnd)`: barras que usam o empenamento e a condicao em cada extremidade: 0 = automatica (impedido em no totalmente engastado,
  compartilhado entre barras colineares no no, livre nos demais), 1 = livre (bimomento nulo), 2 = impedido (theta' = 0). Substitui a lista anterior.
- `structura_set_taper(count, memberIds, d1, d2, bf, tw, tf, segments)`: barras de secao I de alma variavel (NBR 8800, Anexo G): alturas nas extremidades I e J, largura/espessura das mesas e
  espessura da alma (unidades do modelo) e numero de trechos prismaticos. O solver subdivide a barra em `segments` trechos com nos internos (propriedades da altura media de cada trecho) e devolve
  os esforcos das extremidades da barra original; os nos internos nao aparecem nos resultados. Substitui a lista anterior.
- `structura_get_warping(count, memberIds, out4)`: por barra, (theta' em I, theta' em J, forca generalizada em I, em J). As forcas generalizadas sao os bimomentos aplicados a barra
  (+B em I, -B em J, B = -E Cw theta''). Devolve o numero de barras com empenamento.
- Nao e combinado com flambagem linear nem modal (usa Saint-Venant nessas execucoes, com aviso em `structura_get_warnings`).
- Torque distribuido uniforme (momento distribuido local mx) usa as forcas de engastamento exatas (torques de extremidade m L / 2 mais o par de bimomentos).
- Teoria, validacao e limitacoes: `docs/BARRAS_COMPOSTAS_E_TORCAO_COM_EMPENAMENTO.md`.
