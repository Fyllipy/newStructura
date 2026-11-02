#include "MainWindow.h"

#include "CapacityCurveDialog.h"
#include "ConnectivityDialog.h"
#include "MemberEndsDialog.h"
#include "NonlinearOptionsDialog.h"
#include "SelectionModel.h"
#include "app/services/DesignService.h"
#include "viz/SceneController.h"
#include "app/services/SectionPropertiesService.h"

#include <QHash>
#include <QInputDialog>
#include <QMessageBox>

#include <algorithm>
#include <cmath>

using Structura::App::Services::DesignService;
using Structura::App::Services::SectionPropertiesService;
namespace PD = Structura::App::ProjectData;

namespace {

} // namespace

void MainWindow::onEditConnectivity()
{
    if (!m_sceneController) {
        return;
    }
    QSet<int> existing;
    for (const auto &node : m_sceneController->nodeInfos()) {
        existing.insert(node.externalId);
    }
    QVector<int> selectedNodes;
    if (m_selectionModel) {
        for (const QUuid &id : m_selectionModel->selectedNodes()) {
            if (const auto *node = m_sceneController->findNode(id); node && node->externalId() > 0) {
                selectedNodes.append(node->externalId());
            }
        }
        std::sort(selectedNodes.begin(), selectedNodes.end());
    }
    StateGuard guard(this, tr("Vínculos e ligações entre nós"));
    Structura::UI::ConnectivityDialog dialog(m_extras, selectedNodes, existing, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const PD::ProjectExtras edited = dialog.result();
    m_extras.diaphragms = edited.diaphragms;
    m_extras.rigidLinks = edited.rigidLinks;
    m_extras.inclinedSupports = edited.inclinedSupports;
    m_extras.constraints = edited.constraints;
    m_extras.links = edited.links;
    showStatusMessage(tr("Vínculos atualizados: %1 diafragma(s), %2 ligação(ões) rígida(s), %3 apoio(s) inclinado(s), %4 ligação(ões) com lei de força, %5 vínculo(s) linear(es).")
                          .arg(m_extras.diaphragms.size()).arg(m_extras.rigidLinks.size()).arg(m_extras.inclinedSupports.size()).arg(m_extras.links.size()).arg(m_extras.constraints.size()),
                      7000);
}

void MainWindow::onEditMemberEnds()
{
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    const QSet<QUuid> selected = m_selectionModel->selectedBars();
    if (selected.isEmpty()) {
        QMessageBox::information(this, tr("Ligações de extremidade"), tr("Selecione ao menos uma barra na viewport."));
        return;
    }
    // sugestoes a partir da primeira barra selecionada: Mp = fy Z, rigidez = 1000 EI/L, Ny = fy A
    QVector<int> memberIds;
    for (const QUuid &id : selected) {
        if (const auto *bar = m_sceneController->findBar(id); bar && bar->externalId() > 0) {
            memberIds.append(bar->externalId());
        }
    }
    std::sort(memberIds.begin(), memberIds.end());
    Structura::UI::MemberEndsDialog::Defaults defaults;
    {
        QHash<QUuid, std::array<double, 3>> position;
        for (const auto &node : m_sceneController->nodeInfos()) {
            position.insert(node.id, {node.x, node.y, node.z});
        }
        for (const auto &bar : m_sceneController->bars()) {
            if (bar.externalId != memberIds.first()) {
                continue;
            }
            const auto a = position.value(bar.startNodeId), b = position.value(bar.endNodeId);
            const double length = std::sqrt(std::pow(a[0] - b[0], 2) + std::pow(a[1] - b[1], 2) + std::pow(a[2] - b[2], 2));
            const MaterialInfo *material = nullptr;
            const SectionInfo *section = nullptr;
            for (const auto &m : m_materials) {
                material = m.uuid == bar.materialId ? &m : material;
            }
            for (const auto &s : m_sections) {
                section = s.uuid == bar.sectionId ? &s : section;
            }
            if (!material || !section || length <= 0.0) {
                break;
            }
            const double forceFactor = SectionPropertiesService::unitFactors(m_materials).first;   // N por unidade de forca do modelo
            const double eiToKnm2 = forceFactor / 1000.0;
            defaults.stiffnessZ = 1000.0 * material->youngModulus * section->iz * eiToKnm2 / length;
            defaults.stiffnessY = 1000.0 * material->youngModulus * section->iy * eiToKnm2 / length;
            const auto design = SectionPropertiesService::makeSection(*section, m_extras.design.rectTubeStandard);
            if (material->fy > 0.0 && design.g.Zx > 0.0) {
                const bool strongIsZ = section->iz >= section->iy;
                const double zStrong = design.g.Zx, zWeak = design.g.Zy > 0.0 ? design.g.Zy : design.g.Zx;
                defaults.yieldMomentZ = material->fy * (strongIsZ ? zStrong : zWeak) * 1e-6;   // MPa x mm3 = N.mm -> kN.m
                defaults.yieldMomentY = material->fy * (strongIsZ ? zWeak : zStrong) * 1e-6;
                defaults.axialYield = material->fy * design.g.area * 1e-3;
            }
            break;
        }
    }
    PD::MemberConnectionSpec initial;
    for (const auto &spec : m_extras.memberConnections) {
        if (spec.memberId == memberIds.first()) {
            initial = spec;
        }
    }
    StateGuard guard(this, tr("Ligações de extremidade"));
    Structura::UI::MemberEndsDialog dialog(initial, defaults, memberIds.size(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const PD::MemberConnectionSpec edited = dialog.spec();
    for (const int memberId : memberIds) {
        for (int i = m_extras.memberConnections.size() - 1; i >= 0; --i) {
            if (m_extras.memberConnections[i].memberId == memberId) {
                m_extras.memberConnections.removeAt(i);
            }
        }
        if (!edited.isDefault()) {
            PD::MemberConnectionSpec spec = edited;
            spec.memberId = memberId;
            m_extras.memberConnections.append(spec);
        }
    }
    syncBarTypeMarkers();
    showStatusMessage(edited.isDefault() ? tr("Ligações de extremidade removidas de %1 barra(s).").arg(memberIds.size())
                                         : tr("Ligações de extremidade aplicadas a %1 barra(s): %2 rótula(s) plástica(s) por barra. Rode a análise para ver os efeitos.")
                                               .arg(memberIds.size()).arg(edited.hinges.size()),
                      7000);
}

void MainWindow::onNonlinearOptions()
{
    StateGuard guard(this, tr("Análise não linear"));
    Structura::UI::NonlinearOptionsDialog dialog(m_extras.nonlinear, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_extras.nonlinear = dialog.settings();
    showStatusMessage(tr("Opções da análise não linear atualizadas."), 5000);
}

void MainWindow::onShowCapacityCurve()
{
    QStringList names;
    QVector<int> indices;
    for (int i = 0; i < m_resultViews.size(); ++i) {
        if (m_resultViews[i].result.nonlinear.used) {
            names << m_resultViews[i].name;
            indices << i;
        }
    }
    if (names.isEmpty()) {
        QMessageBox::information(this, tr("Análise não linear"),
                                 tr("Nenhum resultado de análise não linear. Defina rótulas plásticas (Cálculo > Ligações de extremidade), ligações com lei não linear "
                                    "(Cálculo > Vínculos) ou marque a análise incremental em Opções não lineares e processe a estrutura."));
        return;
    }
    int chosen = indices.first();
    if (names.size() > 1) {
        bool ok = false;
        const QString name = QInputDialog::getItem(this, tr("Análise não linear"), tr("Combinação / caso:"), names, 0, false, &ok);
        if (!ok) {
            return;
        }
        chosen = indices[names.indexOf(name)];
    }
    // forcas e momentos do solver estao na unidade do modelo: convertidos a kN e kN.m para a exibicao
    auto result = m_resultViews[chosen].result.nonlinear;
    const double toKn = SectionPropertiesService::unitFactors(m_materials).first / 1000.0;
    for (auto &hinge : result.hinges) {
        hinge.moment *= toKn;
    }
    for (auto &link : result.links) {
        for (double &f : link.force) {
            f *= toKn;
        }
    }
    Structura::UI::CapacityCurveDialog dialog(m_resultViews[chosen].name, result, this);
    dialog.exec();
}
