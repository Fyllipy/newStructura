#pragma once

#include <QString>
#include <QStringList>

namespace Structura::App::Services {

/**
 * @brief Modelos parametricos prontos (galpao de duas aguas e edificio de porticos) gerados como arquivo .dat
 *        (kN, m), com perfis, grupos, travamentos em tirante, casos de carga e combinacoes iniciais.
 */
class ModelTemplateService
{
public:
    struct ShedParams {
        double span {20.0};          ///< vao (m)
        double eaveHeight {6.0};     ///< altura dos pilares (m)
        double slopePercent {10.0};  ///< inclinacao da cobertura (%)
        int frames {6};              ///< numero de porticos
        double frameSpacing {6.0};   ///< espacamento entre porticos (m)
        int panelsPerSlope {6};      ///< trechos de cada agua (terças nos nos)
        QString columnProfile {QStringLiteral("W 410 x 53.0")};
        QString rafterProfile {QStringLiteral("W 360 x 44.6")};
        bool pinnedBases {false};
        bool roofBracing {true};     ///< contraventamento em X da cobertura (tirantes) nos vaos extremos
        double roofLiveLoad {0.25};  ///< sobrecarga da cobertura (kN/m2, por area em planta)
        double roofDeadLoad {0.15};  ///< peso de telhas e acessorios (kN/m2, por area em planta)
    };

    struct BuildingParams {
        int baysX {2};
        int baysY {4};
        int stories {3};
        double bayX {6.0};
        double bayY {6.0};
        double storyHeight {3.6};
        QString columnProfile {QStringLiteral("W 310 x 74.0 (H)")};
        QString beamXProfile {QStringLiteral("W 410 x 53.0")};
        QString beamYProfile {QStringLiteral("W 310 x 32.7")};
        bool bracing {true};         ///< X em tirantes nos vaos extremos das fachadas
        double floorDeadLoad {4.0};  ///< kN/m2
        double floorLiveLoad {3.0};  ///< kN/m2
    };

    /// Conteudo do .dat (ASCII, quebras CRLF).
    static QString buildShed(const ShedParams &params, QString *error = nullptr);
    static QString buildBuilding(const BuildingParams &params, QString *error = nullptr);

    /// Nomes de perfis do catalogo de uma familia (ex.: "W Laminado").
    static QStringList catalogNames(const QString &family);
};

} // namespace Structura::App::Services
