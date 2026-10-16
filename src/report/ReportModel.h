#pragma once

#include <string>
#include <variant>
#include <vector>

namespace Structura::Report {

/// Trecho de texto de um paragrafo: texto simples ou formula em linha (`latex` nao vazio).
struct Run {
    std::string text;
    std::string latex;
    bool bold {false};
    bool italic {false};
    std::string color;   ///< "RRGGBB" ou vazio
};

enum class ParagraphStyle { Normal, Small, Caption, Note, Ok, Warn, Fail, Label };

struct Paragraph {
    std::vector<Run> runs;
    ParagraphStyle style {ParagraphStyle::Normal};
};

struct Heading {
    int level {1};
    std::string text;
};

/// Equacao em destaque (centralizada).
struct Equation {
    std::string latex;
};

/// Status usado para colorir celulas de tabela.
enum class CellStatus { None = 0, Ok, Warn, Fail };

struct Table {
    std::string caption;
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
    std::vector<double> widths;                 ///< larguras relativas (opcional)
    std::vector<char> align;                    ///< 'l', 'c' ou 'r' por coluna (opcional)
    std::vector<std::vector<CellStatus>> status;   ///< mesma forma de `rows` (opcional)
    bool firstColumnBold {false};
};

struct Image {
    std::vector<unsigned char> png;
    double widthCm {8.0};
    std::string caption;
};

struct PageBreak {};

using Block = std::variant<Heading, Paragraph, Equation, Table, Image, PageBreak>;

struct ReportDocument {
    std::string title;
    std::string subtitle;
    std::string project;
    std::string author;
    std::string date;
    std::string headerText;
    bool cover {true};
    bool tableOfContents {true};
    std::vector<Block> blocks;

    // Construcao
    void heading(int level, const std::string &text) { blocks.push_back(Heading {level, text}); }
    void paragraph(const std::string &text, ParagraphStyle style = ParagraphStyle::Normal)
    {
        Paragraph p;
        p.style = style;
        p.runs.push_back({text, {}, false, false, {}});
        blocks.push_back(std::move(p));
    }
    void equation(const std::string &latex) { blocks.push_back(Equation {latex}); }
    void table(Table t) { blocks.push_back(std::move(t)); }
    void pageBreak() { blocks.push_back(PageBreak {}); }
};

} // namespace Structura::Report
