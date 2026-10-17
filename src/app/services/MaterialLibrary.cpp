#include <mutex>
#include "MaterialLibrary.h"

#include <cmath>

namespace Structura::App::Services {

namespace {

QVector<MaterialLibrary::Entry> &customSteels()
{
    static QVector<MaterialLibrary::Entry> list;
    return list;
}

QVector<MaterialLibrary::Entry> buildLibrary()
{
    using C = MaterialLibrary::Category;
    QVector<MaterialLibrary::Entry> list;
    auto steel = [&](C cat, const char *name, double fy, double fu, const char *note) {
        MaterialLibrary::Entry e;
        e.category = cat;
        e.name = QString::fromUtf8(name);
        e.fy = fy;
        e.fu = fu;
        e.eGPa = 200.0;
        e.gGPa = 77.0;
        e.unitWeight = 78.5;
        e.note = QString::fromUtf8(note);
        list.append(e);
    };
    steel(C::StructuralSteel, "MR250", 250, 400, "NBR 7007 — média resistência");
    steel(C::StructuralSteel, "AR350", 350, 450, "NBR 7007 — alta resistência");
    steel(C::StructuralSteel, "AR350COR", 350, 485, "NBR 5008 — resistente à corrosão");
    steel(C::StructuralSteel, "AR415", 415, 520, "NBR 7007 — alta resistência");
    steel(C::StructuralSteel, "ASTM A36", 250, 400, "Perfis e chapas");
    steel(C::StructuralSteel, "ASTM A572 Gr.42", 290, 415, "Alta resistência e baixa liga");
    steel(C::StructuralSteel, "ASTM A572 Gr.50", 345, 450, "Alta resistência e baixa liga");
    steel(C::StructuralSteel, "ASTM A572 Gr.60", 415, 520, "Alta resistência e baixa liga");
    steel(C::StructuralSteel, "ASTM A588", 345, 485, "Resistente à corrosão");
    steel(C::StructuralSteel, "ASTM A913 Gr.50", 345, 450, "Perfis laminados");
    steel(C::StructuralSteel, "ASTM A913 Gr.65", 450, 550, "Perfis laminados");
    steel(C::StructuralSteel, "ASTM A992", 345, 450, "Perfis W");
    steel(C::ColdFormedSteel, "ZAR 250", 250, 360, "NBR 6663 — formados a frio");
    steel(C::ColdFormedSteel, "ZAR 280", 280, 380, "NBR 6663 — formados a frio");
    steel(C::ColdFormedSteel, "ZAR 320", 320, 390, "NBR 6663 — formados a frio");
    steel(C::ColdFormedSteel, "ZAR 345", 345, 430, "NBR 6663 — formados a frio");
    steel(C::ColdFormedSteel, "ZAR 400", 400, 450, "NBR 6663 — formados a frio");
    steel(C::ColdFormedSteel, "ASTM A500 Gr.B", 317, 400, "Tubos conformados a frio");
    steel(C::ColdFormedSteel, "ASTM A500 Gr.C", 345, 427, "Tubos conformados a frio");

    for (const int fck : {20, 25, 30, 35, 40, 45, 50}) {
        MaterialLibrary::Entry e;
        e.category = C::Concrete;
        e.name = QStringLiteral("Concreto C%1").arg(fck);
        e.fy = fck;
        e.fu = 0.0;
        const double eci = 5.6 * std::sqrt(static_cast<double>(fck));   // GPa (agregado granito, alpha_E = 1,0)
        e.eGPa = 0.85 * eci;                                            // modulo secante Ecs
        e.gGPa = e.eGPa / 2.4;
        e.unitWeight = 25.0;
        e.note = QStringLiteral("NBR 6118: Ecs = 0,85 · 5600 √fck; G = Ecs / 2,4");
        list.append(e);
    }
    return list;
}

} // namespace

namespace {
QVector<MaterialLibrary::Entry> g_userEntries;
bool g_dirty = true;
} // namespace

const QVector<MaterialLibrary::Entry> &MaterialLibrary::entries()
{
    // Chamado tambem pelas threads do dimensionamento paralelo: a reconstrucao precisa ser exclusiva.
    static std::mutex mutex;
    static QVector<Entry> merged;
    std::lock_guard<std::mutex> lock(mutex);
    if (g_dirty || merged.isEmpty()) {
        merged = buildLibrary();
        merged += g_userEntries;
        g_dirty = false;
    }
    return merged;
}

void MaterialLibrary::setUserEntries(const QVector<Entry> &entries)
{
    g_userEntries = entries;
    for (auto &e : g_userEntries) {
        e.user = true;
    }
    g_dirty = true;
}

const MaterialLibrary::Entry *MaterialLibrary::find(const QString &name)
{
    for (const auto &e : entries()) {
        if (e.name.compare(name, Qt::CaseInsensitive) == 0) {
            return &e;
        }
    }
    for (const auto &e : customSteels()) {
        if (e.name.compare(name, Qt::CaseInsensitive) == 0) {
            return &e;
        }
    }
    return nullptr;
}

QString MaterialLibrary::categoryName(Category category)
{
    switch (category) {
    case Category::StructuralSteel: return QStringLiteral("Aço estrutural");
    case Category::ColdFormedSteel: return QStringLiteral("Aço formado a frio");
    case Category::Concrete: return QStringLiteral("Concreto");
    case Category::Custom: return QStringLiteral("Personalizados");
    }
    return {};
}

void MaterialLibrary::registerCustomSteel(const QString &name, double fy, double fu, bool coldFormed)
{
    if (name.isEmpty() || find(name)) {
        return;
    }
    Entry e;
    e.category = coldFormed ? Category::ColdFormedSteel : Category::StructuralSteel;
    e.name = name;
    e.fy = fy;
    e.fu = fu;
    customSteels().append(e);
}

QVector<MaterialLibrary::Entry> MaterialLibrary::steels()
{
    QVector<Entry> list;
    for (const auto &e : entries()) {
        if (e.isSteel()) {
            list.append(e);
        }
    }
    list += customSteels();
    return list;
}

} // namespace Structura::App::Services
