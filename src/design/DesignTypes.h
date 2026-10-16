#pragma once

// Tipos comuns do modulo de dimensionamento de estruturas de aco (NBR 8800:2024 e NBR 14762:2010).
// Unidades internas deste modulo: mm, N, N*mm, MPa. A conversao para as unidades do modelo
// e feita na camada de servico (app/services/DesignService).

#include <array>
#include <map>
#include <string>
#include <vector>

namespace Structura::Design {

constexpr double kPi = 3.14159265358979323846;

/// Aco estrutural. E e G conforme as normas (E = 200 GPa, G = 77 GPa).
struct SteelGrade {
    std::string name;
    double fy {250.0};   ///< MPa
    double fu {400.0};   ///< MPa
    double E {200000.0}; ///< MPa
    double G {77000.0};  ///< MPa
    bool coldFormed {false};
};

/// Norma de dimensionamento aplicada a um perfil.
enum class Standard { NBR8800, NBR14762 };

/// Tipo de secao reconhecido pelo dimensionamento.
enum class SectionKind {
    I,            ///< I/H laminado (duplamente simetrico)
    Channel,      ///< U laminado
    Angle,        ///< L laminado (cantoneira simples)
    Tee,          ///< T laminado
    RectTube,     ///< tubular retangular/quadrado
    RoundTube,    ///< tubular circular
    SolidRound,
    SolidRect,
    // formados a frio (NBR 14762)
    ColdChannel,        ///< U simples
    ColdLippedChannel,  ///< U enrijecido
    ColdZ,              ///< Z simples
    ColdAngle,          ///< L simples
    ColdHat,            ///< cartola
    Unsupported
};

enum class Axis { X, Y };   ///< X: eixo horizontal da secao (flexao no plano da altura); Y: eixo vertical

using Loop = std::vector<std::array<double, 2>>;   ///< vertices (a = largura, b = altura) em mm

/// Descricao parametrica de um perfil (familia + dimensoes em mm).
struct ProfileSpec {
    std::string family;
    std::map<std::string, double> params;
    bool mirrored {false};
    double param(const std::string &key, double fallback = 0.0) const
    {
        const auto it = params.find(key);
        return it == params.end() ? fallback : it->second;
    }
};

/// Um resultado individual de verificacao (um estado-limite).
struct CheckResult {
    std::string id;        ///< chave curta (ex.: "compression")
    std::string title;     ///< descricao (ex.: "Compressao axial")
    std::string clause;    ///< item da norma (ex.: "NBR 8800:2024, 5.3")
    double demand {0.0};   ///< esforco solicitante de calculo (mesma unidade de `resistance`)
    double resistance {0.0};
    double ratio {0.0};    ///< aproveitamento (>1 = nao atende)
    std::string unit;      ///< "kN", "kN*m", ""
    bool applicable {true};
    std::vector<std::string> lines;   ///< memoria de calculo (linhas de texto)
};

/// Resultado de verificacao de uma secao/barra para uma combinacao e uma posicao.
struct SectionVerdict {
    std::vector<CheckResult> checks;
    double maxRatio {0.0};
    std::string governing;
};

} // namespace Structura::Design
