#pragma once

#include <QMutex>
#include <QString>

#include <atomic>
#include <exception>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

class QWidget;

namespace Structura::UI {

/// Progresso informado pela tarefa em segundo plano (seguro entre threads).
class BusyProgress
{
public:
    /// `total` = 0: progresso indeterminado. `detail` vazio mantem o texto anterior.
    void report(int value, int total, const QString &detail = QString());
    void setDetail(const QString &detail);

    // leitura pela interface
    [[nodiscard]] int value() const { return m_value.load(); }
    [[nodiscard]] int total() const { return m_total.load(); }
    [[nodiscard]] QString detail() const;
    [[nodiscard]] bool takeDetailChanged();

private:
    std::atomic<int> m_value {0};
    std::atomic<int> m_total {0};
    mutable QMutex m_mutex;
    QString m_detail;
    bool m_changed {false};
};

/// Executa `job` em outra thread mostrando uma janela flutuante "Calculando…" com o nome do calculo e uma barra de progresso.
/// A interface continua respondendo (sem aceitar cliques). Excecoes do job sao relancadas na thread da interface.
/// A janela so aparece se o calculo durar mais que ~250 ms. Chamadas aninhadas executam direto, sem nova janela.
void runBusyImpl(QWidget *parent, const QString &calculation, const QString &detail, const std::function<void(BusyProgress &)> &job);

/// Verdadeiro enquanto houver um calculo em andamento (para suspender salvamento automatico e dicas do mouse).
[[nodiscard]] bool isBusy();

/// Janela "Calculando…" para laços executados na própria thread da interface (que mexem no modelo e na cena):
/// aparece imediatamente, é atualizada a cada etapa e faz os `runBusy` internos executarem direto, sem novas janelas.
/// Enquanto existir, `isBusy()` é verdadeiro (o indicador de "resultados desatualizados" e o salvamento automático ficam quietos).
class BusyScope
{
public:
    BusyScope(QWidget *parent, const QString &calculation, const QString &detail);
    ~BusyScope();
    BusyScope(const BusyScope &) = delete;
    BusyScope &operator=(const BusyScope &) = delete;
    /// `total` = 0: indeterminado. Processa eventos de pintura (sem aceitar cliques).
    void update(int value, int total, const QString &detail = QString());

private:
    struct Impl;
    Impl *m_impl {nullptr};
};

template <typename F>
auto runBusy(QWidget *parent, const QString &calculation, const QString &detail, F &&task)
{
    auto call = [&task](BusyProgress &progress) {
        if constexpr (std::is_invocable_v<F, BusyProgress &>) {
            return task(progress);
        } else {
            (void)progress;
            return task();
        }
    };
    using R = decltype(call(std::declval<BusyProgress &>()));
    if constexpr (std::is_void_v<R>) {
        runBusyImpl(parent, calculation, detail, [&](BusyProgress &progress) { call(progress); });
    } else {
        std::optional<R> result;
        runBusyImpl(parent, calculation, detail, [&](BusyProgress &progress) { result.emplace(call(progress)); });
        return std::move(*result);
    }
}

} // namespace Structura::UI
