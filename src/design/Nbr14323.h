#pragma once

// Verificacao de barras de aco em situacao de incendio pelo metodo simplificado da ABNT NBR 14323:2013 (secao 8):
// fatores de reducao do aco (5.1.1.2, tabelas 1 e 2), elevacao da temperatura do aco com ou sem revestimento contra fogo (8.5.1)
// e capacidades resistentes de barras tracionadas, comprimidas e fletidas (8.4). Unidades: mm, N, N*mm, MPa.

#include "design/BarDesign.h"

#include <string>
#include <vector>

namespace Structura::Design::Nbr14323 {

/// Fatores de reducao relativos a 20 C (tabelas 1 e 2, interpolacao linear): resistencia ao escoamento, modulo de elasticidade e
/// resistencia ao escoamento de secoes sujeitas a flambagem local.
double ky(double theta);
double kE(double theta);
double kSigma(double theta);

/// Temperatura dos gases (C) no incendio-padrao (ISO 834 / NBR 14432) apos `minutes` minutos.
double standardFireGas(double minutes);
/// Calor especifico do aco (J/kg C), Anexo E.3.1.
double steelSpecificHeat(double theta);

/// Revestimento contra fogo (8.5.1.2). `type`: 0 = sem revestimento; 1 = tipo contorno; 2 = tipo caixa.
struct Protection {
    int type {0};
    double thickness {0.0};        ///< tm (mm)
    double conductivity {0.2};     ///< lambda_m (W/m C)
    double density {300.0};        ///< rho_m (kg/m3)
    double specificHeat {1200.0};  ///< c_m (J/kg C)
};

struct Thermal {
    double theta {20.0};           ///< temperatura do aco apos o TRRF (C), distribuicao uniforme (8.1.2)
    double massivity {0.0};        ///< u/Ag (ou um/Ag), m-1
    double ksh {1.0};              ///< fator de sombreamento (8.5.1.1.2)
    std::vector<std::string> lines;
};

/// Temperatura do aco apos `trrfMinutes` de incendio-padrao. `threeSides`: face superior protegida por laje (perimetro exposto sem a face superior).
Thermal steelTemperature(const DesignSection &section, const Protection &protection, bool threeSides, double trrfMinutes);

struct FireInput {
    double trrf {60.0};                    ///< tempo requerido de resistencia ao fogo (min)
    Protection protection;
    bool threeSides {false};               ///< viga com laje no quarto lado: kappa = 1,40 (sem revestimento) ou 1,15 (com revestimento), 8.4.3.2.4
    std::vector<ComboEffects> combos;      ///< esforcos de calculo em situacao de incendio (combinacoes do item 6.3 ou 70 % dos esforcos a frio, 8.2.3)
    std::string basis;                     ///< texto da origem dos esforcos, para a memoria
};

/// Verifica a barra em incendio. Usa `in` (secao, aco, comprimentos de flambagem, contencao lateral) e `fire.combos`. Verificacoes com id "fire_*".
BarDesignOutput designBarFire(const BarDesignInput &in, const FireInput &fire);

} // namespace Structura::Design::Nbr14323
