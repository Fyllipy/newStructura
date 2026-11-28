#pragma once

// Rigidez inicial do T-stub (EN 1993-1-8:2005, 6.3.2, Tabela 6.11) - FASE B da etapa A3. Matematica pura: sem Qt, sem interface, sem NBR.
//
// ESCOPO: uma fileira de parafusos (2 parafusos), mesa/chapa em flexao (k4 ou k5) e parafusos em tracao (k10), em serie. NAO calcula resistencia (ver TStub.h) e NAO
// altera nenhuma verificacao existente. Varias fileiras (efeito de grupo nos comprimentos efetivos, interacao entre fileiras): NAO implementado (PLANEJADO).
//
// Unidades: mm e MPa. Os coeficientes ki da EN tem dimensao de COMPRIMENTO (mm); a rigidez em forca e ki x E (N/mm) - ver forceStiffness.
// Os coeficientes (0,9 e 1,6) sao EXCLUSIVAMENTE da EN, separados de EnCoefficients (resistencia). Nao existe equivalente na NBR 8800.
// Nenhum dado e assumido por padrao: campos nao informados sao NaN e produzem estado invalido com a causa.
// Metodologia e referencias: docs/METODOLOGIA_COMPORTAMENTO_MECANICO_TSTUB.md.

#include <limits>
#include <string>
#include <vector>

namespace Structura::Design::TStub {

/// EN 1993-1-8, Tabela 6.11: k4 = k5 = 0,9 leff t^3 / m^3 (mesa do pilar / chapa em flexao, "for a single bolt-row in tension") e k10 = 1,6 As / Lb.
struct EnStiffnessCoefficients {
    double flangeBending {0.9};
    double boltTension {1.6};
};

/// Comprimento de alongamento do parafuso (Tabela 6.11, k10): espessura total do material agarrado + arruelas + (altura da cabeca + altura da porca)/2.
/// Todos os campos sao obrigatorios (NaN = nao informado); `washers` pode ser 0 (sem arruela), mas deve ser informado.
struct BoltElongation {
    double grip {std::numeric_limits<double>::quiet_NaN()};         ///< espessura total do material agarrado (mm)
    double washers {std::numeric_limits<double>::quiet_NaN()};      ///< espessura total das arruelas (mm), >= 0
    double headHeight {std::numeric_limits<double>::quiet_NaN()};   ///< altura da cabeca (mm)
    double nutHeight {std::numeric_limits<double>::quiet_NaN()};    ///< altura da porca (mm)
    [[nodiscard]] bool valid() const;
    [[nodiscard]] double length() const;   ///< Lb (mm); so tem sentido se valid()
};

/// Coeficiente de rigidez de um componente (mm).
struct ComponentStiffness {
    bool valid {false};
    std::string error;
    double k {0.0};
    std::vector<std::string> lines;   ///< equacao e valores substituidos (memoria de calculo)
};

/// k4 (mesa do pilar) ou k5 (chapa de extremidade): 0,9 leff t^3 / m^3. `leff` = o menor dos comprimentos efetivos DESTA fileira (EN 6.3.2, Tabela 6.11, nota de leff);
/// o chamador fornece o valor (o nucleo nao recalcula comprimentos efetivos aqui). `symbol` = "k4" ou "k5".
ComponentStiffness flangeBending(double m, double leff, double thickness, const EnStiffnessCoefficients &c, const std::string &symbol);
/// k10: 1,6 As / Lb, por fileira, com As de UM parafuso (conferido com o exemplo C2 do SCI P398 / New Steel Construction: 1,6 x 353 / 70,5 = 8,01 mm).
ComponentStiffness boltsInTension(double stressArea, const BoltElongation &bolt, const EnStiffnessCoefficients &c);

/// Associacao em SERIE de coeficientes de uma mesma fileira: 1 / soma(1/ki). `infinity` representa um componente infinitamente rigido (contribui 0).
/// Lista vazia, valor nao positivo ou NaN: devolve 0 (verifique `valid` de cada componente antes).
double seriesCoefficient(const std::vector<double> &k);

/// Dados de um T-stub de UMA fileira com 2 parafusos.
struct StiffnessInput {
    double m {std::numeric_limits<double>::quiet_NaN()};                 ///< mm (mesmo m da resistencia)
    double leff {std::numeric_limits<double>::quiet_NaN()};              ///< mm, menor comprimento efetivo desta fileira
    double thickness {std::numeric_limits<double>::quiet_NaN()};         ///< tf da mesa ou tp da chapa (mm)
    double boltStressArea {std::numeric_limits<double>::quiet_NaN()};    ///< As de um parafuso (mm2)
    BoltElongation bolt;
};

struct TStubStiffness {
    bool valid {false};
    std::string error;
    double kFlange {0.0};   ///< mm
    double kBolt {0.0};     ///< mm
    double k {0.0};         ///< mm: 1 / (1/kFlange + 1/kBolt)
    std::vector<std::string> lines;
};

/// Rigidez inicial do T-stub de uma fileira: mesa/chapa em flexao em serie com os parafusos em tracao.
TStubStiffness tStubStiffness(const StiffnessInput &input, const EnStiffnessCoefficients &c, const std::string &flangeSymbol, const std::string &label);

/// Rigidez em forca (N/mm) a partir do coeficiente ki (mm): ki x E. `youngModulus` em MPa; <= 0 ou nao finito: 0.
double forceStiffness(double coefficientMm, double youngModulus);

} // namespace Structura::Design::TStub
