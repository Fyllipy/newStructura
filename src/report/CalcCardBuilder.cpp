#include "report/CalcCardBuilder.h"

#include "report/FormulaText.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <regex>
#include <set>

namespace Structura::Report {

namespace {

struct Entry {
    const char *key;
    const char *meaning;
};

// Dicionario de simbolos das linhas de calculo (chaves normalizadas: sem "_", "gamma" -> "gama").
// Simbolos ambiguos (D, a, c, l, m, r, ...) ficam de fora: a ficha nunca atribui um significado que possa estar errado.
const Entry kGlossary[] = {
    // aco
    {"fy", "Resistência ao escoamento do aço"},
    {"fu", "Resistência à ruptura do aço"},
    {"E", "Módulo de elasticidade do aço"},
    {"G", "Módulo de elasticidade transversal do aço"},
    {"gama_a1", "Coeficiente de ponderação das resistências para escoamento e instabilidade"},
    {"gama_a2", "Coeficiente de ponderação das resistências para ruptura"},
    {"gama", "Coeficiente de ponderação da resistência"},
    {"ga1", "Coeficiente de ponderação das resistências para escoamento e instabilidade"},
    {"Yt", "Razão entre a resistência ao escoamento e a ruptura (fy/fu)"},
    // geometria
    {"A", "Área bruta da seção transversal"},
    {"Ag", "Área bruta da seção transversal"},
    {"An", "Área líquida da seção transversal"},
    {"An0", "Área líquida da seção na região da ligação"},
    {"Ae", "Área líquida efetiva da seção transversal"},
    {"Aef", "Área efetiva da seção transversal (descontadas as perdas por flambagem local)"},
    {"Afg", "Área bruta da mesa tracionada"},
    {"Afn", "Área líquida da mesa tracionada"},
    {"Aw", "Área efetiva de cisalhamento (alma)"},
    {"Ix", "Momento de inércia da seção em relação ao eixo X"},
    {"Iy", "Momento de inércia da seção em relação ao eixo Y"},
    {"Ix1", "Momento de inércia de um perfil componente em relação ao eixo X"},
    {"J", "Constante de torção da seção"},
    {"Cw", "Constante de empenamento da seção"},
    {"W", "Módulo de resistência elástico da seção"},
    {"Wx", "Módulo de resistência elástico em relação ao eixo X"},
    {"Wy", "Módulo de resistência elástico em relação ao eixo Y"},
    {"Wxt", "Módulo de resistência elástico da fibra tracionada"},
    {"Wt", "Módulo de resistência elástico da mesa tracionada"},
    {"WT", "Módulo de resistência à torção"},
    {"Wef", "Módulo de resistência elástico efetivo"},
    {"Wc,ef", "Módulo de resistência elástico efetivo da fibra comprimida"},
    {"rx", "Raio de giração em relação ao eixo X"},
    {"ry", "Raio de giração em relação ao eixo Y"},
    {"r", "Raio de giração da seção"},
    {"r0", "Raio de giração polar da seção em relação ao centro de cisalhamento"},
    {"x0", "Distância do centro de cisalhamento ao centroide, direção X"},
    {"y0", "Distância do centro de cisalhamento ao centroide, direção Y"},
    {"d", "Altura total da seção"},
    {"bf", "Largura da mesa"},
    {"bw", "Altura da alma"},
    {"tw", "Espessura da alma"},
    {"tf", "Espessura da mesa"},
    {"t", "Espessura do elemento"},
    {"b", "Largura do elemento plano"},
    {"h", "Altura da parte reta da alma"},
    {"ho", "Distância entre os centroides das mesas"},
    {"bef", "Largura efetiva do elemento comprimido"},
    {"ds", "Largura efetiva do enrijecedor de borda"},
    {"Is", "Momento de inércia do enrijecedor de borda"},
    {"Ia", "Momento de inércia de referência do enrijecedor"},
    {"Q", "Momento estático da meia seção em relação ao eixo neutro"},
    {"Mpl", "Momento de plastificação da seção"},
    {"Zx", "Módulo de resistência plástico em relação ao eixo X"},
    {"Zy", "Módulo de resistência plástico em relação ao eixo Y"},
    // comprimentos e esbeltez
    {"L", "Comprimento da barra"},
    {"Lx", "Comprimento de flambagem por flexão em relação ao eixo X"},
    {"Ly", "Comprimento de flambagem por flexão em relação ao eixo Y"},
    {"Lz", "Comprimento de flambagem por torção"},
    {"Lb", "Comprimento destravado (distância entre contenções laterais da mesa comprimida)"},
    {"Lv", "Distância entre as seções de cortante máximo e nulo"},
    {"KL", "Comprimento de flambagem"},
    {"lambda", "Índice de esbeltez"},
    {"lambda_p", "Parâmetro de esbeltez limite para seção compacta (plastificação)"},
    {"lambda_r", "Parâmetro de esbeltez limite para seção semicompacta (início de escoamento)"},
    {"lambda_y", "Parâmetro de esbeltez limite para o início de escoamento"},
    {"lambda_p0", "Índice de esbeltez reduzido do elemento"},
    {"lambda0", "Índice de esbeltez reduzido da barra comprimida"},
    {"lambda_dist", "Índice de esbeltez reduzido para flambagem distorcional"},
    {"chi", "Fator de redução associado à resistência à compressão"},
    {"chi_dist", "Fator de redução associado à flambagem distorcional"},
    {"chi_FLT", "Fator de redução associado à flambagem lateral com torção"},
    // forcas e momentos
    {"Ne", "Força axial de flambagem elástica"},
    {"Nex", "Força axial de flambagem elástica por flexão em relação ao eixo X"},
    {"Ney", "Força axial de flambagem elástica por flexão em relação ao eixo Y"},
    {"Nez", "Força axial de flambagem elástica por torção"},
    {"Ne,0", "Força axial de flambagem elástica da barra composta como unidade"},
    {"Ne,m", "Força axial de flambagem elástica reduzida pelo cisalhamento das ligações (barra composta)"},
    {"Ndist", "Força axial de flambagem elástica distorcional"},
    {"Nc,Rd", "Força axial de compressão resistente de cálculo"},
    {"Nt,Rd", "Força axial de tração resistente de cálculo"},
    {"Nc,Sd", "Força axial de compressão solicitante de cálculo"},
    {"Nt,Sd", "Força axial de tração solicitante de cálculo"},
    {"NRd", "Força axial resistente de cálculo"},
    {"NSd", "Força axial solicitante de cálculo"},
    {"Mx,Rd", "Momento fletor resistente de cálculo em relação ao eixo X"},
    {"My,Rd", "Momento fletor resistente de cálculo em relação ao eixo Y"},
    {"Mx,Sd", "Momento fletor solicitante de cálculo em relação ao eixo X"},
    {"My,Sd", "Momento fletor solicitante de cálculo em relação ao eixo Y"},
    {"MRd", "Momento fletor resistente de cálculo"},
    {"MSd", "Momento fletor solicitante de cálculo"},
    {"Mcr", "Momento fletor de flambagem elástica"},
    {"Me", "Momento fletor crítico elástico"},
    {"Mdist", "Momento fletor de flambagem elástica distorcional"},
    {"Mnt", "Momento fletor da estrutura com nós impedidos de se deslocar (nt)"},
    {"Mlt", "Momento fletor devido aos deslocamentos laterais (lt)"},
    {"Nnt", "Força axial da estrutura com nós impedidos de se deslocar (nt)"},
    {"Nlt", "Força axial devido aos deslocamentos laterais (lt)"},
    {"VRd", "Força cortante resistente de cálculo"},
    {"VSd", "Força cortante solicitante de cálculo"},
    {"Vy,Sd", "Força cortante solicitante de cálculo na direção da altura (eixo Y)"},
    {"Vz,Sd", "Força cortante solicitante de cálculo na direção da largura (eixo Z)"},
    {"Vpl", "Força cortante correspondente à plastificação da alma"},
    {"Vy", "Força cortante na direção da altura"},
    {"Vz", "Força cortante na direção da largura"},
    {"TRd", "Momento de torção resistente de cálculo"},
    {"TSd", "Momento de torção solicitante de cálculo"},
    {"Tsv", "Parcela do momento de torção resistida por torção de Saint-Venant"},
    {"Tw", "Parcela do momento de torção resistida por empenamento"},
    {"FRd", "Força resistente de cálculo"},
    {"Fbr", "Força de contenção lateral"},
    {"Sbr", "Rigidez mínima da contenção lateral"},
    // fatores
    {"Cb", "Fator de modificação para diagrama de momento fletor não uniforme"},
    {"Cm", "Coeficiente de equivalência de momento"},
    {"Cd", "Coeficiente de curvatura do diagrama de momento na contenção"},
    {"Cf", "Constante da categoria de detalhe (fadiga)"},
    {"Ct", "Coeficiente de redução da área líquida"},
    {"B1", "Fator de amplificação dos momentos (efeito P-δ)"},
    {"B2", "Fator de amplificação dos momentos (efeito P-Δ)"},
    {"B1x", "Fator de amplificação dos momentos em relação ao eixo X (efeito P-δ)"},
    {"Ki", "Fator de interação das ligações da barra composta"},
    {"kv", "Coeficiente de flambagem da alma por cisalhamento"},
    {"sigma", "Tensão normal"},
    {"sigma_el", "Tensão crítica elástica de flambagem local"},
    {"sigma_w", "Tensão normal de empenamento (B ω / Cw)"},
    {"sigma_TH", "Limite de variação de tensão (fadiga)"},
    {"sigma_SR", "Faixa de variação das tensões normais"},
    {"tau", "Tensão de cisalhamento"},
    {"tau_cr", "Tensão crítica de cisalhamento da alma"},
    {"tau_sv", "Tensão de cisalhamento de Saint-Venant"},
    {"tau_w", "Tensão de cisalhamento de empenamento"},
    {"tau_V", "Tensão de cisalhamento devida à força cortante"},
    {"tau_max", "Tensão de cisalhamento máxima"},
    {"omega_max", "Ordenada setorial normalizada máxima"},
};

} // namespace

std::string normalizeSymbol(const std::string &symbol)
{
    std::string out;
    for (const char c : symbol) {
        if (c != '_') {
            out.push_back(c);
        }
    }
    // gamma_a1 / gama_a1
    const auto g = out.find("gamma");
    if (g == 0) {
        out.replace(0, 5, "gama");
    }
    // "gama" + a1 / a2 (sem sublinhado depois de normalizado) mantem a forma usada no dicionario
    if (out == "gamaa1") {
        return "gama_a1";
    }
    if (out == "gamaa2") {
        return "gama_a2";
    }
    // chaves do dicionario escritas com sublinhado interno (lambda_p...) casam por forma sem sublinhado
    return out;
}

namespace {

const std::map<std::string, std::string> &glossary()
{
    static const std::map<std::string, std::string> g = []() {
        std::map<std::string, std::string> m;
        for (const Entry &e : kGlossary) {
            if (e.meaning[0] != '\0') {
                m[normalizeSymbol(e.key)] = e.meaning;
            }
        }
        return m;
    }();
    return g;
}

std::string prettyUnit(const std::string &u)
{
    static const std::map<std::string, std::string> map = {{"mm2", "mm²"}, {"mm3", "mm³"}, {"mm4", "mm⁴"}, {"mm6", "mm⁶"}, {"cm2", "cm²"}, {"cm3", "cm³"}, {"cm4", "cm⁴"},
                                                           {"m2", "m²"}, {"m3", "m³"}, {"m4", "m⁴"}, {"kN*m", "kN·m"}, {"kN*m2", "kN·m²"}, {"N*mm", "N·mm"}, {"deg", "°"}};
    const auto it = map.find(u);
    return it == map.end() ? u : it->second;
}

bool isUnit(const std::string &u)
{
    static const std::set<std::string> units = {"kN", "N", "MPa", "GPa", "mm", "mm2", "mm3", "mm4", "mm6", "m", "cm", "cm2", "cm3", "cm4", "kN*m", "kN*m2", "N*mm", "deg", "m2", "m3", "m4", "kg"};
    return units.count(u) > 0;
}

/// Valor final de uma atribuicao ("... = 665.45 kN"): numero e unidade opcional.
bool parseResult(const std::string &text, std::string &value, std::string &unit)
{
    static const std::regex pattern(R"(^\s*([+-]?[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)\s*([A-Za-z][A-Za-z0-9*]*)?\s*$)");
    std::smatch match;
    if (!std::regex_match(text, match, pattern)) {
        return false;
    }
    unit.clear();
    if (match[2].matched) {
        if (!isUnit(match[2].str())) {
            return false;
        }
        unit = match[2].str();
    }
    value = decimalComma(match[1].str());
    return true;
}

bool isPlainSymbol(const std::string &s)
{
    if (s.empty() || !std::isalpha(static_cast<unsigned char>(s[0]))) {
        return false;
    }
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ','; });
}

