#pragma once

#include "app/SolverBridge.h"

#include <QDialog>
#include <QString>

namespace Structura::UI {

/// Resultados da analise nao linear incremental de uma combinacao: curva de capacidade (fator de carga x deslocamento), rotulas plasticas e ligacoes.
class CapacityCurveDialog final : public QDialog
{
    Q_OBJECT

public:
    CapacityCurveDialog(const QString &title, const Structura::App::SolverBridge::NonlinearResult &result, QWidget *parent = nullptr);
};

} // namespace Structura::UI
