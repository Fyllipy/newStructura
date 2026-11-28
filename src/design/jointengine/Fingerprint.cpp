#include "design/jointengine/Fingerprint.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Structura::Design::JointEngine {

namespace {

// ---------------------------------------------------------------------------------------------------------------------- SHA-256
constexpr std::array<std::uint32_t, 64> kK = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void compress(std::array<std::uint32_t, 8> &h, const unsigned char *block)
{
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t(block[4 * i]) << 24) | (std::uint32_t(block[4 * i + 1]) << 16) | (std::uint32_t(block[4 * i + 2]) << 8) | std::uint32_t(block[4 * i + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = hh + S1 + ch + kK[i] + w[i];
        const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = S0 + maj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

// -------------------------------------------------------------------------------------------------------- serializacao canonica
class Canon
{
public:
    void tag(const char *name) { m_s += name; m_s += '{'; }
    void end() { m_s += '}'; }
    void text(const std::string &s)
    {
        m_s += std::to_string(s.size());
        m_s += ':';
        m_s += s;
        m_s += ';';
    }
    void num(double v)
    {
        if (!std::isfinite(v)) {
            m_s += "~;";   // ausente (NaN/inf) nunca vira zero
            return;
        }
        if (v == 0.0) {
            v = 0.0;   // normaliza -0
        }
        char buf[64];
        const auto r = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::general, 17);
        m_s.append(buf, r.ptr);
        m_s += ';';
    }
    void integer(long long v) { m_s += std::to_string(v); m_s += ';'; }
    void quantity(const Quantity &q)
    {
        num(q.value);
        text(q.unit);
        text(q.origin ? originName(*q.origin) : std::string("~"));
        text(q.source);
    }
    void quantities(const char *name, const std::map<std::string, Quantity> &m)
    {
        tag(name);
        for (const auto &kv : m) {
            text(kv.first);
            quantity(kv.second);
        }
        end();
    }
    [[nodiscard]] const std::string &str() const { return m_s; }

private:
    std::string m_s;
};

void material(Canon &c, const std::optional<MaterialSpec> &m)
{
    if (!m) {
        c.text("~");
        return;
    }
    c.tag("mat");
    c.integer(static_cast<int>(m->kind));
    c.text(m->name);
    c.quantities("props", m->properties);
    c.end();
}

void topology(Canon &c, const JointTopology &t)
{
    c.tag("topology");
    c.text(t.familyId);
    c.integer(static_cast<int>(t.status));
    c.text(t.unavailableReason);
    std::vector<const PartRef *> parts;
    for (const PartRef &p : t.parts) parts.push_back(&p);
    std::sort(parts.begin(), parts.end(), [](const PartRef *a, const PartRef *b) { return a->id < b->id; });
    c.tag("parts");
    for (const PartRef *p : parts) {
        c.integer(p->id);
        c.integer(static_cast<int>(p->kind));
        c.integer(p->memberId);
        c.text(p->role);
        material(c, p->material);
    }
    c.end();
    std::vector<const Link *> links;
    for (const Link &l : t.links) links.push_back(&l);
    std::sort(links.begin(), links.end(), [](const Link *a, const Link *b) { return a->id < b->id; });
    c.tag("links");
    for (const Link *l : links) {
        c.integer(l->id);
        c.integer(static_cast<int>(l->kind));
        c.integer(static_cast<int>(l->resolution));
        std::vector<int> ids = l->partIds;
        std::sort(ids.begin(), ids.end());
        for (const int id : ids) c.integer(id);
        c.text(l->note);
    }
    c.end();
    std::vector<const LoadPath *> paths;
    for (const LoadPath &p : t.paths) paths.push_back(&p);
    std::sort(paths.begin(), paths.end(), [](const LoadPath *a, const LoadPath *b) { return a->id < b->id; });
    c.tag("paths");
    for (const LoadPath *p : paths) {   // a ORDEM das uniões de um caminho e informacao: nao e reordenada
        c.integer(p->id);
        for (const int id : p->linkIds) c.integer(id);
        c.text(p->basis);
    }
    c.end();
    std::vector<std::string> gaps;
    for (const DataGap &g : t.gaps) gaps.push_back(g.code + "|" + g.description);
    std::sort(gaps.begin(), gaps.end());
    c.tag("gaps");
    for (const std::string &g : gaps) c.text(g);
    c.end();
    c.end();
}

std::string finish(const Canon &c) { return sha256Hex(c.str()); }

} // namespace

std::string sha256Hex(const std::string &data)
{
    std::array<std::uint32_t, 8> h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::string msg = data;
    const std::uint64_t bits = static_cast<std::uint64_t>(data.size()) * 8ULL;
    msg.push_back(static_cast<char>(0x80));
    while (msg.size() % 64 != 56) {
        msg.push_back('\0');
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<char>((bits >> (8 * i)) & 0xff));
    }
    for (std::size_t off = 0; off < msg.size(); off += 64) {
        compress(h, reinterpret_cast<const unsigned char *>(msg.data()) + off);
    }
    static const char *hex = "0123456789abcdef";
    std::string out;
    for (const std::uint32_t v : h) {
        for (int i = 7; i >= 0; --i) {
            out.push_back(hex[(v >> (4 * i)) & 0xf]);
        }
    }
    return out;
}

