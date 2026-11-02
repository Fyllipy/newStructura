#include "SectionPropertiesService.h"

#include "app/services/SectionPlacementService.h"
#include "viz/structure/SectionShapes.h"

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

using Structura::Visualization::SectionShapes;

// ---------------------------------------------------------------------------------------------------------------------
// Utilitarios compartilhados entre analise e dimensionamento (extraidos do DesignService sem alterar nenhuma formula)
// ---------------------------------------------------------------------------------------------------------------------

std::pair<double, double> SectionPropertiesService::shearAreasMm2(const Structura::Design::DesignSection &section)
{
    using Structura::Design::SectionKind;
    const double d = section.d, bf = section.bf, tw = section.tw, tf = section.tf, t = section.t;
    switch (section.kind) {
    case SectionKind::I:
    case SectionKind::Channel:
        return {d * tw, 2.0 * bf * tf};
    case SectionKind::Tee:
        return {d * tw, bf * tf};
    case SectionKind::RectTube: {
        if (section.box) {
            const double hClear = std::max(0.0, d - 2.0 * section.tFlange), wClear = std::max(0.0, bf - 2.0 * section.tWeb);
            return {2.0 * hClear * section.tWeb, 2.0 * wClear * section.tFlange};
        }
        const double h = std::max(0.0, d - 2.0 * t), w = std::max(0.0, bf - 2.0 * t);
        return {2.0 * h * t, 2.0 * w * t};
    }
    case SectionKind::RoundTube:
    case SectionKind::LatticedAngles:
        return {section.g.area / 2.0, section.g.area / 2.0};
    case SectionKind::DoubleAngleT:
        return {2.0 * section.leg * t, 2.0 * section.leg * t};
    case SectionKind::DoubleChannelBack:
    case SectionKind::DoubleChannelBox:
        return {2.0 * d * tw, 4.0 * 0.5 * (bf - section.gapE) * tf};
    case SectionKind::SolidRound:
        return {0.9 * section.g.area, 0.9 * section.g.area};
    case SectionKind::SolidRect:
        return {5.0 / 6.0 * section.g.area, 5.0 / 6.0 * section.g.area};
    case SectionKind::ColdChannel:
    case SectionKind::ColdLippedChannel:
    case SectionKind::ColdZ:
    case SectionKind::ColdHat:
        return {d * t, 2.0 * bf * t};
    default:
        return {0.0, 0.0};
    }
}

std::pair<double, double> SectionPropertiesService::unitFactors(const QVector<MaterialInfo> &materials)
{
    double e = 0.0;
    for (const auto &m : materials) {
        e = std::max(e, m.youngModulus);
    }
    // E do aco: 2e11 Pa (forca em N) ou 2e8 kN/m2 (forca em kN); comprimento em m nos dois casos.
    const double forceFactor = (e >= 1e10 || e <= 0.0) ? 1.0 : 1000.0;
    return {forceFactor, 1000.0};
}

Structura::Design::DesignSection SectionPropertiesService::makeSection(const SectionInfo &section, const QString &rectTubeStandard)
{
    using namespace Structura::Design;
    DesignSection none;
    if (section.family.isEmpty()) {
        return none;
    }
    const auto shape = SectionShapes::build(section.family, section.geometry, section.mirrored);
    if (!shape) {
        return none;
    }
    ProfileSpec spec;
    spec.family = section.family.toStdString();
    spec.mirrored = section.mirrored;
    for (auto it = section.geometry.constBegin(); it != section.geometry.constEnd(); ++it) {
        spec.params[it.key().toStdString()] = it.value();
    }
    std::vector<Loop> loops;
    for (const auto &loop : shape->loops) {
        loops.emplace_back(loop.begin(), loop.end());
    }
    // "NBR8800" / "NBR14762" forcam a norma; qualquer outro valor (padrao "AUTO") reconhece pela serie do tubo (laminado a quente x formado a frio)
    const Standard tube = rectTubeStandard.compare(QLatin1String("NBR8800"), Qt::CaseInsensitive) == 0 ? Standard::NBR8800
        : (rectTubeStandard.compare(QLatin1String("NBR14762"), Qt::CaseInsensitive) == 0 ? Standard::NBR14762 : automaticRectTubeStandard(spec));
    const double jMm4 = section.j * 1e12;

    // Eixos principais (giro nos mesmos eixos locais do solver): escolhe o sinal que anula Ixy.
    double angle = 0.0;
    const auto principal = SectionPlacementService::principalProperties(section);
    if (principal.valid && principal.inclined) {
        double best = 1e300;
        for (const double candidate : {principal.thetaDeg, -principal.thetaDeg}) {
            const DesignSection trial = makeDesignSection(spec, loops, jMm4, tube, candidate, shape->outerCount);
            if (!trial.supported()) {
                continue;
            }
            const double residual = std::abs(trial.g.Ixy) / std::max(1.0, std::max(trial.g.Ix, trial.g.Iy));
            if (residual < best) {
                best = residual;
                angle = candidate;
            }
        }
    }
    DesignSection s = makeDesignSection(spec, loops, jMm4, tube, angle, shape->outerCount);
    s.name = section.name.toStdString();
    return s;
}

} // namespace Structura::App::Services
