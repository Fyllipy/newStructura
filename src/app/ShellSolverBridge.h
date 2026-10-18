#pragma once

#include "shell/ShellResults.h"

#include <QString>

namespace Structura::App {

/// Carrega a DLL do MEF de cascas (mefs) e resolve a analise de um trabalho (`Structura::Shell::Job`).
class ShellSolverBridge
{
public:
    explicit ShellSolverBridge(QString libraryPath = QString());

    /// Resolve todos os casos do trabalho. Em caso de falha, `Results::error` traz a mensagem.
    [[nodiscard]] Structura::Shell::Results solve(const Structura::Shell::Model &model, const Structura::Shell::Mesh &mesh,
                                                  const Structura::Shell::Job &job) const;

    /// A biblioteca pode ser carregada e exporta as funcoes esperadas.
    [[nodiscard]] bool available(QString *reason = nullptr) const;

private:
    QString m_libraryPath;
};

} // namespace Structura::App
