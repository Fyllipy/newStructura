#include "report/DocxWriter.h"

#include "report/MathParser.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>

namespace Structura::Report {

namespace {

// ---------------------------------------------------------------------------------------------------------------------
// XML
// ---------------------------------------------------------------------------------------------------------------------

std::string esc(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out.push_back(c);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// OMML
// ---------------------------------------------------------------------------------------------------------------------

std::string ommlRun(const std::string &text, bool upright)
{
    if (text.empty()) {
        return {};
    }
    std::string x = "<m:r>";
    x += "<m:rPr>";
    if (upright) {
        x += "<m:sty m:val=\"p\"/>";
    }
    x += "</m:rPr><w:rPr><w:rFonts w:ascii=\"Cambria Math\" w:hAnsi=\"Cambria Math\" w:cs=\"Cambria Math\"/></w:rPr>";
    x += "<m:t xml:space=\"preserve\">" + esc(text) + "</m:t></m:r>";
    return x;
}

std::string emitNode(const MathPtr &node);

std::string emitChildren(const std::vector<MathPtr> &children)
{
    std::string out;
    std::size_t i = 0;
    while (i < children.size()) {
        const auto &c = children[i];
        // delimitadores que acompanham o conteudo: ( ... )
        if (c && c->kind == MathNode::Kind::Run && (c->text == "(" || c->text == "[")) {
            const std::string closing = c->text == "(" ? ")" : "]";
            int depth = 0;
            std::size_t j = i;
            bool found = false;
            for (; j < children.size(); ++j) {
                const auto &d = children[j];
                if (d && d->kind == MathNode::Kind::Run) {
                    if (d->text == c->text) {
                        ++depth;
                    } else if (d->text == closing && --depth == 0) {
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                std::vector<MathPtr> inner(children.begin() + static_cast<std::ptrdiff_t>(i) + 1, children.begin() + static_cast<std::ptrdiff_t>(j));
                out += "<m:d><m:dPr><m:begChr m:val=\"" + c->text + "\"/><m:endChr m:val=\"" + closing + "\"/></m:dPr><m:e>" + emitChildren(inner) + "</m:e></m:d>";
                i = j + 1;
                continue;
            }
        }
        out += emitNode(c);
        ++i;
    }
    return out;
}

std::string emitNode(const MathPtr &node)
{
    if (!node) {
        return {};
    }
    switch (node->kind) {
    case MathNode::Kind::Run: return ommlRun(node->text, node->upright);
    case MathNode::Kind::Space: {
        const std::string space = node->width >= 0.9 ? "\xE2\x80\x83" : (node->width >= 0.15 ? "\xE2\x80\x89" : "");
        return ommlRun(space, true);
    }
    case MathNode::Kind::Row: return emitChildren(node->children);
    case MathNode::Kind::Frac:
        return "<m:f><m:num>" + emitNode(node->children[0]) + "</m:num><m:den>" + emitNode(node->children[1]) + "</m:den></m:f>";
    case MathNode::Kind::Script: {
        const std::string base = emitNode(node->children[0]);
        const bool hasSub = node->children[1] != nullptr;
        const bool hasSup = node->children[2] != nullptr;
        if (hasSub && hasSup) {
            return "<m:sSubSup><m:e>" + base + "</m:e><m:sub>" + emitNode(node->children[1]) + "</m:sub><m:sup>" + emitNode(node->children[2]) + "</m:sup></m:sSubSup>";
        }
        if (hasSub) {
            return "<m:sSub><m:e>" + base + "</m:e><m:sub>" + emitNode(node->children[1]) + "</m:sub></m:sSub>";
        }
        if (hasSup) {
            return "<m:sSup><m:e>" + base + "</m:e><m:sup>" + emitNode(node->children[2]) + "</m:sup></m:sSup>";
        }
        return base;
    }
    case MathNode::Kind::Sqrt: {
        const bool hasIndex = node->children.size() > 1 && node->children[1] != nullptr;
        std::string x = "<m:rad><m:radPr>";
        if (!hasIndex) {
            x += "<m:degHide m:val=\"1\"/>";
        }
        x += "</m:radPr><m:deg>" + (hasIndex ? emitNode(node->children[1]) : std::string()) + "</m:deg><m:e>" + emitNode(node->children[0]) + "</m:e></m:rad>";
        return x;
    }
    }
    return {};
}

// ---------------------------------------------------------------------------------------------------------------------
// ZIP (sem compressao)
// ---------------------------------------------------------------------------------------------------------------------

std::uint32_t crc32(const unsigned char *data, std::size_t size)
{
    static std::uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            table[i] = c;
        }
        init = true;
    }
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

class Zip
{
public:
    void add(const std::string &name, const std::string &content)
    {
        add(name, reinterpret_cast<const unsigned char *>(content.data()), content.size());
    }
    void add(const std::string &name, const std::vector<unsigned char> &content)
    {
        add(name, content.data(), content.size());
    }
    std::vector<unsigned char> finish()
    {
        const std::uint32_t centralStart = static_cast<std::uint32_t>(m_out.size());
        for (const Entry &e : m_entries) {
            u32(0x02014b50);
            u16(20);
            u16(20);
            u16(0x0800);   // nomes em UTF-8
            u16(0);
            u16(0);
            u16(0x21);
            u32(e.crc);
            u32(e.size);
            u32(e.size);
            u16(static_cast<std::uint16_t>(e.name.size()));
            u16(0);
            u16(0);
            u16(0);
            u16(0);
            u32(0);
            u32(e.offset);
            bytes(e.name.data(), e.name.size());
        }
        const std::uint32_t centralSize = static_cast<std::uint32_t>(m_out.size()) - centralStart;
        u32(0x06054b50);
        u16(0);
        u16(0);
        u16(static_cast<std::uint16_t>(m_entries.size()));
        u16(static_cast<std::uint16_t>(m_entries.size()));
        u32(centralSize);
        u32(centralStart);
        u16(0);
        return std::move(m_out);
    }

private:
    struct Entry {
        std::string name;
        std::uint32_t crc;
        std::uint32_t size;
        std::uint32_t offset;
    };
    std::vector<unsigned char> m_out;
    std::vector<Entry> m_entries;

    void u16(std::uint16_t v)
    {
        m_out.push_back(static_cast<unsigned char>(v & 0xFF));
        m_out.push_back(static_cast<unsigned char>(v >> 8));
    }
    void u32(std::uint32_t v)
    {
        u16(static_cast<std::uint16_t>(v & 0xFFFF));
        u16(static_cast<std::uint16_t>(v >> 16));
    }
    void bytes(const void *p, std::size_t n)
    {
        const auto *b = static_cast<const unsigned char *>(p);
        m_out.insert(m_out.end(), b, b + n);
    }
    void add(const std::string &name, const unsigned char *data, std::size_t size)
    {
        Entry e {name, crc32(data, size), static_cast<std::uint32_t>(size), static_cast<std::uint32_t>(m_out.size())};
        u32(0x04034b50);
        u16(20);
        u16(0x0800);
        u16(0);
        u16(0);
        u16(0x21);
        u32(e.crc);
        u32(e.size);
        u32(e.size);
        u16(static_cast<std::uint16_t>(name.size()));
        u16(0);
        bytes(name.data(), name.size());
        bytes(data, size);
        m_entries.push_back(e);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Documento
// ---------------------------------------------------------------------------------------------------------------------

constexpr int kPageWidthDxa = 11906 - 2 * 1134;

struct Palette {
    static constexpr const char *kPrimary = "1F4E79";
    static constexpr const char *kAccent = "2E75B6";
    static constexpr const char *kHeaderFill = "1F4E79";
    static constexpr const char *kZebra = "F2F6FA";
    static constexpr const char *kBorder = "C9D3E0";
};

std::string runXml(const Run &r, const std::string &defaultColor = {}, bool forceBold = false, int sizeHalfPoints = 0)
{
    if (!r.latex.empty()) {
        return "<m:oMath>" + emitNode(parseLatex(r.latex)) + "</m:oMath>";
    }
    std::string x = "<w:r><w:rPr>";
    if (r.bold || forceBold) {
        x += "<w:b/>";
    }
    if (r.italic) {
        x += "<w:i/>";
    }
    const std::string color = r.color.empty() ? defaultColor : r.color;
    if (!color.empty()) {
        x += "<w:color w:val=\"" + color + "\"/>";
    }
    if (sizeHalfPoints > 0) {
        x += "<w:sz w:val=\"" + std::to_string(sizeHalfPoints) + "\"/>";
    }
    x += "</w:rPr><w:t xml:space=\"preserve\">" + esc(r.text) + "</w:t></w:r>";
    return x;
}

std::string paragraphStyleId(ParagraphStyle s)
{
    switch (s) {
    case ParagraphStyle::Normal: return "";
    case ParagraphStyle::Small: return "Pequeno";
    case ParagraphStyle::Caption: return "Legenda";
    case ParagraphStyle::Note: return "Nota";
    case ParagraphStyle::Ok: return "Aprovado";
    case ParagraphStyle::Warn: return "Atencao";
    case ParagraphStyle::Fail: return "Reprovado";
    case ParagraphStyle::Label: return "Rotulo";
    }
    return "";
}

class DocBuilder
{
public:
    explicit DocBuilder(const ReportDocument &doc) : m_doc(doc) {}

    std::string body()
    {
        std::string x;
        if (m_doc.cover) {
            x += cover();
        }
        if (m_doc.tableOfContents) {
            x += "<w:p><w:pPr><w:pStyle w:val=\"TituloSumario\"/></w:pPr><w:r><w:t>Sumário</w:t></w:r></w:p>";
            x += "<w:p><w:r><w:fldChar w:fldCharType=\"begin\" w:dirty=\"true\"/></w:r><w:r><w:instrText xml:space=\"preserve\"> TOC \\o \"1-2\" \\h \\z \\u </w:instrText></w:r>"
                 "<w:r><w:fldChar w:fldCharType=\"separate\"/></w:r><w:r><w:t>Atualize o campo (F9) para gerar o sumário.</w:t></w:r><w:r><w:fldChar w:fldCharType=\"end\"/></w:r></w:p>";
            x += pageBreak();
        }
        for (const Block &b : m_doc.blocks) {
            x += std::visit([this](const auto &blk) { return emit(blk); }, b);
        }
        return x;
    }

    const std::vector<std::pair<std::string, const std::vector<unsigned char> *>> &images() const { return m_images; }

private:
    const ReportDocument &m_doc;
    std::vector<std::pair<std::string, const std::vector<unsigned char> *>> m_images;
    int m_imageCounter {0};

    static std::string pageBreak() { return "<w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>"; }

    std::string cover() const
    {
        std::string x;
        x += "<w:p><w:pPr><w:spacing w:before=\"2400\"/></w:pPr></w:p>";
        x += "<w:p><w:pPr><w:pStyle w:val=\"Titulo\"/></w:pPr><w:r><w:t xml:space=\"preserve\">" + esc(m_doc.title) + "</w:t></w:r></w:p>";
        if (!m_doc.subtitle.empty()) {
            x += "<w:p><w:pPr><w:pStyle w:val=\"Subtitulo\"/></w:pPr><w:r><w:t xml:space=\"preserve\">" + esc(m_doc.subtitle) + "</w:t></w:r></w:p>";
        }
        x += "<w:p><w:pPr><w:pBdr><w:top w:val=\"single\" w:sz=\"18\" w:space=\"8\" w:color=\"2E75B6\"/></w:pBdr><w:spacing w:before=\"480\"/></w:pPr></w:p>";
        auto line = [&](const std::string &label, const std::string &value) {
            if (value.empty()) {
                return std::string();
            }
            return "<w:p><w:pPr><w:pStyle w:val=\"DadosCapa\"/></w:pPr><w:r><w:rPr><w:b/><w:color w:val=\"1F4E79\"/></w:rPr><w:t xml:space=\"preserve\">" + esc(label) +
                   " </w:t></w:r><w:r><w:t xml:space=\"preserve\">" + esc(value) + "</w:t></w:r></w:p>";
        };
        x += line("Projeto:", m_doc.project);
        x += line("Responsável:", m_doc.author);
        x += line("Data:", m_doc.date);
        x += pageBreak();
        return x;
    }

    std::string emit(const Heading &h) const
    {
        const int level = std::clamp(h.level, 1, 3);
        return "<w:p><w:pPr><w:pStyle w:val=\"Ttulo" + std::to_string(level) + "\"/></w:pPr><w:r><w:t xml:space=\"preserve\">" + esc(h.text) + "</w:t></w:r></w:p>";
    }

    std::string emit(const Paragraph &p) const
    {
        std::string x = "<w:p>";
        const std::string style = paragraphStyleId(p.style);
        if (!style.empty()) {
            x += "<w:pPr><w:pStyle w:val=\"" + style + "\"/></w:pPr>";
        }
        for (const Run &r : p.runs) {
            x += runXml(r);
        }
        x += "</w:p>";
        return x;
    }

    std::string emit(const Equation &e) const
    {
        return "<w:p><w:pPr><w:pStyle w:val=\"Equacao\"/></w:pPr><m:oMathPara><m:oMathParaPr><m:jc m:val=\"center\"/></m:oMathParaPr><m:oMath>" + emitNode(parseLatex(e.latex)) +
               "</m:oMath></m:oMathPara></w:p>";
    }

    std::string emit(const PageBreak &) const { return pageBreak(); }

    std::string emit(const Image &img)
    {
        if (img.png.empty()) {
            return {};
        }
        const int id = ++m_imageCounter;
        const std::string name = "image" + std::to_string(id) + ".png";
        m_images.emplace_back(name, &img.png);
        // proporcao do PNG
        std::uint32_t w = 1;
        std::uint32_t h = 1;
        if (img.png.size() > 24) {
            w = (img.png[16] << 24) | (img.png[17] << 16) | (img.png[18] << 8) | img.png[19];
            h = (img.png[20] << 24) | (img.png[21] << 16) | (img.png[22] << 8) | img.png[23];
        }
        const long long cx = static_cast<long long>(img.widthCm * 360000.0);
        const long long cy = static_cast<long long>(cx * static_cast<double>(h) / std::max<std::uint32_t>(w, 1));
        std::string x = "<w:p><w:pPr><w:keepNext/><w:jc w:val=\"center\"/></w:pPr><w:r><w:drawing><wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\"><wp:extent cx=\"" +
                        std::to_string(cx) + "\" cy=\"" + std::to_string(cy) + "\"/><wp:docPr id=\"" + std::to_string(id) + "\" name=\"Figura " + std::to_string(id) +
                        "\"/><a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\"><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
                        "<pic:pic xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\"><pic:nvPicPr><pic:cNvPr id=\"" +
                        std::to_string(id) + "\" name=\"" + name + "\"/><pic:cNvPicPr/></pic:nvPicPr><pic:blipFill><a:blip r:embed=\"rIdImg" + std::to_string(id) +
                        "\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill><pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"" + std::to_string(cx) + "\" cy=\"" +
                        std::to_string(cy) + "\"/></a:xfrm><a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr></pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r></w:p>";
        if (!img.caption.empty()) {
            x += "<w:p><w:pPr><w:pStyle w:val=\"Legenda\"/><w:jc w:val=\"center\"/></w:pPr><w:r><w:t xml:space=\"preserve\">" + esc(img.caption) + "</w:t></w:r></w:p>";
        }
        return x;
    }

    std::string emit(const Table &t) const
    {
        const std::size_t cols = std::max<std::size_t>({t.header.size(), t.rows.empty() ? 0 : t.rows.front().size(), 1});
        std::vector<double> rel = t.widths;
        rel.resize(cols, 1.0);
        double sum = 0.0;
        for (double w : rel) {
            sum += w;
        }
        std::vector<int> dxa;
        for (double w : rel) {
            dxa.push_back(static_cast<int>(kPageWidthDxa * w / sum));
        }
        std::string x;
        if (!t.caption.empty()) {
            x += "<w:p><w:pPr><w:pStyle w:val=\"Legenda\"/><w:keepNext/></w:pPr><w:r><w:t xml:space=\"preserve\">" + esc(t.caption) + "</w:t></w:r></w:p>";
        }
        x += "<w:tbl><w:tblPr><w:tblW w:w=\"" + std::to_string(kPageWidthDxa) + "\" w:type=\"dxa\"/><w:tblLayout w:type=\"fixed\"/><w:tblBorders>";
        for (const char *side : {"top", "left", "bottom", "right", "insideH", "insideV"}) {
            x += std::string("<w:") + side + " w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"" + Palette::kBorder + "\"/>";
        }
        x += "</w:tblBorders><w:tblCellMar><w:top w:w=\"50\" w:type=\"dxa\"/><w:left w:w=\"90\" w:type=\"dxa\"/><w:bottom w:w=\"50\" w:type=\"dxa\"/><w:right w:w=\"90\" w:type=\"dxa\"/></w:tblCellMar></w:tblPr>";
        x += "<w:tblGrid>";
        for (int w : dxa) {
            x += "<w:gridCol w:w=\"" + std::to_string(w) + "\"/>";
        }
        x += "</w:tblGrid>";
        auto jc = [&](std::size_t c) {
            const char a = c < t.align.size() ? t.align[c] : 'l';
            return a == 'r' ? "right" : (a == 'c' ? "center" : "left");
        };
        auto cell = [&](const std::string &text, std::size_t c, const std::string &fill, const std::string &color, bool bold) {
            std::string y = "<w:tc><w:tcPr><w:tcW w:w=\"" + std::to_string(dxa[c]) + "\" w:type=\"dxa\"/>";
            if (!fill.empty()) {
                y += "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"" + fill + "\"/>";
            }
            y += "<w:vAlign w:val=\"center\"/></w:tcPr><w:p><w:pPr><w:pStyle w:val=\"Tabela\"/><w:jc w:val=\"" + std::string(jc(c)) + "\"/></w:pPr><w:r><w:rPr>";
            if (bold) {
                y += "<w:b/>";
            }
            if (!color.empty()) {
                y += "<w:color w:val=\"" + color + "\"/>";
            }
            y += "</w:rPr><w:t xml:space=\"preserve\">" + esc(text) + "</w:t></w:r></w:p></w:tc>";
            return y;
        };
        if (!t.header.empty()) {
            x += "<w:tr><w:trPr><w:cantSplit/><w:tblHeader/></w:trPr>";
            for (std::size_t c = 0; c < cols; ++c) {
                x += cell(c < t.header.size() ? t.header[c] : std::string(), c, Palette::kHeaderFill, "FFFFFF", true);
            }
            x += "</w:tr>";
        }
        for (std::size_t r = 0; r < t.rows.size(); ++r) {
            x += "<w:tr><w:trPr><w:cantSplit/></w:trPr>";
            for (std::size_t c = 0; c < cols; ++c) {
                const std::string text = c < t.rows[r].size() ? t.rows[r][c] : std::string();
                std::string fill = (r % 2 == 1) ? Palette::kZebra : "";
                std::string color;
                bool bold = t.firstColumnBold && c == 0;
                if (r < t.status.size() && c < t.status[r].size()) {
                    switch (t.status[r][c]) {
                    case CellStatus::Ok: fill = "E3F4E8"; color = "1E6B3A"; bold = true; break;
                    case CellStatus::Warn: fill = "FFF1D6"; color = "8A5A00"; bold = true; break;
                    case CellStatus::Fail: fill = "FBE1DE"; color = "A32A1E"; bold = true; break;
                    case CellStatus::None: break;
                    }
                }
                x += cell(text, c, fill, color, bold);
            }
            x += "</w:tr>";
        }
        x += "</w:tbl><w:p><w:pPr><w:pStyle w:val=\"Pequeno\"/><w:spacing w:after=\"120\"/></w:pPr></w:p>";
        return x;
    }
};

const char *kNamespaces =
    "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
    "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
    "xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\" "
    "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\"";

std::string stylesXml()
{
    auto paragraphStyle = [](const std::string &id, const std::string &name, const std::string &based, const std::string &pPr, const std::string &rPr, bool quick = true) {
        return "<w:style w:type=\"paragraph\" w:styleId=\"" + id + "\"><w:name w:val=\"" + name + "\"/>" + (based.empty() ? "" : "<w:basedOn w:val=\"" + based + "\"/>") +
               (quick ? "<w:qFormat/>" : "") + "<w:pPr>" + pPr + "</w:pPr><w:rPr>" + rPr + "</w:rPr></w:style>";
    };
    std::string x = std::string("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:styles ") + kNamespaces + ">";
    x += "<w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii=\"Calibri\" w:hAnsi=\"Calibri\" w:cs=\"Calibri\" w:eastAsia=\"Calibri\"/><w:sz w:val=\"21\"/><w:szCs w:val=\"21\"/>"
         "<w:lang w:val=\"pt-BR\"/></w:rPr></w:rPrDefault><w:pPrDefault><w:pPr><w:spacing w:after=\"100\" w:line=\"276\" w:lineRule=\"auto\"/></w:pPr></w:pPrDefault></w:docDefaults>";
    x += "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\"><w:name w:val=\"Normal\"/><w:qFormat/></w:style>";
    x += paragraphStyle("Titulo", "Title", "Normal", "<w:spacing w:after=\"120\"/>", "<w:rFonts w:ascii=\"Calibri Light\" w:hAnsi=\"Calibri Light\"/><w:b/><w:color w:val=\"1F4E79\"/><w:sz w:val=\"64\"/>");
    x += paragraphStyle("Subtitulo", "Subtitle", "Normal", "<w:spacing w:after=\"240\"/>", "<w:color w:val=\"5A6472\"/><w:sz w:val=\"30\"/>");
    x += paragraphStyle("DadosCapa", "Cover data", "Normal", "<w:spacing w:after=\"80\"/>", "<w:sz w:val=\"24\"/>");
    x += paragraphStyle("TituloSumario", "Contents heading", "Normal", "<w:keepNext/><w:spacing w:before=\"240\" w:after=\"200\"/>", "<w:b/><w:color w:val=\"1F4E79\"/><w:sz w:val=\"32\"/>");
    x += paragraphStyle("Ttulo1", "heading 1", "Normal",
                        "<w:keepNext/><w:keepLines/><w:pBdr><w:bottom w:val=\"single\" w:sz=\"8\" w:space=\"4\" w:color=\"2E75B6\"/></w:pBdr><w:spacing w:before=\"360\" w:after=\"160\"/><w:outlineLvl w:val=\"0\"/>",
                        "<w:rFonts w:ascii=\"Calibri Light\" w:hAnsi=\"Calibri Light\"/><w:b/><w:color w:val=\"1F4E79\"/><w:sz w:val=\"34\"/>");
    x += paragraphStyle("Ttulo2", "heading 2", "Normal", "<w:keepNext/><w:keepLines/><w:spacing w:before=\"280\" w:after=\"100\"/><w:outlineLvl w:val=\"1\"/>",
                        "<w:rFonts w:ascii=\"Calibri Light\" w:hAnsi=\"Calibri Light\"/><w:b/><w:color w:val=\"2E75B6\"/><w:sz w:val=\"27\"/>");
    x += paragraphStyle("Ttulo3", "heading 3", "Normal", "<w:keepNext/><w:keepLines/><w:spacing w:before=\"200\" w:after=\"60\"/><w:outlineLvl w:val=\"2\"/>", "<w:b/><w:color w:val=\"404B5A\"/><w:sz w:val=\"23\"/>");
    x += paragraphStyle("Legenda", "caption", "Normal", "<w:keepNext/><w:spacing w:before=\"120\" w:after=\"80\"/>", "<w:i/><w:color w:val=\"5A6472\"/><w:sz w:val=\"18\"/>");
    x += paragraphStyle("Pequeno", "Small", "Normal", "<w:spacing w:after=\"60\"/>", "<w:color w:val=\"5A6472\"/><w:sz w:val=\"18\"/>");
    x += paragraphStyle("Tabela", "Table text", "Normal", "<w:spacing w:before=\"0\" w:after=\"0\" w:line=\"240\" w:lineRule=\"auto\"/>", "<w:sz w:val=\"18\"/>");
    x += paragraphStyle("Rotulo", "Label", "Normal", "<w:keepNext/><w:spacing w:before=\"140\" w:after=\"40\"/>", "<w:b/><w:color w:val=\"404B5A\"/>");
    x += paragraphStyle("Equacao", "Equation", "Normal", "<w:keepLines/><w:spacing w:before=\"60\" w:after=\"60\"/><w:jc w:val=\"center\"/>", "");
    auto callout = [&](const std::string &id, const std::string &name, const std::string &fill, const std::string &bar, const std::string &color) {
        return paragraphStyle(id, name, "Normal",
                              "<w:keepLines/><w:pBdr><w:left w:val=\"single\" w:sz=\"36\" w:space=\"8\" w:color=\"" + bar + "\"/></w:pBdr><w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"" + fill +
                                  "\"/><w:spacing w:before=\"80\" w:after=\"120\"/><w:ind w:left=\"180\" w:right=\"60\"/>",
                              "<w:color w:val=\"" + color + "\"/>");
    };
    x += callout("Nota", "Note", "EEF3F9", "2E75B6", "1F3A5A");
    x += callout("Aprovado", "Approved", "E8F5EC", "2E9E5B", "1E6B3A");
    x += callout("Atencao", "Caution", "FFF6E0", "E0A100", "7A4B00");
    x += callout("Reprovado", "Failed", "FBE9E7", "C0392B", "8E241A");
    x += "<w:style w:type=\"paragraph\" w:styleId=\"Cabecalho\"><w:name w:val=\"header\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:pBdr><w:bottom w:val=\"single\" w:sz=\"4\" w:space=\"4\" w:color=\"C9D3E0\"/></w:pBdr>"
         "<w:spacing w:after=\"0\"/></w:pPr><w:rPr><w:color w:val=\"5A6472\"/><w:sz w:val=\"17\"/></w:rPr></w:style>";
    x += "<w:style w:type=\"paragraph\" w:styleId=\"Rodape\"><w:name w:val=\"footer\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:spacing w:after=\"0\"/></w:pPr><w:rPr><w:color w:val=\"5A6472\"/><w:sz w:val=\"17\"/></w:rPr></w:style>";
    x += "</w:styles>";
    return x;
}

std::string headerXml(const ReportDocument &d)
{
    return std::string("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:hdr ") + kNamespaces + "><w:p><w:pPr><w:pStyle w:val=\"Cabecalho\"/></w:pPr><w:r><w:t xml:space=\"preserve\">" +
           esc(d.headerText.empty() ? d.title : d.headerText) + "</w:t></w:r></w:p></w:hdr>";
}

std::string footerXml(const ReportDocument &d)
{
    auto field = [](const std::string &instr) {
        return "<w:r><w:fldChar w:fldCharType=\"begin\"/></w:r><w:r><w:instrText xml:space=\"preserve\"> " + instr + " </w:instrText></w:r><w:r><w:fldChar w:fldCharType=\"separate\"/></w:r><w:r><w:t>1</w:t></w:r><w:r><w:fldChar w:fldCharType=\"end\"/></w:r>";
    };
    return std::string("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:ftr ") + kNamespaces +
           "><w:p><w:pPr><w:pStyle w:val=\"Rodape\"/><w:tabs><w:tab w:val=\"right\" w:pos=\"" + std::to_string(kPageWidthDxa) + "\"/></w:tabs></w:pPr><w:r><w:t xml:space=\"preserve\">" + esc(d.project) +
           "</w:t></w:r><w:r><w:tab/></w:r><w:r><w:t xml:space=\"preserve\">Página </w:t></w:r>" + field("PAGE") + "<w:r><w:t xml:space=\"preserve\"> de </w:t></w:r>" + field("NUMPAGES") + "</w:p></w:ftr>";
}

} // namespace

std::string latexToOmml(const std::string &latex)
{
    return "<m:oMath>" + emitNode(parseLatex(latex)) + "</m:oMath>";
}

std::vector<unsigned char> writeDocx(const ReportDocument &doc)
{
    DocBuilder builder(doc);
    const std::string body = builder.body();

    std::string document = std::string("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:document ") + kNamespaces + "><w:body>" + body +
                           "<w:sectPr><w:headerReference w:type=\"default\" r:id=\"rIdHdr\"/><w:footerReference w:type=\"default\" r:id=\"rIdFtr\"/>"
                           "<w:pgSz w:w=\"11906\" w:h=\"16838\"/><w:pgMar w:top=\"1418\" w:right=\"1134\" w:bottom=\"1304\" w:left=\"1134\" w:header=\"624\" w:footer=\"567\" w:gutter=\"0\"/>";
    if (doc.cover) {
        document += "<w:titlePg/>";
    }
    document += "</w:sectPr></w:body></w:document>";

    std::string rels = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                       "<Relationship Id=\"rIdStyles\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
                       "<Relationship Id=\"rIdSettings\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings\" Target=\"settings.xml\"/>"
                       "<Relationship Id=\"rIdHdr\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/header\" Target=\"header1.xml\"/>"
                       "<Relationship Id=\"rIdFtr\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer\" Target=\"footer1.xml\"/>";
    int imageIndex = 0;
    for (const auto &img : builder.images()) {
        ++imageIndex;
        rels += "<Relationship Id=\"rIdImg" + std::to_string(imageIndex) + "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/" + img.first + "\"/>";
    }
    rels += "</Relationships>";

    const std::string contentTypes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Default Extension=\"png\" ContentType=\"image/png\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "<Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
        "<Override PartName=\"/word/settings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml\"/>"
        "<Override PartName=\"/word/header1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml\"/>"
        "<Override PartName=\"/word/footer1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml\"/>"
        "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
        "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/></Types>";

    const std::string rootRels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/></Relationships>";

    const std::string core =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><dc:title>" +
        esc(doc.title) + "</dc:title><dc:creator>" + esc(doc.author.empty() ? "Structura" : doc.author) + "</dc:creator></cp:coreProperties>";
    const std::string app = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\"><Application>Structura</Application></Properties>";
    const std::string settings = std::string("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:settings ") + kNamespaces + ">" +
                                 (doc.tableOfContents ? "<w:updateFields w:val=\"true\"/>" : "") +
                                 "<m:mathPr><m:mathFont m:val=\"Cambria Math\"/><m:dispDef/></m:mathPr><w:defaultTabStop w:val=\"720\"/><w:compat><w:compatSetting w:name=\"compatibilityMode\" "
                                 "w:uri=\"http://schemas.microsoft.com/office/word\" w:val=\"15\"/></w:compat></w:settings>";

    Zip zip;
    zip.add("[Content_Types].xml", contentTypes);
    zip.add("_rels/.rels", rootRels);
    zip.add("docProps/core.xml", core);
    zip.add("docProps/app.xml", app);
    zip.add("word/document.xml", document);
    zip.add("word/_rels/document.xml.rels", rels);
    zip.add("word/styles.xml", stylesXml());
    zip.add("word/settings.xml", settings);
    zip.add("word/header1.xml", headerXml(doc));
    zip.add("word/footer1.xml", footerXml(doc));
    for (const auto &img : builder.images()) {
        zip.add("word/media/" + img.first, *img.second);
    }
    return zip.finish();
}

} // namespace Structura::Report
