#include "SectionPlacementService.h"

#include "viz/structure/SectionShapes.h"

#include <QCoreApplication>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

std::optional<SectionPlacementService::Extents> SectionPlacementService::extents(const SectionInfo &section)
{
    if (section.family.isEmpty()) {
        return std::nullopt;
    }
    const auto shape = Structura::Visualization::SectionShapes::build(section.family, section.geometry, section.mirrored);
    if (!shape) {
        return std::nullopt;
    }
    constexpr double kMmToM = 1e-3;
    Extents result;
    result.minA = shape->bounds[0] * kMmToM;
    result.maxA = shape->bounds[1] * kMmToM;
    result.minB = shape->bounds[2] * kMmToM;
    result.maxB = shape->bounds[3] * kMmToM;
    return result;
}

std::array<double, 2> SectionPlacementService::referencePoint(const Extents &e, int reference)
{
    using Ref = Structura::App::ProjectData::SectionReference;
    switch (static_cast<Ref>(reference)) {
    case Ref::BottomLeft: return {e.minA, e.minB};
    case Ref::BottomRight: return {e.maxA, e.minB};
    case Ref::TopLeft: return {e.minA, e.maxB};
    case Ref::TopRight: return {e.maxA, e.maxB};
    case Ref::BottomMiddle: return {0.0, e.minB};
    case Ref::RightMiddle: return {e.maxA, 0.0};
    case Ref::TopMiddle: return {0.0, e.maxB};
    case Ref::LeftMiddle: return {e.minA, 0.0};
    case Ref::Center:
    default: return {0.0, 0.0};
    }
}

SectionPlacementService::AxisOffset SectionPlacementService::axisOffset(const SectionInfo *section,
                                                                        const MemberPlacement &placement)
{
    double refA = 0.0;
    double refB = 0.0;
    if (section && placement.reference != 0) {
        if (const auto bounds = extents(*section)) {
            const auto point = referencePoint(*bounds, placement.reference);
            refA = point[0];
            refB = point[1];
        }
    }
    // Centroide em relacao ao no = deslocamento adicional - ponto de referencia (a -> z, b -> y).
    AxisOffset offset;
    offset.dz = placement.offsetA - refA;
    offset.dy = placement.offsetB - refB;
    return offset;
}

