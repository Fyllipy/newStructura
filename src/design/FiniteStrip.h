#pragma once

#include <array>
#include <vector>

/// Metodo das faixas finitas semi-analitico (harmonica unica, extremidades simplesmente apoiadas) para
/// flambagem elastica de secoes de paredes finas. Usado para obter as cargas/momentos criticos de flambagem
/// local e distorcional exigidos pela NBR 14762:2010 (9.7.3 e 9.8.2.3: "analise de estabilidade elastica").
namespace Structura::Design {

struct FiniteStripModel {
    std::vector<std::array<double, 2>> nodes;          ///< (a, b) da linha media, mm
    std::vector<std::array<int, 2>> strips;            ///< pares de nos conectados
    std::vector<double> thickness;                     ///< espessura de cada faixa (mm)
    std::vector<double> nodeStress;                    ///< tensao longitudinal de referencia em cada no (MPa, compressao POSITIVA)
    double E {200000.0};
    double nu {0.3};
    std::vector<std::array<int, 2>> fixedDofs;         ///< (no, grau 0..3 = Ua, V, Wb, theta) restritos (testes)
};

struct FiniteStripMinimum {
    double halfWaveLength {0.0};   ///< mm
    double loadFactor {0.0};       ///< multiplicador da tensao de referencia (flambagem elastica)
};

/// Fator de carga minimo para o comprimento de meia-onda `length`.
double finiteStripLoadFactor(const FiniteStripModel &model, double length);

/// Curva de assinatura e seus minimos locais (do menor comprimento para o maior).
std::vector<FiniteStripMinimum> finiteStripMinima(const FiniteStripModel &model, double minLength, double maxLength,
                                                  int samples = 70, std::vector<FiniteStripMinimum> *curve = nullptr);

} // namespace Structura::Design
