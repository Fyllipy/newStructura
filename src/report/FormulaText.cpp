#include "report/FormulaText.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <set>

namespace Structura::Report {

namespace {

// ---------------------------------------------------------------------------------------------------------------------
// Vocabulario
// ---------------------------------------------------------------------------------------------------------------------

const std::map<std::string, std::string> &greekNames()
{
    static const std::map<std::string, std::string> m = {
        {"gama", "\\gamma"}, {"gamma", "\\gamma"}, {"chi", "\\chi"}, {"lambda", "\\lambda"}, {"sigma", "\\sigma"}, {"tau", "\\tau"},
        {"psi", "\\psi"}, {"phi", "\\phi"}, {"eta", "\\eta"}, {"pi", "\\pi"}, {"theta", "\\theta"}, {"beta", "\\beta"},
        {"alpha", "\\alpha"}, {"delta", "\\delta"}, {"mu", "\\mu"}, {"rho", "\\rho"}, {"epsilon", "\\varepsilon"}, {"omega", "\\omega"},
        {"Delta", "\\Delta"}, {"nu", "\\nu"}};
    return m;
}

const std::set<std::string> &uprightSubscripts()
{
    static const std::set<std::string> s = {"Rd", "Sd", "ef", "el", "pl", "cr", "max", "min", "dist", "eq", "red", "tot", "lim", "rel",
                                            "FLT", "FLM", "FLA", "dim", "ult", "ser", "nom", "adm", "yy", "zz", "cb", "cm", "net", "ne"};
    return s;
}

const std::set<std::string> &unitNames()
{
    static const std::set<std::string> s = {"kN", "N", "MPa", "GPa", "mm", "mm2", "mm3", "mm4", "mm6", "m", "cm", "cm2", "cm4", "kg", "deg", "m2", "m3", "m4"};
    return s;
}

std::string unitLatex(const std::string &u)
{
    if (u == "deg") {
        return "^{\\circ}";
    }
    std::string base = u;
    std::string power;
    if (u.size() > 1 && std::isdigit(static_cast<unsigned char>(u.back()))) {
        base = u.substr(0, u.size() - 1);
        power = std::string("^{") + u.back() + "}";
    }
    return "\\mathrm{" + base + "}" + power;
}

std::string mapSubscript(const std::string &sub)
{
    if (sub.empty()) {
        return {};
    }
    if (uprightSubscripts().count(sub)) {
        return "\\mathrm{" + sub + "}";
    }
    // "c,Rd" -> c,\mathrm{Rd}
    std::string out;
    std::string word;
    auto flush = [&]() {
        if (word.empty()) {
            return;
        }
        out += uprightSubscripts().count(word) ? "\\mathrm{" + word + "}" : word;
        word.clear();
    };
    for (const char c : sub) {
        if (c == ',') {
            flush();
            out += ",";
        } else if (c == '_') {
            flush();
            out += " ";
        } else {
            word.push_back(c);
        }
    }
    flush();
    return out;
}

std::string mapIdentifier(const std::string &id)
{
    if (id.empty()) {
        return {};
    }
    // potencias coladas: pi2, Lx2, Ly2, Lz2, Lb2
    static const std::set<std::string> squared = {"pi2", "Lx2", "Ly2", "Lz2", "Lb2", "L2", "Lx1,eq2", "Ne2"};
    if (squared.count(id)) {
        return mapIdentifier(id.substr(0, id.size() - 1)) + "^{2}";
    }
    // gregas (com subscrito: lambda_p, gama_a1, sigma_el, lambda0)
    std::string head = id;
    std::string sub;
    const auto us = id.find('_');
    if (us != std::string::npos) {
        head = id.substr(0, us);
        sub = id.substr(us + 1);
    }
    const auto g = greekNames().find(head);
    if (g != greekNames().end()) {
        return g->second + (sub.empty() ? "" : "_{" + mapSubscript(sub) + "}");
    }
    // grega com digito: lambda0, lambda1
    for (const auto &kv : greekNames()) {
        if (head.size() > kv.first.size() && head.compare(0, kv.first.size(), kv.first) == 0) {
            const std::string rest = head.substr(kv.first.size());
            if (std::all_of(rest.begin(), rest.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
                return kv.second + "_{" + rest + (sub.empty() ? "" : "," + mapSubscript(sub)) + "}";
            }
        }
    }
    // excecoes
    if (id == "KL") {
        return "KL";
    }
    if (id.size() >= 3 && std::all_of(id.begin(), id.end(), [](char c) { return std::isupper(static_cast<unsigned char>(c)) != 0; })) {
        return "\\mathrm{" + id + "}";   // siglas: FLT, FLM, FLA, MLE
    }
    if (id == "Ct") {
        return "C_{t}";
    }
    if (id.size() == 1) {
        return id;
    }
    // palavras longas em minusculas: texto
    const bool allLower = std::all_of(id.begin(), id.end(), [](char c) { return std::islower(static_cast<unsigned char>(c)) != 0; });
    if (allLower && id.size() >= 4 && id != "bef" ) {
        return "\\mathrm{" + id + "}";
    }
    // inicial + subscrito: Ag, fy, Nex, Mpl, Nc,Rd, B2, r0 ...
    std::string base(1, id[0]);
    std::string rest = id.substr(1);
    if (!sub.empty()) {
        rest = id.substr(1, us - 1) + (us > 1 ? "," : "") + sub;
    }
    // "Cw", "Ix": subscrito simples
    return base + "_{" + mapSubscript(rest) + "}";
}

// ---------------------------------------------------------------------------------------------------------------------
// Tokens
// ---------------------------------------------------------------------------------------------------------------------

struct Token {
    enum class Kind { Num, Ident, Op, Open, Close, Comma } kind;
    std::string text;
    bool spaceBefore {false};
};

std::vector<Token> tokenize(const std::string &s)
{
    std::vector<Token> out;
    std::size_t i = 0;
    bool space = false;
    auto push = [&](Token::Kind k, std::string t) {
        out.push_back({k, std::move(t), space});
        space = false;
    };
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isspace(c)) {
            space = true;
            ++i;
            continue;
        }
        if (std::isdigit(c) || (c == '.' && i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1])))) {
            std::size_t j = i;
            while (j < s.size() && (std::isdigit(static_cast<unsigned char>(s[j])) || s[j] == '.' || (s[j] == ',' && j + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[j + 1]))))) {
                // "1.2.3" (referencias) nao e numero simples, mas aqui trata ate o fim
                ++j;
            }
            // expoente cientifico
            if (j < s.size() && (s[j] == 'e' || s[j] == 'E') && j + 1 < s.size() && (s[j + 1] == '+' || s[j + 1] == '-' || std::isdigit(static_cast<unsigned char>(s[j + 1])))) {
                std::size_t k = j + 1;
                if (s[k] == '+' || s[k] == '-') {
                    ++k;
                }
                if (k < s.size() && std::isdigit(static_cast<unsigned char>(s[k]))) {
                    while (k < s.size() && std::isdigit(static_cast<unsigned char>(s[k]))) {
                        ++k;
                    }
                    j = k;
                }
            }
            push(Token::Kind::Num, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (std::isalpha(c) || c >= 0x80) {
            std::size_t j = i;
            while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_' || static_cast<unsigned char>(s[j]) >= 0x80 ||
                                    (s[j] == ',' && j + 1 < s.size() && std::isalpha(static_cast<unsigned char>(s[j + 1])) && j > i &&
                                     std::isalnum(static_cast<unsigned char>(s[j - 1])) && !std::isspace(static_cast<unsigned char>(s[j - 1]))))) {
                ++j;
            }
            push(Token::Kind::Ident, s.substr(i, j - i));
            i = j;
            continue;
        }
        if (c == '(' || c == '[') {
            push(Token::Kind::Open, std::string(1, static_cast<char>(c)));
            ++i;
            continue;
        }
        if (c == ')' || c == ']') {
            push(Token::Kind::Close, std::string(1, static_cast<char>(c)));
            ++i;
            continue;
        }
        if (c == ',') {
            push(Token::Kind::Comma, ",");
            ++i;
            continue;
        }
        // operadores compostos
        static const char *two[] = {"->", "<=", ">=", "!=", "=="};
        bool matched = false;
        for (const char *op : two) {
            if (s.compare(i, 2, op) == 0) {
                push(Token::Kind::Op, op);
                i += 2;
                matched = true;
                break;
            }
        }
        if (matched) {
            continue;
        }
        push(Token::Kind::Op, std::string(1, static_cast<char>(c)));
        ++i;
    }
    return out;
}

