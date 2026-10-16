#pragma once

#include "app/ProjectDataTypes.h"
#include "app/services/MaterialLibrary.h"

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Banco de dados do programa: perfis e materiais criados pelo usuario, gravados em JSON na pasta de
 *        dados do aplicativo (valem para todos os projetos).
 *
 * Arquivos: `<AppData>/library/profiles.json` e `<AppData>/library/materials.json`.
 */
class UserLibrary
{
public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;

    /// Pasta do banco (criada sob demanda). Pode ser trocada nos testes.
    static QString directory();
    static void setDirectory(const QString &path);

    static QVector<SectionInfo> loadProfiles();
    /// Grava (ou substitui, pelo par familia + nome) um perfil.
    static bool saveProfile(const SectionInfo &section);
    static bool removeProfile(const QString &family, const QString &name);

    static QVector<MaterialLibrary::Entry> loadMaterials();
    static bool saveMaterial(const MaterialLibrary::Entry &entry);
    static bool removeMaterial(const QString &name);
};

} // namespace Structura::App::Services
