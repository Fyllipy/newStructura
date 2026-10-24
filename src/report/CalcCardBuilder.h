#pragma once

#include "report/ReportModel.h"

#include <map>
#include <string>
#include <vector>

namespace Structura::Report {

/// Valor conhecido de um simbolo (ja com virgula decimal) e sua unidade.
struct CalcEnvValue {
    std::string value;
    std::string unit;
};

/// Valores por simbolo normalizado (`normalizeSymbol`): propriedades da secao, do aco, comprimentos... O construtor da ficha
/// acrescenta os valores definidos nas proprias linhas de calculo (o ultimo valor de cada simbolo prevalece).
using CalcEnv = std::map<std::string, CalcEnvValue>;

struct CalcCardInput {
    std::string title;
    std::string clause;
    std::string description;       ///< o que a verificacao faz (texto corrido)
    std::vector<std::string> lines;   ///< linhas de calculo em texto simples (ver FormulaText.h)
    double demand {0.0};
    double resistance {0.0};
    std::string unit;              ///< unidade de `demand`/`resistance` (vazio = adimensional)
    double ratio {0.0};
    std::string inequalityLatex;   ///< desigualdade de resultado (vazio = "S_d <= R_d")
    std::string resultLabel;       ///< rotulo do valor de resultado (padrao "η")
    CalcEnv env;
};

/// "N_c,Rd" -> "Nc,Rd"; "gamma_a1" -> "gama_a1" ... chave de dicionario/ambiente.
std::string normalizeSymbol(const std::string &symbol);

/// Significado de um simbolo do dicionario (vazio quando desconhecido).
std::string symbolMeaning(const std::string &symbol);

/// Monta a ficha: passos a partir das linhas, lista "Onde:" com os simbolos conhecidos, e o resultado.
CalcCard buildCalcCard(const CalcCardInput &input);

/// Numero com virgula decimal e casas fixas.
std::string formatNumber(double value, int decimals);

} // namespace Structura::Report
