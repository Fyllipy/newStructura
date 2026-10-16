#include "report/MathParser.h"

#include <cctype>
#include <map>

namespace Structura::Report {

namespace {

const std::map<std::string, std::string> &symbolTable()
{
    static const std::map<std::string, std::string> table = {
        {"alpha", "α"}, {"beta", "β"}, {"gamma", "γ"}, {"delta", "δ"}, {"epsilon", "ε"}, {"varepsilon", "ε"}, {"zeta", "ζ"},
        {"eta", "η"}, {"theta", "θ"}, {"lambda", "λ"}, {"mu", "μ"}, {"nu", "ν"}, {"xi", "ξ"}, {"pi", "π"}, {"rho", "ρ"},
        {"sigma", "σ"}, {"tau", "τ"}, {"phi", "φ"}, {"varphi", "φ"}, {"chi", "χ"}, {"psi", "ψ"}, {"omega", "ω"},
        {"Gamma", "Γ"}, {"Delta", "Δ"}, {"Theta", "Θ"}, {"Lambda", "Λ"}, {"Sigma", "Σ"}, {"Phi", "Φ"}, {"Psi", "Ψ"}, {"Omega", "Ω"},
        {"cdot", "·"}, {"times", "×"}, {"pm", "±"}, {"le", "≤"}, {"leq", "≤"}, {"ge", "≥"}, {"geq", "≥"}, {"ne", "≠"}, {"neq", "≠"},
        {"approx", "≈"}, {"to", "→"}, {"rightarrow", "→"}, {"Rightarrow", "⇒"}, {"infty", "∞"}, {"sum", "∑"}, {"deg", "°"},
        {"degree", "°"}, {"ldots", "…"}, {"cdots", "…"}, {"min", "min"}, {"max", "max"}, {"sin", "sen"}, {"cos", "cos"}, {"tan", "tan"},
        {"ln", "ln"}, {"log", "log"}};
    return table;
}

bool isUprightFunction(const std::string &name)
{
    return name == "min" || name == "max" || name == "sin" || name == "cos" || name == "tan" || name == "ln" || name == "log";
}

class Parser
{
public:
    explicit Parser(const std::string &s) : m_s(s) {}

    MathPtr parse()
    {
        MathPtr row = parseRow(false);
        return row;
    }

private:
    const std::string &m_s;
    std::size_t m_i {0};

    bool atEnd() const { return m_i >= m_s.size(); }
    char peek() const { return m_s[m_i]; }

    static MathPtr make(MathNode::Kind kind)
    {
        auto n = std::make_shared<MathNode>();
        n->kind = kind;
        return n;
    }
    static MathPtr run(const std::string &text, bool upright)
    {
        auto n = make(MathNode::Kind::Run);
        n->text = text;
        n->upright = upright;
        return n;
    }

    // Le um argumento: {grupo} ou um unico simbolo.
    MathPtr parseArgument()
    {
        skipSpaces();
        if (atEnd()) {
            return make(MathNode::Kind::Row);
        }
        if (peek() == '{') {
            ++m_i;
            MathPtr row = parseRow(true);
            return row;
        }
        MathPtr atom = parseAtom();
        auto row = make(MathNode::Kind::Row);
        if (atom) {
            row->children.push_back(atom);
        }
        return row;
    }

    void skipSpaces()
    {
        while (!atEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
            ++m_i;
        }
    }

    std::string readUtf8Char()
    {
        const unsigned char c = static_cast<unsigned char>(m_s[m_i]);
        std::size_t len = 1;
        if (c >= 0xF0) {
            len = 4;
        } else if (c >= 0xE0) {
            len = 3;
        } else if (c >= 0xC0) {
            len = 2;
        }
        std::string out = m_s.substr(m_i, len);
        m_i += len;
        return out;
    }

    std::string readBraced()
    {
        skipSpaces();
        if (atEnd() || peek() != '{') {
            return {};
        }
        ++m_i;
        int depth = 1;
        std::string out;
        while (!atEnd() && depth > 0) {
            const char c = m_s[m_i++];
            if (c == '{') {
                ++depth;
            } else if (c == '}') {
                if (--depth == 0) {
                    break;
                }
            }
            out.push_back(c);
        }
        return out;
    }

