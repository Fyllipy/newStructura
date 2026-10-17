#pragma once

#include "app/ProjectDataTypes.h"
#include "design/ShearConnections.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Ligacoes do modelo: valores padrao, verificacao, proposta automatica e lista de pecas.
 *
 * Tipologias: chapa de cisalhamento e cantoneira dupla (viga-apoio), chapa de no (gusset) de trelica/contraventamento,
 * emenda de viga parafusada, base de pilar com chumbadores (NBR 8800:2024, 6.7) e ligacoes soldadas de trelica tubular (CIDECT DG1).
 * Os parametros ficam em `NodeConnection::values` (chaves abaixo); esforcos em N e N.mm.
 */
class ConnectionService
{
public:
    struct FieldDef {
        QString key;
        QString label;
        double min {0.0};
        double max {1e6};
        int decimals {1};
        QString suffix;
        QString group;          ///< titulo da secao do formulario
        QStringList options;    ///< se nao vazio, campo de escolha (o valor guardado e o indice)
        QString tip;
    };

    /// Barra (viga, pilar, diagonal ou banzo) a que a ligacao se refere.
    struct BeamInfo {
        QString name;
        QString family;
        QMap<QString, double> geometry;
        double depth {0.0};   ///< mm
        double bf {0.0};
        double tw {6.0};
        double tf {10.0};
        double area {0.0};    ///< mm2
        double fy {250.0};
        double fu {400.0};
    };

    /// Diagonal ou montante que chega ao no do banzo (ligacao tubular).
    struct BraceInfo {
        int memberId {0};
        bool atEnd {false};
        double d {0.0};
        double t {0.0};
        double fy {355.0};
        double angleDeg {90.0};   ///< angulo entre a diagonal e o eixo do banzo (0 a 90)
        bool sameSideAsFirst {true};
    };

    /// Combinacao de esforcos (N, N.mm) na extremidade da barra.
    struct ForceCase {
        QString name;
        double shear {0.0};
        double axial {0.0};   ///< + tracao
        double moment {0.0};
        QVector<double> braceAxial;   ///< modulo da forca axial de cada diagonal (ligacao tubular)
    };

    struct Forces {
        double shear {0.0};         ///< N (resultante)
        double axial {0.0};         ///< N (+ tracao), dominante
        double tension {0.0};       ///< maior tracao (N)
        double compression {0.0};   ///< maior compressao (N)
        double moment {0.0};        ///< maior momento (N.mm)
        QVector<ForceCase> cases;   ///< por combinacao (usado por emenda, base e ligacao tubular)
        /// Solicitacao das OUTRAS ligacoes que chegam a mesma barra de apoio no mesmo no (N), somada a desta na verificacao do apoio.
        double hostTension {0.0};
        double hostCompression {0.0};
        int hostConnections {0};
    };

    /// Forcas (tracao, compressao; N) que a ligacao transmite normalmente a face da barra de apoio.
    static std::pair<double, double> supportDemand(const Structura::App::ProjectData::NodeConnection &connection, const Forces &forces);

    struct Evaluation {
        std::vector<Structura::Design::Connections::Check> checks;
        std::vector<Structura::Design::Connections::Part> parts;
        double maxRatio {0.0};
        QString governing;
        QString governingCase;   ///< combinacao que governa (tipologias avaliadas por combinacao)
        QString description;     ///< resumo da ligacao projetada
    };

    static QVector<FieldDef> fields(int type);
    static QString typeName(int type);
    static QString typeSummary(int type);
    /// A tipologia usa parafusos (grau e plano de corte no formulario).
    static bool usesBolts(int type);
    /// Chaves de fy/fu da chapa principal (preenchidas pelo atalho de aco); vazio se nao ha.
    static QPair<QString, QString> steelKeys(int type);

    /// Valores iniciais a partir da barra e do aco padrao das chapas.
    static Structura::App::ProjectData::NodeConnection makeDefault(int type, const BeamInfo &beam, double plateFy, double plateFu);
    /// Idem, para a ligacao tubular (banzo + diagonais que chegam ao no).
    static Structura::App::ProjectData::NodeConnection makeTubular(const BeamInfo &chord, const QVector<BraceInfo> &braces);

    static Evaluation evaluate(const Structura::App::ProjectData::NodeConnection &connection, const Forces &forces);

    /// Substitui geometria pela proposta mais leve que atende; false se nenhuma atende (ou a tipologia nao tem proposta automatica).
    static bool propose(Structura::App::ProjectData::NodeConnection &connection, const Forces &forces);
    static bool hasProposal(int type);

    /// Resumo em uma linha ("Chapa 9,5 mm, 3 parafusos d20 A325, solda 6 mm").
    static QString describe(const Structura::App::ProjectData::NodeConnection &connection);
};

} // namespace Structura::App::Services
