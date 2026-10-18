#include "MainWindow.h"

#include "PlacementController.h"
#include "SelectionModel.h"
#include "ShellDialog.h"
#include "app/services/DesignService.h"
#include "app/services/ShellCouplingService.h"
#include "shell/ShellFactory.h"
#include "shell/ShellMesh.h"
#include "shell/ShellVisual.h"
#include "viz/SceneController.h"
#include "viz/construction/ConstructionPlane.h"
#include "viz/construction/ConstructionScene.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <cmath>

using Structura::Shell::Kind;
using Structura::Shell::Object;
using ShellVec = std::array<double, 3>;

namespace {

constexpr double kPi = 3.14159265358979323846;

ShellVec sub(const ShellVec &a, const ShellVec &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
ShellVec add(const ShellVec &a, const ShellVec &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
ShellVec scaled(const ShellVec &a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double length(const ShellVec &a) { return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
ShellVec cross(const ShellVec &a, const ShellVec &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double dotp(const ShellVec &a, const ShellVec &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Editor (analise) e visual
// ---------------------------------------------------------------------------------------------------------------------

double MainWindow::shellUnitsPerMeter() const
{
    const double lengthFactor = Structura::App::Services::DesignService::unitFactors(m_materials).second;   // mm por unidade do modelo
    return lengthFactor > 0.0 ? 1000.0 / lengthFactor : 1.0;
}

void MainWindow::onShellEditor()
{
    if (!m_sceneController) {
        return;
    }
    if (m_shellDialog) {
        m_shellDialog->show();
        m_shellDialog->raise();
        m_shellDialog->activateWindow();
        return;
    }
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("mefs.dll"));
    const QString library = QFile::exists(bundled) ? bundled : QString();
    Structura::UI::ShellDialog::Callbacks callbacks;
    callbacks.caseNames = [this]() {
        QStringList names;
        for (const auto &c : m_extras.loadCases) {
            names << c.name;
        }
        if (names.isEmpty()) {
            names << Structura::App::ProjectData::defaultLoadCaseName();
        }
        return names;
    };
    callbacks.combinations = [this]() { return m_extras.combinations; };
    callbacks.render = [this](const Structura::Shell::RenderMesh &mesh) {
        if (m_sceneController) {
            m_shellResultsShown = mesh.hasValues;
            m_sceneController->setShellMesh(mesh);
        }
    };
    callbacks.setVisible = [this](bool visible) {
        if (m_sceneController) {
            m_sceneController->setShellVisible(visible);
        }
    };
    callbacks.unitsPerMeter = [this]() { return shellUnitsPerMeter(); };
    callbacks.modelChanged = [this](const Structura::Shell::Model &model) {
        StateGuard guard(this, tr("Editar cascas"));
        m_extras.shell = model;
        m_shellResultsShown = false;
        refreshShellVisuals(true);
        markModified();
    };
    callbacks.coupledResults = [this](const QStringList &cases, bool runAnalysis, Structura::Shell::Results &out, std::string &error) {
        return coupledShellResults(cases, runAnalysis, out, error);
    };
    callbacks.selected = [this](int objectId) {
        if (m_selectedShellObject != objectId) {
            m_selectedShellObject = objectId;
            if (!m_shellResultsShown) {
                refreshShellVisuals(true);
            }
        }
    };
    auto *dialog = new Structura::UI::ShellDialog(m_extras.shell, callbacks, library, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_shellDialog = dialog;
    dialog->show();
    dialog->selectObjectId(m_selectedShellObject);
}

bool MainWindow::coupledShellResults(const QStringList &cases, bool runAnalysis, Structura::Shell::Results &out, std::string &error)
{
    if (runAnalysis && !executeAnalysis(nullptr)) {
        error = tr("A análise do projeto não foi concluída (veja as mensagens exibidas).").toStdString();
        return false;
    }
    if (m_caseRuns.isEmpty()) {
        error = tr("Não há resultados do projeto: execute a análise (aba Análise).").toStdString();
        return false;
    }
    const double forceUnit = Structura::App::Services::DesignService::unitFactors(m_materials).first;
    std::shared_ptr<const Structura::App::Services::ShellCouplingService::Mapping> mapping;
    for (const QString &name : cases) {
        const auto run = m_caseRuns.constFind(name);
        if (run == m_caseRuns.constEnd()) {
            continue;
        }
        if (!run->model.shell || run->model.shell->empty()) {
            error = tr("A última análise não incluiu as cascas: ative \"Acoplar às barras\" e analise de novo.").toStdString();
            return false;
        }
        if (!mapping) {
            mapping = Structura::App::Services::ShellCouplingService::mapping(m_extras.shell, run->model, forceUnit);
        }
        out.cases.push_back(Structura::App::Services::ShellCouplingService::caseResult(*mapping, run->result, name.toStdString()));
    }
    if (out.cases.empty()) {
        error = tr("Nenhum dos casos de carga das cascas foi analisado.").toStdString();
        return false;
    }
    return true;
}

void MainWindow::onShellCoupleToggled(bool on)
{
    if (m_extras.shell.coupled == on) {
        return;
    }
    {
        StateGuard guard(this, on ? tr("Acoplar cascas às barras") : tr("Desacoplar cascas das barras"));
        m_extras.shell.coupled = on;
        markModified();
    }
    if (m_shellCoupleAction && m_shellCoupleAction->isChecked() != on) {
        m_shellCoupleAction->setChecked(on);
    }
    if (m_shellDialog) {
        m_shellDialog->setModel(m_extras.shell);
    }
    if (!on) {
        showStatusMessage(tr("Cascas desacopladas: a análise de barras ignora as cascas."), 5000);
        return;
    }
    if (m_extras.shell.objects.empty()) {
        showStatusMessage(tr("Acoplamento ativo: desenhe cascas para que entrem na análise das barras."), 5000);
        return;
    }
    const int pending = splitBarsAtShellNodes(false);
    if (pending > 0) {
        const auto answer = QMessageBox::question(this, tr("Acoplar cascas às barras"),
                                                   tr("%1 nó(s) da malha das cascas caem sobre o eixo de barras sem ser extremidade delas.\n\n"
                                                      "Para que a barra e a casca trabalhem juntas ao longo dessa linha, a barra precisa ser dividida nesses nós. "
                                                      "Dividir as barras agora? (Ctrl+Z desfaz)"));
        if (answer == QMessageBox::Yes) {
            const int done = splitBarsAtShellNodes(true);
            showStatusMessage(tr("Acoplamento ativo: %1 corte(s) nas barras.").arg(done), 6000);
            return;
        }
    }
    showStatusMessage(tr("Acoplamento ativo: nós coincidentes ou sobre a casca são ligados; use Analisar para resolver barras e cascas juntas."), 7000);
}

void MainWindow::refreshShellVisuals(bool force)
{
    if (m_shellCoupleAction && m_shellCoupleAction->isChecked() != m_extras.shell.coupled) {
        const QSignalBlocker blocker(m_shellCoupleAction);
        m_shellCoupleAction->setChecked(m_extras.shell.coupled);
    }
    if (!m_sceneController) {
        return;
    }
    if (m_shellResultsShown && !force) {
        return;
    }
    m_shellResultsShown = false;
    const auto &model = m_extras.shell;
    if (model.objects.empty()) {
        m_shellMesh = Structura::Shell::Mesh {};
        m_sceneController->setShellVisible(false);
        m_sceneController->setExtraSnapPoints({});
        return;
    }
    m_shellMesh = Structura::Shell::generateMesh(model);
    Structura::Shell::RenderOptions options;
    options.unitsPerMeter = shellUnitsPerMeter();
    for (std::size_t i = 0; i < model.objects.size(); ++i) {
        if (model.objects[i].id == m_selectedShellObject) {
            options.highlightObject = static_cast<int>(i);
        }
    }
    m_sceneController->setShellMesh(Structura::Shell::buildRenderMesh(m_shellMesh, model, nullptr, options));
    m_sceneController->setShellVisible(true);
    // vertices definidores das cascas atraem o cursor (snap)
    const double upm = shellUnitsPerMeter();
    std::vector<ShellVec> snap;
    for (const Object &o : model.objects) {
        for (const ShellVec &p : o.polygon) {
            snap.push_back(scaled(p, upm));
            if (o.kind == Kind::Extrusion) {
                snap.push_back(scaled(add(p, {o.param("vx"), o.param("vy"), o.param("vz")}), upm));
            }
        }
        if (o.kind == Kind::RectTank || o.kind == Kind::CylTank || o.kind == Kind::Vessel) {
            snap.push_back(scaled({o.param("x0"), o.param("y0"), o.param("z0")}, upm));
        }
    }
    m_sceneController->setExtraSnapPoints(std::move(snap));
    if (!m_shellMesh.warnings.empty()) {
        showStatusMessage(QString::fromStdString(m_shellMesh.warnings.front()), 5000);
    }
}

int MainWindow::pickShellObjectAtWidgetPos(const QPoint &widgetPos) const
{
    if (!m_sceneController || m_shellMesh.quads.empty()) {
        return -1;
    }
    const QPoint disp = toDisplayPoint(widgetPos);
    const int quad = m_sceneController->pickShellQuad(disp.x(), disp.y());
    if (quad < 0 || quad >= static_cast<int>(m_shellMesh.quads.size())) {
        return -1;
    }
    const int object = m_shellMesh.quads[static_cast<std::size_t>(quad)].object;
    return object >= 0 && object < static_cast<int>(m_extras.shell.objects.size()) ? m_extras.shell.objects[static_cast<std::size_t>(object)].id : -1;
}

void MainWindow::selectShellObject(int id)
{
    if (m_selectedShellObject == id) {
        return;
    }
    m_selectedShellObject = id;
    if (!m_shellResultsShown) {
        refreshShellVisuals(true);
    }
    if (m_shellDialog) {
        m_shellDialog->selectObjectId(id);
    }
    if (id >= 0) {
        for (const Object &o : m_extras.shell.objects) {
            if (o.id == id) {
                showStatusMessage(tr("Casca selecionada: %1 (Del exclui; botão direito: propriedades)").arg(QString::fromStdString(o.name)), 5000);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Criacao de cascas no modelo 3D
// ---------------------------------------------------------------------------------------------------------------------

namespace {

/// Propriedades da nova casca (espessura, material, malha, opcoes do tipo). false = cancelado.
bool askShellProperties(QWidget *parent, Structura::Shell::Model &model, Object &o, double &lastThicknessMm, int &lastMaterial, const QString &title)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto *form = new QFormLayout(&dialog);
    auto *name = new QLineEdit(QString::fromStdString(o.name));
    form->addRow(QObject::tr("Nome"), name);
    auto *material = new QComboBox;
    for (const auto &m : model.materials) {
        material->addItem(QString::fromStdString(m.name));
    }
    material->setCurrentIndex(std::clamp(lastMaterial, 0, static_cast<int>(model.materials.size()) - 1));
    form->addRow(QObject::tr("Material"), material);
    auto *thickness = new QDoubleSpinBox;
    thickness->setRange(0.5, 3000.0);
    thickness->setDecimals(1);
    thickness->setSuffix(QObject::tr(" mm"));
    thickness->setValue(lastThicknessMm);
    form->addRow(QObject::tr("Espessura"), thickness);
    auto *mesh = new QDoubleSpinBox;
    mesh->setRange(0.01, 20.0);
    mesh->setDecimals(3);
    mesh->setSuffix(QObject::tr(" m"));
    mesh->setValue(o.meshSize);
    mesh->setToolTip(QObject::tr("Dimensão alvo dos elementos finitos."));
    form->addRow(QObject::tr("Dimensão da malha"), mesh);
    QDoubleSpinBox *inner = nullptr, *scale = nullptr, *angle = nullptr;
    if (o.kind == Kind::Disk) {
        inner = new QDoubleSpinBox;
        inner->setRange(0.0, 1e4);
        inner->setDecimals(3);
        inner->setSuffix(QObject::tr(" m"));
        inner->setValue(o.param("rIn"));
        form->addRow(QObject::tr("Raio interno (0 = disco)"), inner);
    }
    if (o.kind == Kind::Extrusion && o.param("circle") != 0.0) {
        scale = new QDoubleSpinBox;
        scale->setRange(0.0, 10.0);
        scale->setDecimals(3);
        scale->setValue(o.param("endScale", 1.0));
        scale->setToolTip(QObject::tr("Escala do círculo na extremidade: 1 = cilindro, 0 = cone, outro valor = tronco de cone."));
        form->addRow(QObject::tr("Escala da extremidade"), scale);
    }
    if (o.kind == Kind::Revolution) {
        angle = new QDoubleSpinBox;
        angle->setRange(1.0, 360.0);
        angle->setDecimals(1);
        angle->setSuffix(QStringLiteral(" °"));
        angle->setValue(o.param("angle", 360.0));
        form->addRow(QObject::tr("Ângulo de revolução"), angle);
    }
    auto *flip = new QCheckBox(QObject::tr("Inverter a normal (pressões positivas atuam no sentido da normal)"));
    flip->setChecked(false);
    form->addRow(QString(), flip);
    auto *buttons = new QPushButton(QObject::tr("Criar"));
    buttons->setDefault(true);
    form->addRow(buttons);
    QObject::connect(buttons, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    o.name = name->text().toStdString();
    o.material = material->currentIndex();
    lastMaterial = o.material;
    o.p["t"] = thickness->value() / 1000.0;
    lastThicknessMm = thickness->value();
    o.meshSize = mesh->value();
    if (inner) {
        o.p["rIn"] = inner->value();
    }
    if (scale) {
        o.p["endScale"] = scale->value();
    }
    if (angle) {
        o.p["angle"] = angle->value();
    }
    if (flip->isChecked()) {
        if (o.kind == Kind::Plate) {
            std::reverse(o.polygon.begin(), o.polygon.end());
        } else {
            o.p["flip"] = 1.0;
        }
    }
    return true;
}

} // namespace

QString MainWindow::shellKindLabel(int kind) const
{
    switch (static_cast<Kind>(kind)) {
    case Kind::Plate: return tr("Chapa");
    case Kind::RectTank: return tr("Reservatório");
    case Kind::CylTank: return tr("Tanque");
    case Kind::Vessel: return tr("Vaso");
    case Kind::Disk: return tr("Disco");
    case Kind::Extrusion: return tr("Extrusão");
    case Kind::Revolution: return tr("Revolução");
    }
    return tr("Casca");
}

bool MainWindow::commitShell(Object o, const QString &undoText)
{
    Structura::Shell::ensureDefaultMaterials(m_extras.shell);
    int id = 1;
    for (const Object &other : m_extras.shell.objects) {
        id = std::max(id, other.id + 1);
    }
    o.id = id;
    o.name = o.name.empty() ? (shellKindLabel(static_cast<int>(o.kind)) + QStringLiteral(" %1").arg(id)).toStdString() : o.name;
    if (!askShellProperties(this, m_extras.shell, o, m_lastShellThickness, m_lastShellMaterial, undoText)) {
        return false;
    }
    bool autoCoupled = false;
    {
        StateGuard guard(this, undoText);
        m_extras.shell.objects.push_back(o);
        // projeto com barras: a casca nasce acoplada, para que suas cargas cheguem as vigas (quinhoes) na analise
        if (!m_extras.shell.coupled && m_sceneController && !m_sceneController->bars().empty()) {
            m_extras.shell.coupled = true;
            autoCoupled = true;
        }
        m_selectedShellObject = id;
        m_shellResultsShown = false;
        refreshShellVisuals(true);
        if (m_shellDialog) {
            m_shellDialog->setModel(m_extras.shell);
            m_shellDialog->selectObjectId(id);
        }
        markModified();
    }
    showStatusMessage(autoCoupled ? tr("%1 criada e acoplada às barras (as cargas da casca chegam às vigas). Use \"Dividir barras nos nós\" para ligar as vigas de borda.").arg(QString::fromStdString(o.name))
                                  : tr("%1 criada. Use o editor de cascas para cargas, apoios, análise e verificação.").arg(QString::fromStdString(o.name)), 8000);
    return true;
}

void MainWindow::restartShellStage()
{
    m_shellDraft = ShellDraft {};
    if (!m_placement) {
        return;
    }
    using Mode = Structura::UI::PlacementController::Mode;
    switch (m_command) {
    case Command::ShellPolygon:
    case Command::ShellExtrude:
    case Command::ShellRevolve: m_placement->start(Mode::Polyline); break;
    case Command::ShellRectangle: m_placement->start(Mode::Rectangle); break;
    case Command::ShellDisk:
    case Command::ShellCylinder: m_placement->start(Mode::Circle); break;
    default: break;
    }
    updateStatus();
}

bool MainWindow::onShellShape(int modeValue, const std::vector<std::array<double, 3>> &points, bool closed)
{
    using Mode = Structura::UI::PlacementController::Mode;
    const Mode mode = static_cast<Mode>(modeValue);
    auto *construction = m_sceneController->constructionScene();
    if (!construction) {
        return false;
    }
    const double upm = shellUnitsPerMeter();
    const auto axes = Structura::Construction::planeAxes(construction->planeMode());
    const std::size_t u = static_cast<std::size_t>(axes.u), v = static_cast<std::size_t>(axes.v), n = static_cast<std::size_t>(axes.normal);
    std::vector<ShellVec> meters;
    meters.reserve(points.size());
    for (const auto &p : points) {
        meters.push_back(scaled(p, 1.0 / upm));
    }
    (void)u;
    (void)v;
    const Structura::Shell::PlaneSpec plane {axes.u, axes.v, axes.normal};
    (void)n;
    auto segmentsOf = [&](const std::vector<ShellVec> &pts, bool close) {
        std::vector<std::pair<Structura::Construction::Vec3, Structura::Construction::Vec3>> segs;
        for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
            segs.push_back({scaled(pts[i], upm), scaled(pts[i + 1], upm)});
        }
        if (close && pts.size() > 2) {
            segs.push_back({scaled(pts.back(), upm), scaled(pts.front(), upm)});
        }
        return segs;
    };

    switch (m_command) {
    case Command::ShellPolygon: {
        const auto o = mode == Mode::Polyline ? Structura::Shell::makePlate(meters, plane) : std::nullopt;
        if (!o) {
            showStatusMessage(tr("Uma chapa precisa de ao menos 3 pontos não alinhados."), 3000);
            return false;
        }
        commitShell(*o, tr("Nova chapa"));
        return true;   // a polilinha e descartada mesmo se o usuario cancelou o dialogo
    }
    case Command::ShellRectangle: {
        const auto o = (mode == Mode::Rectangle && meters.size() >= 2) ? Structura::Shell::makeRectangle(meters[0], meters[1], plane) : std::nullopt;
        if (!o) {
            showStatusMessage(tr("Os dois cantos devem diferir nas duas direções do plano."), 3000);
            return false;
        }
        commitShell(*o, tr("Nova chapa retangular"));
        return true;
    }
    case Command::ShellDisk: {
        const auto o = (mode == Mode::Circle && meters.size() >= 2) ? Structura::Shell::makeDisk(meters[0], meters[1], plane) : std::nullopt;
        if (!o) {
            showStatusMessage(tr("O raio deve ser maior que zero."), 3000);
            return false;
        }
        commitShell(*o, tr("Novo disco"));
        return true;
    }
    case Command::ShellExtrude: {
        if (mode != Mode::Polyline || meters.size() < 2) {
            showStatusMessage(tr("A linha de extrusão precisa de ao menos 2 pontos."), 3000);
            return false;
        }
        m_shellDraft.stage = 1;
        m_shellDraft.points = meters;
        m_shellDraft.closed = closed;
        m_placement->setGhostSource(segmentsOf(meters, closed));
        m_placement->start(Mode::Extrude);
        updateStatus();
        return true;
    }
    case Command::ShellCylinder: {
        if (mode != Mode::Circle || meters.size() < 2) {
            return false;
        }
        const auto uu = static_cast<std::size_t>(plane.u), vv = static_cast<std::size_t>(plane.v);
        const double radius = std::hypot(meters[1][uu] - meters[0][uu], meters[1][vv] - meters[0][vv]);
        if (radius < 1e-6) {
            showStatusMessage(tr("O raio deve ser maior que zero."), 3000);
            return false;
        }
        m_shellDraft.stage = 1;
        m_shellDraft.points = {meters[0]};
        m_shellDraft.radius = radius;
        std::vector<std::pair<Structura::Construction::Vec3, Structura::Construction::Vec3>> ring;
        ShellVec previous = meters[0];
        for (int i = 0; i <= 64; ++i) {
            ShellVec pt = meters[0];
            pt[uu] += radius * std::cos(2.0 * kPi * i / 64.0);
            pt[vv] += radius * std::sin(2.0 * kPi * i / 64.0);
            if (i > 0) {
                ring.push_back({scaled(previous, upm), scaled(pt, upm)});
            }
            previous = pt;
        }
        m_placement->setGhostSource(ring);
        m_placement->start(Mode::Extrude);
        updateStatus();
        return true;
    }
    case Command::ShellRevolve: {
        if (mode != Mode::Polyline || meters.size() < 2) {
            showStatusMessage(tr("O perfil precisa de ao menos 2 pontos."), 3000);
            return false;
        }
        m_shellDraft.stage = 1;
        m_shellDraft.points = meters;
        m_placement->start(Mode::Axis);
        updateStatus();
        return true;
    }
    default:
        return false;
    }
}

bool MainWindow::applyShellDisplacement(int modeValue, const std::array<double, 3> &from, const std::array<double, 3> &to)
{
    using Mode = Structura::UI::PlacementController::Mode;
    const Mode mode = static_cast<Mode>(modeValue);
    const double upm = shellUnitsPerMeter();
    const ShellVec delta = scaled(sub(to, from), 1.0 / upm);
    if (length(delta) < 1e-9) {
        showStatusMessage(tr("Os dois pontos devem ser diferentes."), 3000);
        return false;
    }
    if (mode == Mode::Extrude) {
        const auto o = m_command == Command::ShellCylinder ? Structura::Shell::makeCylinder(m_shellDraft.points.front(), m_shellDraft.radius, delta)
                                                           : Structura::Shell::makeExtrusion(m_shellDraft.points, m_shellDraft.closed, delta);
        if (!o) {
            return false;
        }
        commitShell(*o, m_command == Command::ShellCylinder ? tr("Novo cilindro / cone") : tr("Nova extrusão"));
        QTimer::singleShot(0, this, [this]() { restartShellStage(); });
        return true;
    }
    if (mode == Mode::Axis) {
        const auto o = Structura::Shell::makeRevolution(m_shellDraft.points, scaled(from, 1.0 / upm), delta);
        if (!o) {
            return false;
        }
        commitShell(*o, tr("Nova revolução"));
        QTimer::singleShot(0, this, [this]() { restartShellStage(); });
        return true;
    }
    // mover / copiar a casca selecionada
    if (m_selectedShellObject < 0) {
        return false;
    }
    StateGuard guard(this, mode == Mode::Copy ? tr("Copiar casca") : tr("Mover casca"));
    int copyId = 0;
    if (mode == Mode::Copy) {
        copyId = 1;
        for (const Object &other : m_extras.shell.objects) {
            copyId = std::max(copyId, other.id + 1);
        }
    }
    if (!Structura::Shell::translateObject(m_extras.shell, m_selectedShellObject, delta, copyId)) {
        return false;
    }
    if (copyId > 0) {
        m_selectedShellObject = copyId;
    }
    m_shellResultsShown = false;
    refreshShellVisuals(true);
    if (m_shellDialog) {
        m_shellDialog->setModel(m_extras.shell);
    }
    markModified();
    return true;
}

void MainWindow::deleteSelectedShell()
{
    if (m_selectedShellObject < 0) {
        return;
    }
    StateGuard guard(this, tr("Excluir casca"));
    auto &model = m_extras.shell;
    const int id = m_selectedShellObject;
    model.objects.erase(std::remove_if(model.objects.begin(), model.objects.end(), [id](const Object &o) { return o.id == id; }), model.objects.end());
    model.loads.erase(std::remove_if(model.loads.begin(), model.loads.end(), [id](const Structura::Shell::Load &l) { return l.object == id; }), model.loads.end());
    model.supports.erase(std::remove_if(model.supports.begin(), model.supports.end(), [id](const Structura::Shell::Support &s) { return s.object == id; }), model.supports.end());
    m_selectedShellObject = -1;
    m_shellResultsShown = false;
    refreshShellVisuals(true);
    if (m_shellDialog) {
        m_shellDialog->setModel(model);
    }
    markModified();
    showStatusMessage(tr("Casca excluída."), 3000);
}

void MainWindow::flipSelectedShell()
{
    if (m_selectedShellObject < 0) {
        return;
    }
    StateGuard guard(this, tr("Inverter normal da casca"));
    for (Object &o : m_extras.shell.objects) {
        if (o.id != m_selectedShellObject) {
            continue;
        }
        if (o.kind == Kind::Plate) {
            std::reverse(o.polygon.begin(), o.polygon.end());
        } else {
            o.p["flip"] = o.param("flip") != 0.0 ? 0.0 : 1.0;
        }
    }
    m_shellResultsShown = false;
    refreshShellVisuals(true);
    if (m_shellDialog) {
        m_shellDialog->setModel(m_extras.shell);
    }
    markModified();
}

void MainWindow::showShellContextMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    menu.addAction(tr("Editor de cascas (propriedades, cargas, apoios, análise)…"), this, [this]() {
        onShellEditor();
        if (m_shellDialog) {
            m_shellDialog->selectObjectId(m_selectedShellObject);
        }
    });
    menu.addAction(tr("Inverter normal"), this, [this]() { flipSelectedShell(); });
    menu.addSeparator();
    menu.addAction(tr("Mover"), this, [this]() { setCommand(Command::ShellMove); });
    menu.addAction(tr("Copiar"), this, [this]() { setCommand(Command::ShellCopy); });
    menu.addSeparator();
    menu.addAction(tr("Excluir"), this, [this]() { deleteSelectedShell(); });
    menu.exec(globalPos);
}