std::string numberLatex(const std::string &n)
{
    // 6.391e+09 -> 6{,}391 \cdot 10^{9}
    std::string mantissa = n;
    std::string exponent;
    const auto e = n.find_first_of("eE");
    if (e != std::string::npos) {
        mantissa = n.substr(0, e);
        exponent = n.substr(e + 1);
    }
    std::string out;
    for (const char c : mantissa) {
        out += (c == '.' || c == ',') ? "{,}" : std::string(1, c);
    }
    if (!exponent.empty()) {
        int value = 0;
        try {
            value = std::stoi(exponent);
        } catch (...) {
        }
        out += "\\cdot 10^{" + std::to_string(value) + "}";
    }
    return out;
}

class ExprParser
{
public:
    explicit ExprParser(std::vector<Token> tokens) : m_t(std::move(tokens)) {}

    std::string parse()
    {
        return sequence(false);
    }

private:
    std::vector<Token> m_t;
    std::size_t m_i {0};

    bool atEnd() const { return m_i >= m_t.size(); }
    const Token &peek(std::size_t k = 0) const { return m_t[m_i + k]; }

    static bool isRelation(const std::string &op)
    {
        return op == "=" || op == "<" || op == ">" || op == "<=" || op == ">=" || op == "!=" || op == "->" || op == "==" || op == "+" || op == "-" || op == "~";
    }
    static std::string relationLatex(const std::string &op)
    {
        if (op == "<=") return "\\le";
        if (op == ">=") return "\\ge";
        if (op == "->") return "\\to";
        if (op == "!=") return "\\ne";
        if (op == "==") return "=";
        if (op == "~") return "\\approx";
        return op;
    }

