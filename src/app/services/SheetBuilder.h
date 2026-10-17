#pragma once

#include "app/ProjectDataTypes.h"
#include "drawing/DrawingModel.h"

#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVector>

#include <vector>

namespace Structura::App::Services {

/**
 * @brief Gera as pranchas do projeto: plantas, elevacoes de porticos, lista de barras e detalhes de ligacoes.
 *
 * Tudo em milimetros de papel (Structura::Drawing::Sheet); as saidas PDF/DXF/previa usam o mesmo desenho.
 */
class SheetBuilder
{
public:
    enum class Paper { A4, A3, A2, A1 };

    struct Node {
        int id {0};
        double x {0}, y {0}, z {0};   ///< m
    };
    struct Bar {
        int id {0};
        int nodeI {0};
        int nodeJ {0};
        QString section;
        QString group;
        int memberType {0};           ///< 0 normal, 1 trelica, 2 tirante
        bool hingeI {false};
        bool hingeJ {false};
        double widthMm {100.0};       ///< largura da secao (mesa, eixo local z)
        double depthMm {100.0};       ///< altura da secao (eixo local y)
        double flangeMm {0.0};        ///< espessura da mesa (0 = secao retangular)
        double webMm {0.0};           ///< espessura da alma
        bool iShape {false};          ///< perfil I/H (contorno detalhado)
        double rotationDeg {0.0};     ///< giro da secao em torno do eixo da barra
        QString family;               ///< familia do perfil (contorno e linhas de dobra); vazio = retangulo largura x altura
        QMap<QString, double> geometry;
        bool mirrored {false};
        // posicao do centroide em relacao a cada no (alinhamento / excentricidade), nos eixos locais girados (mm)
        double offsetIy {0.0}, offsetIz {0.0};
        double offsetJy {0.0}, offsetJz {0.0};
        double trimI {0.0}, trimJ {0.0};   ///< recuo (mm) das extremidades I e J ate a face do apoio da ligacao
        double massPerMeter {0.0};    ///< kg/m
    };
    using Info = Structura::App::ProjectData::TitleBlockSettings;
    struct ConnectionDetail {
        Structura::App::ProjectData::NodeConnection connection;
        QString beamSection;
        double beamDepthMm {0.0};
        double beamFlangeMm {0.0};
        double shearKN {0.0};
        double axialKN {0.0};
        bool hasForces {false};
        double eta {0.0};
    };
    struct Input {
        QVector<Node> nodes;
        QVector<Bar> bars;
        Structura::App::ProjectData::ReferenceAxes axes;
        QVector<ConnectionDetail> connections;
        QVector<Structura::App::ProjectData::SavedView3D> views3d;
        Info info;
        QVector<Structura::App::ProjectData::SheetLayoutDef> layouts;   ///< folhas compostas salvas no projeto
    };

    enum class ViewKind { PlanProjection, PlanLevel, ElevationXZ, ElevationYZ, BarList, ConnectionDetail, View3D };
    struct ViewDef {
        QString id;
        QString title;
        ViewKind kind {ViewKind::PlanProjection};
        double coordinate {0.0};   ///< m: nivel (planta), y (elevacao XZ) ou x (elevacao YZ)
        int barCount {0};
        int connectionIndex {-1};
        int view3dIndex {-1};         ///< indice em Input::views3d (-1 = vista isometrica padrao)
        bool defaultOn {true};
    };

    /// Nivel de detalhe dos perfis nas vistas.
    enum class ProfileDetail { Axis, Outline, OutlineWithFlanges };

    /// Folha montada pelo usuario: varias vistas, cada uma com sua escala (0 = automatica) e posicao (automatica em grade ou manual).
    using SheetLayout = Structura::App::ProjectData::SheetLayoutDef;

    struct Options {
        Paper paper {Paper::A3};
        ProfileDetail profiles {ProfileDetail::OutlineWithFlanges};
        bool coloredLines {true};        ///< false: todas as linhas em preto (plotagem monocromatica)
        double lineWeightScale {1.0};    ///< multiplica a espessura das linhas (0,7 fina; 1 normal; 1,5 grossa)
        bool hiddenLines3D {false};      ///< mostra as linhas ocultas (tracejadas) nas vistas 3D
        bool faceDimensions {true};      ///< cota o vao livre (entre faces dos apoios) de cada barra
        double scaleDenominator {0.0};   ///< 0 = automatica
        bool barLabels {true};
        bool dimensions {true};
        QSet<QString> views;             ///< ids escolhidos (vazio = todos os padrao)
        QVector<SheetLayout> customSheets;   ///< folhas compostas (geradas depois das automaticas)
    };

    /// Vistas possiveis para o modelo.
    static QVector<ViewDef> availableViews(const Input &input);

    static std::vector<Structura::Drawing::Sheet> build(const Input &input, const Options &options);

    /// Marca de cada barra (P = pilar, V = viga, D = diagonal, T = tirante) e lista agrupada.
    struct MarkRow {
        QString mark;
        QString section;
        int count {0};
        double lengthMm {0.0};
        double massPerMeter {0.0};
        double unitMass {0.0};
        double totalMass {0.0};
        QVector<int> bars;
    };
    static QHash<int, QString> assignMarks(const Input &input, QVector<MarkRow> *rows = nullptr);

    static void paperSize(Paper paper, double &width, double &height);

    /// Carimbo isolado (para pre-visualizacao da edicao).
    static Structura::Drawing::Sheet previewTitleBlock(const Info &info);
    static QString paperName(Paper paper);
};

} // namespace Structura::App::Services
