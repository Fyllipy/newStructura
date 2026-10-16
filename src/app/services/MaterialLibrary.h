#pragma once

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/// Biblioteca padrao de materiais: acos estruturais (NBR 7007 / ASTM / NBR 14762) e concretos (NBR 6118).
class MaterialLibrary
{
public:
    enum class Category { StructuralSteel, ColdFormedSteel, Concrete, Custom };

    struct Entry {
        Category category {Category::StructuralSteel};
        QString name;          ///< ex.: "ASTM A572 Gr.50"
        double fy {0.0};       ///< MPa (aco) ou fck (concreto)
        double fu {0.0};       ///< MPa (aco)
        double eGPa {200.0};
        double gGPa {77.0};
        double unitWeight {78.5};   ///< kN/m3
        QString note;
        bool user {false};     ///< criado pelo usuario (banco de materiais do programa)
        [[nodiscard]] bool isSteel() const { return category == Category::Custom ? (fy > 0.0 && fu > 0.0) : category != Category::Concrete; }
        [[nodiscard]] bool coldFormed() const { return category == Category::ColdFormedSteel; }
    };

    static const QVector<Entry> &entries();   ///< biblioteca padrao + materiais do usuario
    /// Substitui os materiais do usuario (carregados do banco do programa).
    static void setUserEntries(const QVector<Entry> &entries);
    static const Entry *find(const QString &name);
    static QString categoryName(Category category);

    /// Acos adicionais criados pelo usuario (ficam disponiveis ao dimensionamento nesta sessao).
    static void registerCustomSteel(const QString &name, double fy, double fu, bool coldFormed);
    static QVector<Entry> steels();   ///< acos da biblioteca + personalizados
};

} // namespace Structura::App::Services