    bool startsOperand(std::size_t k = 0) const
    {
        if (m_i + k >= m_t.size()) {
            return false;
        }
        const Token &t = m_t[m_i + k];
        return t.kind == Token::Kind::Num || t.kind == Token::Kind::Ident || t.kind == Token::Kind::Open;
    }

    // sequencia ate fecha-parenteses (se `inParen`) ou fim
    std::string sequence(bool inParen)
    {
        std::string out;
        std::vector<std::string> product;
        auto flushProduct = [&]() {
            for (std::size_t k = 0; k < product.size(); ++k) {
                if (k > 0 && product[k] != "\\cdot") {
                    if (product[k - 1] != "\\cdot") {
                        out += "\\,";
                    }
                }
                out += (k > 0 && product[k] == "\\cdot") ? "\\cdot " : product[k];
            }
            product.clear();
        };
        bool lastWasOperand = false;
        while (!atEnd()) {
            const Token &t = peek();
            if (t.kind == Token::Kind::Close) {
                if (inParen) {
                    break;
                }
                ++m_i;
                continue;
            }
            if (t.kind == Token::Kind::Op) {
                if (t.text == "/") {
                    ++m_i;
                    // fracao: numerador = produto ate aqui; denominador = proximo fator
                    std::string numerator;
                    {
                        std::string saved = out;
                        out.clear();
                        flushProduct();
                        numerator = out;
                        out = saved;
                    }
                    if (numerator.empty()) {
                        numerator = "1";
                    }
                    const std::string denominator = factor();
                    product.push_back("\\frac{" + stripParens(numerator) + "}{" + stripParens(denominator) + "}");
                    lastWasOperand = true;
                    continue;
                }
                if (t.text == "*" || (t.text == "x" )) {
                    ++m_i;
                    product.push_back("\\cdot");
                    lastWasOperand = false;
                    continue;
                }
                if (t.text == "^") {
                    ++m_i;
                    const std::string exponent = factor();
                    if (!product.empty()) {
                        product.back() = "{" + product.back() + "}^{" + stripParens(exponent) + "}";
                    }
                    continue;
                }
                flushProduct();
                if (isRelation(t.text)) {
                    out += " " + relationLatex(t.text) + " ";
                } else if (t.text == "%") {
                    out += "\\%";
                } else {
                    out += t.text;
                }
                ++m_i;
                lastWasOperand = false;
                continue;
            }
            if (t.kind == Token::Kind::Comma) {
                flushProduct();
                out += ",\\;";
                ++m_i;
                lastWasOperand = false;
                continue;
            }
            // 'x' isolado entre operandos = multiplicacao
            if (t.kind == Token::Kind::Ident && t.text == "x" && lastWasOperand && startsOperand(1)) {
                ++m_i;
                product.push_back("\\cdot");
                lastWasOperand = false;
                continue;
            }
            product.push_back(factor());
            lastWasOperand = true;
        }
        flushProduct();
        return out;
    }

