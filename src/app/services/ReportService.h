#pragma once

#include "app/ProjectDataTypes.h"
#include "app/services/DesignService.h"
#include "app/services/StructuralChecks.h"
#include "report/ReportModel.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Monta o memorial de calculo e o relatorio geral do projeto (modelo de documento independente de formato).
 *
 * O mesmo modelo gera a visualizacao na tela (HTML com equacoes renderizadas) e o arquivo .docx com
 * equacoes nativas do Word.
 */
class ReportService
{
public:
    struct NodeRow {
        int id {0};
        double x {0.0}, y {0.0}, z {0.0};
        QString support;   ///< ex.: "UX UY UZ RX RY RZ" ou vazio
    };

    struct BarRow {
        int id {0};
        int nodeI {0};
        int nodeJ {0};
        double length {0.0};   ///< m
        QString section;
        QString material;
        QString group;
        QString placement;     ///< giro / articulacoes resumidos
    };

    struct ReactionRow {
        int node {0};
        double values[6] {0, 0, 0, 0, 0, 0};
    };

    struct ResultSummary {
        QString name;
        bool isCombination {false};
        double maxDisplacementMm {0.0};
        int maxDisplacementNode {0};
        QVector<ReactionRow> reactions;
        double sumRx {0.0}, sumRy {0.0}, sumRz {0.0};
    };

    struct Input {
        QString projectName;
        QString author;
        QString date;
        QString forceUnit {QStringLiteral("kN")};
        QVector<Structura::App::ProjectData::MaterialInfo> materials;
        QVector<Structura::App::ProjectData::SectionInfo> sections;
        QVector<NodeRow> nodes;
        QVector<BarRow> bars;
        Structura::App::ProjectData::ProjectExtras extras;
        QVector<Structura::App::ProjectData::NodalLoad> nodalLoads;
        QVector<Structura::App::ProjectData::MemberLoad> memberLoads;
        QVector<ResultSummary> results;
        // dimensionamento (opcional)
        bool hasDesign {false};
        QVector<DesignService::MemberVerdict> verdicts;
        QHash<int, Structura::App::ProjectData::SectionInfo> sectionOfMember;
        QHash<int, double> lengthMOfMember;
        QVector<StructuralChecks::DeflectionResult> deflections;
        QVector<StructuralChecks::Story> stories;
        QVector<StructuralChecks::StoryResult> storyResults;
        QStringList designWarnings;
        Structura::App::ProjectData::DesignSettings designSettings;
    };

    enum class BarScope { All, Failing, Selected };

    struct Options {
        bool cover {true};
        bool toc {true};
        bool criteria {true};
        bool materialsAndSections {true};
        bool geometry {true};
        bool supports {true};
        bool loads {true};
        bool analysisResults {true};
        bool designSummary {true};
        bool designDetails {true};
        bool serviceChecks {true};
        bool sectionDrawings {true};
        bool showFormulas {true};         ///< false: so o resultado de cada verificacao (sem formulas)
        BarScope scope {BarScope::All};
        QVector<int> selectedMembers;
        QSet<QString> itemIds;            ///< itens de calculo incluidos (vazio = todos)
        int maxDetailBars {0};            ///< limita as barras detalhadas (previa); 0 = todas
    };

    struct ItemInfo {
        QString id;
        QString title;
    };
    /// Itens de calculo disponiveis (tracao, compressao, flexao, cortante...).
    static QVector<ItemInfo> calculationItems();

    static Structura::Report::ReportDocument buildGlobal(const Input &input, const Options &options);

    /// Memorial de uma barra. `itemIds` vazio = completo.
    static Structura::Report::ReportDocument buildBar(const Input &input, int memberId, const QSet<QString> &itemIds, bool showFormulas = true,
                                                      bool withSectionDrawing = true);

};

} // namespace Structura::App::Services
