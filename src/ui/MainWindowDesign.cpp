#include "MainWindow.h"

#include "DesignDialogs.h"
#include "DesignPanel.h"
#include "BusyProgress.h"
#include <QCryptographicHash>
#include "DesignRunData.h"
#include "ViewOptionsBar.h"
#include "app/services/DesignService.h"
#include "app/services/IdGeneratorService.h"
#include "app/services/MaterialLibrary.h"
#include "app/services/StructuralChecks.h"
#include "ui/MainWindowPresenter.h"
#include "ui/SelectionModel.h"
#include "viz/SceneController.h"
#include "viz/postprocessor/PostProcessorRenderer.h"

#include <QApplication>
#include <QDockWidget>
#include <QMessageBox>
#include <QStatusBar>

#include <algorithm>
#include <optional>
#include <atomic>
#include <map>
#include <thread>

using Structura::App::ProjectData::DesignSettings;
using Structura::App::ProjectData::SectionInfo;
using Structura::App::Services::DesignService;
using Structura::App::Services::StructuralChecks;

namespace {

constexpr double kSteelDensity = 7850.0;   // kg/m3

bool isServiceCombination(const QString &name)
{
    return name.startsWith(QLatin1String("ELS"), Qt::CaseInsensitive);
}

/// O aco de cada barra vem do material quando a barra nao tem aco proprio.
DesignSettings effectiveSettings(const DesignSettings &base, const std::vector<Structura::App::SceneBarInfo> &bars,
                                 const QVector<Structura::App::ProjectData::MaterialInfo> &materials)
{
    DesignSettings settings = base;
    for (const auto &bar : bars) {
        const Structura::App::ProjectData::MaterialInfo *material = nullptr;
        for (const auto &m : materials) {
            if (m.uuid == bar.materialId) {
                material = &m;
                break;
            }
        }
        if (!material || material->fy <= 0.0) {
            continue;
        }
        auto params = settings.paramsFor(bar.externalId);
        if (!params.steel.isEmpty()) {
            continue;
        }
        const QString grade = material->library.isEmpty() ? material->name : material->library;
        if (!DesignService::findSteel(grade) && material->fu > 0.0) {
            Structura::App::Services::MaterialLibrary::registerCustomSteel(grade, material->fy, material->fu, false);
        }
        if (!DesignService::findSteel(grade)) {
            continue;
        }
        params.steel = grade;
        params.memberId = bar.externalId;
        auto it = std::find_if(settings.members.begin(), settings.members.end(), [&](const auto &m) { return m.memberId == bar.externalId; });
        if (it != settings.members.end()) {
            *it = params;
        } else {
            settings.members.append(params);
        }
    }
    return settings;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------

void MainWindow::onDesignSettings()
{
    QStringList names;
    for (const auto &view : m_resultViews) {
        if (view.isCombination) {
            names.append(view.name);
        }
    }
    QVector<int> selected;
    if (m_selectionModel && m_sceneController) {
        const auto selectedBars = m_selectionModel->selectedBars();
        for (const auto &bar : m_sceneController->bars()) {
            if (selectedBars.contains(bar.id)) {
                selected.append(bar.externalId);
            }
        }
    }
    Structura::UI::DesignSettingsDialog dialog(m_extras.design, names, selected, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    auto settings = dialog.settings();
    if (dialog.applyToSelection()) {
        const auto params = dialog.selectionParams();
        for (const int id : selected) {
            auto p = params;
            p.memberId = id;
            auto &members = settings.members;
            auto it = std::find_if(members.begin(), members.end(), [id](const auto &m) { return m.memberId == id; });
            auto defaults = settings.defaults;
            defaults.memberId = id;
            if (p == defaults) {
                if (it != members.end()) {
                    members.erase(it);
                }
            } else if (it != members.end()) {
                *it = p;
            } else {
                members.append(p);
            }
        }
    }
    const bool analysisChanged = settings.secondOrder != m_extras.design.secondOrder
                                 || (settings.secondOrder && settings.stiffnessReduction != m_extras.design.stiffnessReduction);
    m_extras.design = settings;
    showStatusMessage(analysisChanged ? tr("Parâmetros atualizados. A mudança de 2ª ordem vale após rodar a análise novamente (F5).")
                                      : tr("Parâmetros de dimensionamento atualizados."),
                      5000);
    if (m_designPanel && m_designDock && m_designDock->isVisible() && !analysisChanged) {
        showDesignResults(false);
    }
}

void MainWindow::onVerifyDesign()
{
    showDesignResults(false);
}

QUuid MainWindow::sectionForCandidate(const SectionInfo &candidate)
{
    for (const auto &s : m_sections) {
        if (s.family == candidate.family && s.name == candidate.name) {
            return s.uuid;
        }
    }
    SectionInfo info = candidate;
    info.uuid = QUuid::createUuid();
    info.externalId = nextSectionExternalId();
    m_sections.append(info);
    return info.uuid;
}

QString MainWindow::designCacheKey() const
{
    QByteArray key = QByteArray::number(quickFingerprint()) + '|' + QByteArray::number(m_resultsFingerprint) + '|';
    const auto &d = m_extras.design;
    key += d.defaultSteel.toUtf8() + '|' + d.rectTubeStandard.toUtf8() + '|';
    key += QByteArray::number(static_cast<int>(d.amplifyB1)) + QByteArray::number(static_cast<int>(d.slendernessGoverns)) + QByteArray::number(static_cast<int>(d.secondOrder))
        + QByteArray::number(static_cast<int>(d.autoB2)) + QByteArray::number(static_cast<int>(d.singleStorey)) + QByteArray::number(static_cast<int>(d.groupByFamily))
        + QByteArray::number(static_cast<int>(d.notionalLoads)) + QByteArray::number(d.notionalFactor, 'g', 8);
    key += '|' + QByteArray::number(d.stiffnessReduction, 'g', 8) + '|' + QByteArray::number(d.rs, 'g', 8) + '|' + QByteArray::number(d.targetEta, 'g', 8) + '|';
    key += d.combinations.join(QLatin1Char(',')).toUtf8() + '|';
    auto params = [&key](const Structura::App::ProjectData::DesignMemberParams &p) {
        key += QByteArray::number(p.memberId) + p.steel.toUtf8() + ':' + QByteArray::number(p.kx, 'g', 8) + ',' + QByteArray::number(p.ky, 'g', 8) + ',' + QByteArray::number(p.kz, 'g', 8)
            + ',' + QByteArray::number(p.segments) + ',' + QByteArray::number(p.lb, 'g', 8) + ',' + QByteArray::number(static_cast<int>(p.braced)) + QByteArray::number(static_cast<int>(p.cbOne))
            + ',' + QByteArray::number(p.b2, 'g', 8) + ',' + QByteArray::number(p.netAreaRatio, 'g', 8) + ',' + QByteArray::number(p.deflDenominator) + ',' + QByteArray::number(p.stiffenerSpacing, 'g', 8) + ',' + QByteArray::number(p.panelTensionFlange) + ',' + QByteArray::number(static_cast<int>(p.coldWork)) + ',' + QByteArray::number(p.bearingLength, 'g', 8) + ',' + QByteArray::number(p.fatigueCategory) + ',' + QByteArray::number(p.fatigueCycles, 'g', 10) + ','
            + QByteArray::number(p.openingDiameter, 'g', 8) + ',' + QByteArray::number(p.openingPosition, 'g', 8) + ',' + QByteArray::number(p.openingOffset, 'g', 8) + ','
            + QByteArray::number(p.openingNeighbourD, 'g', 8) + ',' + QByteArray::number(p.openingSpacing, 'g', 8) + ','
            + QByteArray::number(p.lx, 'g', 8) + ',' + QByteArray::number(p.ly, 'g', 8) + ',' + QByteArray::number(p.lz, 'g', 8) + ';';
    };
    params(d.defaults);
    for (const auto &m : d.members) {
        params(m);
    }
    return QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Md5).toHex());
}

bool MainWindow::buildDesignRun(DesignRunData &run)
{
    if (m_caseRuns.isEmpty() || m_resultViews.isEmpty()) {
        if (!executeAnalysis(nullptr)) {
            return false;
        }
    }
    if (m_caseRuns.isEmpty()) {
        return false;
    }
    const auto bars = m_sceneController->bars();
    run.settings = effectiveSettings(m_extras.design, bars, m_materials);
    const auto factors = DesignService::unitFactors(m_materials);
    run.lengthFactor = factors.second;
    run.forceFactor = factors.first;

    const Structura::App::SolverBridge::ModelData &geometry = m_caseRuns.constBegin()->model;
    std::vector<DesignService::ResultSet> ultimate;
    std::vector<DesignService::ResultSet> service;
    for (const auto &view : m_resultViews) {
        if (!view.isCombination) {
            continue;
        }
        DesignService::ResultSet set;
        set.name = view.name;
        set.result = &view.result;
        for (const auto &term : view.terms) {
            const auto runIt = m_caseRuns.constFind(term.first);
            if (runIt != m_caseRuns.constEnd()) {
                set.terms.emplace_back(&runIt->model, term.second);
            }
        }
        (isServiceCombination(view.name) ? service : ultimate).push_back(std::move(set));
    }
    if (ultimate.empty()) {
        run.error = tr("Não há combinações últimas calculadas. Crie combinações (ELU) em Carregamentos > Casos e combinações; "
                       "combinações cujo nome começa por \"ELS\" são tratadas como de serviço.");
        return false;
    }
    // Esforcos, B2 e flechas sao caros (percorrem todas as barras e combinacoes): ficam em cache enquanto o modelo,
    // os resultados e os parametros de projeto nao mudarem.
    const QString cacheKey = designCacheKey();
    if (m_designCache && cacheKey == m_designCacheKey) {
        run.effects = m_designCache->effects;
        run.deflections = m_designCache->deflections;
        run.elsStories = m_designCache->elsStories;
        run.warnings = m_designCache->warnings;
    } else {
        const bool secondOrder = m_extras.design.secondOrder;
        const bool autoB2 = m_extras.design.autoB2;
        const auto designSettings = m_extras.design;
        // Rigidez de servico dos perfis formados a frio: inercia da secao efetiva para a tensao de servico (NBR 14762, 9.8.5)
        QHash<int, const SectionInfo *> sectionOfBar;
        for (const auto &bar : bars) {
            for (const auto &sec : m_sections) {
                if (sec.uuid == bar.sectionId) {
                    sectionOfBar.insert(bar.externalId, &sec);
                    break;
                }
            }
        }
        auto designSections = std::make_shared<QHash<QString, Structura::Design::DesignSection>>();
        const double momentToNmm = factors.first * factors.second;
        const StructuralChecks::InertiaRatio inertiaRatio = [sectionOfBar, designSections, designSettings, momentToNmm](int memberId, double mz, double my) -> std::array<double, 2> {
            const SectionInfo *section = sectionOfBar.value(memberId, nullptr);
            if (!section) {
                return {1.0, 1.0};
            }
            const auto params = designSettings.paramsFor(memberId);
            const QString steelName = params.steel.isEmpty() ? designSettings.defaultSteel : params.steel;
            const QString key = section->uuid.toString();
            auto it = designSections->find(key);
            if (it == designSections->end()) {
                it = designSections->insert(key, DesignService::makeSection(*section, designSettings.rectTubeStandard));
            }
            return DesignService::serviceInertiaRatios(it.value(), steelName, mz * momentToNmm, my * momentToNmm);
        };
        DesignRunData heavy;
        heavy = Structura::UI::runBusy(this, tr("Dimensionamento"), tr("Reunindo os esforços das combinações…"),
                                       [&](Structura::UI::BusyProgress &progress) {
            DesignRunData out;
            progress.setDetail(tr("Esforços de cálculo por barra e combinação…"));
            out.effects = DesignService::collectEffects(geometry, ultimate, factors.first, factors.second);
            // B2 por andar (so na analise de 1a ordem)
            if (secondOrder) {
                out.warnings.append(designSettings.notionalLoads
                                        ? tr("Análise de 2ª ordem: os esforços já incluem P-Δ e P-δ; B1 e B2 não são aplicados. As imperfeições geométricas entram pelas forças nocionais das combinações.")
                                        : tr("Análise de 2ª ordem: os esforços já incluem P-Δ e P-δ; B1 e B2 não são aplicados. "
                                             "Imperfeições geométricas (carga nocional 0,3%%) devem ser lançadas como cargas ou ativadas em Parâmetros de dimensionamento."));
            } else if (autoB2) {
                progress.setDetail(tr("Coeficiente B2 por andar…"));
                const auto b2 = StructuralChecks::stories(geometry, ultimate, designSettings, factors.first, factors.second);
                DesignService::applyB2(out.effects, b2.b2);
                out.warnings += b2.warnings;
            }
            // Servico
            if (service.empty()) {
                out.warnings.append(tr("Não há combinações de serviço: crie uma combinação cujo nome comece por \"ELS\" para verificar flechas e deslocamentos."));
            } else {
                progress.setDetail(tr("Flechas e deslocamentos de serviço…"));
                out.deflections = StructuralChecks::deflections(geometry, service, designSettings, factors.second, inertiaRatio);
                out.elsStories = StructuralChecks::stories(geometry, service, designSettings, factors.first, factors.second);
            }
            return out;
        });
        run.effects = heavy.effects;
        run.deflections = heavy.deflections;
        run.elsStories = heavy.elsStories;
        run.warnings = heavy.warnings;
        m_designCache = std::make_shared<DesignRunData>(std::move(heavy));
        m_designCacheKey = cacheKey;
    }

    // Barras, perfis e pesos
    QHash<int, double> lengthM;
    for (const auto &e : run.effects) {
        lengthM.insert(e.memberId, e.lengthMm / 1000.0);
    }
    for (const auto &bar : bars) {
        const SectionInfo *section = nullptr;
        for (const auto &s : m_sections) {
            if (s.uuid == bar.sectionId) {
                section = &s;
                break;
            }
        }
        if (!section) {
            continue;
        }
        run.refs.push_back({bar.externalId, section});
        run.weightKg.insert(bar.externalId, section->area * lengthM.value(bar.externalId, 0.0) * kSteelDensity);
    }
    return true;
}

void MainWindow::ensureDesignPanel()
{
    if (m_designPanel) {
        return;
    }
    m_designPanel = new Structura::UI::DesignPanel(this);
    m_designDock = new QDockWidget(tr("Dimensionamento"), this);
    m_designDock->setObjectName(QStringLiteral("designDock"));
    m_designDock->setWidget(m_designPanel);
    m_designDock->setAllowedAreas(Qt::NoDockWidgetArea);   // janela flutuante
    m_designDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);
    m_designDock->setFloating(true);
    m_designDock->resize(420, 600);
    m_designDock->move(mapToGlobal(QPoint(std::max(40, width() - 460), 220)));
    connect(m_designPanel, &Structura::UI::DesignPanel::verifyRequested, this, &MainWindow::onVerifyDesign);
    connect(m_designPanel, &Structura::UI::DesignPanel::optimizeRequested, this, &MainWindow::onOptimizeDesign);
    connect(m_designPanel, &Structura::UI::DesignPanel::settingsRequested, this, &MainWindow::onDesignSettings);
    connect(m_designPanel, &Structura::UI::DesignPanel::memberActivated, this, &MainWindow::focusMember);
    connect(m_designPanel, &Structura::UI::DesignPanel::changeProfileRequested, this, &MainWindow::onChangeProfileForMember);
    connect(m_designDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible && !m_designEta.isEmpty()) {
            m_designEta.clear();
            applyAppearance();
        }
    });
}

