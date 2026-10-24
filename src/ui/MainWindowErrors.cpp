#include "MainWindow.h"

#include "ModelCheckDialog.h"
#include "app/services/ModelCheckService.h"

#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>

namespace {

struct Friendly {
    QString what;
    QString hint;
    int nodeId {0};
};

Friendly translate(const QString &raw)
{
    Friendly f;
    const QRegularExpression nodeRx(QStringLiteral("node\\s+(\\d+)"));
    if (const auto m = nodeRx.match(raw); m.hasMatch()) {
        f.nodeId = m.captured(1).toInt();
    }
    auto has = [&](const char *text) { return raw.contains(QLatin1String(text), Qt::CaseInsensitive); };
    if (has("Unstable structure")) {
        f.what = QObject::tr("A estrutura é instável: há carga aplicada em um grau de liberdade sem rigidez%1.").arg(f.nodeId > 0 ? QObject::tr(" (nó %1)").arg(f.nodeId) : QString());
        f.hint = QObject::tr("Confira os apoios (o modelo precisa impedir os 6 movimentos de corpo rígido), articulações em excesso no nó e barras desconectadas.");
    } else if (has("No free DOF")) {
        f.what = QObject::tr("Todos os graus de liberdade estão restringidos: não há nada a calcular.");
        f.hint = QObject::tr("Remova restrições em algum nó ou acrescente nós/barras livres.");
    } else if (has("Input has no nodes")) {
        f.what = QObject::tr("O modelo não tem nós.");
        f.hint = QObject::tr("Crie barras ou nós antes de analisar.");
    } else if (has("decompose") || has("Failed to solve") || has("residual") || has("singular")) {
        f.what = QObject::tr("A matriz de rigidez é singular ou mal condicionada (mecanismo).");
        f.hint = QObject::tr("Verifique apoios insuficientes, nós sem ligação à estrutura, articulações que formam mecanismo e seções com propriedades nulas.");
    } else if (has("references unknown") || has("Node not found")) {
        f.what = QObject::tr("O modelo tem uma referência a elemento inexistente (carga, barra, material ou seção).");
        f.hint = QObject::tr("Use Ferramentas > Checar modelo para localizar o problema.");
    } else if (has("has no mass")) {
        f.what = QObject::tr("O modelo não tem massa.");
        f.hint = QObject::tr("Defina o peso específico dos materiais ou escolha uma fonte de massa (combinação) que contenha cargas gravitacionais.");
    } else if (has("too large for the dense eigen solver")) {
        f.what = QObject::tr("O modelo é grande demais para a extração de autovalores (limite de 2200 graus de liberdade livres).");
        f.hint = QObject::tr("Reduza a malha (menos subdivisões de barras) ou use um subconjunto do modelo.");
    } else if (has("did not converge") || has("Second-order analysis")) {
        const bool unstable = has("unstable");
        const QRegularExpression factorRx(QStringLiteral("between ([0-9.]+) and ([0-9.]+)"));
        const auto factors = factorRx.match(raw);
        if (factors.hasMatch()) {
            f.what = unstable ? QObject::tr("A estrutura é instável sob essas cargas na análise de segunda ordem: o fator de carga crítico fica entre %1 e %2 da carga aplicada.")
                                    .arg(factors.captured(1), factors.captured(2))
                              : QObject::tr("A análise de segunda ordem não convergiu: a estrutura só equilibra até %1 da carga aplicada; o fator de carga crítico fica entre %1 e %2.")
                                    .arg(factors.captured(1), factors.captured(2));
        } else {
            f.what = QObject::tr("A análise de segunda ordem não convergiu ou a estrutura é instável sob essas cargas.");
        }
        f.hint = QObject::tr("As cargas estão próximas ou acima da carga crítica: aumente a rigidez (perfis, contraventamentos), reduza as cargas ou rode em primeira ordem.");
    } else if (has("eigenvalue problem failed")) {
        f.what = QObject::tr("Não foi possível extrair os modos (rigidez singular ou instável).");
        f.hint = QObject::tr("Confira os apoios e as articulações do modelo.");
    }
    return f;
}

} // namespace

QString MainWindow::friendlySolverWarning(const QString &raw) const
{
    auto has = [&](const char *text) { return raw.contains(QLatin1String(text), Qt::CaseInsensitive); };
    if (has("no stiffness (all members released")) {
        const QRegularExpression rx(QStringLiteral("Node (\d+), dof (\d+)"));
        const auto m = rx.match(raw);
        return m.hasMatch() ? tr("Nó %1, grau de liberdade %2: sem rigidez (todas as barras articuladas nele); foi fixado automaticamente.").arg(m.captured(1), m.captured(2)) : raw;
    }
    if (has("Ill-conditioned")) {
        const QRegularExpression rx(QStringLiteral("pivot ratio ([0-9.e+]+)"));
        const auto m = rx.match(raw);
        return tr("Matriz de rigidez mal condicionada (razão de pivôs %1): confira rigidezes muito díspares, barras muito curtas, ligações quase rígidas ou quase-mecanismos.")
            .arg(m.hasMatch() ? m.captured(1) : QStringLiteral("> 1e12"));
    }
    if (has("converged only with incremental loading")) {
        return tr("Segunda ordem: a iteração direta divergiu; o resultado foi obtido por incremento de carga. Confira a proximidade da carga crítica.");
    }
    if (has("Stability functions assume Euler-Bernoulli")) {
        return tr("Funções de estabilidade pressupõem barras de Euler-Bernoulli: com a deformação por cortante ligada usou-se a rigidez geométrica consistente.");
    }
    if (has("oscillate between active and inactive")) {
        return tr("Tirantes / apoios só de compressão alternam entre ativo e inativo sem estabilizar: ficou o último estado. Revise o modelo (cabos frouxos, tirantes redundantes) ou o carregamento.");
    }
    if (has("without settling the active set")) {
        return tr("Tirantes / apoios só de compressão: 40 iterações sem estabilizar o conjunto ativo; mantido o último resultado.");
    }
    return raw;
}

QString MainWindow::friendlySolverMessage(const QString &raw) const
{
    const Friendly f = translate(raw);
    if (f.what.isEmpty()) {
        return raw;
    }
    return tr("%1\nO que fazer: %2\n(Detalhe técnico: %3)").arg(f.what, f.hint, raw);
}

void MainWindow::reportActionableError(const QString &title, const QString &raw)
{
    const Friendly f = translate(raw);
    QMessageBox box(this);
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle(title);
    box.setText(f.what.isEmpty() ? raw : f.what);
    QString info;
    if (!f.hint.isEmpty()) {
        info = tr("O que fazer: %1").arg(f.hint);
    }
    if (!f.what.isEmpty()) {
        box.setDetailedText(raw);
    }
    box.setInformativeText(info);
    QPushButton *check = box.addButton(tr("Checar modelo"), QMessageBox::ActionRole);
    QPushButton *goTo = nullptr;
    if (f.nodeId > 0) {
        goTo = box.addButton(tr("Ir ao nó %1").arg(f.nodeId), QMessageBox::ActionRole);
    }
    box.addButton(tr("Fechar"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == check) {
        onCheckModel();
    } else if (goTo && box.clickedButton() == goTo) {
        focusCheckTarget(static_cast<int>(Structura::App::Services::ModelCheckService::TargetKind::Node), f.nodeId);
    }
}
