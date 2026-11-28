#pragma once

#include <QDialog>

namespace Structura::UI {

/// "Dizeres legais": avisos e licenças das bibliotecas de terceiros (Qt, VTK e outras), embutidos no executável (resources/legal).
class LegalNoticesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LegalNoticesDialog(QWidget *parent = nullptr);
};

} // namespace Structura::UI
