#include "design/SupportChecks.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using detail::fmtn;
using detail::kModulusE;
using detail::ratioN;

std::vector<Check> checkSupportMember(const SupportInput &in)
{
    std::vector<Check> out;
    if (in.face == SupportInput::None || in.tf <= 0.0 || in.tw <= 0.0 || in.d <= 0.0) {
        return out;
    }
    const double g = in.gammaA1;
    const double k = in.k > 0.0 ? in.k : 1.5 * in.tf;
    const double n = std::max(in.lengthAlong, 1.0);
    const double ft = in.tension, fc = in.compression;
    const double fmax = std::max(ft, fc);

    if (in.face == SupportInput::Flange) {
        // flexao local da mesa (so para tracao; dispensada se a carga atua em menos de 0,15 bf transversalmente)
        {
            double rd = 6.25 * in.tf * in.tf * in.fy / g;
            if (in.atMemberEnd) {
                rd *= 0.5;   // 5.7.2.3: carga a menos de 10 tf da extremidade
            }
            const bool exempt = in.lengthTransverse < 0.15 * in.bf;
            Check c = ratioN("Apoio: flexão local da mesa", "NBR 8800:2024, 5.7.2", exempt ? 0.0 : ft, rd);
            c.lines.push_back(fmtn("R_d = 6,25 tf² fy/γa1%s = %.1f kN (tf = %.1f mm, fy = %.0f MPa)", in.atMemberEnd ? " × 0,5 (extremidade)" : "", rd / 1e3, in.tf, in.fy));
            c.lines.push_back(exempt ? fmtn("Dispensada (5.7.2.1): a força atua em %.1f mm < 0,15 bf = %.1f mm transversalmente.", in.lengthTransverse, 0.15 * in.bf)
                                     : fmtn("Força de tração normal à mesa = %.1f kN.", ft / 1e3));
            out.push_back(std::move(c));
        }
        // escoamento local da alma (tracao ou compressao)
        {
            const double rd = in.atMemberEnd ? 1.10 * (2.5 * k + n) * in.fy * in.tw / g : 1.10 * (5.0 * k + n) * in.fy * in.tw / g;
            Check c = ratioN("Apoio: escoamento local da alma", "NBR 8800:2024, 5.7.3", fmax, rd);
            c.lines.push_back(fmtn("R_d = 1,10 (%s k + n) fy tw/γa1 = %.1f kN (k = %.1f mm, n = %.0f mm, tw = %.1f mm)", in.atMemberEnd ? "2,5" : "5", rd / 1e3, k, n, in.tw));
            out.push_back(std::move(c));
        }
        // enrugamento da alma (compressao)
        if (fc > 0.0) {
            const double nd = n / in.d;
            const double ratioT = std::pow(in.tw / in.tf, 1.5);
            const double root = std::sqrt(kModulusE * in.fy * in.tf / in.tw);
            double rd;
            if (!in.atMemberEnd) {
                rd = 0.66 * in.tw * in.tw * (1.0 + 3.0 * nd * ratioT) * root / g;
            } else if (nd <= 0.2) {
                rd = 0.33 * in.tw * in.tw * (1.0 + 3.0 * nd * ratioT) * root / g;
            } else {
                rd = 0.33 * in.tw * in.tw * (1.0 + (4.0 * nd - 0.2) * ratioT) * root / g;
            }
            Check c = ratioN("Apoio: enrugamento da alma", "NBR 8800:2024, 5.7.4", fc, rd);
            c.lines.push_back(fmtn("R_d = %.1f kN (n/d = %.2f, tw/tf = %.2f)", rd / 1e3, nd, in.tw / in.tf));
            out.push_back(std::move(c));
        }
        // flambagem lateral da alma (5.7.5): compressao na mesa quando o deslocamento lateral relativo das mesas nao esta impedido
        if (fc > 0.0 && in.unbracedLength > 0.0) {
            const double h = std::max(in.d - 2.0 * k, 1.0);
            const double slender = (h / in.tw) / (in.unbracedLength / in.bf);
            const double cr = 16.0 * kModulusE;   // MSd >= Mr (conservador: o momento do pilar nao e conhecido aqui)
            const double base = cr * std::pow(in.tw, 3.0) * in.tf / (g * h * h);
            // rotacao da mesa carregada nao impedida (conservador): 5.7.5.2-b, valida para (h/tw)/(l/bf) <= 1,70
            Check c;
            if (slender <= 1.70) {
                const double rd = base * 0.37 * std::pow(slender, 3.0);
                c = ratioN("Apoio: flambagem lateral da alma", "NBR 8800:2024, 5.7.5", fc, rd);
                c.lines.push_back(fmtn("R_d = Cr tw³ tf/(γa1 h²) [0,37 ((h/tw)/(ℓ/bf))³] = %.1f kN; Cr = 16E; h = %.0f mm; ℓ = %.0f mm; (h/tw)/(ℓ/bf) = %.2f", rd / 1e3, h, in.unbracedLength, slender));
                c.lines.push_back("Rotação da mesa carregada considerada não impedida (5.7.5.2-b). Se não atender, contenção lateral (4.12) ou enrijecedores (5.7.9).");
            } else {
                c = ratioN("Apoio: flambagem lateral da alma", "NBR 8800:2024, 5.7.5", 0.0, 1.0);
                c.lines.push_back(fmtn("(h/tw)/(ℓ/bf) = %.2f > 1,70: o estado-limite de flambagem lateral da alma não ocorre (5.7.5.3).", slender));
            }
            out.push_back(std::move(c));
        }
        return out;
    }

    // ligacao na alma: a alma, entre as mesas, trabalha como faixa fixa nas mesas (linhas de plastificacao)
    const double h = std::max(in.d - 2.0 * in.tf, 1.0);
    const double width = n + h;   // dispersao a 45 graus na altura da alma
    const double mpPerWidth = in.fy * in.tw * in.tw / 4.0;
    const double rd = 8.0 * mpPerWidth * width / h / g;
    Check c = ratioN("Apoio: flexão transversal da alma", "Linhas de plastificação (estimativa)", fmax, rd);
    c.lines.push_back(fmtn("Faixa de alma fixa nas mesas: R_d = 8 (fy tw²/4) b/h /γa1 = %.1f kN (h = %.0f mm, b = n + h = %.0f mm, tw = %.1f mm)", rd / 1e3, h, width, in.tw));
    c.lines.push_back("Sem enrijecedores na face oposta. Se não atender, enrijeça a alma ou ligue na mesa.");
    out.push_back(std::move(c));
    return out;
}

} // namespace Structura::Design::Connections
