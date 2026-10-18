#include "ShellDialog.h"

#include "app/ShellSolverBridge.h"
#include "shell/ShellJob.h"
#include "shell/ShellMesh.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <set>

using namespace Structura::Shell;

namespace Structura::UI {

namespace {

QDoubleSpinBox *makeSpin(double min, double max, double value, int decimals, double step, const QString &suffix = QString())
{
    auto *box = new QDoubleSpinBox;
    box->setRange(min, max);
    box->setDecimals(decimals);
    box->setSingleStep(step);
    box->setValue(value);
    if (!suffix.isEmpty()) {
        box->setSuffix(suffix);
    }
    box->setKeyboardTracking(false);
    return box;
}

struct Param {
    const char *key;
    const char *label;
    double min, max, def;
    int decimals;
    const char *suffix;
    double scale;   ///< exibicao = valor SI x scale
};

std::vector<Param> paramsOf(const Object &object)
{
    switch (object.kind) {
    case Kind::Plate:
        return {{"t", "Espessura", 1, 3000, 200, 1, " mm", 1000.0}};
    case Kind::RectTank:
        return {{"lx", "Comprimento X", 0.2, 500, 4, 2, " m", 1.0},  {"ly", "Comprimento Y", 0.2, 500, 3, 2, " m", 1.0}, {"h", "Altura", 0.2, 200, 2.5, 2, " m", 1.0},
                {"tWall", "Espessura das paredes", 1, 3000, 200, 1, " mm", 1000.0}, {"tBottom", "Espessura do fundo", 1, 3000, 250, 1, " mm", 1000.0},
                {"tRoof", "Espessura da tampa (0 = aberto)", 0, 3000, 0, 1, " mm", 1000.0}, {"x0", "Canto X", -1e4, 1e4, 0, 3, " m", 1.0},
                {"y0", "Canto Y", -1e4, 1e4, 0, 3, " m", 1.0}, {"z0", "Cota da base", -1e3, 1e3, 0, 3, " m", 1.0}};
    case Kind::CylTank:
        return {{"r", "Raio", 0.3, 200, 5, 2, " m", 1.0}, {"h", "Altura do costado", 0.3, 200, 8, 2, " m", 1.0}, {"courses", "Numero de virolas", 1, 8, 4, 0, "", 1.0},
                {"tBottom", "Espessura do fundo (0 = sem)", 0, 200, 8, 1, " mm", 1000.0}, {"roof", "Teto (0 aberto, 1 plano, 2 conico, 3 domo)", 0, 3, 0, 0, "", 1.0},
                {"roofRise", "Flecha do teto", 0.05, 50, 1, 2, " m", 1.0}, {"tRoof", "Espessura do teto", 1, 200, 5, 1, " mm", 1000.0},
                {"x0", "Centro X", -1e4, 1e4, 0, 3, " m", 1.0}, {"y0", "Centro Y", -1e4, 1e4, 0, 3, " m", 1.0}, {"z0", "Cota da base", -1e3, 1e3, 0, 3, " m", 1.0}};
    case Kind::Disk:
        return {{"t", "Espessura", 1, 3000, 200, 1, " mm", 1000.0}, {"r", "Raio externo", 0.01, 1e4, 1, 3, " m", 1.0}, {"rIn", "Raio interno (0 = disco)", 0, 1e4, 0, 3, " m", 1.0}};
    case Kind::Extrusion: {
        std::vector<Param> list = {{"t", "Espessura", 1, 3000, 200, 1, " mm", 1000.0}, {"vx", "Vetor de extrusao X", -1e4, 1e4, 0, 3, " m", 1.0},
                                   {"vy", "Vetor de extrusao Y", -1e4, 1e4, 0, 3, " m", 1.0}, {"vz", "Vetor de extrusao Z", -1e4, 1e4, 1, 3, " m", 1.0}};
        if (object.param("circle") != 0.0) {
            list.push_back({"r", "Raio do circulo", 0.01, 1e4, 1, 3, " m", 1.0});
            list.push_back({"endScale", "Escala da extremidade (1 cilindro, 0 cone)", 0, 10, 1, 3, "", 1.0});
        } else {
            list.push_back({"closed", "Linha fechada (0/1)", 0, 1, 0, 0, "", 1.0});
        }
        return list;
    }
    case Kind::Revolution:
        return {{"t", "Espessura", 1, 3000, 10, 1, " mm", 1000.0}, {"angle", "Angulo de revolucao", 1, 360, 360, 1, " graus", 1.0}, {"ax", "Eixo: ponto X", -1e4, 1e4, 0, 3, " m", 1.0},
                {"ay", "Eixo: ponto Y", -1e4, 1e4, 0, 3, " m", 1.0}, {"az", "Eixo: ponto Z", -1e4, 1e4, 0, 3, " m", 1.0}, {"dx", "Eixo: direcao X", -1, 1, 0, 3, "", 1.0},
                {"dy", "Eixo: direcao Y", -1, 1, 0, 3, "", 1.0}, {"dz", "Eixo: direcao Z", -1, 1, 1, 3, "", 1.0}};
    case Kind::Vessel:
        return {{"r", "Raio medio", 0.1, 50, 1, 3, " m", 1.0}, {"length", "Comprimento do cilindro", 0.1, 200, 3, 2, " m", 1.0}, {"t", "Espessura do casco", 1, 300, 12, 1, " mm", 1000.0},
                {"head", "Tampos (0 hemisferico, 1 elipsoidal 2:1, 2 plano)", 0, 2, 1, 0, "", 1.0}, {"tHead", "Espessura dos tampos", 1, 300, 12, 1, " mm", 1000.0},
                {"horizontal", "Horizontal (0 vertical, 1 horizontal)", 0, 1, 0, 0, "", 1.0}, {"skirt", "Altura da saia (vertical; 0 = sem)", 0, 50, 0, 2, " m", 1.0},
                {"tSkirt", "Espessura da saia", 1, 300, 12, 1, " mm", 1000.0}, {"x0", "Centro X", -1e4, 1e4, 0, 3, " m", 1.0}, {"y0", "Centro Y", -1e4, 1e4, 0, 3, " m", 1.0},
                {"z0", "Cota da base do cilindro", -1e3, 1e3, 0, 3, " m", 1.0}};
    }
    return {};
}

QString kindName(Kind k)
{
    switch (k) {
    case Kind::Plate: return QObject::tr("Chapa / placa / laje");
    case Kind::RectTank: return QObject::tr("Reservatório retangular");
    case Kind::CylTank: return QObject::tr("Tanque cilíndrico");
    case Kind::Vessel: return QObject::tr("Vaso de pressão");
    case Kind::Disk: return QObject::tr("Disco / anel");
    case Kind::Extrusion: return QObject::tr("Extrusão / cilindro");
    case Kind::Revolution: return QObject::tr("Superfície de revolução");
    }
    return {};
}

QString loadTypeName(Load::Type t)
{
    switch (t) {
    case Load::SelfWeight: return QObject::tr("Peso próprio");
    case Load::Pressure: return QObject::tr("Pressão uniforme");
    case Load::Hydrostatic: return QObject::tr("Líquido (hidrostática)");
    case Load::Thermal: return QObject::tr("Temperatura");
    case Load::Area: return QObject::tr("Carga de área (global)");
    }
    return {};
}

/// Fator de exibicao (SI -> unidade do usuario) de cada valor de carga.
double loadScale(Load::Type t, int index)
{
    switch (t) {
    case Load::Pressure: return index == 0 ? 1e-3 : 1.0;           // kPa
    case Load::Hydrostatic: return index == 0 ? 1e-3 : (index == 2 ? 1e-3 : 1.0);   // kN/m3, m, kPa
    case Load::Area: return index < 3 ? 1e-3 : 1.0;                // kN/m2
    default: return 1.0;
    }
}

QString loadHelp(Load::Type t)
{
    switch (t) {
    case Load::SelfWeight: return QObject::tr("V1 = fator do peso próprio (1,0 = peso total do material).");
    case Load::Pressure: return QObject::tr("V1 = pressão (kPa), positiva no sentido da normal da casca (para fora em tanques e vasos: pressão interna).");
    case Load::Hydrostatic: return QObject::tr("V1 = peso específico do líquido (kN/m³); V2 = cota da superfície livre (m, global); V3 = pressão do gás sobre o líquido (kPa).");
    case Load::Thermal: return QObject::tr("V1 = variação uniforme de temperatura (°C); V2 = gradiente na espessura (T da face + menos T da face −, °C).");
    case Load::Area: return QObject::tr("V1, V2, V3 = força por área nas direções X, Y, Z (kN/m²), distribuída nos nós; V4 ≠ 0: por área projetada em planta.");
    }
    return {};
}

QString zoneRatioText(double r)
{
    return QString::number(r, 'f', 2);
}

} // namespace

ShellDialog::ShellDialog(Model model, Callbacks callbacks, QString femLibrary, QWidget *parent)
    : QDialog(parent)
    , m_model(std::move(model))
    , m_callbacks(std::move(callbacks))
    , m_femLibrary(std::move(femLibrary))
{
    setWindowTitle(tr("Cascas e chapas — placas, lajes, reservatórios, tanques e vasos de pressão"));
    setModal(false);
    resize(1180, 760);
    ensurePresets();
    buildUi();
    refreshObjectList();
    refreshMaterialTable();
    refreshLoadTable();
    refreshSupportTable();
    if (!m_model.objects.empty()) {
        m_objects->setCurrentRow(0);
    }
}

void ShellDialog::setModel(Model model)
{
    m_model = std::move(model);
    ensurePresets();
    m_hasMesh = false;
    m_results = Results {};
    m_combined.clear();
    m_design = DesignOutput {};
    refreshObjectList();
    refreshMaterialTable();
    refreshLoadTable();
    refreshSupportTable();
    m_current = std::min(m_current, static_cast<int>(m_model.objects.size()) - 1);
    if (m_current < 0 && !m_model.objects.empty()) {
        m_current = 0;
    }
    if (m_current >= 0) {
        m_objects->setCurrentRow(m_current);
    }
    rebuildGeometryForm();
    if (m_meshInfo) {
        m_meshInfo->setText(tr("Modelo alterado: gere a malha e analise novamente."));
    }
}

void ShellDialog::selectObjectId(int id)
{
    for (int i = 0; i < static_cast<int>(m_model.objects.size()); ++i) {
        if (m_model.objects[static_cast<std::size_t>(i)].id == id) {
            m_objects->setCurrentRow(i);
            return;
        }
    }
}

void ShellDialog::ensurePresets()
{
    Structura::Shell::ensureDefaultMaterials(m_model);
}

// ---------------------------------------------------------------------------------------------------------------------
// Estrutura da janela
// ---------------------------------------------------------------------------------------------------------------------

void ShellDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    auto *split = new QSplitter(Qt::Horizontal);
    root->addWidget(split, 1);