/// Clausulas ("rotulo: a = b, c = d (nota)") de uma linha, com o rotulo e as atribuicoes simples (simbolo, valor, unidade).
struct ParsedClause {
    std::string label;
    std::string body;
};

std::vector<ParsedClause> clausesOf(const std::string &line)
{
    std::vector<ParsedClause> out;
    for (const std::string &raw : splitTopLevelText(line, "; ")) {
        std::string clause = trimText(raw);
        if (clause.empty()) {
            continue;
        }
        ParsedClause c;
        const auto eq = clause.find('=');
        const auto colon = clause.find(": ");
        if (colon != std::string::npos && (eq == std::string::npos || colon < eq)) {
            c.label = trimText(clause.substr(0, colon));
            clause = trimText(clause.substr(colon + 2));
        }
        c.body = clause;
        out.push_back(c);
    }
    return out;
}

std::string stripTrailingNote(const std::string &segment)
{
    std::string s = trimText(segment);
    if (s.empty() || s.back() != ')') {
        return s;
    }
    int depth = 0;
    for (std::size_t i = s.size(); i-- > 0;) {
        if (s[i] == ')') {
            ++depth;
        } else if (s[i] == '(') {
            if (--depth == 0) {
                return i > 0 && s.find('=') != std::string::npos && s.find('=') < i ? trimText(s.substr(0, i)) : s;
            }
        }
    }
    return s;
}