std::string configurationFingerprint(const JointTopology &t, const ConfigurationData &data)
{
    Canon c;
    c.tag("CFP1");
    topology(c, t);
    c.quantities("quantities", data.quantities);
    c.tag("texts");
    for (const auto &kv : data.texts) {
        c.text(kv.first);
        c.text(kv.second);
    }
    c.end();
    c.tag("legacy");   // origem nao registrada no legado: valores entram como estao
    for (const auto &kv : data.legacyValues) {
        c.text(kv.first);
        c.num(kv.second);
    }
    c.end();
    c.end();
    return finish(c);
}

std::string analysisFingerprint(const std::string &cfp, const AnalysisData &d)
{
    Canon c;
    c.tag("AFP1");
    c.text(cfp);
    c.quantities("actions", d.actions.actions);
    auto models = d.models;
    std::sort(models.begin(), models.end());
    c.tag("models");
    for (const auto &m : models) {
        c.text(m.first);
        c.text(m.second);
    }
    c.end();
    c.text(d.strategy);
    c.tag("controls");
    for (const auto &opt : {d.controls.maxIterations, d.controls.incrementSteps}) {
        opt ? c.integer(*opt) : c.text("~");
    }
    d.controls.tolerance ? c.num(*d.controls.tolerance) : c.text("~");
    c.end();
    c.quantities("parameters", d.parameters);
    auto assumptions = d.assumptions;
    std::sort(assumptions.begin(), assumptions.end());
    c.tag("assumptions");
    for (const std::string &a : assumptions) c.text(a);
    c.end();
    c.end();
    return finish(c);
}

std::string responseFingerprint(const MechanicalResponse &r)
{
    Canon c;
    c.tag("RFP1");
    c.integer(static_cast<int>(r.kind));
    c.integer(static_cast<int>(r.status));
    c.tag("curves");
    for (const Curve &cv : r.curves) {
        c.text(cv.actionDof);
        c.text(cv.actionUnit);
        c.text(cv.dispUnit);
        for (const auto &p : cv.points) {
            c.num(p.first);
            c.num(p.second);
        }
    }
    c.end();
    for (const auto *s : {&r.initialStiffness, &r.tangentStiffness, &r.secantStiffness}) {
        if (*s) {
            c.num((*s)->value);
            c.text((*s)->unit);
        } else {
            c.text("~");
        }
    }
    c.tag("resistances");
    for (const ResistanceItem &it : r.resistances) {
        c.text(it.name);
        c.num(it.value);
        c.text(it.unit);
        c.integer(static_cast<int>(it.kind));
        c.integer(it.governing ? 1 : 0);
    }
    c.end();
    r.deformationCapacity ? c.quantity(*r.deformationCapacity) : c.text("~");
    c.text(r.governingMechanism);
    c.end();
    return finish(c);
}

std::string classificationFingerprint(const std::string &rfp, const ClassificationKey &k)
{
    Canon c;
    c.tag("NFP1");
    c.text(rfp);
    c.text(k.standard);
    c.text(k.edition);
    c.text(k.criterion);
    c.quantities("parameters", k.parameters);
    c.end();
    return finish(c);
}

Staleness compare(const FingerprintSet &stored, const FingerprintSet &current)
{
    Staleness s;
    if (stored.configuration != current.configuration) {
        s.analysis = true;
        s.classification = true;
        return s;
    }
    if (stored.analysis != current.analysis) {
        s.analysis = true;
        s.classification = true;   // pendente da reexecucao
        return s;
    }
    if (stored.classification != current.classification) {
        s.classification = true;
    }
    return s;
}

} // namespace Structura::Design::JointEngine