    // lista de objetos
    auto *left = new QWidget;
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(new QLabel(tr("Objetos")));
    m_objects = new QListWidget;
    leftLayout->addWidget(m_objects, 1);
    auto *addButton = new QPushButton(tr("Adicionar ▾"));
    auto *menu = new QMenu(addButton);
    menu->addAction(kindName(Kind::Plate), this, [this]() { addObject(Kind::Plate); });
    menu->addAction(kindName(Kind::RectTank), this, [this]() { addObject(Kind::RectTank); });
    menu->addAction(kindName(Kind::CylTank), this, [this]() { addObject(Kind::CylTank); });
    menu->addAction(kindName(Kind::Vessel), this, [this]() { addObject(Kind::Vessel); });
    addButton->setMenu(menu);
    auto *duplicate = new QPushButton(tr("Duplicar"));
    auto *remove = new QPushButton(tr("Remover"));
    auto *row = new QHBoxLayout;
    row->addWidget(addButton);
    row->addWidget(duplicate);
    row->addWidget(remove);
    leftLayout->addLayout(row);
    split->addWidget(left);

    m_tabs = new QTabWidget;
    m_tabs->addTab(buildGeometryTab(), tr("Geometria"));
    m_tabs->addTab(buildMaterialsTab(), tr("Materiais"));
    m_tabs->addTab(buildLoadsTab(), tr("Cargas"));
    m_tabs->addTab(buildSupportsTab(), tr("Apoios"));
    m_tabs->addTab(buildResultsTab(), tr("Análise e resultados"));
    m_tabs->addTab(buildDesignTab(), tr("Verificação"));
    split->addWidget(m_tabs);
    split->setStretchFactor(1, 1);
    split->setSizes({230, 950});

    auto *bottom = new QHBoxLayout;
    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_meshInfo = new QLabel;
    auto *mesh = new QPushButton(tr("Gerar malha"));
    auto *run = new QPushButton(tr("Analisar"));
    run->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 16px; border-radius: 4px; }"));
    auto *close = new QPushButton(tr("Fechar"));
    bottom->addWidget(m_meshInfo, 1);
    bottom->addWidget(mesh);
    bottom->addWidget(run);
    bottom->addWidget(close);
    root->addWidget(m_status);
    root->addLayout(bottom);

    connect(m_objects, &QListWidget::currentRowChanged, this, [this](int r) { selectObject(r); });
    connect(duplicate, &QPushButton::clicked, this, [this]() { duplicateObject(); });
    connect(remove, &QPushButton::clicked, this, [this]() { removeObject(); });
    connect(mesh, &QPushButton::clicked, this, [this]() { generateMesh(); });
    connect(run, &QPushButton::clicked, this, [this]() { analyze(); });
    connect(close, &QPushButton::clicked, this, &QDialog::close);
}

QWidget *ShellDialog::buildGeometryTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    m_geometryHost = new QWidget;
    m_geometryForm = new QFormLayout(m_geometryHost);
    layout->addWidget(m_geometryHost);
    layout->addStretch(1);
    return page;
}

void ShellDialog::refreshObjectList()
{
    const int keep = m_objects->currentRow();
    m_building = true;
    m_objects->clear();
    for (const Object &o : m_model.objects) {
        m_objects->addItem(QStringLiteral("%1  ·  %2").arg(QString::fromStdString(o.name), kindName(o.kind)));
    }
    m_building = false;
    if (keep >= 0 && keep < m_objects->count()) {
        m_objects->setCurrentRow(keep);
    }
}

void ShellDialog::selectObject(int row)
{
    if (m_building) {
        return;
    }
    m_current = (row >= 0 && row < static_cast<int>(m_model.objects.size())) ? row : -1;
    rebuildGeometryForm();
    if (m_callbacks.selected) {
        m_callbacks.selected(m_current >= 0 ? m_model.objects[static_cast<std::size_t>(m_current)].id : -1);
    }
}