    static std::string stripParens(const std::string &s)
    {
        const std::string left = "\\left(";
        const std::string right = "\\right)";
        if (s.size() > left.size() + right.size() && s.compare(0, left.size(), left) == 0 && s.compare(s.size() - right.size(), right.size(), right) == 0) {
            // so remove se os parenteses externos casam entre si
            int depth = 0;
            for (std::size_t i = 0; i < s.size();) {
                if (s.compare(i, left.size(), left) == 0) {
                    ++depth;
                    i += left.size();
                } else if (s.compare(i, right.size(), right) == 0) {
                    --depth;
                    i += right.size();
                    if (depth == 0 && i < s.size()) {
                        return s;
                    }
                } else {
                    ++i;
                }
            }
            return s.substr(left.size(), s.size() - left.size() - right.size());
        }
        return s;
    }

    std::string factor()
    {
        if (atEnd()) {
            return {};
        }
        const Token t = peek();
        std::string result;
        if (t.kind == Token::Kind::Num) {
            ++m_i;
            result = numberLatex(t.text);
            // unidade
            if (!atEnd() && peek().kind == Token::Kind::Ident && unitNames().count(peek().text)) {
                std::string unit = unitLatex(peek().text);
                ++m_i;
                while (m_i + 1 < m_t.size() && m_t[m_i].kind == Token::Kind::Op && (m_t[m_i].text == "*" || m_t[m_i].text == "/") &&
                       m_t[m_i + 1].kind == Token::Kind::Ident && unitNames().count(m_t[m_i + 1].text)) {
                    unit += m_t[m_i].text == "*" ? "\\cdot " : "/";
                    unit += unitLatex(m_t[m_i + 1].text);
                    m_i += 2;
                }
                result += "\\," + unit;
            }
        } else if (t.kind == Token::Kind::Ident) {
            ++m_i;
            if (t.text == "sqrt" && !atEnd() && peek().kind == Token::Kind::Open) {
                ++m_i;
                const std::string inner = sequence(true);
                if (!atEnd() && peek().kind == Token::Kind::Close) {
                    ++m_i;
                }
                result = "\\sqrt{" + inner + "}";
            } else if ((t.text == "min" || t.text == "max") && !atEnd() && peek().kind == Token::Kind::Open) {
                ++m_i;
                const std::string inner = sequence(true);
                if (!atEnd() && peek().kind == Token::Kind::Close) {
                    ++m_i;
                }
                result = "\\" + t.text + "\\left(" + inner + "\\right)";
            } else {
                result = mapIdentifier(t.text);
            }
        } else if (t.kind == Token::Kind::Open) {
            ++m_i;
            const std::string inner = sequence(true);
            if (!atEnd() && peek().kind == Token::Kind::Close) {
                ++m_i;
            }
            result = "\\left(" + inner + "\\right)";
        } else {
            ++m_i;
            result = t.text;
        }
        // expoente imediato
        if (!atEnd() && peek().kind == Token::Kind::Op && peek().text == "^") {
            ++m_i;
            const std::string exponent = factor();
            result = "{" + result + "}^{" + stripParens(exponent) + "}";
        }
        return result;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Linha -> pecas
// ---------------------------------------------------------------------------------------------------------------------

std::vector<std::string> splitTopLevel(const std::string &s, const std::string &separator)
{
    std::vector<std::string> parts;
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(' || s[i] == '[') {
            ++depth;
        } else if (s[i] == ')' || s[i] == ']') {
            --depth;
        } else if (depth == 0 && s.compare(i, separator.size(), separator) == 0) {
            parts.push_back(s.substr(start, i - start));
            start = i + separator.size();
            i += separator.size() - 1;
        }
    }
    parts.push_back(s.substr(start));
    return parts;
}

std::string trim(const std::string &s)
{
    std::size_t a = 0;
    std::size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) {
        ++a;
    }
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
        --b;
    }
    return s.substr(a, b - a);
}

bool looksLikeProse(const std::string &inner)
{
    static const std::regex clause(R"(^\s*(Tab\.?\s*)?[0-9A-Z]+(\.[0-9A-Za-z]+)*(-[a-z])?(\s*e\s*[0-9.A-Za-z-]+)*\s*$)");
    if (std::regex_match(inner, clause)) {
        return true;
    }
    // duas ou mais palavras minusculas de 3+ letras seguidas
    static const std::regex words(R"([A-Za-zÀ-ÿ]{3,}\s+[A-Za-zÀ-ÿ]{3,}|[A-Za-zÀ-ÿ]{5,})");
    return std::regex_search(inner, words);
}

} // namespace

