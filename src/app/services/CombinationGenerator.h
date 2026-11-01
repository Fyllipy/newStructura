#pragma once

#include "app/ProjectDataTypes.h"

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Gera combinacoes de acoes conforme ABNT NBR 8681:2003 e NBR 8800:2024 (4.8.7, tabelas 1 e 2).
 *
 * - ELU normais (4.8.7.2.1): `ELU*`.
 * - ELU especiais (4.8.7.2.2) e de construcao (4.8.7.2.3): `ESP*` e `CONS*`; uma combinacao por acao variavel especial/de construcao.
 *   As demais variaveis entram com psi0,ef = psi0, ou psi2 quando a acao especial tem duracao muito pequena.
 * - ELU excepcionais (4.8.7.2.4): `EXC*`; uma combinacao por acao excepcional (F_Q,exc sem majoracao), as variaveis com psi2.
 * - Incendio (NBR 14323:2013, 6.3): `FOGO*`; gama_g das permanentes (tabela 3), as sobrecargas de uso com 0,21/0,28/0,42 (conforme o tipo de ocupacao) e,
 *   uma por vento, a combinacao G + 0,20 W do item 6.3.2. A acao termica nao tem efeito mecanico no modelo (sem restricao de dilatacao: 8.2.2).
 * - ELS: quase permanentes `ELS-QP`, frequentes `ELS-CF` e raras `ELS-CR`.
 *
 * Os casos de vento sao tratados como mutuamente exclusivos (um vento por combinacao).
 */
class CombinationGenerator
{
public:
    enum class Kind {
        SteelSelfWeight,   ///< peso proprio de estrutura metalica
        Permanent,         ///< demais permanentes (cobertura, fechamentos, pisos...)
        Variable,          ///< sobrecarga em locais sem predominancia de pesos fixos nem elevadas concentracoes de pessoas (psi 0,5/0,4/0,3)
        Wind,
        Temperature,       ///< variacao uniforme de temperatura da atmosfera
        Accidental,        ///< acao excepcional (impacto, explosao, incendio, sismo...): so entra nas combinacoes `EXC*`
        Crane,             ///< cargas moveis e efeitos dinamicos (vigas de rolamento de pontes rolantes)
        TemperatureEquipment,   ///< temperatura gerada por equipamentos
        PermanentPrecast,  ///< peso proprio de estruturas pre-moldadas, de madeira e elementos construtivos industrializados
        PermanentGeneral,  ///< peso proprio de elementos construtivos em geral e equipamentos
        Indirect,          ///< permanente indireta (recalques, retracao, fluencia)
        VariableHeavy,     ///< sobrecarga em edificacoes industriais, comerciais e de acesso ao publico (psi 0,7/0,6/0,4)
        VariableStorage,   ///< bibliotecas, arquivos, depositos, oficinas, garagens e coberturas (psi 0,8/0,7/0,6)
        Footbridge         ///< passarelas de pedestres (psi 0,6/0,4/0,3)
    };

    /// Situacao de projeto em que a acao variavel e a principal.
    enum class Situation {
        Normal,
        Special,        ///< acao variavel especial (4.8.7.2.2)
        Construction    ///< carregamento de construcao (4.8.7.2.3)
    };

    struct CaseSetup {
        QString name;
        Kind kind {Kind::Permanent};
        Situation situation {Situation::Normal};   ///< so tem efeito nas acoes variaveis
        double gammaUnfavorable {1.35};
        double gammaFavorable {1.0};
        double gammaSpecial {1.25};       ///< coluna "especiais ou de construcao" da tabela 1 (desfavoravel)
        double gammaExceptional {1.15};   ///< coluna "excepcionais" da tabela 1 (desfavoravel)
        double psi0 {0.5};
        double psi1 {0.4};
        double psi2 {0.3};
        bool shortDuration {false};       ///< acao especial de duracao muito pequena: psi0,ef = psi2 nas demais acoes
    };

    struct Options {
        bool ultimate {true};
        bool serviceQuasiPermanent {true};
        bool serviceFrequent {true};
        bool serviceRare {false};
        bool favorablePermanents {true};   ///< combinacoes com permanentes favoraveis (vento de sucao / arrancamento)
        bool special {true};               ///< gera `ESP*` para as acoes marcadas como especiais
        bool construction {true};          ///< gera `CONS*` para as acoes marcadas como de construcao
        bool exceptional {true};           ///< gera `EXC*` para as acoes excepcionais
        bool fire {false};                 ///< gera `FOGO*` (situacao de incendio, NBR 14323:2013, 6.3) para a verificacao de incendio do dimensionamento
    };

    /// Valores usuais (NBR 8800:2024 tab. 1 e 2 / NBR 8681) para o tipo de acao.
    static CaseSetup defaults(const QString &name, Kind kind);
    /// Deduz o tipo a partir do nome/sigla (PP, CP, SC, VT, ...).
    static Kind guessKind(const QString &name, bool selfWeight);

    static QVector<Structura::App::ProjectData::LoadCombination> generate(const QVector<CaseSetup> &cases, const Options &options);
};

} // namespace Structura::App::Services