void MainWindow::showDesignResults(bool optimized)
{
    DesignRunData run;
    if (!buildDesignRun(run)) {
        if (!run.error.isEmpty()) {
            QMessageBox::information(this, tr("Dimensionamento"), run.error);
        }
        return;
    }
    const auto verdicts = Structura::UI::runBusy(this, tr("Verificação dos perfis"), tr("Verificando as barras…"), [&](Structura::UI::BusyProgress &progress) {
        return DesignService::verifyAll(run.effects, run.refs, run.settings, run.lengthFactor,
                                        [&progress](int done, int total) { progress.report(done, total, QObject::tr("Barra %1 de %2").arg(done).arg(total)); });
    });

    Structura::UI::DesignPanelData data;
    data.title = optimized ? tr("Dimensionamento concluído") : tr("Verificação dos perfis adotados");
    data.verdicts = verdicts;
    data.memberWeightKg = run.weightKg;
    data.deflections = run.deflections;
    data.stories = run.elsStories.stories;
    data.storyResults = run.elsStories.storyResults;
    data.warnings = run.warnings;
    data.settings = m_extras.design;

    ensureDesignPanel();
    m_designPanel->setData(data);
    m_designDock->show();
    m_designDock->raise();

    // Cores por aproveitamento no viewport
    m_designEta.clear();
    for (const auto &v : verdicts) {
        if (v.supported && !v.output.checks.empty()) {
            m_designEta.insert(v.memberId, v.output.maxRatio);
        }
    }
    if (m_viewOptionsBar) {
        m_viewOptionsBar->setColorMode(3);
    }
    applyAppearance();
}

