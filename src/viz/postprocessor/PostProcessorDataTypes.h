#pragma once

#include <array>
#include <cmath>
#include <vector>

namespace Structura::Visualization {

/// Esforços internos nos extremos do elemento (convenção do solver).
/// Índices [0..5]  = nó i: N1, Vy1, Vz1, Mt1, My1, Mz1
/// Índices [6..11] = nó j: N2, Vy2, Vz2, Mt2, My2, Mz2
struct EndForces {
    double N1{0}, Vy1{0}, Vz1{0}, Mt1{0}, My1{0}, Mz1{0};
    double N2{0}, Vy2{0}, Vz2{0}, Mt2{0}, My2{0}, Mz2{0};
};

/// Carga distribuída uniforme sobre o elemento.
/// Único tipo de carga suportado pelo modelo atual.
struct UniformDistributedLoad {
    double qx{0}, qy{0}, qz{0};
    bool isLocalSystem{true}; ///< true = sistema local, false = sistema global
};

/// Momento distribuído uniforme (por unidade de comprimento) sobre o elemento.
struct UniformDistributedMoment {
    double mx{0}, my{0}, mz{0};
    bool isLocalSystem{true};
};

/// Carga com posição sobre o elemento (trapezoidal/parcial, força pontual ou momento pontual).
/// Posições `a` e `b` medidas sobre o eixo a partir do nó i, na unidade do modelo.
struct PositionedLoad {
    enum class Kind { Trapezoid, PointForce, PointMoment };
    Kind kind{Kind::Trapezoid};
    double a{0}, b{0};
    std::array<double, 3> v1{};   ///< Trapezoid: (qx,qy,qz) em a; PointForce: (Fx,Fy,Fz); PointMoment: (Mx,My,Mz)
    std::array<double, 3> v2{};   ///< Trapezoid: (qx,qy,qz) em b
    bool isLocalSystem{true};
};

/// Dados completos de um elemento de barra para pós-processamento.
struct BeamElementResults {
    int externalId{0};

    std::array<double, 3> node1{};   ///< Coordenadas mundo do nó i
    std::array<double, 3> node2{};   ///< Coordenadas mundo do nó j

    /// Eixos locais normalizados (calculados via DefaultLocalAxisProvider + auxK).
    /// Idênticos ao sistema local utilizado pelo solver.
    std::array<double, 3> xLocal{}; ///< Direção da barra (i → j)
    std::array<double, 3> yLocal{}; ///< Eixo local Y
    std::array<double, 3> zLocal{}; ///< Eixo local Z

    EndForces endForces;

    /// Cargas distribuídas sobre o elemento. Pode estar vazio.
    std::vector<UniformDistributedLoad> distributedLoads;

    /// Momentos distribuídos sobre o elemento. Pode estar vazio.
    std::vector<UniformDistributedMoment> distributedMoments;

    /// Cargas com posição (trapezoidais/parciais, pontuais e momentos pontuais). Pode estar vazio.
    /// Temperatura e retração não entram: não são cargas transversais e não alteram o equilíbrio das seções.
    std::vector<PositionedLoad> positionedLoads;
};

/// Reescreve o elemento no SISTEMA DE REFERENCIA da barra: eixos locais definidos so pela geometria, sem o giro da secao.
/// O solver trabalha com os eixos girados junto com a secao (y', z' = y, z girados de `rotationDeg` em torno de x); os diagramas, porem, devem
/// seguir um unico sistema para todas as barras. Os esforcos (cortantes e momentos), as cargas locais e os eixos sao reescritos na base de
/// referencia: v_y = cos.v_y' - sen.v_z' ; v_z = sen.v_y' + cos.v_z'. Cargas globais nao mudam (sao projetadas nos eixos depois).
/// O dimensionamento nao usa esta funcao: ele precisa dos esforcos nos eixos da propria secao.
inline void toReferenceFrame(BeamElementResults &el, double rotationDeg)
{
    if (rotationDeg == 0.0) {
        return;
    }
    constexpr double kPi = 3.14159265358979323846;
    const double c = std::cos(rotationDeg * kPi / 180.0), s = std::sin(rotationDeg * kPi / 180.0);
    auto rotate = [c, s](double &vy, double &vz) {
        const double y = c * vy - s * vz, z = s * vy + c * vz;
        vy = y;
        vz = z;
    };
    for (int i = 0; i < 3; ++i) {
        const double y = c * el.yLocal[static_cast<std::size_t>(i)] - s * el.zLocal[static_cast<std::size_t>(i)];
        const double z = s * el.yLocal[static_cast<std::size_t>(i)] + c * el.zLocal[static_cast<std::size_t>(i)];
        el.yLocal[static_cast<std::size_t>(i)] = y;
        el.zLocal[static_cast<std::size_t>(i)] = z;
    }
    rotate(el.endForces.Vy1, el.endForces.Vz1);
    rotate(el.endForces.My1, el.endForces.Mz1);
    rotate(el.endForces.Vy2, el.endForces.Vz2);
    rotate(el.endForces.My2, el.endForces.Mz2);
    for (auto &load : el.distributedLoads) {
        if (load.isLocalSystem) {
            rotate(load.qy, load.qz);
        }
    }
    for (auto &moment : el.distributedMoments) {
        if (moment.isLocalSystem) {
            rotate(moment.my, moment.mz);
        }
    }
    for (auto &load : el.positionedLoads) {
        if (load.isLocalSystem) {
            rotate(load.v1[1], load.v1[2]);
            rotate(load.v2[1], load.v2[2]);
        }
    }
}

/// Tipo de diagrama de esforços internos.
enum class DiagramType {
    None,
    N,   ///< Esforço Normal
    Vy,  ///< Cortante Y local
    Vz,  ///< Cortante Z local
    Mt,  ///< Torção
    My,  ///< Momento fletor Y
    Mz   ///< Momento fletor Z
};

/// Modo de normalização da escala visual.
enum class DiagramScaleMode {
    Global,          ///< Usa máximo absoluto de todos os elementos
    CurrentLoadCase, ///< Usa máximo dos elementos visíveis no caso atual
    SelectedElements ///< Usa máximo dos elementos atualmente selecionados
};

} // namespace Structura::Visualization