void ShellDialog::rebuildGeometryForm()
{
    m_building = true;
    // os widgets antigos sao destruidos depois do retorno do sinal que os acionou (evita apagar o remetente)
    while (m_geometryForm->rowCount() > 0) {
        QFormLayout::TakeRowResult row = m_geometryForm->takeRow(0);
        auto discard = [](QLayoutItem *item) {
            if (!item) {
                return;
            }
            if (QWidget *w = item->widget()) {
                w->hide();
                w->deleteLater();
            } else if (QLayout *l = item->layout()) {
                while (QLayoutItem *child = l->takeAt(0)) {
                    if (QWidget *cw = child->widget()) {
                        cw->hide();
                        cw->deleteLater();
                    }
                    delete child;
                }
                l->deleteLater();
            }
            delete item;
        };
        discard(row.labelItem);
        discard(row.fieldItem);
    }
    if (m_current < 0) {
        m_geometryForm->addRow(new QLabel(tr("Adicione um objeto (chapa, reservatório, tanque ou vaso) para começar.")));
        m_building = false;
        return;
    }
    Object &o = m_model.objects[static_cast<std::size_t>(m_current)];
    auto *name = new QLineEdit(QString::fromStdString(o.name));
    m_geometryForm->addRow(tr("Nome"), name);
    connect(name, &QLineEdit::editingFinished, this, [this, name]() {
        if (m_current < 0) {
            return;
        }
        m_model.objects[static_cast<std::size_t>(m_current)].name = name->text().toStdString();
        refreshObjectList();
        refreshLoadTable();
        refreshSupportTable();
        notifyChanged();
    });
    m_geometryForm->addRow(tr("Tipo"), new QLabel(kindName(o.kind)));
    auto *material = new QComboBox;
    for (const Material &m : m_model.materials) {
        material->addItem(QString::fromStdString(m.name));
    }
    material->setCurrentIndex(std::clamp(o.material, 0, static_cast<int>(m_model.materials.size()) - 1));
    m_geometryForm->addRow(tr("Material"), material);
    connect(material, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_building || m_current < 0) {
            return;
        }
        m_model.objects[static_cast<std::size_t>(m_current)].material = i;
        rebuildGeometryForm();
        notifyChanged();
    });
    auto *mesh = makeSpin(0.01, 20.0, o.meshSize, 3, 0.05, tr(" m"));
    mesh->setToolTip(tr("Dimensão alvo dos elementos. Placas de 4 vértices usam malha estruturada; polígonos gerais são triangulados e subdivididos em quadriláteros."));
    m_geometryForm->addRow(tr("Dimensão da malha"), mesh);
    connect(mesh, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (m_building || m_current < 0) {
            return;
        }
        m_model.objects[static_cast<std::size_t>(m_current)].meshSize = v;
        notifyChanged();
    });
    auto *visible = new QCheckBox(tr("Incluir na análise e na vista"));
    visible->setChecked(o.visible);
    m_geometryForm->addRow(QString(), visible);
    connect(visible, &QCheckBox::toggled, this, [this](bool on) {
        if (m_building || m_current < 0) {
            return;
        }
        m_model.objects[static_cast<std::size_t>(m_current)].visible = on;
        notifyChanged();
    });

    const int courses = std::max(1, static_cast<int>(o.param("courses", 1.0) + 0.5));
    for (const Param &p : paramsOf(o)) {
        const QString key = QString::fromLatin1(p.key);
        const double stored = o.param(p.key, p.def / p.scale);
        auto *spin = makeSpin(p.min, p.max, stored * p.scale, p.decimals, p.decimals == 0 ? 1.0 : std::max(0.01, (p.max - p.min) / 2000.0), QString::fromUtf8(p.suffix));
        m_geometryForm->addRow(tr(p.label), spin);
        o.p.emplace(p.key, p.def / p.scale);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, key, p](double v) {
            if (m_building || m_current < 0) {
                return;
            }
            Object &obj = m_model.objects[static_cast<std::size_t>(m_current)];
            obj.p[key.toStdString()] = v / p.scale;
            notifyChanged();
            if (key == QLatin1String("courses")) {
                rebuildGeometryForm();
            }
        });
    }
    if (o.kind == Kind::CylTank) {
        for (int c = 1; c <= courses; ++c) {
            const std::string key = "t" + std::to_string(c);
            auto *spin = makeSpin(1.0, 200.0, o.param(key, o.param("t1", 0.008)) * 1000.0, 1, 0.5, tr(" mm"));
            m_geometryForm->addRow(tr("Espessura da virola %1 (de baixo para cima)").arg(c), spin);
            connect(spin, &QDoubleSpinBox::valueChanged, this, [this, key](double v) {
                if (m_building || m_current < 0) {
                    return;
                }
                m_model.objects[static_cast<std::size_t>(m_current)].p[key] = v / 1000.0;
                notifyChanged();
            });
        }
    }
    if (o.kind == Kind::Plate || o.kind == Kind::Extrusion || o.kind == Kind::Revolution || o.kind == Kind::Disk) {
        auto *table = new QTableWidget(static_cast<int>(o.polygon.size()), 3);
        table->setHorizontalHeaderLabels({tr("X (m)"), tr("Y (m)"), tr("Z (m)")});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setMinimumHeight(170);
        for (int r = 0; r < table->rowCount(); ++r) {
            for (int c = 0; c < 3; ++c) {
                table->setItem(r, c, new QTableWidgetItem(QString::number(o.polygon[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)], 'g', 8)));
            }
        }
        m_geometryForm->addRow(o.kind == Kind::Plate ? tr("Vértices (anti-horário vistos do lado da normal positiva)") : tr("Pontos definidores (m)"), table);
        connect(table, &QTableWidget::itemChanged, this, [this, table](QTableWidgetItem *item) {
            if (m_building || m_current < 0) {
                return;
            }
            bool ok = false;
            const double v = item->text().toDouble(&ok);
            if (!ok) {
                return;
            }
            m_model.objects[static_cast<std::size_t>(m_current)].polygon[static_cast<std::size_t>(item->row())][static_cast<std::size_t>(item->column())] = v;
            (void)table;
            notifyChanged();
        });
        auto *buttons = new QHBoxLayout;
        auto *addVertex = new QPushButton(tr("+ vértice"));
        auto *removeVertex = new QPushButton(tr("− vértice"));
        auto *rectangle = new QPushButton(tr("Retângulo…"));
        auto *flip = new QPushButton(tr("Inverter normal"));
        for (auto *b : {addVertex, removeVertex, rectangle, flip}) {
            buttons->addWidget(b);
        }
        rectangle->setVisible(o.kind == Kind::Plate);
        buttons->addStretch(1);
        m_geometryForm->addRow(QString(), buttons);
        connect(addVertex, &QPushButton::clicked, this, [this]() {
            if (m_current < 0) {
                return;
            }
            auto &poly = m_model.objects[static_cast<std::size_t>(m_current)].polygon;
            poly.push_back(poly.empty() ? Vec3 {0, 0, 0} : poly.back());
            notifyChanged();
            rebuildGeometryForm();
        });
        connect(removeVertex, &QPushButton::clicked, this, [this, table]() {
            if (m_current < 0) {
                return;
            }
            auto &poly = m_model.objects[static_cast<std::size_t>(m_current)].polygon;
            const int row = table->currentRow() >= 0 ? table->currentRow() : static_cast<int>(poly.size()) - 1;
            const std::size_t minimum = m_model.objects[static_cast<std::size_t>(m_current)].kind == Kind::Plate ? 3 : 1;
            if (poly.size() > minimum && row >= 0 && row < static_cast<int>(poly.size())) {
                poly.erase(poly.begin() + row);
                notifyChanged();
                rebuildGeometryForm();
            }
        });
        connect(flip, &QPushButton::clicked, this, [this]() {
            if (m_current < 0) {
                return;
            }
            Object &obj = m_model.objects[static_cast<std::size_t>(m_current)];
            if (obj.kind == Kind::Plate) {
                std::reverse(obj.polygon.begin(), obj.polygon.end());
            } else {
                obj.p["flip"] = obj.param("flip") != 0.0 ? 0.0 : 1.0;
            }
            notifyChanged();
            rebuildGeometryForm();
        });
        connect(rectangle, &QPushButton::clicked, this, [this]() {
            if (m_current < 0) {
                return;
            }
            QDialog dlg(this);
            dlg.setWindowTitle(tr("Retângulo horizontal"));
            auto *form = new QFormLayout(&dlg);
            auto *x0 = makeSpin(-1e4, 1e4, 0, 3, 0.5, tr(" m"));
            auto *y0 = makeSpin(-1e4, 1e4, 0, 3, 0.5, tr(" m"));
            auto *z0 = makeSpin(-1e3, 1e3, 0, 3, 0.5, tr(" m"));
            auto *lx = makeSpin(0.1, 1e3, 4, 3, 0.5, tr(" m"));
            auto *ly = makeSpin(0.1, 1e3, 3, 3, 0.5, tr(" m"));
            form->addRow(tr("Canto X"), x0);
            form->addRow(tr("Canto Y"), y0);
            form->addRow(tr("Cota Z"), z0);
            form->addRow(tr("Comprimento X"), lx);
            form->addRow(tr("Comprimento Y"), ly);
            auto *ok = new QPushButton(tr("Aplicar"));
            form->addRow(ok);
            connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
            if (dlg.exec() != QDialog::Accepted) {
                return;
            }
            auto &poly = m_model.objects[static_cast<std::size_t>(m_current)].polygon;
            poly = {{x0->value(), y0->value(), z0->value()},
                    {x0->value() + lx->value(), y0->value(), z0->value()},
                    {x0->value() + lx->value(), y0->value() + ly->value(), z0->value()},
                    {x0->value(), y0->value() + ly->value(), z0->value()}};
            notifyChanged();
            rebuildGeometryForm();
        });
    }
    if (m_model.materials[static_cast<std::size_t>(std::clamp(o.material, 0, static_cast<int>(m_model.materials.size()) - 1))].concrete) {
        struct C {
            const char *key;
            const char *label;
            double def, scale, min, max;
            const char *suffix;
        };
        const C list[] = {{"cover", "Cobrimento (NBR 6118, Tabela 7.2)", 0.03, 1000.0, 10, 100, " mm"}, {"bar", "Diâmetro adotado das barras", 0.01, 1000.0, 5, 32, " mm"},
                          {"fyk", "fyk do aço da armadura (CA-50)", 500.0e6, 1.0e-6, 250, 600, " MPa"}};
        for (const C &c : list) {
            auto *spin = makeSpin(c.min, c.max, o.param(c.key, c.def) * c.scale, 1, 1.0, QString::fromUtf8(c.suffix));
            m_geometryForm->addRow(tr(c.label), spin);
            const std::string key = c.key;
            const double scale = c.scale;
            connect(spin, &QDoubleSpinBox::valueChanged, this, [this, key, scale](double v) {
                if (m_building || m_current < 0) {
                    return;
                }
                m_model.objects[static_cast<std::size_t>(m_current)].p[key] = v / scale;
                notifyChanged();
            });
        }
    }
    m_building = false;
}

