#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>

class QTableWidget;

namespace Structura::UI {

/// Janela com todas as propriedades geometricas e de engenharia de um perfil: dimensoes, propriedades da secao bruta (integracao do contorno), torcao e
/// empenamento, parametros usados no dimensionamento (NBR 8800 / NBR 14762) e os valores que o aplicativo usa na analise.
class ProfilePropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;

    explicit ProfilePropertiesDialog(const SectionInfo &section, QWidget *parent = nullptr);

private:
    void addGroup(const QString &title);
    void addRow(const QString &name, const QString &value, const QString &unit, const QString &note = QString());
    void fill(const SectionInfo &section);
    void copyToClipboard() const;

    QTableWidget *m_table;
};

} // namespace Structura::UI
