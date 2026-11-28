// SONDA TEMPORARIA (auditoria pos-A2): capacidade de momento da ligacao padrao com 1 e 2 fileiras (regra atual x semantica anterior ao A2)
#include "app/services/ConnectionService.h"
#include "design/ColumnFlangeLever.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace Structura::App::Services;
using Structura::App::ProjectData::NodeConnection;
namespace D = Structura::Design::Connections;

struct B { const char *n; double d, bf, tw, tf; };
struct C { const char *n; double d, bf, tf, tw, r; };

static ConnectionService::BeamInfo mk(const B &b)
{
    ConnectionService::BeamInfo x; x.name = b.n; x.family = "W Laminado";
    x.geometry = {{"d", b.d}, {"bf", b.bf}, {"tw", b.tw}, {"tf", b.tf}};
    x.depth = b.d; x.bf = b.bf; x.tw = b.tw; x.tf = b.tf; return x;
}

// maior momento (kN.m) com todas as verificacoes <= 1; excludeA2 reproduz a semantica anterior ao A2
static double capacity(const NodeConnection &c, bool excludeA2, std::string &gov, bool &nv)
{
    auto eval = [&](double m) {
        ConnectionService::Forces f; f.moment = m * 1e6; f.shear = 0.0;
        return ConnectionService::evaluate(c, f);
    };
    auto worst = [&](double m, std::string *g) {
        const auto e = eval(m);
        double w = 0.0;
        for (const auto &k : e.checks) {
            if (excludeA2 && D::isColumnFlangeLeverCheck(k)) continue;
            if (k.status == D::CheckStatus::Complementary || k.status == D::CheckStatus::Information || k.status == D::CheckStatus::NotApplicable) continue;
            if (k.ratio() > w) { w = k.ratio(); if (g) *g = k.name; }
        }
        return w;
    };
    nv = false;
    if (!excludeA2) {
        const auto e = eval(1.0);
        for (const auto &k : e.checks) if (k.status == D::CheckStatus::Unverifiable) nv = true;
        if (nv) return -1.0;
    }
    double lo = 0.0, hi = 1.0;
    while (worst(hi, nullptr) <= 1.0 && hi < 1e5) { lo = hi; hi *= 2.0; }
    for (int i = 0; i < 50; ++i) { const double mid = 0.5 * (lo + hi); (worst(mid, nullptr) <= 1.0 ? lo : hi) = mid; }
    worst(hi, &gov);
    return lo;
}

int main()
{
    const std::vector<B> beams = {{"IPE200", 200, 100, 5.6, 8.5}, {"IPE300", 300, 150, 7.1, 10.7}, {"IPE400", 400, 180, 8.6, 13.5}, {"IPE500", 500, 200, 10.2, 16.0}};
    const std::vector<C> cols = {{"HEB200", 200, 200, 15, 9, 18}, {"HEB300", 300, 300, 19, 11, 27}};
    std::printf("%-8s %-7s | %-4s | %-30s | %-34s\n", "viga", "pilar", "fil", "regra ATUAL (M kN.m, governa)", "semantica ANTERIOR ao A2 (sem A2)");
    for (const auto &b : beams) for (const auto &col : cols) for (int rows : {1, 2}) {
        NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, mk(b), 250.0, 400.0);
        c.nodeId = 4; c.memberId = 5;
        c.values["supD"] = col.d; c.values["supBf"] = col.bf; c.values["supTf"] = col.tf; c.values["supTw"] = col.tw; c.values["supR"] = col.r;
        c.values["supFy"] = 250; c.values["supFu"] = 400; c.values["supConfirmed"] = 1; c.values["tRows"] = rows;
        std::string g1, g2; bool nv = false, nv2 = false;
        const double a = capacity(c, false, g1, nv), p = capacity(c, true, g2, nv2);
        char buf[64];
        if (nv) std::snprintf(buf, sizeof buf, "NAO VERIFICAVEL"); else std::snprintf(buf, sizeof buf, "%.1f", a);
        std::printf("%-8s %-7s | %-4d | %-15s %-14.14s | %.1f  (%.34s)\n", b.n, col.n, rows, buf, nv ? "" : g1.c_str(), p, g2.c_str());
    }
    return 0;
}
