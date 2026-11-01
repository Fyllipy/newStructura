#include "MainWindow.h"

#include "MemberActionDialog.h"
#include "SelectionModel.h"
#include "viz/SceneController.h"

#include <QMessageBox>
#include <QStatusBar>

#include <algorithm>
#include <cmath>

namespace {
namespace Kind = Structura::App::ProjectData::MemberActionKind;

QString number(double value, int decimals = 3)
{
    QString text = QString::number(value, 'f', decimals);
    while (text.contains(QLatin1Char('.')) && (text.endsWith(QLatin1Char('0')) || text.endsWith(QLatin1Char('.')))) {
        text.chop(1);
    }
    return text;
}

} // namespace

/// Comprimento (m) de cada barra, por id externo.
static QHash<int, double> barLengths(const SceneController *scene)
{
    QHash<int, double> lengths;
    if (!scene) {
        return lengths;
    }
    QHash<QUuid, std::array<double, 3>> position;
    for (const auto &node : scene->nodeInfos()) {
        position.insert(node.id, {node.x, node.y, node.z});
    }
    for (const auto &bar : scene->bars()) {
        const auto a = position.constFind(bar.startNodeId);
        const auto b = position.constFind(bar.endNodeId);
        if (a != position.constEnd() && b != position.constEnd()) {
            lengths.insert(bar.externalId, std::sqrt(std::pow((*a)[0] - (*b)[0], 2) + std::pow((*a)[1] - (*b)[1], 2) + std::pow((*a)[2] - (*b)[2], 2)));
        }
    }
    return lengths;
}

QString MainWindow::describeMemberAction(const Structura::App::ProjectData::MemberAction &a) const
{
    const QString head = tr("Barra %1 (caso %2): ").arg(a.memberId).arg(a.loadCase);
    const QString system = a.system.isEmpty() ? QStringLiteral("LOCAL") : a.system.toUpper();
    switch (a.kind) {
    case Kind::kTrapezoid:
        return head + tr("carga de %1 a %2 m, q(a) = [%3; %4; %5], q(b) = [%6; %7; %8] kN/m (%9)")
                          .arg(number(a.a)).arg(number(a.b)).arg(number(a.v1[0])).arg(number(a.v1[1])).arg(number(a.v1[2]))
                          .arg(number(a.v2[0])).arg(number(a.v2[1])).arg(number(a.v2[2])).arg(system);
    case Kind::kPointForce:
        return head + tr("força concentrada em %1 m, F = [%2; %3; %4] kN (%5)").arg(number(a.a)).arg(number(a.v1[0])).arg(number(a.v1[1])).arg(number(a.v1[2])).arg(system);
    case Kind::kPointMoment:
        return head + tr("momento concentrado em %1 m, M = [%2; %3; %4] kN·m (%5)").arg(number(a.a)).arg(number(a.v1[0])).arg(number(a.v1[1])).arg(number(a.v1[2])).arg(system);
    case Kind::kTemperature:
        return head + tr("temperatura ΔT = %1 °C, gradiente y = %2 °C/m, z = %3 °C/m").arg(number(a.v1[0])).arg(number(a.v1[1])).arg(number(a.v1[2]));
    case Kind::kShrinkage:
        return head + tr("retração ε = %1 ‰").arg(number(a.v1[0] * 1000.0, 4));
    default:
        return head;
    }
}

void MainWindow::onApplyMemberAction()
{
    if (m_selectionModel && m_selectionModel->selectedBars().isEmpty()) {
        beginLoadPick(LoadPick::Positioned);
        return;
    }
    StateGuard guard(this, tr("Carga de barra com posição"));
    if (!m_selectionModel || !m_sceneController) {
        return;
    }
    const QSet<QUuid> selectedBars = m_selectionModel->selectedBars();
    if (selectedBars.isEmpty()) {
        QMessageBox::information(this, tr("Carga de barra"), tr("Selecione ao menos uma barra para aplicar a carga."));
        return;
    }

    const QHash<int, double> lengths = barLengths(m_sceneController);
    QVector<int> memberIds;
    for (const QUuid &barId : selectedBars) {
        if (const auto *bar = m_sceneController->findBar(barId); bar && bar->externalId() > 0) {
            memberIds.append(bar->externalId());
        }
    }
    std::sort(memberIds.begin(), memberIds.end());

    MemberActionDialog dialog(this);
    dialog.setLoadCases(loadCaseNames(), dialogLoadCase());
    dialog.setSelectedCount(memberIds.size());
    dialog.setMemberLength(memberIds.size() == 1 ? lengths.value(memberIds.first(), 0.0) : 0.0);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto values = dialog.values();
    const QString chosenCase = dialog.loadCase();

    Structura::App::ProjectData::MemberAction probe;
    probe.kind = values.kind;
    probe.v1 = values.v1;
    probe.v2 = values.v2;
    if (probe.isZero()) {
        showStatusMessage(tr("Valores nulos: nenhuma carga aplicada."), 4000);
        return;
    }

    int applied = 0;
    QStringList skipped;
    for (const int memberId : memberIds) {
        const double length = lengths.value(memberId, 0.0);
        Structura::App::ProjectData::MemberAction record = probe;
        record.memberId = memberId;
        record.loadCase = chosenCase;
        record.system = values.system;
        record.a = values.percent ? values.a / 100.0 * length : values.a;
        record.b = values.percent ? values.b / 100.0 * length : values.b;
        if (values.kind <= Kind::kPointMoment) {
            if (record.a > length + 1e-9 || record.b > length + 1e-9) {
                skipped << QString::number(memberId);
                continue;
            }
            if (values.kind == Kind::kTrapezoid && std::abs(record.b - record.a) < 1e-9) {
                skipped << QString::number(memberId);
                continue;
            }
        }
        m_extras.memberActions.append(record);
        ++applied;
    }
    syncLoadVisuals();
    refreshPropertiesPanel();
    QString message = tr("Carga aplicada a %1 barra(s).").arg(applied);
    if (!skipped.isEmpty()) {
        message += tr(" Fora do comprimento ou sem extensão (ignoradas): barra(s) %1.").arg(skipped.join(QStringLiteral(", ")));
    }
    showStatusMessage(message, 6000);
}

bool MainWindow::editMemberAction(int index)
{
    if (index < 0 || index >= m_extras.memberActions.size() || !m_sceneController) {
        return false;
    }
    auto &action = m_extras.memberActions[index];
    const QHash<int, double> lengths = barLengths(m_sceneController);
    MemberActionDialog dialog(this);
    dialog.setLoadCases(loadCaseNames(), action.loadCase);
    dialog.setSelectedCount(1);
    dialog.setMemberLength(lengths.value(action.memberId, 0.0));
    dialog.setInitial(action);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const auto values = dialog.values();
    const double length = lengths.value(action.memberId, 0.0);
    action.system = values.system;
    action.a = values.percent ? values.a / 100.0 * length : values.a;
    action.b = values.percent ? values.b / 100.0 * length : values.b;
    action.v1 = values.v1;
    action.v2 = values.v2;
    action.loadCase = dialog.loadCase();
    if (action.isZero()) {
        m_extras.memberActions.removeAt(index);
    }
    return true;
}
