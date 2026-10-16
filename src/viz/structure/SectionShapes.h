#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <optional>
#include <vector>

namespace Structura::Visualization {

/// Um parametro geometrico de uma familia de perfis (em milimetros).
struct SectionParameter {
    QString key;
    QString label;
    double defaultValue {0.0};
    bool optional {false}; ///< Parametros opcionais (raios) podem ficar em zero = padrao.
};

/// Familia de perfis: nome (identificador), material de referencia e parametros.
struct SectionFamily {
    QString name;
    QString material; ///< "Aco", "Concreto"
    QVector<SectionParameter> parameters;
};

/// Contornos 2D de uma secao. Coordenadas em mm, centradas no centroide.
/// Eixo `a` = largura (eixo local z da barra), eixo `b` = altura (eixo local y, para cima).
struct SectionShape {
    using Point = std::array<double, 2>;
    using Loop = std::vector<Point>;

    std::vector<Loop> loops; ///< O primeiro e o contorno externo; os demais sao furos.
    Point centroid {0.0, 0.0};
    std::array<double, 4> bounds {0.0, 0.0, 0.0, 0.0}; ///< min a, max a, min b, max b
};

/// Propriedades calculadas (unidades SI: m2, m4).
struct SectionProperties {
    double area {0.0};
    double iz {0.0}; ///< Inercia para flexao no plano da altura (b)
    double iy {0.0}; ///< Inercia para flexao no plano da largura (a)
    double j {0.0};  ///< Constante de torcao (aproximada)
};

/// Contornos parametricos das familias de perfis de aco e concreto.
class SectionShapes
{
public:
    using Parameters = QMap<QString, double>;

    /// Familias disponiveis, na ordem de exibicao.
    static const QVector<SectionFamily> &families();
    static const SectionFamily *findFamily(const QString &name);

    /// Gera os contornos; devolve nullopt (com `error`) se as dimensoes forem invalidas.
    static std::optional<SectionShape> build(const QString &family,
                                             const Parameters &parameters,
                                             bool mirrored,
                                             QString *error = nullptr);

    /// Area, inercias e torcao calculadas a partir da geometria (mm de entrada, SI na saida).
    static std::optional<SectionProperties> properties(const QString &family,
                                                       const Parameters &parameters,
                                                       QString *error = nullptr);
};

} // namespace Structura::Visualization
