#pragma once

#include "app/ProjectDataTypes.h"
#include "app/services/ConnectionService.h"
#include "app/services/ConnectionSolids.h"

#include <QMap>
#include <QString>
#include <QVector>

#include <functional>
#include <string>
#include <vector>

namespace Structura::App::Services {

/**
 * @brief Catalogo de tipologias de ligacao e emenda (grupos: bases de pilar, ligacoes de viga, treliças, ligacoes rigidas, vigas mistas,
 *        reticulados tubulares e emendas). Cada tipologia e uma variante (`preset`) de uma familia de calculo.
 *
 * Os tipos do catalogo tem id >= 100 e sao despachados pelo `ConnectionService` (campos, valores padrao, verificacao, descricao),
 * pelo `ConnectionSolids` (modelo 3D) e pelo desenho de detalhamento. Os parametros ficam em `NodeConnection::values`; o `preset`
 * grava os valores que definem a variante (nao editaveis pelo usuario).
 */
class ConnectionCatalog
{
public:
    static constexpr int kFirstId = 100;

    enum class Family {
        WebShear = 1,   ///< ligacao de viga por tala(s) de alma (simples ou dupla; parafusada ou soldada a alma)
        Moment = 6,           ///< ligacao rigida viga-pilar (mesas soldadas ou perfis T; alma direta, tala simples ou dupla)
        CoverSplice = 4,      ///< emenda de barra axial (chapa, cantoneira) com cobrejuntas parafusadas
        TubeSplice = 5,       ///< emenda de tubo circular por chapas de extremidade parafusadas
        EndPlateSplice = 3,   ///< emenda de vigas com chapas de extremidade parafusadas (no ou fora do no)
        TrussNode = 2,  ///< no de trelica plana: barras soldadas/parafusadas direto no banzo ou em chapa de no
        FlexEndPlate = 7,     ///< chapa de extremidade flexivel (altura parcial) soldada a alma da viga e parafusada ao apoio
        Composite = 8,        ///< ligacao mista de aco e concreto (Anexo O): armadura da laje, conectores e ligacao metalica
        Pin = 9,              ///< chapa tracionada ligada por pino (NBR 8800:2024, 5.2.6) e o proprio pino (6.4, 6.6.2)
    };

    struct Entry;
    using ExtraFields = std::function<QVector<ConnectionService::FieldDef>()>;
    using ExtraDefaults = std::function<void(Structura::App::ProjectData::NodeConnection &, const ConnectionService::BeamInfo &)>;

    struct Entry {
        int id {0};
        Family family {Family::WebShear};
        QString group;                 ///< grupo (capitulo) do catalogo
        QString name;
        QString summary;
        QMap<QString, double> preset;  ///< valores fixos da variante
        bool usesBolts {true};
        bool usesSupport {true};       ///< a ligacao tem barra de apoio (pilar ou viga principal)
        int legacyType {-1};           ///< >= 0: variante de uma tipologia anterior (kEndPlate, kSplice...): calcula, desenha e modela com ela
        bool vertical {false};         ///< barra principal vertical (pilar): miniatura e vista com a barra para cima
        // variantes de tipologias anteriores (legacyType >= 0): campos, valores e verificacoes adicionais
        ExtraFields extraFields;
        ExtraDefaults extraDefaults;
    };

    using FieldDef = ConnectionService::FieldDef;
    using Evaluation = ConnectionService::Evaluation;
    using Forces = ConnectionService::Forces;
    using BeamInfo = ConnectionService::BeamInfo;
    using BraceInfo = ConnectionService::BraceInfo;
    using Primitive = ConnectionSolids::Primitive;

    struct Dim {
        double s1 {0.0}, t1 {0.0}, s2 {0.0}, t2 {0.0};
        double offset {10.0};   ///< afastamento da linha de cota (mm no desenho; negativo = outro lado)
        std::string text;
    };

    /// Operacoes de uma familia de calculo.
    struct FamilyOps {
        std::function<QVector<FieldDef>(const Entry &)> fields;
        std::function<Structura::App::ProjectData::NodeConnection(const Entry &, const BeamInfo &, double plateFy, double plateFu)> makeDefault;
        std::function<Evaluation(const Structura::App::ProjectData::NodeConnection &, const Forces &)> evaluate;
        std::function<QString(const Structura::App::ProjectData::NodeConnection &)> describe;
        /// Opcional: valores padrao a partir das barras que chegam ao no (alem da barra principal).
        std::function<Structura::App::ProjectData::NodeConnection(const Entry &, const BeamInfo &, double, double, const QVector<BraceInfo> &)> makeWithBraces;
        std::function<void(const Structura::App::ProjectData::NodeConnection &, bool context, std::vector<Primitive> &out)> solids;
        std::function<bool(Structura::App::ProjectData::NodeConnection &, const Forces &)> propose;   ///< opcional
        std::function<QPair<QString, QString>()> steelKeys;
        std::function<QPair<QString, QString>(const Entry &)> steelKeysFor;   ///< opcional: chaves de aco que dependem da variante
        /// Cotas do detalhamento na vista frontal (s horizontal, t vertical, mm).
        std::function<std::vector<Dim>(const Structura::App::ProjectData::NodeConnection &)> dims;
        /// Notas do detalhamento (linhas de texto, em maiusculas).
        std::function<std::vector<std::string>(const Structura::App::ProjectData::NodeConnection &)> notes;
    };

    static bool isCatalog(int type) { return type >= kFirstId; }
    /// A tipologia usa as demais barras que chegam ao no (no de trelica).
    static bool usesBraces(int type);
    static Structura::App::ProjectData::NodeConnection makeDefaultWithBraces(int type, const BeamInfo &beam, double plateFy, double plateFu, const QVector<BraceInfo> &braces);
    /// Tipologia de calculo: a anterior (`legacyType`) das variantes, ou o proprio tipo.
    static int baseType(int type);
    /// Copia da ligacao com o tipo da tipologia anterior (para os servicos que despacham por `type`).
    static Structura::App::ProjectData::NodeConnection asLegacy(const Structura::App::ProjectData::NodeConnection &connection);
    static bool isVertical(int type);
    static const QVector<Entry> &entries();
    static const Entry *find(int type);
    static const FamilyOps &ops(Family family);

    // despacho (usados pelos servicos)
    static QVector<FieldDef> fields(int type);
    static Structura::App::ProjectData::NodeConnection makeDefault(int type, const BeamInfo &beam, double plateFy, double plateFu);
    static Evaluation evaluate(const Structura::App::ProjectData::NodeConnection &connection, const Forces &forces);
    static QString describe(const Structura::App::ProjectData::NodeConnection &connection);
    static void solids(const Structura::App::ProjectData::NodeConnection &connection, bool context, std::vector<Primitive> &out);
    static bool propose(Structura::App::ProjectData::NodeConnection &connection, const Forces &forces);
    static bool hasProposal(int type);
    static QPair<QString, QString> steelKeys(int type);
};

} // namespace Structura::App::Services
