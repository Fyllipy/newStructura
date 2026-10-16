#pragma once

#include <memory>
#include <string>
#include <vector>

/// Subconjunto de LaTeX para equacoes de memoriais de calculo: fracoes, indices, expoentes, raizes,
/// letras gregas e simbolos. A mesma arvore alimenta o renderizador da tela e o exportador para
/// equacoes do Word (OMML).
namespace Structura::Report {

struct MathNode;
using MathPtr = std::shared_ptr<MathNode>;

struct MathNode {
    enum class Kind {
        Run,      ///< texto corrido (`text`); `upright` = romano, senao italico matematico
        Row,      ///< sequencia horizontal (`children`)
        Frac,     ///< children[0] / children[1]
        Script,   ///< base = children[0]; sub = children[1] (pode ser nulo); sup = children[2] (pode ser nulo)
        Sqrt,     ///< children[0] = radicando; children[1] = indice (pode ser nulo)
        Space     ///< espaco (`width` em em)
    };

    Kind kind {Kind::Row};
    std::string text;      ///< UTF-8
    bool upright {false};
    double width {0.0};
    std::vector<MathPtr> children;
};

/// Converte uma expressao LaTeX em arvore. Comandos desconhecidos viram texto romano.
MathPtr parseLatex(const std::string &latex);

} // namespace Structura::Report