std::string expressionToLatex(const std::string &expression)
{
    ExprParser parser(tokenize(expression));
    return parser.parse();
}

std::string decimalComma(const std::string &text)
{
    std::string out = text;
    for (std::size_t i = 1; i + 1 < out.size(); ++i) {
        if (out[i] == '.' && std::isdigit(static_cast<unsigned char>(out[i - 1])) && std::isdigit(static_cast<unsigned char>(out[i + 1]))) {
            // referencias de item da norma (5.4.2.2) mantem o ponto
            std::size_t start = i - 1;
            while (start > 0 && std::isdigit(static_cast<unsigned char>(out[start - 1]))) {
                --start;
            }
            const bool dotBefore = start > 0 && out[start - 1] == '.';
            std::size_t end = i + 1;
            while (end < out.size() && std::isdigit(static_cast<unsigned char>(out[end]))) {
                ++end;
            }
            const bool dotAfter = end + 1 < out.size() && out[end] == '.' && std::isdigit(static_cast<unsigned char>(out[end + 1]));
            if (!dotBefore && !dotAfter) {
                out[i] = ',';
            }
        }
    }
    return out;
}

std::string restoreAccents(const std::string &text)
{
    static const std::vector<std::pair<std::string, std::string>> table = {
        {"secao", "seção"}, {"Secao", "Seção"}, {"secoes", "seções"}, {"tracao", "tração"}, {"Tracao", "Tração"}, {"compressao", "compressão"},
        {"Compressao", "Compressão"}, {"flexao", "flexão"}, {"Flexao", "Flexão"}, {"Limitacao", "Limitação"}, {"limitacao", "limitação"},
        {"resistencia", "resistência"}, {"Resistencia", "Resistência"}, {"ligacao", "ligação"}, {"Ligacao", "Ligação"}, {"area", "área"},
        {"Area", "Área"}, {"relacao", "relação"}, {"Relacao", "Relação"}, {"nao", "não"}, {"Nao", "Não"}, {"torcao", "torção"}, {"Torcao", "Torção"},
        {"Flambagem", "Flambagem"}, {"macica", "maciça"}, {"Macica", "Maciça"}, {"tubular", "tubular"}, {"cortante", "cortante"},
        {"Forca", "Força"}, {"forca", "força"}, {"interacao", "interação"}, {"Interacao", "Interação"}, {"calculo", "cálculo"}, {"Calculo", "Cálculo"},
        {"analise", "análise"}, {"Analise", "Análise"}, {"estaveis", "estáveis"}, {"minimo", "mínimo"}, {"maximo", "máximo"}, {"Maximo", "Máximo"},
        {"util", "útil"}, {"serie", "série"}, {"proprio", "próprio"}, {"proprios", "próprios"}, {"esforco", "esforço"}, {"esforcos", "esforços"},
        {"indice", "índice"}, {"esbeltez", "esbeltez"}, {"minima", "mínima"}, {"maxima", "máxima"}, {"momento", "momento"},
        {"combinacao", "combinação"}, {"Combinacao", "Combinação"}, {"combinacoes", "combinações"}, {"deslocamento", "deslocamento"},
        {"mesa", "mesa"}, {"inicio", "início"}, {"escoamento", "escoamento"}, {"efetiva", "efetiva"}, {"equacao", "equação"},
        {"contencao", "contenção"}, {"condicoes", "condições"}, {"cantoneira", "cantoneira"}, {"liquida", "líquida"}, {"bruta", "bruta"},
        {"referencia", "referência"}, {"posicao", "posição"}, {"rotacao", "rotação"}, {"verificacao", "verificação"}, {"Verificacao", "Verificação"},
        {"dispensada", "dispensada"}, {"identificado", "identificado"}, {"uniforme", "uniforme"}, {"transversal", "transversal"},
        {"lateral", "lateral"}, {"torcao", "torção"}, {"estavel", "estável"}, {"concentrada", "concentrada"}, {"distribuida", "distribuída"},
        {"hipotese", "hipótese"}, {"apoio", "apoio"}, {"momentos", "momentos"}, {"comprimento", "comprimento"}, {"tensao", "tensão"}, {"tensoes", "tensões"}};
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (std::isalpha(static_cast<unsigned char>(text[i]))) {
            std::size_t j = i;
            while (j < text.size() && std::isalpha(static_cast<unsigned char>(text[j]))) {
                ++j;
            }
            const std::string word = text.substr(i, j - i);
            bool replaced = false;
            for (const auto &kv : table) {
                if (kv.first == word) {
                    out += kv.second;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                out += word;
            }
            i = j;
        } else {
            out.push_back(text[i++]);
        }
    }
    return out;
}

