#pragma once

#include "app/ProjectDataTypes.h"
#include "viz/structure/SectionShapes.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>

namespace Structura::UI {

/// Biblioteca de perfis comerciais, disponivel sem precisar criar cada secao: o perfil so vira
/// uma secao do projeto (com identificador estavel) quando e atribuido a uma barra.
class ProfileLibrary
{
public:
    struct Entry {
        QUuid id; ///< Deterministico (v5) a partir de familia + nome
        QString family;
        QString name;
        Structura::Visualization::SectionShapes::Parameters parameters;
        bool user {false};            ///< criado pelo usuario (banco de perfis do programa)
        bool propertiesOnly {false};  ///< definido so por propriedades (sem geometria)
        double area {0.0};            ///< m2 (so propertiesOnly)
        double iz {0.0};
        double iy {0.0};
        double j {0.0};
    };

    /// Categoria de fabricacao do perfil (base para o dimensionamento futuro de estruturas metalicas).
    enum class Category { Laminado = 0, MoldadoAFrio = 1, Concreto = 2, SemGeometria = 3 };

    static const QVector<Entry> &entries();   ///< catalogo + perfis do usuario
    /// Substitui os perfis do usuario (carregados do banco do programa).
    static void setUserProfiles(const QVector<Structura::App::ProjectData::SectionInfo> &profiles);
    static const Entry *find(const QUuid &id);
    static QStringList families();

    static QList<Category> categories();
    static QString categoryName(Category category);
    /// Categoria da familia (familia vazia = perfil definido so por propriedades).
    static Category categoryOf(const QString &family);
    /// Familias de uma categoria, na ordem de exibicao.
    static QStringList familiesIn(Category category);
    /// Nome curto do tipo, sem o sufixo da categoria (ex.: "W Laminado" -> "W").
    static QString typeLabel(const QString &family);
    /// Secao do projeto (propriedades calculadas pela geometria) correspondente ao perfil.
    static Structura::App::ProjectData::SectionInfo makeSection(const Entry &entry);
};

} // namespace Structura::UI
