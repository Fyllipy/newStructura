#pragma once

#include "app/services/QuantityService.h"

#include <QDialog>

namespace Structura::UI {

/// Lista de materiais por perfil com totais e exportacao CSV.
class QuantityDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit QuantityDialog(const Structura::App::Services::QuantityService::Summary &summary, QWidget *parent = nullptr);

private:
    Structura::App::Services::QuantityService::Summary m_summary;
};

} // namespace Structura::UI