void ShellDialog::addObject(Kind kind)
{
    Object o;
    int id = 1;
    for (const Object &other : m_model.objects) {
        id = std::max(id, other.id + 1);
    }
    o.id = id;
    o.kind = kind;
    o.name = kindName(kind).toStdString() + " " + std::to_string(id);
    switch (kind) {
    case Kind::Plate:
        o.polygon = {{0, 0, 0}, {4, 0, 0}, {4, 3, 0}, {0, 3, 0}};
        o.meshSize = 0.25;
        o.material = std::max(0, static_cast<int>(m_model.materials.size()) - 1);   // laje: concreto
        break;
    case Kind::RectTank:
        o.meshSize = 0.4;
        o.material = std::max(0, static_cast<int>(m_model.materials.size()) - 1);
        break;
    case Kind::CylTank:
        o.meshSize = 0.6;
        o.material = 0;
        break;
    case Kind::Vessel:
        o.meshSize = 0.15;
        o.material = std::min(2, static_cast<int>(m_model.materials.size()) - 1);
        break;
    }
    for (const Param &p : paramsOf(o)) {
        o.p[p.key] = p.def / p.scale;
    }
    if (kind == Kind::CylTank) {
        for (int c = 1; c <= 8; ++c) {
            o.p["t" + std::to_string(c)] = (0.016 - 0.0015 * (c - 1));
        }
        o.p["roof"] = 0.0;
    }
    m_model.objects.push_back(o);
    refreshObjectList();
    m_objects->setCurrentRow(static_cast<int>(m_model.objects.size()) - 1);
    refreshLoadTable();
    refreshSupportTable();
    notifyChanged();
}

void ShellDialog::duplicateObject()
{
    if (m_current < 0) {
        return;
    }
    Object o = m_model.objects[static_cast<std::size_t>(m_current)];
    int id = 1;
    for (const Object &other : m_model.objects) {
        id = std::max(id, other.id + 1);
    }
    o.id = id;
    o.name += " (cópia)";
    m_model.objects.push_back(o);
    refreshObjectList();
    m_objects->setCurrentRow(static_cast<int>(m_model.objects.size()) - 1);
    notifyChanged();
}

void ShellDialog::removeObject()
{
    if (m_current < 0) {
        return;
    }
    const int id = m_model.objects[static_cast<std::size_t>(m_current)].id;
    m_model.objects.erase(m_model.objects.begin() + m_current);
    m_model.loads.erase(std::remove_if(m_model.loads.begin(), m_model.loads.end(), [id](const Load &l) { return l.object == id; }), m_model.loads.end());
    m_model.supports.erase(std::remove_if(m_model.supports.begin(), m_model.supports.end(), [id](const Support &s) { return s.object == id; }), m_model.supports.end());
    m_current = -1;
    refreshObjectList();
    refreshLoadTable();
    refreshSupportTable();
    if (m_objects->count() > 0) {
        m_objects->setCurrentRow(0);
    } else {
        rebuildGeometryForm();
    }
    notifyChanged();
}

void ShellDialog::notifyChanged()
{
    // qualquer edicao invalida a malha e os resultados
    m_hasMesh = false;
    m_results = Results {};
    m_combined.clear();
    m_design = DesignOutput {};
    if (m_meshInfo) {
        m_meshInfo->setText(tr("Modelo alterado: gere a malha e analise novamente."));
    }
    if (m_callbacks.modelChanged) {
        m_callbacks.modelChanged(m_model);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Materiais
// ---------------------------------------------------------------------------------------------------------------------

QWidget *ShellDialog::buildMaterialsTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    m_materials = new QTableWidget(0, 8);
    m_materials->setHorizontalHeaderLabels({tr("Nome"), tr("Tipo"), tr("E (GPa)"), tr("ν"), tr("γ (kN/m³)"), tr("α (10⁻⁶/°C)"), tr("fy / fck (MPa)"), tr("fu (MPa)")});
    m_materials->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_materials, 1);
    auto *buttons = new QHBoxLayout;
    auto *addSteel = new QPushButton(tr("+ Aço"));
    auto *addConcrete = new QPushButton(tr("+ Concreto"));
    auto *remove = new QPushButton(tr("Remover"));
    buttons->addWidget(addSteel);
    buttons->addWidget(addConcrete);
    buttons->addWidget(remove);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    auto *note = new QLabel(tr("Concreto: E = αi · 5600 √fck (Ecs, NBR 6118:2023, 8.2.8); a coluna fy/fck guarda o fck. Aço: fy e fu da chapa (NBR 8800 / especificação do vaso)."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:#5a6472;"));
    layout->addWidget(note);
    connect(addSteel, &QPushButton::clicked, this, [this]() {
        Material m;
        m.name = "Aco novo";
        m_model.materials.push_back(m);
        refreshMaterialTable();
        rebuildGeometryForm();
        notifyChanged();
    });
    connect(addConcrete, &QPushButton::clicked, this, [this]() {
        Material m;
        m.name = "Concreto novo";
        m.concrete = true;
        m.nu = 0.2;
        m.unitWeight = 25000.0;
        m.alpha = 1.0e-5;
        m.fy = 30.0e6;
        m.fu = 0.0;
        m.E = 0.85 * 5600.0 * std::sqrt(30.0) * 1.0e6;
        m_model.materials.push_back(m);
        refreshMaterialTable();
        rebuildGeometryForm();
        notifyChanged();
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        const int row = m_materials->currentRow();
        if (row < 1 || row >= static_cast<int>(m_model.materials.size())) {
            return;   // o primeiro material e o padrao
        }
        for (Object &o : m_model.objects) {
            if (o.material == row) {
                o.material = 0;
            } else if (o.material > row) {
                --o.material;
            }
        }
        m_model.materials.erase(m_model.materials.begin() + row);
        refreshMaterialTable();
        rebuildGeometryForm();
        notifyChanged();
    });
    connect(m_materials, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_building) {
            return;
        }
        const int row = item->row();
        if (row < 0 || row >= static_cast<int>(m_model.materials.size())) {
            return;
        }
        Material &m = m_model.materials[static_cast<std::size_t>(row)];
        if (item->column() == 0) {
            m.name = item->text().toStdString();
        } else {
            bool ok = false;
            const double v = item->text().toDouble(&ok);
            if (!ok) {
                return;
            }
            switch (item->column()) {
            case 2: m.E = v * 1e9; break;
            case 3: m.nu = std::clamp(v, 0.0, 0.499); break;
            case 4: m.unitWeight = v * 1e3; break;
            case 5: m.alpha = v * 1e-6; break;
            case 6: m.fy = v * 1e6; break;
            case 7: m.fu = v * 1e6; break;
            default: break;
            }
        }
        notifyChanged();
    });
    return page;
}

void ShellDialog::refreshMaterialTable()
{
    m_building = true;
    m_materials->setRowCount(static_cast<int>(m_model.materials.size()));
    for (int r = 0; r < static_cast<int>(m_model.materials.size()); ++r) {
        const Material &m = m_model.materials[static_cast<std::size_t>(r)];
        m_materials->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(m.name)));
        auto *type = new QTableWidgetItem(m.concrete ? tr("Concreto") : tr("Aço"));
        type->setFlags(type->flags() & ~Qt::ItemIsEditable);
        m_materials->setItem(r, 1, type);
        const double values[6] = {m.E / 1e9, m.nu, m.unitWeight / 1e3, m.alpha / 1e-6, m.fy / 1e6, m.fu / 1e6};
        for (int c = 0; c < 6; ++c) {
            m_materials->setItem(r, 2 + c, new QTableWidgetItem(QString::number(values[c], 'g', 6)));
        }
    }
    m_building = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Cargas
// ---------------------------------------------------------------------------------------------------------------------