void MainWindow::onOptimizeDesign()
{
    if (!m_sceneController) {
        return;
    }
    struct Group {
        QString key;
        QString label;
        QVector<QUuid> bars;
        QVector<int> members;
        SectionInfo representative;
    };
    auto buildGroups = [&](bool byFamily) {
        std::map<QString, Group> map;
        for (const auto &bar : m_sceneController->bars()) {
            const SectionInfo *section = nullptr;
            for (const auto &s : m_sections) {
                if (s.uuid == bar.sectionId) {
                    section = &s;
                    break;
                }
            }
            if (!section || section->family.isEmpty()) {
                continue;
            }
            const QString key = byFamily ? section->family : section->uuid.toString();
            Group &g = map[key];
            g.key = key;
            g.label = byFamily ? section->family : section->name;
            g.bars.append(bar.id);
            g.members.append(bar.externalId);
            if (g.representative.name.isEmpty() || section->area > g.representative.area) {
                g.representative = *section;
            }
        }
        QVector<Group> groups;
        for (auto &kv : map) {
            groups.append(kv.second);
        }
        return groups;
    };

    // ── Previa (esforcos atuais) ──────────────────────────────────────────
    DesignRunData run;
    if (!buildDesignRun(run)) {
        if (!run.error.isEmpty()) {
            QMessageBox::information(this, tr("Dimensionamento"), run.error);
        }
        return;
    }
    const bool byFamily = m_extras.design.groupByFamily;
    const double target = m_extras.design.targetEta;
    const auto groups = buildGroups(byFamily);
    const auto rows = Structura::UI::runBusy(this, tr("Dimensionamento automático"), tr("Avaliando %1 grupos de barras…").arg(groups.size()),
                                             [&](Structura::UI::BusyProgress &progress) {
    QVector<Structura::UI::DesignPreviewDialog::Row> rows;
    int groupIndex = 0;
    for (const Group &g : groups) {
        progress.report(groupIndex, static_cast<int>(groups.size()), tr("Grupo %1").arg(g.label));
        Structura::UI::DesignPreviewDialog::Row row;
        row.key = g.key;
        row.label = byFamily ? tr("%1 (família)").arg(g.label) : g.label;
        row.bars = g.members.size();
        row.current = byFamily ? tr("vários") : g.representative.name;
        double currentWeight = 0.0;
        for (const int id : g.members) {
            currentWeight += run.weightKg.value(id, 0.0);
        }
        row.currentWeightKg = currentWeight;
        // eta atual do grupo
        double worst = 0.0;
        for (const auto &ref : run.refs) {
            if (g.members.contains(ref.memberId)) {
                for (const auto &e : run.effects) {
                    if (e.memberId == ref.memberId) {
                        const auto v = DesignService::verifyMember(e, *ref.section, run.settings.paramsFor(ref.memberId), run.settings, run.lengthFactor);
                        worst = std::max(worst, v.output.maxRatio);
                    }
                }
            }
        }
        row.currentEta = worst;
        const auto best = DesignService::chooseBest(run.effects, g.members, g.representative, run.settings, run.lengthFactor, target);
        row.found = best.found;
        row.note = best.note;
        if (best.found) {
            row.proposed = best.section.name;
            row.proposedEta = best.maxRatio;
            double lengthSum = 0.0;
            for (const int id : g.members) {
                for (const auto &e : run.effects) {
                    if (e.memberId == id) {
                        lengthSum += e.lengthMm / 1000.0;
                    }
                }
            }
            row.proposedWeightKg = best.section.area * lengthSum * kSteelDensity;
            if (best.section.name == g.representative.name && !byFamily) {
                row.proposed = g.representative.name;
            }
        }
        rows.append(row);
        ++groupIndex;
    }
    return rows;
    });
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Dimensionamento"), tr("Não há barras com perfil de aço para dimensionar."));
        return;
    }
    Structura::UI::DesignPreviewDialog preview(rows, target, this);
    if (preview.exec() != QDialog::Accepted) {
        return;
    }
    const QStringList excluded = preview.excludedKeys();

    // ── Aplicacao: troca, reanalisa e repete ──────────────────────────────
    std::optional<Structura::UI::BusyScope> scope;
    scope.emplace(this, tr("Dimensionamento automático"), tr("Trocando perfis e reanalisando…"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList notes;
    int changedTotal = 0;
    constexpr int kMaxIterations = 14;
    bool converged = false;
    QMap<QString, double> inertiaFloor;   // piso de inercia por grupo (limite de flecha, ELS)
    double weightBefore = 0.0;
    for (const auto &w : run.weightKg) {
        weightBefore += w;
    }
    double weightAfter = weightBefore;
    int deflectionRaises = 0;
    for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
        scope->update(iteration, kMaxIterations, tr("Iteração %1: esforços, escolha dos perfis e reanálise").arg(iteration + 1));
        DesignRunData current;
        if (!buildDesignRun(current)) {
            QApplication::restoreOverrideCursor();
            if (!current.error.isEmpty()) {
                QMessageBox::information(this, tr("Dimensionamento"), current.error);
            }
            return;
        }
        notes.clear();
        int changed = 0;
        for (const Group &g : buildGroups(byFamily)) {
            if (excluded.contains(g.key)) {
                continue;
            }
            const auto best = DesignService::chooseBest(current.effects, g.members, g.representative, current.settings, current.lengthFactor, target, inertiaFloor.value(g.key, 0.0));
            if (!best.found) {
                notes.append(tr("%1: %2").arg(g.label, best.note));
                continue;
            }
            // Troca apenas as barras cujo perfil difere do escolhido.
            QVector<QUuid> toChange;
            const QUuid targetSection = sectionForCandidate(best.section);
            for (const QUuid &barId : g.bars) {
                const auto *bar = m_sceneController->findBar(barId);
                if (bar && bar->sectionId() != targetSection) {
                    toChange.append(barId);
                }
            }
            if (toChange.isEmpty()) {
                continue;
            }
            m_presenter->handleBarSectionEdited(toChange, targetSection);
            ++changed;
        }
        changedTotal += changed;
        weightAfter = 0.0;
        for (const auto &w : current.weightKg) {
            weightAfter += w;
        }
        if (changed == 0) {
            // Resistencia atendida: verifica as flechas (ELS) e eleva o piso de inercia dos grupos que excedem o limite.
            bool raised = false;
            for (const Group &g : buildGroups(byFamily)) {
                if (excluded.contains(g.key)) {
                    continue;
                }
                double floor = inertiaFloor.value(g.key, 0.0);
                for (const auto &d : current.deflections) {
                    if (d.ratio <= 1.0 || !g.members.contains(d.memberId)) {
                        continue;
                    }
                    for (const auto &ref : current.refs) {
                        if (ref.memberId == d.memberId && ref.section) {
                            floor = std::max(floor, std::max(ref.section->iz, ref.section->iy) * d.ratio * 1.02);
                        }
                    }
                }
                if (floor > inertiaFloor.value(g.key, 0.0)) {
                    inertiaFloor.insert(g.key, floor);
                    raised = true;
                }
            }
            if (raised && deflectionRaises++ < 6) {
                continue;   // reaplica a escolha com o piso de rigidez (os esforcos continuam validos)
            }
            converged = true;
            break;
        }
        if (!executeAnalysis(nullptr)) {
            QApplication::restoreOverrideCursor();
            return;
        }
    }
    QApplication::restoreOverrideCursor();
    scope->update(kMaxIterations, kMaxIterations, tr("Atualizando resultados…"));
    refreshPropertiesPanel();
    if (m_postProcessor) {
        showResultView(m_viewOptionsBar ? std::max(0, m_viewOptionsBar->currentResultView()) : 0);
    }
    scope.reset();
    updateStaleIndicator();
    showStatusMessage(converged ? tr("Dimensionamento convergiu (%1 troca(s); massa de aço %2 → %3 kg, flechas ELS consideradas). Ctrl+Z desfaz a troca de perfis.")
                                      .arg(changedTotal).arg(weightBefore, 0, 'f', 0).arg(weightAfter, 0, 'f', 0)
                                : tr("Dimensionamento interrompido após %1 iterações sem convergência.").arg(kMaxIterations),
                      8000);
    showDesignResults(true);
    if (!notes.isEmpty()) {
        QMessageBox::warning(this, tr("Dimensionamento"), tr("Observações:\n%1").arg(notes.join(QLatin1Char('\n'))));
    }
}

