#pragma once

#include <string>
#include <vector>

namespace Structura::Report {

/// Trecho de uma linha de calculo: texto corrido ou formula (LaTeX).
struct FormulaPiece {
    enum class Kind { Text, Math };
    Kind kind {Kind::Text};
    std::string text;
};

/// Converte uma linha de memoria de calculo escrita em texto simples ("Nc,Rd = chi Aef fy / 1.10 = 142.5 kN",
/// com `x` como multiplicacao, `/` como fracao, `sqrt(...)`, letras gregas por extenso e indices por
/// concatenacao) em uma sequencia de texto e formulas LaTeX. Rotulos antes de ":" e notas finais entre
/// parenteses viram texto. Numeros ficam com virgula decimal.
std::vector<FormulaPiece> convertCalculationLine(const std::string &line);

/// Expressao em texto simples -> LaTeX (sem rotulos).
std::string expressionToLatex(const std::string &expression);

/// Restaura acentos de palavras comuns do vocabulario do calculo ("secao" -> "seção").
std::string restoreAccents(const std::string &text);

/// Troca o ponto decimal por virgula em numeros ("1.25" -> "1,25").
std::string decimalComma(const std::string &text);

} // namespace Structura::Report