QWidget *ShellDialog::buildLoadsTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    m_loads = new QTableWidget(0, 7);
    m_loads->setHorizontalHeaderLabels({tr("Caso de carga"), tr("Objeto"), tr("Tipo"), tr("V1"), tr("V2"), tr("V3"), tr("V4")});
    m_loads->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_loads, 1);
    m_loadHelp = new QLabel;
    m_loadHelp->setWordWrap(true);
    m_loadHelp->setStyleSheet(QStringLiteral("color:#5a6472;"));
    layout->addWidget(m_loadHelp);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(tr("+ Carga"));
    auto *remove = new QPushButton(tr("Remover"));
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    auto *note = new QLabel(tr("Os casos de carga são os do projeto (aba Modelo). Cada caso é resolvido separadamente; as combinações do projeto são formadas por superposição linear "
                               "dos resultados. Os coeficientes de ponderação vêm das combinações."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:#5a6472;"));
    layout->addWidget(note);
    connect(add, &QPushButton::clicked, this, [this]() {
        Load l;
        const QStringList cases = m_callbacks.caseNames ? m_callbacks.caseNames() : QStringList();
        l.caseName = cases.isEmpty() ? std::string("CASO1") : cases.first().toStdString();
        l.type = Load::SelfWeight;
        l.v = {1.0, 0.0, 0.0, 0.0};
        m_model.loads.push_back(l);
        refreshLoadTable();
        notifyChanged();
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        const int row = m_loads->currentRow();
        if (row >= 0 && row < static_cast<int>(m_model.loads.size())) {
            m_model.loads.erase(m_model.loads.begin() + row);
            refreshLoadTable();
            notifyChanged();
        }
    });
    connect(m_loads, &QTableWidget::currentCellChanged, this, [this](int row) {
        if (row >= 0 && row < static_cast<int>(m_model.loads.size())) {
            m_loadHelp->setText(loadHelp(m_model.loads[static_cast<std::size_t>(row)].type));
        }
    });
    return page;
}

void ShellDialog::refreshLoadTable()
{
    if (!m_loads) {
        return;
    }
    m_building = true;
    m_loads->setRowCount(static_cast<int>(m_model.loads.size()));
    const QStringList cases = m_callbacks.caseNames ? m_callbacks.caseNames() : QStringList();
    for (int r = 0; r < static_cast<int>(m_model.loads.size()); ++r) {
        const Load &l = m_model.loads[static_cast<std::size_t>(r)];
        auto *caseBox = new QComboBox;
        caseBox->setEditable(true);
        caseBox->addItems(cases);
        if (caseBox->findText(QString::fromStdString(l.caseName)) < 0) {
            caseBox->addItem(QString::fromStdString(l.caseName));
        }
        caseBox->setCurrentText(QString::fromStdString(l.caseName));
        m_loads->setCellWidget(r, 0, caseBox);
        connect(caseBox, &QComboBox::currentTextChanged, this, [this, r](const QString &t) {
            if (m_building || r >= static_cast<int>(m_model.loads.size())) {
                return;
            }
            m_model.loads[static_cast<std::size_t>(r)].caseName = t.toStdString();
            notifyChanged();
        });
        auto *objectBox = new QComboBox;
        objectBox->addItem(tr("Todos"), 0);
        for (const Object &o : m_model.objects) {
            objectBox->addItem(QString::fromStdString(o.name), o.id);
        }
        objectBox->setCurrentIndex(std::max(0, objectBox->findData(l.object)));
        m_loads->setCellWidget(r, 1, objectBox);
        connect(objectBox, &QComboBox::currentIndexChanged, this, [this, r, objectBox](int) {
            if (m_building || r >= static_cast<int>(m_model.loads.size())) {
                return;
            }
            m_model.loads[static_cast<std::size_t>(r)].object = objectBox->currentData().toInt();
            notifyChanged();
        });
        auto *typeBox = new QComboBox;
        for (const Load::Type t : {Load::SelfWeight, Load::Pressure, Load::Hydrostatic, Load::Thermal, Load::Area}) {
            typeBox->addItem(loadTypeName(t), static_cast<int>(t));
        }
        typeBox->setCurrentIndex(static_cast<int>(l.type));
        m_loads->setCellWidget(r, 2, typeBox);
        connect(typeBox, &QComboBox::currentIndexChanged, this, [this, r, typeBox](int) {
            if (m_building || r >= static_cast<int>(m_model.loads.size())) {
                return;
            }
            Load &load = m_model.loads[static_cast<std::size_t>(r)];
            load.type = static_cast<Load::Type>(typeBox->currentData().toInt());
            load.v = {load.type == Load::SelfWeight ? 1.0 : 0.0, 0.0, 0.0, 0.0};
            m_loadHelp->setText(loadHelp(load.type));
            QTimer::singleShot(0, this, [this]() { refreshLoadTable(); });
            notifyChanged();
        });
        for (int i = 0; i < 4; ++i) {
            auto *spin = makeSpin(-1e9, 1e9, l.v[static_cast<std::size_t>(i)] * loadScale(l.type, i), 4, 1.0);
            m_loads->setCellWidget(r, 3 + i, spin);
            connect(spin, &QDoubleSpinBox::valueChanged, this, [this, r, i](double v) {
                if (m_building || r >= static_cast<int>(m_model.loads.size())) {
                    return;
                }
                Load &load = m_model.loads[static_cast<std::size_t>(r)];
                load.v[static_cast<std::size_t>(i)] = v / loadScale(load.type, i);
                notifyChanged();
            });
        }
    }
    m_building = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Apoios
// ---------------------------------------------------------------------------------------------------------------------

QWidget *ShellDialog::buildSupportsTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    m_supports = new QTableWidget(0, 20);
    m_supports->setHorizontalHeaderLabels({tr("Objeto"), tr("Seletor"), tr("Eixo"), tr("Valor / X (m)"), tr("Y (m)"), tr("Z (m)"), tr("Tol (mm)"), tr("Pernas"), tr("Âng. ini (°)"),
                                           tr("Limite eixo"), tr("Limite (m)"), tr("Abaixo"), tr("UX"), tr("UY"), tr("UZ"), tr("RX"), tr("RY"), tr("RZ"), tr("Mola (MN/m³)"), tr("Só compr.")});
    layout->addWidget(m_supports, 1);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(tr("+ Apoio ▾"));
    auto *menu = new QMenu(add);
    auto preset = [this](const QString &label, std::function<void(Support &, const Object *)> fill) {
        return std::make_pair(label, fill);
    };
    const std::vector<std::pair<QString, std::function<void(Support &, const Object *)>>> presets = {
        preset(tr("Base fixa (plano z = cota da base)"), [](Support &s, const Object *o) {
            s.selector = Support::Plane;
            s.axis = 2;
            s.value = o ? o->param("z0") : 0.0;
            s.fixed = {1, 1, 1, 1, 1, 1};
        }),
        preset(tr("Fundação elástica só compressão (plano z)"), [](Support &s, const Object *o) {
            s.selector = Support::Plane;
            s.axis = 2;
            s.value = o ? o->param("z0") : 0.0;
            s.springPerArea = 5.0e7;
            s.compressionOnly = true;
            s.fixed = {0, 0, 0, 0, 0, 0};
        }),
        preset(tr("Bordas livres apoiadas (UZ restrito)"), [](Support &s, const Object *) {
            s.selector = Support::FreeEdge;
            s.fixed = {0, 0, 1, 0, 0, 0};
        }),
        preset(tr("Bordas livres engastadas"), [](Support &s, const Object *) {
            s.selector = Support::FreeEdge;
            s.fixed = {1, 1, 1, 1, 1, 1};
        }),
        preset(tr("Pernas pontuais (4, plano z)"), [](Support &s, const Object *o) {
            s.selector = Support::Plane;
            s.axis = 2;
            s.value = o ? o->param("z0") : 0.0;
            s.legs = 4;
            s.legStart = 45.0;
            s.fixed = {1, 1, 1, 0, 0, 0};
        }),
        preset(tr("Sela de vaso horizontal (plano x, metade inferior)"), [](Support &s, const Object *o) {
            s.selector = Support::Plane;
            s.axis = 0;
            s.value = o ? o->param("x0") + 0.5 : 0.0;
            s.limitAxis = 2;
            s.limitValue = o ? o->param("z0") - 0.5 * o->param("r") : 0.0;
            s.limitBelow = true;
            s.fixed = {0, 0, 1, 0, 0, 0};
        }),
        preset(tr("Ponto fixo (nó mais próximo)"), [](Support &s, const Object *) {
            s.selector = Support::Point;
            s.fixed = {1, 1, 1, 0, 0, 0};
        }),
    };
    for (const auto &[label, fill] : presets) {
        menu->addAction(label, this, [this, fill]() {
            Support s;
            const Object *o = m_current >= 0 ? &m_model.objects[static_cast<std::size_t>(m_current)] : nullptr;
            s.object = o ? o->id : 0;
            fill(s, o);
            m_model.supports.push_back(s);
            refreshSupportTable();
            notifyChanged();
        });
    }
    add->setMenu(menu);
    auto *remove = new QPushButton(tr("Remover"));
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    auto *note = new QLabel(tr("Seletor Plano: nós com a coordenada do eixo = valor (com 'Pernas' > 0, só os nós mais próximos de cada perna; 'Limite' restringe a um semiespaço, p.ex. sela na metade "
                               "inferior). Borda livre: nós de arestas com um único elemento. Ponto: nó mais próximo de (X, Y, Z). Mola > 0 substitui a restrição: apoio elástico por área "
                               "tributária; 'só compressão' resolve por iteração (fundação que não traciona)."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:#5a6472;"));
    layout->addWidget(note);
    connect(remove, &QPushButton::clicked, this, [this]() {
        const int row = m_supports->currentRow();
        if (row >= 0 && row < static_cast<int>(m_model.supports.size())) {
            m_model.supports.erase(m_model.supports.begin() + row);
            refreshSupportTable();
            notifyChanged();
        }
    });
    return page;
}

void ShellDialog::refreshSupportTable()
{
    if (!m_supports) {
        return;
    }
    m_building = true;
    m_supports->setRowCount(static_cast<int>(m_model.supports.size()));
    for (int r = 0; r < static_cast<int>(m_model.supports.size()); ++r) {
        const Support &s = m_model.supports[static_cast<std::size_t>(r)];
        auto edit = [this, r](std::function<void(Support &)> change) {
            if (m_building || r >= static_cast<int>(m_model.supports.size())) {
                return;
            }
            change(m_model.supports[static_cast<std::size_t>(r)]);
            notifyChanged();
        };
        auto *objectBox = new QComboBox;
        objectBox->addItem(tr("Todos"), 0);
        for (const Object &o : m_model.objects) {
            objectBox->addItem(QString::fromStdString(o.name), o.id);
        }
        objectBox->setCurrentIndex(std::max(0, objectBox->findData(s.object)));
        m_supports->setCellWidget(r, 0, objectBox);
        connect(objectBox, &QComboBox::currentIndexChanged, this, [edit, objectBox](int) { edit([objectBox](Support &x) { x.object = objectBox->currentData().toInt(); }); });
        auto *selector = new QComboBox;
        selector->addItems({tr("Plano"), tr("Borda livre"), tr("Ponto")});
        selector->setCurrentIndex(static_cast<int>(s.selector));
        m_supports->setCellWidget(r, 1, selector);
        connect(selector, &QComboBox::currentIndexChanged, this, [edit](int i) { edit([i](Support &x) { x.selector = static_cast<Support::Selector>(i); }); });
        auto *axis = new QComboBox;
        axis->addItems({QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")});
        axis->setCurrentIndex(std::clamp(s.axis, 0, 2));
        m_supports->setCellWidget(r, 2, axis);
        connect(axis, &QComboBox::currentIndexChanged, this, [edit](int i) { edit([i](Support &x) { x.axis = i; }); });
        auto numeric = [&](int col, double value, int decimals, std::function<void(Support &, double)> set) {
            auto *spin = makeSpin(-1e9, 1e9, value, decimals, 0.1);
            m_supports->setCellWidget(r, col, spin);
            connect(spin, &QDoubleSpinBox::valueChanged, this, [edit, set](double v) { edit([set, v](Support &x) { set(x, v); }); });
        };
        numeric(3, s.value, 3, [](Support &x, double v) { x.value = v; });
        numeric(4, s.value2, 3, [](Support &x, double v) { x.value2 = v; });
        numeric(5, s.value3, 3, [](Support &x, double v) { x.value3 = v; });
        numeric(6, s.tol * 1000.0, 3, [](Support &x, double v) { x.tol = v / 1000.0; });
        numeric(7, s.legs, 0, [](Support &x, double v) { x.legs = static_cast<int>(v); });
        numeric(8, s.legStart, 1, [](Support &x, double v) { x.legStart = v; });
        auto *limit = new QComboBox;
        limit->addItems({tr("—"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")});
        limit->setCurrentIndex(s.limitAxis + 1);
        m_supports->setCellWidget(r, 9, limit);
        connect(limit, &QComboBox::currentIndexChanged, this, [edit](int i) { edit([i](Support &x) { x.limitAxis = i - 1; }); });
        numeric(10, s.limitValue, 3, [](Support &x, double v) { x.limitValue = v; });
        auto *below = new QCheckBox;
        below->setChecked(s.limitBelow);
        m_supports->setCellWidget(r, 11, below);
        connect(below, &QCheckBox::toggled, this, [edit](bool on) { edit([on](Support &x) { x.limitBelow = on; }); });
        for (int c = 0; c < 6; ++c) {
            auto *check = new QCheckBox;
            check->setChecked(s.fixed[static_cast<std::size_t>(c)] != 0);
            m_supports->setCellWidget(r, 12 + c, check);
            connect(check, &QCheckBox::toggled, this, [edit, c](bool on) { edit([c, on](Support &x) { x.fixed[static_cast<std::size_t>(c)] = on ? 1 : 0; }); });
        }
        numeric(18, s.springPerArea / 1.0e6, 3, [](Support &x, double v) { x.springPerArea = v * 1.0e6; });
        auto *comp = new QCheckBox;
        comp->setChecked(s.compressionOnly);
        m_supports->setCellWidget(r, 19, comp);
        connect(comp, &QCheckBox::toggled, this, [edit](bool on) { edit([on](Support &x) { x.compressionOnly = on; }); });
    }
    m_supports->resizeColumnsToContents();
    m_building = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Resultados
// ---------------------------------------------------------------------------------------------------------------------

QWidget *ShellDialog::buildResultsTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;
    m_resultCase = new QComboBox;
    m_quantity = new QComboBox;
    for (const QuantityInfo &q : quantityList()) {
        m_quantity->addItem(QStringLiteral("%1 [%2]").arg(QString::fromUtf8(q.name), QString::fromUtf8(q.unit)), static_cast<int>(q.id));
    }
    m_quantity->setCurrentIndex(static_cast<int>(Quantity::StressVonMisesMax));
    m_deformScale = makeSpin(0.0, 1e7, 0.0, 1, 10.0, tr(" ×"));
    m_deformScale->setSpecialValueText(tr("sem deformada"));
    m_smooth = new QCheckBox(tr("Suavizar nos nós (dentro de cada zona)"));
    m_smooth->setChecked(true);
    m_edges = new QCheckBox(tr("Mostrar malha"));
    m_edges->setChecked(true);
    m_fixedRange = new QCheckBox(tr("Faixa fixa"));
    m_rangeMin = makeSpin(-1e12, 1e12, 0.0, 3, 1.0);
    m_rangeMax = makeSpin(-1e12, 1e12, 100.0, 3, 1.0);
    form->addRow(tr("Resultado"), m_resultCase);
    form->addRow(tr("Grandeza"), m_quantity);
    form->addRow(tr("Escala da deformada"), m_deformScale);
    auto *flags = new QHBoxLayout;
    flags->addWidget(m_smooth);
    flags->addWidget(m_edges);
    flags->addWidget(m_fixedRange);
    flags->addWidget(m_rangeMin);
    flags->addWidget(m_rangeMax);
    flags->addStretch(1);
    form->addRow(tr("Exibição"), flags);
    layout->addLayout(form);
    auto *legendRow = new QHBoxLayout;
    m_legend = new QLabel;
    m_legend->setFixedSize(300, 18);
    m_legendText = new QLabel;
    legendRow->addWidget(m_legend);
    legendRow->addWidget(m_legendText, 1);
    layout->addLayout(legendRow);
    m_summary = new QPlainTextEdit;
    m_summary->setReadOnly(true);
    layout->addWidget(m_summary, 1);
    auto *buttons = new QHBoxLayout;
    auto *show = new QPushButton(tr("Exibir / ocultar na vista 3D"));
    auto *csv = new QPushButton(tr("Exportar CSV…"));
    buttons->addWidget(show);
    buttons->addWidget(csv);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    auto refresh = [this]() { refreshView(); };
    connect(m_resultCase, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_quantity, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_deformScale, &QDoubleSpinBox::valueChanged, this, refresh);
    connect(m_smooth, &QCheckBox::toggled, this, refresh);
    connect(m_edges, &QCheckBox::toggled, this, refresh);
    connect(m_fixedRange, &QCheckBox::toggled, this, refresh);
    connect(m_rangeMin, &QDoubleSpinBox::valueChanged, this, refresh);
    connect(m_rangeMax, &QDoubleSpinBox::valueChanged, this, refresh);
    connect(show, &QPushButton::clicked, this, [this]() {
        static bool visible = true;
        visible = !visible;
        if (m_callbacks.setVisible) {
            m_callbacks.setVisible(visible);
        }
    });
    connect(csv, &QPushButton::clicked, this, [this]() { exportCsv(); });
    return page;
}

QStringList ShellDialog::shellCaseNames() const
{
    QStringList names;
    for (const Load &l : m_model.loads) {
        const QString n = QString::fromStdString(l.caseName);
        if (!names.contains(n)) {
            names << n;
        }
    }
    return names;
}

void ShellDialog::generateMesh()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_mesh = Structura::Shell::generateMesh(m_model);
    QApplication::restoreOverrideCursor();
    m_hasMesh = !m_mesh.empty();
    m_results = Results {};
    m_combined.clear();
    m_design = DesignOutput {};
    QString warnings;
    for (const std::string &w : m_mesh.warnings) {
        warnings += QStringLiteral("• %1\n").arg(QString::fromStdString(w));
    }
    if (!m_hasMesh) {
        m_meshInfo->setText(tr("Nenhum elemento gerado."));
        m_status->setText(warnings);
        return;
    }
    m_meshInfo->setText(tr("%1 elementos, %2 nós.").arg(m_mesh.quads.size()).arg(m_mesh.nodes.size()));
    m_status->setText(warnings.trimmed());
    refreshResultSelector();
    refreshView();
    if (m_callbacks.setVisible) {
        m_callbacks.setVisible(true);
    }
}

void ShellDialog::analyze()
{
    if (m_analyzing) {
        return;
    }
    m_analyzing = true;
    struct Reset {
        bool &flag;
        ~Reset() { flag = false; }
    } reset {m_analyzing};
    if (m_model.objects.empty()) {
        QMessageBox::information(this, tr("Cascas"), tr("Adicione ao menos um objeto."));
        return;
    }
    const QStringList cases = shellCaseNames();
    if (cases.isEmpty()) {
        QMessageBox::information(this, tr("Cascas"), tr("Defina ao menos uma carga (aba Cargas)."));
        m_tabs->setCurrentIndex(2);
        return;
    }
    if (m_model.supports.empty() && !m_model.coupled) {
        QMessageBox::information(this, tr("Cascas"), tr("Defina os apoios (aba Apoios): sem apoios a estrutura é instável."));
        m_tabs->setCurrentIndex(3);
        return;
    }
    const bool coupled = m_model.coupled && m_callbacks.coupledResults;
    Job job;
    if (coupled) {
        // uma unica analise do projeto (barras e cascas na mesma matriz de rigidez)
        m_mesh = Structura::Shell::generateMesh(m_model);
        m_hasMesh = !m_mesh.empty();
        m_results = Results {};
        std::string error;
        if (!m_callbacks.coupledResults(cases, m_runProjectAnalysis, m_results, error)) {
            m_results.error = error.empty() ? tr("A análise acoplada do projeto não foi concluída.").toStdString() : error;
        }
    } else {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_mesh = Structura::Shell::generateMesh(m_model);
        m_hasMesh = !m_mesh.empty();
        std::vector<std::string> names;
        for (const QString &c : cases) {
            names.push_back(c.toStdString());
        }
        job = buildJob(m_model, m_mesh, names);
        const Structura::App::ShellSolverBridge bridge(m_femLibrary);
        m_results = bridge.solve(m_model, m_mesh, job);
        QApplication::restoreOverrideCursor();
    }
    QString messages;
    if (coupled && m_results.ok()) {
        messages += tr("• Resultados da análise acoplada às barras (cascas e barras resolvidas juntas).\n");
    }
    for (const std::string &w : m_mesh.warnings) {
        messages += QStringLiteral("• %1\n").arg(QString::fromStdString(w));
    }
    for (const std::string &w : job.warnings) {
        messages += QStringLiteral("• %1\n").arg(QString::fromStdString(w));
    }
    if (!m_results.ok()) {
        m_status->setText(tr("Falha na análise: %1").arg(QString::fromStdString(m_results.error)));
        QMessageBox::warning(this, tr("Cascas"), tr("Falha na análise: %1").arg(QString::fromStdString(m_results.error)));
        return;
    }
    if (m_results.maxWarp > 0.0) {
        double size = 1e300;
        for (const Object &o : m_model.objects) {
            size = std::min(size, o.meshSize);
        }
        if (m_results.maxWarp > 0.05 * size) {
            messages += tr("• Empenamento de elemento de %1 mm: refine a malha em superfícies muito curvas.\n").arg(m_results.maxWarp * 1000.0, 0, 'f', 1);
        }
    }
    if (m_results.springIterations > 1) {
        messages += tr("• Apoios só-compressão: %1 iterações.\n").arg(m_results.springIterations);
    }
    m_status->setText(messages.trimmed());
    m_meshInfo->setText(tr("%1 elementos, %2 nós, %3 casos resolvidos.").arg(m_mesh.quads.size()).arg(m_mesh.nodes.size()).arg(m_results.cases.size()));

    // combinacoes do projeto que envolvem casos das cascas
    m_combined.clear();
    const auto combos = m_callbacks.combinations ? m_callbacks.combinations() : QVector<Structura::App::ProjectData::LoadCombination>();
    for (const auto &combo : combos) {
        std::vector<std::pair<std::string, double>> factors;
        bool any = false;
        for (const auto &[caseName, factor] : combo.factors) {
            factors.emplace_back(caseName.toStdString(), factor);
            any = any || cases.contains(caseName);
        }
        if (any) {
            m_combined.push_back(combine(m_results, factors, combo.name.toStdString()));
        }
    }
    refreshResultSelector();
    m_resultCase->setCurrentIndex(m_results.cases.empty() ? 0 : 1);
    refreshView();
    if (m_callbacks.setVisible) {
        m_callbacks.setVisible(true);
    }
    m_tabs->setCurrentIndex(4);
    // lista da verificacao
    m_ultimate->clear();
    for (const CaseResult &c : m_combined) {
        auto *item = new QListWidgetItem(QString::fromStdString(c.name));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        m_ultimate->addItem(item);
    }
    if (m_combined.empty()) {
        for (const CaseResult &c : m_results.cases) {
            auto *item = new QListWidgetItem(QString::fromStdString(c.name));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
            m_ultimate->addItem(item);
        }
    }
    m_service->clear();
    m_service->addItem(tr("(nenhuma)"));
    for (const CaseResult &c : m_combined) {
        m_service->addItem(QString::fromStdString(c.name));
    }
    for (const CaseResult &c : m_results.cases) {
        m_service->addItem(QString::fromStdString(c.name));
    }
}

void ShellDialog::showProjectResults()
{
    if (m_analyzing || !m_model.coupled || !m_callbacks.coupledResults || m_model.objects.empty() || shellCaseNames().isEmpty()) {
        return;
    }
    m_runProjectAnalysis = false;
    analyze();
    m_runProjectAnalysis = true;
}

void ShellDialog::refreshResultSelector()
{
    const int keep = m_resultCase->currentIndex();
    m_resultCase->blockSignals(true);
    m_resultCase->clear();
    m_resultCase->addItem(tr("(somente a malha)"));
    for (const CaseResult &c : m_results.cases) {
        m_resultCase->addItem(tr("Caso: %1").arg(QString::fromStdString(c.name)));
    }
    for (const CaseResult &c : m_combined) {
        m_resultCase->addItem(tr("Combinação: %1").arg(QString::fromStdString(c.name)));
    }
    m_resultCase->setCurrentIndex(std::clamp(keep, 0, m_resultCase->count() - 1));
    m_resultCase->blockSignals(false);
}

const CaseResult *ShellDialog::currentResult() const
{
    const int index = m_resultCase ? m_resultCase->currentIndex() - 1 : -1;
    if (index < 0) {
        return nullptr;
    }
    if (index < static_cast<int>(m_results.cases.size())) {
        return &m_results.cases[static_cast<std::size_t>(index)];
    }
    const int combo = index - static_cast<int>(m_results.cases.size());
    return combo < m_combined.size() ? &m_combined[combo] : nullptr;
}

void ShellDialog::updateLegend(const RenderMesh &mesh, const QString &title, const QString &unit)
{
    QPixmap pix(m_legend->width(), m_legend->height());
    QPainter painter(&pix);
    for (int x = 0; x < pix.width(); ++x) {
        const auto c = rainbow(static_cast<double>(x) / std::max(1, pix.width() - 1));
        painter.setPen(QColor(c[0], c[1], c[2]));
        painter.drawLine(x, 0, x, pix.height());
    }
    painter.end();
    m_legend->setPixmap(pix);
    if (mesh.hasValues) {
        m_legendText->setText(tr("%1\nmín %2  —  máx %3 %4").arg(title).arg(mesh.minValue, 0, 'g', 5).arg(mesh.maxValue, 0, 'g', 5).arg(unit));
    } else {
        m_legendText->setText(tr("Sem resultados: gere a malha e analise."));
    }
}

void ShellDialog::refreshView()
{
    if (!m_hasMesh || !m_callbacks.render) {
        return;
    }
    RenderOptions opt;
    opt.quantity = static_cast<Quantity>(m_quantity->currentData().toInt());
    opt.deformScale = m_deformScale->value();
    opt.smooth = m_smooth->isChecked();
    opt.edges = m_edges->isChecked();
    opt.unitsPerMeter = m_callbacks.unitsPerMeter ? m_callbacks.unitsPerMeter() : 1.0;
    opt.fixedRange = m_fixedRange->isChecked();
    opt.rangeMin = m_rangeMin->value();
    opt.rangeMax = m_rangeMax->value();
    const CaseResult *result = currentResult();
    std::vector<double> scalars;
    QString title = QString::fromUtf8(quantityInfo(opt.quantity).name);
    QString unit = QString::fromUtf8(quantityInfo(opt.quantity).unit);
    // a vista de verificacao sobrepoe a grandeza
    if (m_designView && m_designView->currentIndex() > 0 && !m_design.elementRatio.empty()) {
        const int view = m_designView->currentIndex();
        if (view == 1) {
            scalars = m_design.elementRatio;
            title = tr("Razão de utilização");
            unit = QString();
        } else if (view >= 2 && view <= 5 && m_design.reinforcement.size() == m_mesh.quads.size()) {
            scalars.resize(m_mesh.quads.size());
            for (std::size_t e = 0; e < scalars.size(); ++e) {
                scalars[e] = m_design.reinforcement[e][static_cast<std::size_t>(view - 2)];
            }
            title = m_designView->currentText();
            unit = tr("cm²/m");
        }
        if (!scalars.empty()) {
            opt.elementScalars = &scalars;
            opt.smooth = false;
        }
    }
    const RenderMesh mesh = buildRenderMesh(m_mesh, m_model, result, opt);
    m_callbacks.render(mesh);
    updateLegend(mesh, title, unit);

    // resumo
    QString text;
    if (result) {
        double umax = 0.0;
        std::size_t unode = 0;
        std::array<double, 3> reaction {0.0, 0.0, 0.0};
        for (std::size_t n = 0; n < result->displacement.size(); ++n) {
            const auto &d = result->displacement[n];
            const double u = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (u > umax) {
                umax = u;
                unode = n;
            }
            for (int c = 0; c < 3; ++c) {
                reaction[static_cast<std::size_t>(c)] += result->reaction[n][static_cast<std::size_t>(c)];
            }
        }
        text += tr("Deslocamento total máximo: %1 mm (nó %2, z = %3 m)\n").arg(umax * 1000.0, 0, 'f', 3).arg(unode).arg(m_mesh.nodes[unode][2], 0, 'f', 3);
        text += tr("Soma das reações: Rx = %1 kN, Ry = %2 kN, Rz = %3 kN\n").arg(reaction[0] / 1e3, 0, 'f', 2).arg(reaction[1] / 1e3, 0, 'f', 2).arg(reaction[2] / 1e3, 0, 'f', 2);
        for (const QuantityInfo &q : quantityList()) {
            if (q.nodal) {
                continue;
            }
            double lo = 1e300, hi = -1e300;
            for (std::size_t e = 0; e < m_mesh.quads.size(); ++e) {
                for (int p = 0; p < 5; ++p) {
                    const double v = elementQuantity(m_mesh, m_model, *result, e, p, q.id) * q.scale;
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            }
            text += QStringLiteral("%1: %2 … %3 %4\n").arg(QString::fromUtf8(q.name)).arg(lo, 0, 'g', 5).arg(hi, 0, 'g', 5).arg(QString::fromUtf8(q.unit));
        }
        text += tr("\nConvenção: m > 0 traciona a face de normal positiva (para fora em tanques e vasos). As direções a/b são circunferencial/meridional nas cascas de "
                   "revolução e X/Y nas chapas. Elementos Q4 (membrana com modos incompatíveis, flexão de Mindlin com MITC4).");
    }
    m_summary->setPlainText(text);
}

void ShellDialog::exportCsv()
{
    const CaseResult *result = currentResult();
    if (!result) {
        QMessageBox::information(this, tr("Cascas"), tr("Escolha um caso ou combinação."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar resultados"), QString(), tr("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    out << "elemento;zona;xc;yc;zc;espessura_m";
    for (const QuantityInfo &q : quantityList()) {
        if (!q.nodal) {
            out << ';' << q.name << " [" << q.unit << ']';
        }
    }
    out << "\n";
    for (std::size_t e = 0; e < m_mesh.quads.size(); ++e) {
        const MeshQuad &q = m_mesh.quads[e];
        double c[3] = {0, 0, 0};
        for (const int n : q.n) {
            for (int k = 0; k < 3; ++k) {
                c[k] += m_mesh.nodes[static_cast<std::size_t>(n)][static_cast<std::size_t>(k)] / 4.0;
            }
        }
        out << e << ';' << QString::fromStdString(m_mesh.zones[static_cast<std::size_t>(q.zone)]) << ';' << c[0] << ';' << c[1] << ';' << c[2] << ';' << q.thickness;
        for (const QuantityInfo &info : quantityList()) {
            if (!info.nodal) {
                out << ';' << elementQuantity(m_mesh, m_model, *result, e, 4, info.id) * info.scale;
            }
        }
        out << "\n";
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Verificacao
// ---------------------------------------------------------------------------------------------------------------------

QWidget *ShellDialog::buildDesignTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *top = new QHBoxLayout;
    auto *left = new QVBoxLayout;
    left->addWidget(new QLabel(tr("Combinações últimas (ELU) a verificar")));
    m_ultimate = new QListWidget;
    m_ultimate->setMaximumHeight(130);
    left->addWidget(m_ultimate);
    top->addLayout(left, 1);
    auto *right = new QFormLayout;
    m_service = new QComboBox;
    m_designView = new QComboBox;
    m_designView->addItems({tr("Grandeza dos resultados"), tr("Razão de utilização"), tr("Armadura face + dir. a"), tr("Armadura face + dir. b"), tr("Armadura face − dir. a"), tr("Armadura face − dir. b")});
    right->addRow(tr("Combinação de serviço (flecha)"), m_service);
    right->addRow(tr("Colorir a malha por"), m_designView);
    auto *run = new QPushButton(tr("Verificar"));
    run->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 16px; border-radius: 4px; }"));
    right->addRow(run);
    top->addLayout(right, 1);
    layout->addLayout(top);
    m_designTable = new QTableWidget(0, 8);
    m_designTable->setHorizontalHeaderLabels({tr("Zona"), tr("Verificação"), tr("Solicitação"), tr("Limite"), tr("Razão"), tr("Un."), tr("Referência"), tr("Observação")});
    m_designTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_designTable->horizontalHeader()->setStretchLastSection(true);
    m_designTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_designTable, 1);
    m_designNotes = new QPlainTextEdit;
    m_designNotes->setReadOnly(true);
    m_designNotes->setMaximumHeight(90);
    layout->addWidget(m_designNotes);
    connect(run, &QPushButton::clicked, this, [this]() { runDesign(); });
    connect(m_designView, &QComboBox::currentIndexChanged, this, [this]() { refreshView(); });
    return page;
}

void ShellDialog::runDesign()
{
    if (!m_results.ok() || !m_hasMesh) {
        QMessageBox::information(this, tr("Cascas"), tr("Analise o modelo antes de verificar."));
        return;
    }
    std::vector<CaseResult> ultimate;
    for (int i = 0; i < m_ultimate->count(); ++i) {
        if (m_ultimate->item(i)->checkState() != Qt::Checked) {
            continue;
        }
        const std::string name = m_ultimate->item(i)->text().toStdString();
        const auto find = [&](const std::vector<CaseResult> &list) -> const CaseResult * {
            for (const CaseResult &c : list) {
                if (c.name == name) {
                    return &c;
                }
            }
            return nullptr;
        };
        const CaseResult *c = nullptr;
        for (const CaseResult &candidate : m_combined) {
            if (candidate.name == name) {
                c = &candidate;
            }
        }
        if (!c) {
            c = find(m_results.cases);
        }
        if (c) {
            ultimate.push_back(*c);
        }
    }
    const CaseResult *service = nullptr;
    const std::string serviceName = m_service->currentIndex() > 0 ? m_service->currentText().toStdString() : std::string();
    for (const CaseResult &c : m_combined) {
        if (c.name == serviceName) {
            service = &c;
        }
    }
    if (!service) {
        for (const CaseResult &c : m_results.cases) {
            if (c.name == serviceName) {
                service = &c;
            }
        }
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_design = designShells(m_mesh, m_model, ultimate, service);
    QApplication::restoreOverrideCursor();
    m_designTable->setRowCount(static_cast<int>(m_design.checks.size()));
    for (int r = 0; r < static_cast<int>(m_design.checks.size()); ++r) {
        const ZoneCheck &c = m_design.checks[static_cast<std::size_t>(r)];
        const QStringList cells = {QString::fromStdString(c.zone), QString::fromStdString(c.criterion), QString::number(c.demand, 'g', 5),
                                   c.limit > 0.0 ? QString::number(c.limit, 'g', 5) : QStringLiteral("—"), c.limit > 0.0 || c.ratio > 0.0 ? zoneRatioText(c.ratio) : QStringLiteral("—"),
                                   QString::fromStdString(c.unit), QString::fromStdString(c.reference), QString::fromStdString(c.note)};
        for (int k = 0; k < cells.size(); ++k) {
            auto *item = new QTableWidgetItem(cells[k]);
            if (c.ratio > 1.0 + 1e-9) {
                item->setBackground(QColor(255, 214, 214));
            } else if (c.ratio > 0.0 && c.limit > 0.0) {
                item->setBackground(c.ratio > 0.85 ? QColor(255, 244, 204) : QColor(222, 244, 226));
            }
            m_designTable->setItem(r, k, item);
        }
    }
    QString notes;
    for (const std::string &n : m_design.notes) {
        notes += QStringLiteral("• %1\n").arg(QString::fromStdString(n));
    }
    notes += tr("Razão máxima: %1 — %2").arg(m_design.maxRatio, 0, 'f', 2).arg(m_design.ok() ? tr("atende") : tr("NÃO atende"));
    m_designNotes->setPlainText(notes);
    refreshView();
}

} // namespace Structura::UI
