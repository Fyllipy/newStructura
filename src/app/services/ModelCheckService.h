#pragma once

#include "app/ProjectDataTypes.h"

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Checagem do modelo antes da analise: conectividade, vinculos, propriedades, cargas e unidades.
 */
class ModelCheckService
{
public:
    enum class Severity { Error, Warning, Info };
    enum class TargetKind { None, Node, Bar };

    struct Issue {
        Severity severity {Severity::Info};
        QString code;          ///< identificador estavel (ex.: "NODE_ORPHAN")
        QString message;
        TargetKind target {TargetKind::None};
        int id {0};            ///< id externo do no/barra
    };

    struct Node {
        int id {0};
        double x {0.0}, y {0.0}, z {0.0};
    };
    struct Bar {
        int id {0};
        int nodeI {0};
        int nodeJ {0};
        bool hasMaterial {false};
        bool hasSection {false};
        QString sectionName;
        QString family;        ///< vazio = secao so por propriedades
        int memberType {0};
    };

    struct Input {
        QVector<Node> nodes;
        QVector<Bar> bars;
        QVector<Structura::App::ProjectData::MaterialInfo> materials;
        QVector<Structura::App::ProjectData::SectionInfo> sections;
        QVector<Structura::App::ProjectData::NodeSupport> supports;
        QVector<Structura::App::ProjectData::NodalLoad> nodalLoads;
        QVector<Structura::App::ProjectData::MemberLoad> memberLoads;
        Structura::App::ProjectData::ProjectExtras extras;
        double tolerance {1e-6};   ///< m
    };

    static QVector<Issue> run(const Input &input);

    static int count(const QVector<Issue> &issues, Severity severity);
};

} // namespace Structura::App::Services