std::vector<FormulaPiece> convertCalculationLine(const std::string &line)
{
    std::vector<FormulaPiece> pieces;
    if (line.find('=') == std::string::npos && line.find("<=") == std::string::npos && line.find(">=") == std::string::npos) {
        pieces.push_back({FormulaPiece::Kind::Text, restoreAccents(decimalComma(line))});
        return pieces;
    }
    for (const std::string &clauseRaw : splitTopLevel(line, "; ")) {
        std::string clause = trim(clauseRaw);
        if (clause.empty()) {
            continue;
        }
        // rotulo antes de ": "
        std::string label;
        const auto eq = clause.find('=');
        const auto colon = clause.find(": ");
        if (colon != std::string::npos && (eq == std::string::npos || colon < eq)) {
            label = trim(clause.substr(0, colon));
            clause = trim(clause.substr(colon + 2));
        }
        if (!label.empty()) {
            pieces.push_back({FormulaPiece::Kind::Text, restoreAccents(decimalComma(label)) + ":"});
        }
        if (clause.find('=') == std::string::npos && clause.find("<=") == std::string::npos && clause.find(">=") == std::string::npos) {
            pieces.push_back({FormulaPiece::Kind::Text, restoreAccents(decimalComma(clause))});
            continue;
        }
        // frase com '=' apenas decorativo ("Todos os elementos com b/t <= ..."): texto corrido
        {
            const std::string lhs = clause.substr(0, clause.find_first_of("=<>"));
            std::string stripped;
            int depth = 0;
            for (const char ch : lhs) {
                if (ch == '(') {
                    ++depth;
                } else if (ch == ')') {
                    --depth;
                } else if (depth == 0) {
                    stripped.push_back(ch);
                }
            }
            static const std::regex prose(R"(\b(os|as|de|da|do|dos|das|com|para|todos|todas|sem|ou|por|nao|que|em|quando|se)\b)");
            if (std::regex_search(stripped, prose)) {
                pieces.push_back({FormulaPiece::Kind::Text, restoreAccents(decimalComma(clause))});
                continue;
            }
        }
        // segmentos separados por ", " cujo todos tem '='
        std::vector<std::string> segments = splitTopLevel(clause, ", ");
        const bool allAssign = std::all_of(segments.begin(), segments.end(), [](const std::string &s) { return s.find('=') != std::string::npos || s.find("<=") != std::string::npos; });
        if (!allAssign) {
            segments = {clause};
        }
        for (std::string segment : segments) {
            segment = trim(segment);
            // nota final entre parenteses
            std::string note;
            if (!segment.empty() && segment.back() == ')') {
                int depth = 0;
                std::size_t open = std::string::npos;
                for (std::size_t i = segment.size(); i-- > 0;) {
                    if (segment[i] == ')') {
                        ++depth;
                    } else if (segment[i] == '(') {
                        if (--depth == 0) {
                            open = i;
                            break;
                        }
                    }
                }
                if (open != std::string::npos && open > 0) {
                    const std::string inner = segment.substr(open + 1, segment.size() - open - 2);
                    if (looksLikeProse(inner) || inner.find('=') != std::string::npos) {
                        note = inner;
                        segment = trim(segment.substr(0, open));
                    }
                }
            }
            if (!segment.empty()) {
                pieces.push_back({FormulaPiece::Kind::Math, expressionToLatex(segment)});
            }
            if (!note.empty()) {
                if (note.find('=') != std::string::npos) {
                    for (const std::string &sub : splitTopLevel(note, ", ")) {
                        if (sub.find('=') != std::string::npos) {
                            pieces.push_back({FormulaPiece::Kind::Math, expressionToLatex(trim(sub))});
                        } else {
                            pieces.push_back({FormulaPiece::Kind::Text, "(" + restoreAccents(decimalComma(trim(sub))) + ")"});
                        }
                    }
                } else {
                    pieces.push_back({FormulaPiece::Kind::Text, "(" + restoreAccents(decimalComma(note)) + ")"});
                }
            }
        }
    }
    return pieces;
}

} // namespace Structura::Report