    // Um atomo (sem scripts)
    MathPtr parseAtom()
    {
        if (atEnd()) {
            return nullptr;
        }
        const char c = peek();
        if (c == '{') {
            ++m_i;
            return parseRow(true);
        }
        if (c == '\\') {
            ++m_i;
            if (atEnd()) {
                return nullptr;
            }
            // comandos de espaco / simbolos de um caractere
            if (!std::isalpha(static_cast<unsigned char>(peek()))) {
                const char d = m_s[m_i++];
                auto space = make(MathNode::Kind::Space);
                switch (d) {
                case ',': space->width = 0.17; return space;
                case ':': space->width = 0.22; return space;
                case ';': space->width = 0.28; return space;
                case '!': space->width = -0.1; return space;
                case ' ': space->width = 0.28; return space;
                default: return run(std::string(1, d), true);
                }
            }
            std::string name;
            while (!atEnd() && std::isalpha(static_cast<unsigned char>(peek()))) {
                name.push_back(m_s[m_i++]);
            }
            if (name == "frac" || name == "dfrac" || name == "tfrac") {
                auto frac = make(MathNode::Kind::Frac);
                frac->children.push_back(parseArgument());
                frac->children.push_back(parseArgument());
                return frac;
            }
            if (name == "sqrt") {
                auto root = make(MathNode::Kind::Sqrt);
                MathPtr index;
                skipSpaces();
                if (!atEnd() && peek() == '[') {
                    ++m_i;
                    std::string idx;
                    while (!atEnd() && peek() != ']') {
                        idx.push_back(m_s[m_i++]);
                    }
                    if (!atEnd()) {
                        ++m_i;
                    }
                    Parser sub(idx);
                    index = sub.parse();
                }
                root->children.push_back(parseArgument());
                root->children.push_back(index);
                return root;
            }
            if (name == "mathrm" || name == "text" || name == "textbf" || name == "operatorname" || name == "mathit") {
                const std::string body = readBraced();
                auto r = run(body, name != "mathit");
                return r;
            }
            if (name == "left" || name == "right") {
                skipSpaces();
                if (!atEnd()) {
                    if (peek() == '.') {
                        ++m_i;
                        return nullptr;
                    }
                    return run(readUtf8Char(), true);
                }
                return nullptr;
            }
            if (name == "quad") {
                auto space = make(MathNode::Kind::Space);
                space->width = 1.0;
                return space;
            }
            if (name == "qquad") {
                auto space = make(MathNode::Kind::Space);
                space->width = 2.0;
                return space;
            }
            const auto it = symbolTable().find(name);
            if (it != symbolTable().end()) {
                bool greekLetter = false;
                static const std::string greek = "alpha beta gamma delta epsilon varepsilon zeta eta theta lambda mu nu xi pi rho sigma tau phi varphi chi psi omega";
                greekLetter = (" " + greek + " ").find(" " + name + " ") != std::string::npos;
                return run(it->second, !greekLetter);
            }
            return run(name, true);
        }
        if (c == '}') {
            return nullptr;
        }
        // caractere comum
        if (std::isalpha(static_cast<unsigned char>(c))) {
            ++m_i;
            return run(std::string(1, c), false);
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == ',') {
            std::string number;
            while (!atEnd() && (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.' || peek() == ',')) {
                // virgula seguida de nao-digito encerra o numero
                if (peek() == ',' && (m_i + 1 >= m_s.size() || !std::isdigit(static_cast<unsigned char>(m_s[m_i + 1])))) {
                    break;
                }
                if (peek() == '.' && (m_i + 1 >= m_s.size() || !std::isdigit(static_cast<unsigned char>(m_s[m_i + 1])))) {
                    break;
                }
                number.push_back(m_s[m_i++]);
            }
            if (number.empty()) {
                ++m_i;
                return run(std::string(1, c), true);
            }
            for (char &ch : number) {
                if (ch == '.') {
                    ch = ',';
                }
            }
            return run(number, true);
        }
        if (c == ' ') {
            ++m_i;
            auto space = make(MathNode::Kind::Space);
            space->width = 0.25;
            return space;
        }
        const unsigned char u = static_cast<unsigned char>(c);
        if (u >= 0x80) {
            const std::string ch = readUtf8Char();
            return run(ch, false);
        }
        ++m_i;
        return run(std::string(1, c), true);
    }

    MathPtr parseRow(bool untilBrace)
    {
        auto row = make(MathNode::Kind::Row);
        while (!atEnd()) {
            if (untilBrace && peek() == '}') {
                ++m_i;
                break;
            }
            if (peek() == '}') {
                ++m_i;
                continue;
            }
            if (peek() == '^' || peek() == '_') {
                // script sem base explicita: aplica ao ultimo filho
                MathPtr base = row->children.empty() ? run("", true) : row->children.back();
                if (!row->children.empty()) {
                    row->children.pop_back();
                }
                row->children.push_back(parseScripts(base));
                continue;
            }
            MathPtr atom = parseAtom();
            if (!atom) {
                continue;
            }
            skipSpaces();
            if (!atEnd() && (peek() == '^' || peek() == '_')) {
                atom = parseScripts(atom);
            }
            row->children.push_back(atom);
        }
        return row;
    }

    MathPtr parseScripts(MathPtr base)
    {
        MathPtr sub;
        MathPtr sup;
        while (!atEnd() && (peek() == '^' || peek() == '_')) {
            const char which = m_s[m_i++];
            MathPtr arg = parseArgument();
            (which == '^' ? sup : sub) = arg;
            skipSpaces();
        }
        auto script = make(MathNode::Kind::Script);
        script->children = {base, sub, sup};
        return script;
    }
};

} // namespace

MathPtr parseLatex(const std::string &latex)
{
    Parser parser(latex);
    return parser.parse();
}

} // namespace Structura::Report
