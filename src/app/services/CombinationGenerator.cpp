#include "CombinationGenerator.h"

#include <algorithm>

namespace Structura::App::Services {

using Structura::App::ProjectData::LoadCombination;
using Kind = CombinationGenerator::Kind;
using Situation = CombinationGenerator::Situation;

CombinationGenerator::CaseSetup CombinationGenerator::defaults(const QString &name, Kind kind)
{
    CaseSetup s;
    s.name = name;
    s.kind = kind;
    // gamma normal / especial-construcao / excepcional (NBR 8800:2024, tabela 1); psi0, psi1, psi2 (tabela 2)
    auto set = [&](double normal, double special, double exceptional, double favorable, double p0, double p1, double p2) {
        s.gammaUnfavorable = normal;
        s.gammaSpecial = special;
        s.gammaExceptional = exceptional;
        s.gammaFavorable = favorable;
        s.psi0 = p0;
        s.psi1 = p1;
        s.psi2 = p2;
    };
    switch (kind) {
    case Kind::SteelSelfWeight: set(1.25, 1.15, 1.10, 1.0, 1.0, 1.0, 1.0); break;
    case Kind::PermanentPrecast: set(1.30, 1.20, 1.15, 1.0, 1.0, 1.0, 1.0); break;
    case Kind::Permanent: set(1.35, 1.25, 1.15, 1.0, 1.0, 1.0, 1.0); break;
    case Kind::PermanentGeneral: set(1.50, 1.40, 1.30, 1.0, 1.0, 1.0, 1.0); break;
    case Kind::Indirect: set(1.20, 1.20, 0.0, 0.0, 1.0, 1.0, 1.0); break;
    case Kind::Variable: set(1.5, 1.3, 1.0, 0.0, 0.5, 0.4, 0.3); break;
    case Kind::VariableHeavy: set(1.5, 1.3, 1.0, 0.0, 0.7, 0.6, 0.4); break;
    case Kind::VariableStorage: set(1.5, 1.3, 1.0, 0.0, 0.8, 0.7, 0.6); break;
    case Kind::Footbridge: set(1.5, 1.3, 1.0, 0.0, 0.6, 0.4, 0.3); break;
    case Kind::Wind: set(1.4, 1.2, 1.0, 0.0, 0.6, 0.3, 0.0); break;
    case Kind::Temperature: set(1.2, 1.0, 1.0, 0.0, 0.6, 0.5, 0.3); break;
    case Kind::TemperatureEquipment: set(1.5, 1.3, 1.0, 0.0, 0.7, 0.6, 0.4); break;
    case Kind::Crane: set(1.5, 1.3, 1.0, 0.0, 1.0, 0.8, 0.5); break;
    case Kind::Accidental: set(1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0); break;
    }
    return s;
}

Kind CombinationGenerator::guessKind(const QString &name, bool selfWeight)
{
    const QString n = name.toUpper();
    if (selfWeight || n == QLatin1String("PP") || n.startsWith(QLatin1String("PESO"))) {
        return Kind::SteelSelfWeight;
    }
    if (n.startsWith(QLatin1String("EXC")) || n.startsWith(QLatin1String("ACID")) || n.startsWith(QLatin1String("IMP")) || n.startsWith(QLatin1String("EXPL"))
        || n.startsWith(QLatin1String("INC")) || n.startsWith(QLatin1String("SISM"))) {
        return Kind::Accidental;
    }
    if (n.startsWith(QLatin1String("VT")) || n.startsWith(QLatin1String("VENTO")) || n.startsWith(QLatin1String("W"))) {
        return Kind::Wind;
    }
    if (n.startsWith(QLatin1String("PR")) || n.startsWith(QLatin1String("PONTE")) || n.startsWith(QLatin1String("CRANE"))) {
        return Kind::Crane;
    }
    if (n.startsWith(QLatin1String("SC")) || n.startsWith(QLatin1String("SOBRE")) || n.startsWith(QLatin1String("Q")) || n.startsWith(QLatin1String("USO"))) {
        return Kind::Variable;
    }
    if (n.startsWith(QLatin1String("TEMP")) || n.startsWith(QLatin1String("T"))) {
        return Kind::Temperature;
    }
    return Kind::Permanent;
}

namespace {

bool isPermanent(Kind k)
{
    return k == Kind::SteelSelfWeight || k == Kind::Permanent || k == Kind::PermanentPrecast || k == Kind::PermanentGeneral || k == Kind::Indirect;
}

void add(QVector<QPair<QString, double>> &factors, const QString &name, double value)
{
    if (value != 0.0) {
        factors.append({name, value});
    }
}

} // namespace

QVector<LoadCombination> CombinationGenerator::generate(const QVector<CaseSetup> &cases, const Options &options)
{
    QVector<LoadCombination> out;
    // permanentes; variaveis de situacao normal (entram nas combinacoes normais e de servico); especiais/de construcao; excepcionais
    QVector<CaseSetup> permanents, variables, specials, constructions, accidentals;
    for (const auto &c : cases) {
        if (isPermanent(c.kind)) {
            permanents.append(c);
        } else if (c.kind == Kind::Accidental) {
            accidentals.append(c);
        } else if (c.situation == Situation::Special) {
            specials.append(c);
        } else if (c.situation == Situation::Construction) {
            constructions.append(c);
        } else {
            variables.append(c);
        }
    }
    // Ventos sao exclusivos entre si. Para cada acao principal, as demais entram uma vez; o vento alterna: sem vento ou um vento por vez.
    auto windChoices = [&](const CaseSetup &main) {
        QVector<QString> choices;
        if (main.kind == Kind::Wind) {
            choices.append(QString());   // o vento principal ja e o unico
            return choices;
        }
        choices.append(QString());
        for (const auto &v : variables) {
            if (v.kind == Kind::Wind) {
                choices.append(v.name);
            }
        }
        return choices;
    };
    auto sameTerms = [](const QVector<QPair<QString, double>> &x, const QVector<QPair<QString, double>> &y) { return x == y; };
    auto appendUnique = [&](LoadCombination c, const QString &prefix, int &index) {
        for (const auto &e : out) {
            if (e.name.startsWith(prefix) && sameTerms(e.factors, c.factors)) {
                return;
            }
        }
        c.name = QStringLiteral("%1%2").arg(prefix).arg(index++);
        out.append(c);
    };
    auto variableTerms = [&](const CaseSetup &main, const QString &wind, auto factorFor) {
        QVector<QPair<QString, double>> terms;
        add(terms, main.name, factorFor(main, true));
        for (const auto &other : variables) {
            if (other.name == main.name) {
                continue;
            }
            if (other.kind == Kind::Wind && other.name != wind) {
                continue;
            }
            add(terms, other.name, factorFor(other, false));
        }
        return terms;
    };

    if (options.ultimate) {
        int index = 1;
        auto ultimate = [&](bool favorable) {
            if (variables.isEmpty()) {
                LoadCombination c;
                c.name = QStringLiteral("ELU%1").arg(index++);
                for (const auto &p : permanents) {
                    add(c.factors, p.name, favorable ? p.gammaFavorable : p.gammaUnfavorable);
                }
                if (!c.factors.isEmpty()) {
                    out.append(c);
                }
                return;
            }
            for (const auto &main : variables) {
                if (favorable && main.kind != Kind::Wind) {
                    continue;   // permanentes favoraveis so importam para acoes que as aliviam (vento)
                }
                for (const QString &wind : windChoices(main)) {
                    LoadCombination c;
                    for (const auto &p : permanents) {
                        add(c.factors, p.name, favorable ? p.gammaFavorable : p.gammaUnfavorable);
                    }
                    for (const auto &t : variableTerms(main, wind, [](const CaseSetup &s, bool isMain) { return isMain ? s.gammaUnfavorable : s.gammaUnfavorable * s.psi0; })) {
                        c.factors.append(t);
                    }
                    appendUnique(c, QStringLiteral("ELU"), index);
                }
            }
        };
        ultimate(false);
        if (options.favorablePermanents && std::any_of(variables.begin(), variables.end(), [](const CaseSetup &s) { return s.kind == Kind::Wind; })) {
            ultimate(true);
        }
    }

    // Combinacoes especiais e de construcao (4.8.7.2.2 e 4.8.7.2.3): uma por acao variavel especial; as demais com psi0,ef
    auto specialLike = [&](const QVector<CaseSetup> &mains, const QString &prefix) {
        int index = 1;
        for (const auto &main : mains) {
            for (const QString &wind : windChoices(main)) {
                LoadCombination c;
                for (const auto &p : permanents) {
                    add(c.factors, p.name, p.gammaSpecial);
                }
                add(c.factors, main.name, main.gammaSpecial);
                for (const auto &o : variables) {
                    if (o.kind == Kind::Wind && o.name != wind) {
                        continue;
                    }
                    const double psiEf = main.shortDuration ? o.psi2 : o.psi0;
                    add(c.factors, o.name, o.gammaSpecial * psiEf);
                }
                appendUnique(c, prefix, index);
            }
        }
    };
    if (options.special) {
        specialLike(specials, QStringLiteral("ESP"));
    }
    if (options.construction) {
        specialLike(constructions, QStringLiteral("CONS"));
    }

    // Combinacoes excepcionais (4.8.7.2.4): F_Q,exc sem majoracao; as demais variaveis com psi2 (psi0,ef de acao de duracao muito pequena)
    if (options.exceptional) {
        int index = 1;
        for (const auto &main : accidentals) {
            for (int pass = 0; pass < 2; ++pass) {
                const bool favorable = (pass == 1);
                if (favorable && !options.favorablePermanents) {
                    continue;
                }
                for (const QString &wind : windChoices(main)) {
                    LoadCombination c;
                    for (const auto &p : permanents) {
                        add(c.factors, p.name, favorable ? p.gammaFavorable : p.gammaExceptional);
                    }
                    add(c.factors, main.name, main.gammaExceptional);
                    for (const auto &o : variables) {
                        if (o.kind == Kind::Wind && o.name != wind) {
                            continue;
                        }
                        add(c.factors, o.name, o.gammaExceptional * o.psi2);
                    }
                    appendUnique(c, QStringLiteral("EXC"), index);
                }
            }
        }
    }

    // Combinacoes de incendio (NBR 14323:2013, 6.3): gama_g FG,k (1,0 se favoravel) + F_Q,exc + psi_fi FQ,k; sobrecargas de uso: 0,21 (sem predominancia de pesos fixos nem
    // grandes concentracoes de pessoas), 0,28 (comercial, escritorios, acesso publico) e 0,42 (bibliotecas, arquivos, depositos, oficinas e garagens). Vento: G + 0,20 W (6.3.2).
    if (options.fire) {
        int index = 1;
        auto fireFactor = [](const CaseSetup &c) {
            switch (c.kind) {
            case Kind::Variable: return 0.21;
            case Kind::VariableHeavy:
            case Kind::Footbridge: return 0.28;
            case Kind::VariableStorage: return 0.42;
            default: return 0.0;
            }
        };
        auto permanentsOf = [&](bool favorable) {
            QVector<QPair<QString, double>> terms;
            for (const auto &p : permanents) {
                add(terms, p.name, favorable ? p.gammaFavorable : p.gammaExceptional);
            }
            return terms;
        };
        LoadCombination base;
        base.factors = permanentsOf(false);
        for (const auto &v : variables) {
            add(base.factors, v.name, fireFactor(v));
        }
        if (!base.factors.isEmpty()) {
            appendUnique(base, QStringLiteral("FOGO"), index);
        }
        for (const auto &v : variables) {
            if (v.kind != Kind::Wind) {
                continue;
            }
            for (int pass = 0; pass < 2; ++pass) {
                if (pass == 1 && !options.favorablePermanents) {
                    continue;
                }
                LoadCombination c;
                c.factors = permanentsOf(pass == 1);
                add(c.factors, v.name, 0.20);
                appendUnique(c, QStringLiteral("FOGO"), index);
            }
        }
    }

    auto service = [&](const QString &prefix, auto mainFactor, auto otherFactor, bool needsMain) {
        int index = 1;
        if (variables.isEmpty() || !needsMain) {
            LoadCombination c;
            c.name = QStringLiteral("%1%2").arg(prefix).arg(index++);
            for (const auto &p : permanents) {
                add(c.factors, p.name, 1.0);
            }
            for (const auto &v : variables) {
                add(c.factors, v.name, otherFactor(v));
            }
            if (!c.factors.isEmpty()) {
                out.append(c);
            }
            return;
        }
        for (const auto &main : variables) {
            for (const QString &wind : windChoices(main)) {
                LoadCombination c;
                for (const auto &p : permanents) {
                    add(c.factors, p.name, 1.0);
                }
                add(c.factors, main.name, mainFactor(main));
                for (const auto &o : variables) {
                    if (o.name == main.name || (o.kind == Kind::Wind && o.name != wind)) {
                        continue;
                    }
                    add(c.factors, o.name, otherFactor(o));
                }
                appendUnique(c, prefix, index);
            }
        }
    };
    if (options.serviceQuasiPermanent) {
        service(QStringLiteral("ELS-QP"), [](const CaseSetup &) { return 0.0; }, [](const CaseSetup &s) { return s.psi2; }, false);
    }
    if (options.serviceFrequent) {
        service(QStringLiteral("ELS-CF"), [](const CaseSetup &s) { return s.psi1; }, [](const CaseSetup &s) { return s.psi2; }, true);
    }
    if (options.serviceRare) {
        service(QStringLiteral("ELS-CR"), [](const CaseSetup &) { return 1.0; }, [](const CaseSetup &s) { return s.psi1; }, true);
    }
    return out;
}

} // namespace Structura::App::Services