// Seleciona a barra no modelo, a exibe (se estiver oculta) e centraliza a camera nela.
void MainWindow::focusMember(int memberId)
{
    for (const auto &bar : m_sceneController->bars()) {
        if (bar.externalId != memberId) {
            continue;
        }
        if (m_hiddenBars.contains(bar.id)) {
            m_hiddenBars.remove(bar.id);
            applyAppearance();
            refreshGroupsPanel();
        }
        m_selectionModel->selectBar(bar.id);
        m_sceneController->focusOnBars({bar.id});
        return;
    }
}

void MainWindow::onChangeProfileForMember(int memberId)
{
    DesignRunData run;
    if (!buildDesignRun(run)) {
        if (!run.error.isEmpty()) {
            QMessageBox::information(this, tr("Dimensionamento"), run.error);
        }
        return;
    }
    const SectionInfo *current = nullptr;
    QUuid barId;
    for (const auto &bar : m_sceneController->bars()) {
        if (bar.externalId == memberId) {
            barId = bar.id;
            for (const auto &s : m_sections) {
                if (s.uuid == bar.sectionId) {
                    current = &s;
                }
            }
        }
    }
    if (!current) {
        return;
    }
    const SectionInfo currentCopy = *current;
    // Barras afetadas: a propria e as do seu grupo
    QVector<int> affected {memberId};
    for (const QUuid &id : expandWithGroups({barId})) {
        if (const auto *bar = m_sceneController->findBar(id)) {
            if (!affected.contains(bar->externalId())) {
                affected.append(bar->externalId());
            }
        }
    }

    QVector<SectionInfo> candidates = DesignService::candidateSeries(currentCopy);
    if (candidates.isEmpty()) {
        QMessageBox::information(this, tr("Trocar perfil"),
                                 tr("Não há série de perfis cadastrada para a família \"%1\". Use o gerenciador de perfis.").arg(currentCopy.family));
        return;
    }
    bool hasCurrent = false;
    for (const auto &c : candidates) {
        hasCurrent = hasCurrent || (c.name == currentCopy.name && c.family == currentCopy.family);
    }
    if (!hasCurrent) {
        candidates.append(currentCopy);
        std::stable_sort(candidates.begin(), candidates.end(), [](const SectionInfo &a, const SectionInfo &b) { return a.area < b.area; });
    }

    // verificacao de todos os perfis candidatos: em paralelo (as verificacoes sao independentes) e com progresso
    const int candidateCount = static_cast<int>(candidates.size());
    const auto rows = Structura::UI::runBusy(this, tr("Trocar perfil — barra %1").arg(memberId),
                                             tr("Verificando %1 perfis candidatos…").arg(candidateCount),
                                             [&](Structura::UI::BusyProgress &progress) {
        QVector<Structura::UI::ProfileChoiceDialog::Row> out(candidateCount);
        std::atomic<int> next {0}, done {0};
        auto worker = [&]() {
            for (;;) {
                const int i = next.fetch_add(1);
                if (i >= candidateCount) {
                    break;
                }
                const SectionInfo &candidate = candidates[i];
                Structura::UI::ProfileChoiceDialog::Row row;
                row.name = candidate.name;
                row.areaCm2 = candidate.area * 1e4;
                row.kgPerM = candidate.area * kSteelDensity;
                row.current = candidate.name == currentCopy.name && candidate.family == currentCopy.family;
                double worst = 0.0;
                bool supported = true;
                for (const int id : affected) {
                    for (const auto &e : run.effects) {
                        if (e.memberId != id) {
                            continue;
                        }
                        const auto verdict = DesignService::verifyMember(e, candidate, run.settings.paramsFor(id), run.settings, run.lengthFactor);
                        if (!verdict.supported || verdict.output.checks.empty()) {
                            supported = false;
                        } else {
                            worst = std::max(worst, verdict.output.maxRatio);
                        }
                    }
                }
                row.eta = worst;
                row.supported = supported;
                out[i] = row;
                progress.report(done.fetch_add(1) + 1, candidateCount);
            }
        };
        const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
        const unsigned workers = std::min<unsigned>(hardware, 8u);
        std::vector<std::thread> pool;
        for (unsigned w = 1; w < workers; ++w) {
            pool.emplace_back(worker);
        }
        worker();
        for (auto &t : pool) {
            t.join();
        }
        return out;
    });

    const QString subtitle = affected.size() > 1
        ? tr("Barra %1 (e outras %2 do grupo). Perfil atual: %3. O η considera todas as barras afetadas, com os esforços da análise atual.")
              .arg(memberId).arg(affected.size() - 1).arg(currentCopy.name)
        : tr("Barra %1. Perfil atual: %2. O η usa os esforços da análise atual; após a troca a estrutura é reanalisada.")
              .arg(memberId).arg(currentCopy.name);
    Structura::UI::ProfileChoiceDialog dialog(tr("Trocar perfil da barra %1").arg(memberId), subtitle, rows, this);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedName().isEmpty()) {
        return;
    }
    for (const SectionInfo &candidate : candidates) {
        if (candidate.name == dialog.selectedName()) {
            if (candidate.name == currentCopy.name) {
                return;
            }
            const QUuid target = sectionForCandidate(candidate);
            m_presenter->handleBarSectionEdited({barId}, target);   // propaga ao grupo
            break;
        }
    }
    refreshPropertiesPanel();
    if (executeAnalysis(nullptr)) {
        if (m_postProcessor) {
            showResultView(m_viewOptionsBar ? std::max(0, m_viewOptionsBar->currentResultView()) : 0);
        }
        showDesignResults(false);
        if (m_designPanel) {
            m_designPanel->selectMember(memberId);
        }
        focusMember(memberId);
    }
}
