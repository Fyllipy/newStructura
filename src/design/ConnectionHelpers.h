#pragma once

#include "design/Connections.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

/// Utilitarios internos dos modulos de ligacao (formatacao do memorial e verificacoes comuns).
namespace Structura::Design::Connections::detail {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSteelDensity = 7850.0;   // kg/m3
constexpr double kModulusE = 200000.0;     // MPa

template <class... Args>
inline std::string fmtn(const char *format, Args... args)
{
    char buffer[512];
    std::snprintf(buffer, sizeof buffer, format, args...);
    return buffer;
}

/// Verificacao do tipo demanda/resistencia em kN (entradas em N).
inline Check ratioN(const std::string &name, const std::string &clause, double demandN, double resistanceN)
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = demandN / 1000.0;
    c.resistance = resistanceN / 1000.0;
    c.unit = "kN";
    return c;
}

/// Verificacao do tipo demanda/resistencia em kN.m (entradas em N.mm).
inline Check ratioNmm(const std::string &name, const std::string &clause, double demandNmm, double resistanceNmm)
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = demandNmm / 1e6;
    c.resistance = resistanceNmm / 1e6;
    c.unit = "kN*m";
    return c;
}

inline Check ratioMPa(const std::string &name, const std::string &clause, double demandMPa, double resistanceMPa)
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = demandMPa;
    c.resistance = resistanceMPa;
    c.unit = "MPa";
    return c;
}

/// Regra de valor minimo: `adopted` >= `minimum` (mm).
inline Check minimumRule(const std::string &name, const std::string &clause, double adopted, double minimum, const char *unit = "mm")
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = adopted;
    c.resistance = minimum;
    c.unit = unit;
    c.minimumRule = true;
    return c;
}

/// Faixa de validade com limite superior: valor <= limite (razao 0 enquanto atendida).
inline Check rangeMax(const std::string &name, const std::string &clause, double value, double limit, const char *unit = "-")
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = value;
    c.resistance = limit;
    c.unit = unit;
    c.rangeRule = true;
    return c;
}

/// Faixa de validade com limite inferior: valor >= limite (razao 0 enquanto atendida).
inline Check rangeMin(const std::string &name, const std::string &clause, double value, double limit, const char *unit = "-")
{
    Check c = rangeMax(name, clause, value, limit, unit);
    c.rangeLower = true;
    return c;
}

/// Adimensional (interacao): demand <= 1.
inline Check dimensionless(const std::string &name, const std::string &clause, double value)
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = value;
    c.resistance = 1.0;
    c.unit = "-";
    return c;
}

/// 6.5.6: Rd = (0,6 fu Anv + Cts fu Ant)/ga2 <= (0,6 fy Agv + Cts fu Ant)/ga2 (Cts = 1).
struct BlockShearResult {
    double rd {0.0};
    double agv {0.0}, anv {0.0}, ant {0.0};
};

inline BlockShearResult blockShearRd(double t, double fy, double fu, double shearLength, double shearHoles, double tensionLength, double tensionHoles, double holeWidth,
                                     double ga2)
{
    BlockShearResult r;
    r.agv = t * shearLength;
    r.anv = t * std::max(0.0, shearLength - shearHoles * holeWidth);
    r.ant = t * std::max(0.0, tensionLength - tensionHoles * holeWidth);
    const double a = (0.6 * fu * r.anv + fu * r.ant) / ga2;
    const double b = (0.6 * fy * r.agv + fu * r.ant) / ga2;
    r.rd = std::min(a, b);
    return r;
}

/// Coeficiente de reducao por flambagem (8800:2024, 5.3.3) para o indice de esbeltez reduzido lambda0.
inline double chiCompression(double lambda0)
{
    return lambda0 <= 1.5 ? std::pow(0.658, lambda0 * lambda0) : 0.877 / (lambda0 * lambda0);
}

inline double plateMass(double lengthMm, double widthMm, double thicknessMm)
{
    return lengthMm * widthMm * thicknessMm * 1e-9 * kSteelDensity;
}

/// Acrescenta as verificacoes de `from` cujo nome comeca por `prefix` (ou todas se vazio), com sufixo no nome.
inline void appendChecks(std::vector<Check> &to, const std::vector<Check> &from, const std::string &prefix, const std::string &suffix)
{
    for (Check c : from) {
        if (prefix.empty() || c.name.rfind(prefix, 0) == 0) {
            c.name += suffix;
            to.push_back(std::move(c));
        }
    }
}

} // namespace Structura::Design::Connections::detail