SectionPlacementService::PrincipalProperties SectionPlacementService::principalProperties(const SectionInfo &section)
{
    PrincipalProperties result;
    if (section.family.isEmpty()) {
        return result;
    }
    const auto shape = Structura::Visualization::SectionShapes::build(section.family, section.geometry, section.mirrored);
    if (!shape) {
        return result;
    }
    double area = 0.0;
    double ib2 = 0.0;   // integral de b^2 dA  (iz)
    double ia2 = 0.0;   // integral de a^2 dA  (iy)
    double iab = 0.0;   // integral de a*b dA  (iyz)
    for (std::size_t i = 0; i < shape->loops.size(); ++i) {
        const auto &loop = shape->loops[i];
        double twiceArea = 0.0;
        for (std::size_t k = 0; k < loop.size(); ++k) {
            const auto &p0 = loop[k];
            const auto &p1 = loop[(k + 1) % loop.size()];
            twiceArea += p0[0] * p1[1] - p1[0] * p0[1];
        }
        const double orientation = twiceArea >= 0.0 ? 1.0 : -1.0;
        const double sign = (i == 0 ? 1.0 : -1.0) * orientation;   // furos subtraem
        for (std::size_t k = 0; k < loop.size(); ++k) {
            const auto &p0 = loop[k];
            const auto &p1 = loop[(k + 1) % loop.size()];
            const double cross = p0[0] * p1[1] - p1[0] * p0[1];
            area += sign * cross / 2.0;
            ib2 += sign * cross * (p0[1] * p0[1] + p0[1] * p1[1] + p1[1] * p1[1]) / 12.0;
            ia2 += sign * cross * (p0[0] * p0[0] + p0[0] * p1[0] + p1[0] * p1[0]) / 12.0;
            iab += sign * cross * (p0[0] * p1[1] + 2.0 * p0[0] * p0[1] + 2.0 * p1[0] * p1[1] + p1[0] * p0[1]) / 24.0;
        }
    }
    // Centroide em (0,0) por construcao do contorno; mm -> m.
    constexpr double kMm2 = 1e-6;
    constexpr double kMm4 = 1e-12;
    result.valid = area > 0.0;
    result.area = area * kMm2;
    result.iz = ib2 * kMm4;
    result.iy = ia2 * kMm4;
    result.iyz = iab * kMm4;

    constexpr double kPi = 3.14159265358979323846;
    const double scale = std::max(std::abs(result.iy), std::abs(result.iz));
    const double aniso = std::abs(result.iz - result.iy);
    result.inclined = scale > 0.0 && std::abs(result.iyz) > 1e-6 * scale;
    if (result.inclined) {
        // tan(2 theta) = 2 Iyz / (Iz - Iy) com y' = c y + s z, z' = -s y + c z  (Iy'z' = 0)
        const double theta = 0.5 * std::atan2(2.0 * result.iyz, result.iz - result.iy);
        const double c = std::cos(theta);
        const double s = std::sin(theta);
        result.thetaDeg = theta * 180.0 / kPi;
        result.izPrincipal = c * c * result.iz + s * s * result.iy + 2.0 * c * s * result.iyz;
        result.iyPrincipal = s * s * result.iz + c * c * result.iy - 2.0 * c * s * result.iyz;
    } else {
        result.thetaDeg = 0.0;
        result.izPrincipal = result.iz;
        result.iyPrincipal = result.iy;
    }
    (void)aniso;
    return result;
}

SectionPlacementService::AxisOffset SectionPlacementService::toPrincipalFrame(const AxisOffset &offset, double thetaDeg)
{
    if (thetaDeg == 0.0) {
        return offset;
    }
    constexpr double kPi = 3.14159265358979323846;
    const double c = std::cos(thetaDeg * kPi / 180.0);
    const double s = std::sin(thetaDeg * kPi / 180.0);
    AxisOffset result;
    result.dy = c * offset.dy + s * offset.dz;
    result.dz = -s * offset.dy + c * offset.dz;
    return result;
}

SectionPlacementService::RotatedInertia SectionPlacementService::inertiaInReferenceAxes(double iy,
                                                                                         double iz,
                                                                                         double rotationDeg)
{
    constexpr double kPi = 3.14159265358979323846;
    const double beta = rotationDeg * kPi / 180.0;
    const double c = std::cos(beta);
    const double s = std::sin(beta);
    // Eixos girados: y' = c*y0 + s*z0, z' = -s*y0 + c*z0  (iz' = int y'^2, iy' = int z'^2).
    RotatedInertia result;
    result.iz = c * c * iz + s * s * iy;
    result.iy = s * s * iz + c * c * iy;
    result.iyz = c * s * (iz - iy);
    return result;
}

QStringList SectionPlacementService::referenceLabels()
{
    return {
        QCoreApplication::translate("SectionPlacement", "Centro (centroide)"),
        QCoreApplication::translate("SectionPlacement", "Inferior esquerdo"),
        QCoreApplication::translate("SectionPlacement", "Inferior direito"),
        QCoreApplication::translate("SectionPlacement", "Superior esquerdo"),
        QCoreApplication::translate("SectionPlacement", "Superior direito"),
        QCoreApplication::translate("SectionPlacement", "Meio inferior"),
        QCoreApplication::translate("SectionPlacement", "Meio direito"),
        QCoreApplication::translate("SectionPlacement", "Meio superior"),
        QCoreApplication::translate("SectionPlacement", "Meio esquerdo"),
    };
}

} // namespace Structura::App::Services
