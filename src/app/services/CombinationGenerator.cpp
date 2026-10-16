#include "CombinationGenerator.h"

#include <algorithm>

namespace Structura::App::Services {

using Structura::App::ProjectData::LoadCombination;
using Kind = CombinationGenerator::Kind;

CombinationGenerator::CaseSetup CombinationGenerator::defaults(const QString &name, Kind kind)
{
    CaseSetup s;
    s.name = name;
    s.kind = kind;
    switch (kind) {
    case Kind::SteelSelfWeight:
        s.gammaUnfavorable = 1.25;
        s.gammaFavorable = 1.0;
        break;
    case Kind::Permanent:
        s.gammaUnfavorable = 1.35;
        s.gammaFavorable = 1.0;
        break;
    case Kind::Variable:   // sobrecarga em locais sem predominancia de pesos/equipamentos fixos
        s.gammaUnfavorable = 1.5;
        s.gammaFavorable = 0.0;
        s.psi0 = 0.5;
        s.psi1 = 0.4;
        s.psi2 = 0.3;
        break;
    case Kind::Wind:
        s.gammaUnfavorable = 1.4;
        s.gammaFavorable = 0.0;
        s.psi0 = 0.6;
        s.psi1 = 0.3;
        s.psi2 = 0.0;
        break;
    case Kind::Temperature:
        s.gammaUnfavorable = 1.2;
        s.gammaFavorable = 0.0;
        s.psi0 = 0.6;
        s.psi1 = 0.5;
        s.psi2 = 0.3;
        break;
    }
    return s;
}

Kind CombinationGenerator::guessKind(const QString &name, bool selfWeight)
{
    const QString n = name.toUpper();
    if (selfWeight || n == QLatin1String("PP") || n.startsWith(QLatin1String("PESO"))) {
        return Kind::SteelSelfWeight;
    }
    if (n.startsWith(QLatin1String("VT")) || n.startsWith(QLatin1String("VENTO")) || n.startsWith(QLatin1String("W"))) {
        return Kind::Wind;
    }
    if (n.startsWith(QLatin1String("SC")) || n.startsWith(QLatin1String("SOBRE")) || n.startsWith(QLatin1String("Q")) || n.startsWith(QLatin1String("USO"))) {
        return Kind::Variable;
    }
    if (n.startsWith(QLatin1String("TEMP")) || n.startsWith(QLatin1String("T")) ) {
        return Kind::Temperature;
    }
    return Kind::Permanent;
}

namespace {

bool isPermanent(Kind k) { return k == Kind::SteelSelfWeight || k == Kind::Permanent; }

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
    QVector<CaseSetup> permanents, variables;
    for (const auto &c : cases) {
        (isPermanent(c.kind) ? permanents : variables).append(c);
    }
    // grupos exclusivos: todos os ventos entre si
    auto exclusive = [](const CaseSetup &a, const CaseSetup &b) { return a.kind == Kind::Wind && b.kind == Kind::Wind; };

    // Para cada acao principal, as demais entram uma vez; ventos (exclusivos) alternam: sem vento ou um vento por vez.
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
