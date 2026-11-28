#include "design/TStubStiffness.h"

#include <cmath>
#include <cstdio>

namespace Structura::Design::TStub {

namespace {

std::string num(const char *format, double value)
{
    char buf[64];
    std::snprintf(buf, sizeof buf, format, value);
    return buf;
}

bool positive(double v)
{
    return std::isfinite(v) && v > 0.0;
}

} // namespace

bool BoltElongation::valid() const
{
    return positive(grip) && std::isfinite(washers) && washers >= 0.0 && positive(headHeight) && positive(nutHeight);
}

double BoltElongation::length() const
{
    return grip + washers + 0.5 * (headHeight + nutHeight);
}

ComponentStiffness flangeBending(double m, double leff, double thickness, const EnStiffnessCoefficients &c, const std::string &symbol)
{
    ComponentStiffness out;
    if (!positive(m) || !positive(leff) || !positive(thickness) || !positive(c.flangeBending)) {
        out.error = symbol + ": dados invalidos ou nao informados (m, leff, espessura ou coeficiente nao positivos)";
        return out;
    }
    out.k = c.flangeBending * leff * thickness * thickness * thickness / (m * m * m);
    out.valid = true;
    out.lines.push_back(symbol + " = " + num("%.2f", c.flangeBending) + " leff t^3 / m^3 = " + num("%.2f", c.flangeBending) + " x " + num("%.2f", leff) + " x " + num("%.2f", thickness) + "^3 / " +
                        num("%.2f", m) + "^3 = " + num("%.3f", out.k) + " mm   [EN 1993-1-8, 6.3.2, Tabela 6.11; leff = menor comprimento efetivo da fileira]");
    return out;
}

ComponentStiffness boltsInTension(double stressArea, const BoltElongation &bolt, const EnStiffnessCoefficients &c)
{
    ComponentStiffness out;
    if (!positive(stressArea) || !bolt.valid() || !positive(c.boltTension)) {
        out.error = "k10: dados invalidos ou nao informados (As, Lb: material agarrado, arruelas, altura da cabeca e da porca, ou coeficiente)";
        return out;
    }
    const double lb = bolt.length();
    out.k = c.boltTension * stressArea / lb;
    out.valid = true;
    out.lines.push_back("Lb = material agarrado + arruelas + (cabeca + porca)/2 = " + num("%.2f", bolt.grip) + " + " + num("%.2f", bolt.washers) + " + (" + num("%.2f", bolt.headHeight) + " + " +
                        num("%.2f", bolt.nutHeight) + ")/2 = " + num("%.2f", lb) + " mm");
    out.lines.push_back("k10 = " + num("%.2f", c.boltTension) + " As / Lb = " + num("%.2f", c.boltTension) + " x " + num("%.1f", stressArea) + " / " + num("%.2f", lb) + " = " + num("%.3f", out.k) +
                        " mm   [EN 1993-1-8, 6.3.2, Tabela 6.11; uma fileira, As de um parafuso]");
    return out;
}

double seriesCoefficient(const std::vector<double> &k)
{
    if (k.empty()) {
        return 0.0;
    }
    double flexibility = 0.0;
    for (const double ki : k) {
        if (std::isnan(ki) || !(ki > 0.0)) {
            return 0.0;
        }
        if (std::isinf(ki)) {
            continue;   // componente rigido: flexibilidade nula
        }
        flexibility += 1.0 / ki;
    }
    return flexibility > 0.0 ? 1.0 / flexibility : std::numeric_limits<double>::infinity();
}

TStubStiffness tStubStiffness(const StiffnessInput &in, const EnStiffnessCoefficients &c, const std::string &flangeSymbol, const std::string &label)
{
    TStubStiffness out;
    const ComponentStiffness flange = flangeBending(in.m, in.leff, in.thickness, c, flangeSymbol);
    if (!flange.valid) {
        out.error = label + ": " + flange.error;
        return out;
    }
    const ComponentStiffness bolts = boltsInTension(in.boltStressArea, in.bolt, c);
    if (!bolts.valid) {
        out.error = label + ": " + bolts.error;
        return out;
    }
    out.kFlange = flange.k;
    out.kBolt = bolts.k;
    out.k = seriesCoefficient({flange.k, bolts.k});
    out.valid = true;
    out.lines.push_back(label + " - rigidez inicial (1 fileira, EN 1993-1-8, 6.3.2):");
    for (const auto &l : flange.lines) {
        out.lines.push_back("  " + l);
    }
    for (const auto &l : bolts.lines) {
        out.lines.push_back("  " + l);
    }
    out.lines.push_back("  k_T = 1 / (1/" + flangeSymbol + " + 1/k10) = " + num("%.3f", out.k) + " mm (em serie: a mesma forca atravessa a mesa e os parafusos); rigidez em forca = k_T x E");
    return out;
}

double forceStiffness(double coefficientMm, double youngModulus)
{
    if (!std::isfinite(coefficientMm) || !std::isfinite(youngModulus) || youngModulus <= 0.0) {
        return 0.0;
    }
    return coefficientMm * youngModulus;
}

} // namespace Structura::Design::TStub