/// Registra no ambiente os valores atribuidos na linha ("Nex = ... = 11141.30 kN").
void harvest(const std::string &line, CalcEnv &env)
{
    for (const ParsedClause &clause : clausesOf(line)) {
        std::vector<std::string> segments;
        for (const std::string &arrowPart : splitTopLevelText(clause.body, " -> ")) {
            for (const std::string &piece : splitTopLevelText(arrowPart, ", ")) {
                segments.push_back(piece);
            }
        }
        const bool allAssign = std::all_of(segments.begin(), segments.end(), [](const std::string &s) { return s.find('=') != std::string::npos; });
        if (!allAssign) {
            segments = {clause.body};
        }
        for (const std::string &segmentRaw : segments) {
            const std::string segment = stripTrailingNote(segmentRaw);
            if (segment.find("<=") != std::string::npos || segment.find(">=") != std::string::npos || segment.find("->") != std::string::npos) {
                continue;
            }
            const auto parts = splitTopLevelText(segment, "=");
            if (parts.size() < 2) {
                continue;
            }
            const std::string lhs = trimText(parts.front());
            if (!isPlainSymbol(lhs)) {
                continue;
            }
            std::string value, unit;
            if (parseResult(parts.back(), value, unit)) {
                env[normalizeSymbol(lhs)] = {value, unit};
            }
        }
    }
}

} // namespace

