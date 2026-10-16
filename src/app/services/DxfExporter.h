#pragma once

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Exporta o modelo (eixos das barras, nos e rotulos) em DXF ASCII (AutoCAD R12), em metros.
 */
class DxfExporter
{
public:
    enum class View { Model3D, PlanXY, ElevationXZ, ElevationYZ };

    struct Node {
        int id {0};
        double x {0}, y {0}, z {0};
    };
    struct Bar {
        int id {0};
        double x1 {0}, y1 {0}, z1 {0};
        double x2 {0}, y2 {0}, z2 {0};
        QString section;     ///< vira a camada (um perfil por camada)
        bool tie {false};
    };
    struct Options {
        View view {View::Model3D};
        bool barLabels {true};
        bool nodeLabels {false};
        bool sectionLabels {true};
        double textHeight {0.15};   ///< m
    };

    static QString build(const QVector<Node> &nodes, const QVector<Bar> &bars, const Options &options);

    /// Nome de camada valido em DXF (maiusculas, sem caracteres proibidos).
    static QString layerName(const QString &text);
};

} // namespace Structura::App::Services
