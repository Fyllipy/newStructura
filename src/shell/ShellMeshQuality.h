#pragma once

#include "shell/ShellMesh.h"

#include <string>
#include <vector>

namespace Structura::Shell {

/// Verificacao de qualidade da malha de quadrilateros (Q4 MITC4 + Wilson): geometria por elemento e continuidade de normais entre vizinhos.
///
/// Limites (fundamentados em varreduras executadas, ver docs/QUALIDADE_DE_MALHA.md):
///  - angulo interno: aceitavel em [45, 135] graus (erro de flecha vs malha fina <= ~5,4 % em placa fina com skew de 45 graus, dentro da ordem do erro de
///    discretizacao da malha 8x8; 'pizzas' de polo de domos, com angulo de ~173 graus, deixam ~4-6 % de erro LOCAL nos esforcos de membrana, que nao diminui
///    com o refino); aviso fora disso; ERRO apenas quando o elemento e matematicamente degenerado ou invertido: jacobiano <= 0 num canto (nao convexo/invertido)
///    ou angulo interno < 1 ou > 179 graus (o jacobiano se anula em 180 graus). Entre 173 e 179 graus nao ha dado medido: a faixa e aviso, nao aprovacao;
///  - razao de aspecto (maior/menor aresta): aceitavel ate 32 (testado ate 32 sem degradacao da flecha); aviso acima; ERRO acima de 1000 (elemento quase colapsado);
///  - empenamento (desvio maximo dos nos ao plano medio): aviso se delta/h > 0,005 ou delta/t > 0,05 (erro de flecha de ~3 % em delta/t = 0,125 e ~42 % em delta/t = 0,6 numa
///    placa fina); ERRO se delta/h > 0,03 ou delta/t > 0,25. A rigidez do elemento descarta o desvio fora do plano, por isso o limite e restritivo;
///  - area: ERRO se area < 1e-10 * (maior aresta)^2 ou nula (elemento degenerado);
///  - normais: ERRO se dois elementos vizinhos do MESMO objeto percorrem a aresta comum no mesmo sentido (normais opostas, malha dobrada sobre si);
///    aviso se o angulo entre normais vizinhas do mesmo objeto/zona passa de 45 graus (faceamento grosseiro de superficie curva).
struct MeshQualityLimits {
    double angleWarnLow {45.0}, angleWarnHigh {135.0};
    double angleErrorLow {1.0}, angleErrorHigh {179.0};
    double aspectWarn {32.0}, aspectError {1000.0};
    double warpWarnOverSize {0.005}, warpWarnOverThickness {0.05};
    double warpErrorOverSize {0.03}, warpErrorOverThickness {0.25};
    double degenerateArea {1.0e-10};
    double dihedralWarn {45.0};
};

enum class QualityLevel { Acceptable = 0, Warning = 1, Error = 2 };

struct QuadQuality {
    QualityLevel level {QualityLevel::Acceptable};
    double minAngle {90.0}, maxAngle {90.0};   ///< graus (angulos internos nos 4 cantos, no plano medio do elemento)
    double minJacobian {1.0};                  ///< menor sen(angulo) com sinal entre os cantos (<= 0: nao convexo/invertido); = detJ normalizado
    double aspect {1.0};                       ///< maior aresta / menor aresta
    double warp {0.0};                         ///< m: desvio maximo dos 4 nos ao plano medio
    double warpOverSize {0.0}, warpOverThickness {0.0};
    double area {0.0};                         ///< m2
    std::string reason;                        ///< motivo da pior classificacao
};

struct MeshQualityReport {
    std::vector<QuadQuality> quads;
    int acceptable {0}, warnings {0}, errors {0};
    int flippedNormalEdges {0};     ///< arestas compartilhadas percorridas no mesmo sentido (mesmo objeto)
    int sharpDihedralEdges {0};     ///< arestas com angulo entre normais acima do limite (mesmo objeto e zona)
    double maxDihedral {0.0};       ///< graus
    double minAngle {180.0}, maxAngle {0.0}, maxAspect {1.0}, maxWarpOverSize {0.0};
    std::vector<std::string> messages;   ///< uma linha por tipo de problema, com a contagem e os 3 piores elementos (indices)
    [[nodiscard]] bool hasErrors() const { return errors > 0 || flippedNormalEdges > 0; }
};

/// Avalia todos os elementos e as arestas internas da malha.
MeshQualityReport analyzeMeshQuality(const Mesh &mesh, const Model &model, const MeshQualityLimits &limits = {});

} // namespace Structura::Shell