std::string symbolMeaning(const std::string &symbol)
{
    const auto it = glossary().find(normalizeSymbol(symbol));
    return it == glossary().end() ? std::string() : it->second;
}

std::string formatNumber(double value, int decimals)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return decimalComma(buffer);
}

CalcCard buildCalcCard(const CalcCardInput &in)
{
    CalcCard card;
    card.title = restoreAccents(in.title);
    card.clause = in.clause;
    card.description = in.description;
    card.status = in.ratio > 1.0 ? CellStatus::Fail : (in.ratio > 0.9 ? CellStatus::Warn : CellStatus::Ok);
    card.ok = in.ratio <= 1.0;

    CalcEnv env = in.env;
    std::set<std::string> listed;
    static const std::regex whereRegex(R"(\s*\(combinacao ([^,()]+), x = ([0-9.]+) mm\)\s*$)");
    for (std::string line : in.lines) {
        std::smatch where;
        if (std::regex_search(line, where, whereRegex)) {
            if (card.location.empty()) {
                card.location = "Combinação " + where[1].str() + ", seção x = " + formatNumber(std::stod(where[2].str()) / 1000.0, 3) + " m";
            }
            line = line.substr(0, static_cast<std::size_t>(where.position(0)));
        }
        harvest(line, env);
        const auto pieces = convertCalculationLine(line);
        CalcStep step;
        std::string pending;   // texto antes da primeira equacao
        bool seenMath = false;
        for (const FormulaPiece &piece : pieces) {
            if (piece.kind == FormulaPiece::Kind::Math) {
                seenMath = true;
                step.equations.push_back(piece.text);
            } else if (!piece.text.empty() && piece.text.back() == ':' && !seenMath) {
                step.label += (step.label.empty() ? "" : " ") + piece.text.substr(0, piece.text.size() - 1);
            } else if (!seenMath && step.label.empty() && step.equations.empty() && pieces.size() == 1) {
                step.notes.push_back(piece.text);   // linha so de texto
            } else {
                step.notes.push_back(piece.text);
            }
        }
        // simbolos conhecidos da linha, sem repetir os ja listados na ficha
        std::string source;
        for (const ParsedClause &clause : clausesOf(line)) {
            source += " " + clause.body;
        }
        if (!step.equations.empty()) {
            for (const std::string &id : identifiersIn(source)) {
                const std::string key = normalizeSymbol(id);
                if (listed.count(key)) {
                    continue;
                }
                const auto meaning = glossary().find(key);
                if (meaning == glossary().end()) {
                    continue;
                }
                const auto known = env.find(key);
                if (known == env.end()) {
                    continue;   // sem valor conhecido: nao lista (evita campos vazios)
                }
                listed.insert(key);
                CalcVariable v;
                v.latex = expressionToLatex(id);
                v.meaning = meaning->second;
                const auto value = env.find(key);
                if (value != env.end()) {
                    v.value = value->second.value;
                    v.unit = prettyUnit(value->second.unit);
                }
                step.variables.push_back(std::move(v));
            }
        }
        // junta equacoes curtas na mesma linha
        {
            std::vector<std::string> merged;
            std::string current;
            for (const std::string &eq : step.equations) {
                if (!current.empty() && current.size() + eq.size() > 110) {
                    merged.push_back(current);
                    current.clear();
                }
                current += (current.empty() ? "" : "\\qquad ") + eq;
            }
            if (!current.empty()) {
                merged.push_back(current);
            }
            step.equations = merged;
        }
        if (!step.equations.empty() || !step.notes.empty() || !step.label.empty()) {
            card.steps.push_back(std::move(step));
        }
    }

    card.inequalityLatex = in.inequalityLatex.empty() ? "S_{d} \\le R_{d}" : in.inequalityLatex;
    card.resultLabel = in.resultLabel.empty() ? "\\eta" : in.resultLabel;
    card.resultValue = formatNumber(in.ratio, 3);
    if (in.resistance != 1.0 || !in.unit.empty()) {
        const std::string unit = in.unit.empty() ? std::string() : " " + prettyUnit(in.unit);
        card.resultDetail = formatNumber(in.demand, 2) + unit + "  /  " + formatNumber(in.resistance, 2) + unit;
    }
    return card;
}

} // namespace Structura::Report
