#include "MainWindow.h"

#include "CompositeDialog.h"
#include "DesignRunData.h"
#include "SelectionModel.h"
#include "app/services/DesignService.h"
#include "viz/SceneController.h"

#include <QMessageBox>

#include <algorithm>
#include <cmath>

void MainWindow::onCompositeFloor()
{
    using Structura::App::Services::DesignService;
    if (!m_sceneController || m_sections.isEmpty()) {
        QMessageBox::information(this, tr("Vigas e lajes mistas"), tr("Não há barras com perfil no modelo."));
        return;
    }
    // esforcos da analise (so se ja existir analise: nao dispara o calculo aqui)
    DesignRunData run;
    bool hasRun = false;
    if (!m_caseRuns.isEmpty() && !m_resultViews.isEmpty()) {
        hasRun = buildDesignRun(run);
    }
    const double lengthFactor = DesignService::unitFactors(m_materials).second;

    QHash<QUuid, const SectionInfo *> sectionById;
    for (const auto &section : m_sections) {
        sectionById.insert(section.uuid, &section);
    }
    QHash<QUuid, std::array<double, 3>> position;
    for (const auto &node : m_sceneController->nodeInfos()) {
        position.insert(node.id, {node.x, node.y, node.z});
    }
    QVector<Structura::UI::CompositeCandidate> candidates;
    int initial = 0;
    const QSet<QUuid> selected = m_selectionModel ? m_selectionModel->selectedBars() : QSet<QUuid>();
    for (const auto &bar : m_sceneController->bars()) {
        const auto it = sectionById.constFind(bar.sectionId);
        if (it == sectionById.constEnd()) {
            continue;
        }
        const SectionInfo &section = **it;
        if (section.family != QLatin1String("W Laminado") && section.family != QLatin1String("I Laminado")) {
            continue;
        }
        Structura::UI::CompositeCandidate c;
        c.memberId = bar.externalId;
        c.name = section.name;
        c.d = section.geometry.value(QStringLiteral("d"));
        c.bf = section.geometry.value(QStringLiteral("bf"));
        c.tf = section.geometry.value(QStringLiteral("tf"));
        c.tw = section.geometry.value(QStringLiteral("tw"));
        c.section = DesignService::makeSection(section, m_extras.design.rectTubeStandard);
        if (!c.section.supported() || c.d <= 0.0) {
            continue;
        }
        const auto a = position.constFind(bar.startNodeId), b = position.constFind(bar.endNodeId);
        if (a != position.constEnd() && b != position.constEnd()) {
            c.lengthMm = std::sqrt(std::pow(a->at(0) - b->at(0), 2.0) + std::pow(a->at(1) - b->at(1), 2.0) + std::pow(a->at(2) - b->at(2), 2.0)) * lengthFactor;
        }
        const auto params = m_extras.design.paramsFor(c.memberId);
        const auto steel = DesignService::findSteel(params.steel.isEmpty() ? m_extras.design.defaultSteel : params.steel);
        if (steel) {
            c.fy = steel->fy;
            c.fu = steel->fu;
        }
        if (hasRun) {
            for (const auto &effects : run.effects) {
                if (effects.memberId != c.memberId) {
                    continue;
                }
                for (const auto &combo : effects.combos) {
                    for (const auto &station : combo.stations) {
                        c.msdAnalysis = std::max(c.msdAnalysis, station.e.Mx);
                        c.vsdAnalysis = std::max(c.vsdAnalysis, std::abs(station.e.Vy));
                    }
                }
                c.hasAnalysis = c.msdAnalysis > 0.0;
            }
        }
        c.saved = m_extras.compositeBeams.value(c.memberId);
        if (selected.contains(bar.id)) {
            initial = c.memberId;
        }
        candidates.append(std::move(c));
    }
    if (candidates.isEmpty()) {
        QMessageBox::information(this, tr("Vigas e lajes mistas"), tr("Nenhuma barra com perfil I/W laminado no modelo. A viga mista é verificada para perfis de aço de seção I."));
        // a aba da laje mista independe das barras: abre mesmo assim
    }
    Structura::UI::CompositeDialog dialog(candidates, initial, m_extras.deckSlab, this);
    connect(&dialog, &Structura::UI::CompositeDialog::beamInputsChanged, this, [this](int memberId, const QMap<QString, double> &values) {
        m_extras.compositeBeams.insert(memberId, values);
        markModified();
    });
    connect(&dialog, &Structura::UI::CompositeDialog::deckInputsChanged, this, [this](const QMap<QString, double> &values) {
        m_extras.deckSlab = values;
        markModified();
    });
    dialog.exec();
}
