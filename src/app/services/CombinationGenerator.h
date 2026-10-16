#pragma once

#include "app/ProjectDataTypes.h"

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Gera combinacoes de acoes conforme ABNT NBR 8681:2003 (ELU normal e ELS) e NBR 8800:2024 (tab. 1 e 2).
 *
 * Os casos de vento sao tratados como mutuamente exclusivos (um vento por combinacao).
 */
class CombinationGenerator
{
public:
    enum class Kind {
        SteelSelfWeight,   ///< peso proprio de estrutura metalica
        Permanent,         ///< demais permanentes (cobertura, fechamentos, pisos...)
        Variable,          ///< sobrecarga de uso
        Wind,
        Temperature
    };

    struct CaseSetup {
        QString name;
        Kind kind {Kind::Permanent};
        double gammaUnfavorable {1.35};
        double gammaFavorable {1.0};
        double psi0 {0.5};
        double psi1 {0.4};
        double psi2 {0.3};
    };

    struct Options {
        bool ultimate {true};
        bool serviceQuasiPermanent {true};
        bool serviceFrequent {true};
        bool serviceRare {false};
        bool favorablePermanents {true};   ///< combinacoes com permanentes favoraveis (vento de sucao / arrancamento)
    };

    /// Valores usuais (NBR 8800:2024 tab. 1 e 2 / NBR 8681) para o tipo de acao.
    static CaseSetup defaults(const QString &name, Kind kind);
    /// Deduz o tipo a partir do nome/sigla (PP, CP, SC, VT, ...).
    static Kind guessKind(const QString &name, bool selfWeight);

    static QVector<Structura::App::ProjectData::LoadCombination> generate(const QVector<CaseSetup> &cases, const Options &options);
};

} // namespace Structura::App::Services
