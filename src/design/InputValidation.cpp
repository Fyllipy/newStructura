#include "design/InputValidation.h"

#include <cmath>
#include <cstdio>
#include <map>

using Structura::Design::Connections::unverifiableCheck;

namespace Structura::Design::Validation {

namespace {

std::string number(double v)
{
    if (std::isnan(v)) {
        return "NaN";
    }
    if (std::isinf(v)) {
        return v > 0 ? "+infinito" : "-infinito";
    }
    char buf[48];
    std::snprintf(buf, sizeof buf, "%g", v);
    return buf;
}

} // namespace

Validator &Validator::group(const std::string &group)
{
    m_group = group;
    return *this;
}

Validator &Validator::finite(const char *field, double value)
{
    if (!std::isfinite(value)) {
        m_issues.push_back({m_group, field, std::string("valor não numérico ou infinito (") + number(value) + ")"});
    }
    return *this;
}

Validator &Validator::positive(const char *field, double value, const char *unit)
{
    if (!std::isfinite(value)) {
        return finite(field, value);
    }
    if (!(value > 0.0)) {
        m_issues.push_back({m_group, field, "deve ser maior que zero (valor " + number(value) + " " + unit + ")"});
    }
    return *this;
}

Validator &Validator::nonNegative(const char *field, double value, const char *unit)
{
    if (!std::isfinite(value)) {
        return finite(field, value);
    }
    if (value < 0.0) {
        m_issues.push_back({m_group, field, "não pode ser negativo (valor " + number(value) + " " + unit + ")"});
    }
    return *this;
}

Validator &Validator::less(const char *fieldA, double a, const char *fieldB, double b, const char *why)
{
    if (std::isfinite(a) && std::isfinite(b) && !(a < b)) {
        m_issues.push_back({m_group, fieldA, std::string("geometria impossível: ") + fieldA + " = " + number(a) + " deve ser menor que " + fieldB + " = " + number(b) + " (" + why + ")"});
    }
    return *this;
}

Validator &Validator::steel(const char *prefix, double fy, double fu)
{
    const std::string p = prefix;
    positive((p + ".fy").c_str(), fy, "MPa");
    positive((p + ".fu").c_str(), fu, "MPa");
    if (std::isfinite(fy) && std::isfinite(fu) && fy > 0.0 && fu > 0.0 && fu < fy) {
        m_issues.push_back({m_group, p + ".fu", "resistência à ruptura fu = " + number(fu) + " MPa menor que o escoamento fy = " + number(fy) + " MPa: material inconsistente"});
    }
    return *this;
}

Validator &Validator::require(bool condition, const char *field, const std::string &message)
{
    if (!condition) {
        m_issues.push_back({m_group, field, message});
    }
    return *this;
}

void Validator::append(const std::vector<Issue> &issues)
{
    m_issues.insert(m_issues.end(), issues.begin(), issues.end());
}

std::vector<Check> toUnverifiableChecks(const std::vector<Issue> &issues, const std::string &clause)
{
    std::vector<Check> out;
    std::map<std::string, std::vector<const Issue *>> byGroup;
    std::vector<std::string> order;
    for (const Issue &i : issues) {
        if (!byGroup.count(i.group)) {
            order.push_back(i.group);
        }
        byGroup[i.group].push_back(&i);
    }
    for (const std::string &group : order) {
        std::string reason;
        std::vector<std::string> details;
        for (const Issue *i : byGroup[group]) {
            details.push_back(i->field + ": " + i->message);
            reason += (reason.empty() ? "" : "; ") + i->field + " " + i->message;
        }
        out.push_back(unverifiableCheck("Dados de entrada: " + group, clause, reason, details));
    }
    return out;
}

std::vector<Issue> validate(const BoltedInput &in)
{
    Validator v("parafusos");
    v.positive("db", in.db).positive("grade.fub", in.grade.fub, "MPa").positive("espessura da parte ligada", in.plateThickness).positive("plateFu", in.plateFu, "MPa");
    v.positive("gammaA2", in.gammaA2, "-").nonNegative("endDistance", in.endDistance).nonNegative("edgeDistance", in.edgeDistance);
    v.finite("shearX", in.shearX).finite("shearY", in.shearY).finite("momentZ", in.momentZ).finite("tension", in.tension).nonNegative("grip", in.grip);
    if (in.nx > 1) {
        v.positive("sx", in.sx);
    }
    if (in.ny > 1) {
        v.positive("sy", in.sy);
    }
    v.require(in.shearPlanes >= 1, "shearPlanes", "o número de planos de corte deve ser pelo menos 1");
    return v.issues();
}

std::vector<Issue> validate(const WeldInput &in)
{
    Validator v("soldas");
    v.positive("leg", in.leg).positive("length", in.length).positive("fw", in.fw, "MPa").positive("plateThickness", in.plateThickness).steel("metal-base", in.plateFy, in.plateFu);
    v.positive("gammaW2", in.gammaW2, "-").positive("gammaA1", in.gammaA1, "-").positive("gammaA2", in.gammaA2, "-").finite("force", in.force).finite("angleDeg", in.angleDeg);
    return v.issues();
}

std::vector<Issue> validate(const SupportInput &in)
{
    Validator v("perfil de apoio (pilar)");
    v.positive("d", in.d).positive("tf", in.tf).positive("tw", in.tw).nonNegative("k", in.k).steel("apoio", in.fy, in.fu).positive("gammaA1", in.gammaA1, "-");
    if (in.face == SupportInput::Flange) {
        v.positive("bf", in.bf);
    } else {
        v.nonNegative("bf", in.bf);
    }
    v.less("2 tf", 2.0 * in.tf, "d", in.d, "as duas mesas não cabem na altura do perfil");
    v.finite("tension", in.tension).finite("compression", in.compression).nonNegative("lengthAlong", in.lengthAlong).nonNegative("lengthTransverse", in.lengthTransverse)
        .nonNegative("unbracedLength", in.unbracedLength).finite("pairForce", in.pairForce);
    return v.issues();
}

std::vector<Issue> validate(const ColumnPanelInput &in)
{
    Validator v("painel de alma do pilar");
    v.positive("d", in.d).positive("bf", in.bf).positive("tf", in.tf).positive("tw", in.tw).nonNegative("radius", in.radius).nonNegative("doubler", in.doubler).positive("fy", in.fy, "MPa");
    v.positive("gammaA1", in.gammaA1, "-").less("2 (tf + r)", 2.0 * (in.tf + in.radius), "d", in.d, "a alma não tem altura livre");
    v.finite("axial", in.axial).finite("beamForce", in.beamForce).finite("columnShear", in.columnShear).finite("panelShear", in.panelShear).nonNegative("stiffenerSpacing", in.stiffenerSpacing);
    return v.issues();
}

std::vector<Issue> validate(const EndPlateInput &in)
{
    std::vector<Issue> out;
    {
        // a origem da secao entra na mensagem do pilar: o usuario ve por que a geometria nao esta disponivel
        Validator column("pilar (" + (in.columnSource.empty() ? std::string("origem não informada") : in.columnSource) + ")");
        column.positive("colDepth (altura d)", in.colDepth).positive("colFlangeWidth (largura da mesa bf)", in.colFlangeWidth).positive("colFlangeThickness (espessura da mesa tf)", in.colFlangeThickness)
            .positive("colWebThickness (espessura da alma tw)", in.colWebThickness).nonNegative("colRadius (raio de concordância)", in.colRadius).steel("pilar", in.column.fy, in.column.fu);
        column.less("2 (tf + r)", 2.0 * (in.colFlangeThickness + in.colRadius), "d", in.colDepth, "a alma do pilar não tem altura livre");
        column.nonNegative("colUnbraced", in.colUnbraced).finite("colAxial", in.colAxial).finite("panelShear", in.panelShear).nonNegative("doublerThickness", in.doublerThickness);
        column.positive("stiffenerThickness", in.stiffenerThickness).positive("stiffenerWidth", in.stiffenerWidth);
        out = column.issues();
    }
    Validator beam("viga");
    beam.positive("beamDepth", in.beamDepth).positive("flangeWidth", in.flangeWidth).positive("flangeThickness", in.flangeThickness).positive("webThickness", in.webThickness).steel("viga", in.beam.fy, in.beam.fu);
    beam.less("2 tf", 2.0 * in.flangeThickness, "d", in.beamDepth, "as mesas da viga não cabem na altura");
    Validator plate("chapa de extremidade");
    plate.positive("plateWidth", in.plateWidth).positive("plateThickness", in.plateThickness).steel("chapa", in.plate.fy, in.plate.fu);
    Validator bolts("parafusos");
    bolts.positive("db", in.db).positive("grade.fub", in.grade.fub, "MPa").positive("pitch", in.pitch).positive("gauge", in.gauge).positive("firstRow", in.firstRow).positive("edgeVertical", in.edgeVertical);
    bolts.require(in.tensionRows >= 1, "tensionRows", "deve haver pelo menos uma fileira tracionada").require(in.shearRows >= 1, "shearRows", "deve haver pelo menos uma fileira de cisalhamento");
    bolts.less("gauge", in.gauge, "plateWidth", in.plateWidth, "o gabarito dos parafusos não cabe na largura da chapa");
    Validator welds("soldas");
    welds.positive("weldFlangeLeg", in.weldFlangeLeg).positive("weldWebLeg", in.weldWebLeg).positive("weldFw", in.weldFw, "MPa");
    Validator loads("esforços e coeficientes");
    loads.finite("moment", in.moment).finite("shear", in.shear).finite("axial", in.axial).positive("gammaA1", in.gammaA1, "-").positive("gammaA2", in.gammaA2, "-").positive("gammaW2", in.gammaW2, "-");
    for (const Validator *v : {&beam, &plate, &bolts, &welds, &loads}) {
        out.insert(out.end(), v->issues().begin(), v->issues().end());
    }
    return out;
}

} // namespace Structura::Design::Validation
