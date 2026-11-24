#include "StairDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

using Structura::App::Services::StairGenerator;

/// Pré-visualização da escada: planta (esquerda) e vista oblíqua (direita), com as barras coloridas por função.
class StairPreview final : public QWidget
{
public:
    explicit StairPreview(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(520, 300);
    }

    void setParams(const StairGenerator::Params &params)
    {
        m_result = StairGenerator::generate(params);
        update();
    }

    [[nodiscard]] const StairGenerator::Result &result() const { return m_result; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), QColor(QStringLiteral("#f4f6f9")));
        if (m_result.nodes.empty()) {
            p.setPen(QColor(QStringLiteral("#5a6472")));
            p.drawText(rect(), Qt::AlignCenter, QObject::tr("Parâmetros inválidos"));
            return;
        }
        const int half = width() / 2;
        const QRectF left(0, 0, half, height() - 22), right(half, 0, width() - half, height() - 22);
        drawView(p, left, false);
        drawView(p, right, true);
        p.setPen(QColor(QStringLiteral("#d0d6de")));
        p.drawLine(half, 8, half, height() - 30);
        p.setPen(QColor(QStringLiteral("#5a6472")));
        p.drawText(QRectF(0, height() - 22, half, 20), Qt::AlignCenter, QObject::tr("Planta"));
        p.drawText(QRectF(half, height() - 22, width() - half, 20), Qt::AlignCenter, QObject::tr("Vista oblíqua"));
    }

private:
    [[nodiscard]] static QPointF project(const StairGenerator::Node &n, bool oblique)
    {
        return oblique ? QPointF(n.x + 0.45 * n.y, n.z + 0.30 * n.y) : QPointF(n.x, n.y);
    }

    void drawView(QPainter &p, const QRectF &box, bool oblique) const
    {
        double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
        for (const auto &n : m_result.nodes) {
            const QPointF q = project(n, oblique);
            minX = std::min(minX, q.x());
            maxX = std::max(maxX, q.x());
            minY = std::min(minY, q.y());
            maxY = std::max(maxY, q.y());
        }
        const QRectF area = box.adjusted(16, 16, -16, -8);
        const double k = std::min(area.width() / std::max(maxX - minX, 1e-6), area.height() / std::max(maxY - minY, 1e-6));
        const QPointF centre(0.5 * (minX + maxX), 0.5 * (minY + maxY));
        auto map = [&](const StairGenerator::Node &n) {
            const QPointF q = project(n, oblique);
            return QPointF(area.center().x() + (q.x() - centre.x()) * k, area.center().y() - (q.y() - centre.y()) * k);
        };
        auto colorOf = [](StairGenerator::Role role) {
            switch (role) {
            case StairGenerator::Role::Stringer: return QColor(QStringLiteral("#1f5f8b"));
            case StairGenerator::Role::Tread: return QColor(QStringLiteral("#2e8b57"));
            case StairGenerator::Role::LandingBeam: return QColor(QStringLiteral("#c0392b"));
            default: return QColor(QStringLiteral("#d98c1f"));
            }
        };
        for (const auto &b : m_result.bars) {
            p.setPen(QPen(colorOf(b.role), b.role == StairGenerator::Role::Stringer ? 3.0 : 1.8));
            p.drawLine(map(m_result.nodes[static_cast<std::size_t>(b.a)]), map(m_result.nodes[static_cast<std::size_t>(b.b)]));
        }
        p.setPen(QPen(QColor(QStringLiteral("#1e232b")), 1));
        p.setBrush(Qt::white);
        for (const int id : m_result.baseNodes) {
            p.drawRect(QRectF(map(m_result.nodes[static_cast<std::size_t>(id)]) - QPointF(3.5, 3.5), QSizeF(7, 7)));
        }
        for (const int id : m_result.topNodes) {
            p.drawEllipse(map(m_result.nodes[static_cast<std::size_t>(id)]), 4.0, 4.0);
        }
    }

    StairGenerator::Result m_result;
};

namespace {

QDoubleSpinBox *spin(double min, double max, double value, int decimals, double step, const QString &suffix)
{
    auto *box = new QDoubleSpinBox;
    box->setRange(min, max);
    box->setDecimals(decimals);
    box->setSingleStep(step);
    box->setValue(value);
    box->setSuffix(suffix);
    return box;
}

QComboBox *namedCombo(const QStringList &items)
{
    auto *box = new QComboBox;
    box->addItems(items);
    return box;
}

} // namespace

StairDialog::StairDialog(const QStringList &sections, const QStringList &materials, const QStringList &loadCases, const QVector<StairLevel> &levels,
                         const StairRequest *initial, QWidget *parent)
    : QDialog(parent)
    , m_loadCases(loadCases)
    , m_levels(levels)
{
    setWindowTitle(initial ? tr("Editar escada") : tr("Gerar escada"));
    resize(1100, 720);
    if (m_loadCases.isEmpty()) {
        m_loadCases << QStringLiteral("CASO1");
    }

    auto *root = new QHBoxLayout(this);
    auto *tabs = new QTabWidget;
    tabs->setFixedWidth(430);
    root->addWidget(tabs);

    // ---- geometria
    auto *geometry = new QWidget;
    auto *geometryLayout = new QVBoxLayout(geometry);

    auto *kind = new QGroupBox(tr("Tipo e longarinas"));
    auto *kindForm = new QFormLayout(kind);
    m_name = new QLineEdit(tr("Escada"));
    m_type = new QComboBox;
    for (const auto type : {StairGenerator::Type::Straight, StairGenerator::Type::LShaped, StairGenerator::Type::UShaped, StairGenerator::Type::Spiral}) {
        m_type->addItem(QString::fromUtf8(StairGenerator::typeName(type)), static_cast<int>(type));
    }
    m_layout = new QComboBox;
    for (const auto layout : {StairGenerator::Layout::Two, StairGenerator::Layout::Three, StairGenerator::Layout::Center}) {
        m_layout->addItem(QString::fromUtf8(StairGenerator::layoutName(layout)), static_cast<int>(layout));
    }
    kindForm->addRow(tr("Nome"), m_name);
    kindForm->addRow(tr("Tipo"), m_type);
    kindForm->addRow(tr("Longarinas"), m_layout);
    geometryLayout->addWidget(kind);

    auto *levelsBox = new QGroupBox(tr("Níveis e degraus"));
    auto *levelsForm = new QFormLayout(levelsBox);
    m_baseLevel = new QComboBox;
    m_topLevel = new QComboBox;
    for (auto *box : {m_baseLevel, m_topLevel}) {
        box->addItem(tr("Cota informada"));
        for (const auto &level : m_levels) {
            box->addItem(tr("%1 (%2 m)").arg(level.name).arg(level.elevation, 0, 'f', 3));
        }
    }
    m_baseZ = spin(-1000.0, 1000.0, 0.0, 3, 0.5, tr(" m"));
    m_topZ = spin(-1000.0, 1000.0, 3.0, 3, 0.5, tr(" m"));
    m_riser = spin(0.05, 0.40, 0.18, 3, 0.005, tr(" m"));
    m_riser->setToolTip(tr("Altura alvo do degrau (espelho). O número de degraus é arredondado e a altura recalculada para vencer o desnível."));
    m_tread = spin(0.10, 1.00, 0.28, 3, 0.01, tr(" m"));
    m_tread->setToolTip(tr("Profundidade do piso do degrau (na espiral, o piso resulta do ângulo)."));
    m_width = spin(0.30, 10.0, 1.0, 3, 0.1, tr(" m"));
    m_width->setToolTip(tr("Largura entre os eixos das longarinas laterais."));
    levelsForm->addRow(tr("Nível inicial"), m_baseLevel);
    levelsForm->addRow(tr("Cota inicial"), m_baseZ);
    levelsForm->addRow(tr("Nível final"), m_topLevel);
    levelsForm->addRow(tr("Cota final"), m_topZ);
    levelsForm->addRow(tr("Altura do degrau"), m_riser);
    levelsForm->addRow(tr("Profundidade do piso"), m_tread);
    levelsForm->addRow(tr("Largura"), m_width);
    geometryLayout->addWidget(levelsBox);

    auto *place = new QGroupBox(tr("Posição"));
    auto *placeForm = new QFormLayout(place);
    m_x0 = spin(-1e6, 1e6, 0.0, 3, 1.0, tr(" m"));
    m_y0 = spin(-1e6, 1e6, 0.0, 3, 1.0, tr(" m"));
    m_azimuth = spin(-360.0, 360.0, 0.0, 1, 15.0, QStringLiteral("°"));
    m_azimuth->setToolTip(tr("Direção de subida no plano: 0° = +X, 90° = +Y."));
    m_turn = new QComboBox;
    m_turn->addItem(tr("Anti-horário (para a esquerda)"), 1);
    m_turn->addItem(tr("Horário (para a direita)"), 0);
    m_endByPoint = new QCheckBox(tr("Definir o fim por um ponto (escada reta)"));
    m_x1 = spin(-1e6, 1e6, 3.0, 3, 1.0, tr(" m"));
    m_y1 = spin(-1e6, 1e6, 0.0, 3, 1.0, tr(" m"));
    placeForm->addRow(tr("Início X"), m_x0);
    placeForm->addRow(tr("Início Y"), m_y0);
    placeForm->addRow(tr("Direção"), m_azimuth);
    placeForm->addRow(tr("Sentido da curva"), m_turn);
    placeForm->addRow(m_endByPoint);
    placeForm->addRow(tr("Fim X"), m_x1);
    placeForm->addRow(tr("Fim Y"), m_y1);
    geometryLayout->addWidget(place);

    auto *special = new QGroupBox(tr("Patamar (L e U) e espiral"));
    auto *specialForm = new QFormLayout(special);
    m_firstSteps = new QSpinBox;
    m_firstSteps->setRange(0, 200);
    m_firstSteps->setSpecialValueText(tr("automático (metade)"));
    m_landing = spin(0.0, 20.0, 0.0, 3, 0.1, tr(" m"));
    m_landing->setSpecialValueText(tr("igual à largura"));
    m_gap = spin(0.0, 5.0, 0.10, 3, 0.05, tr(" m"));
    m_outerR = spin(0.3, 20.0, 1.20, 3, 0.1, tr(" m"));
    m_innerR = spin(0.05, 10.0, 0.15, 3, 0.05, tr(" m"));
    m_angle = spin(0.0, 3600.0, 0.0, 1, 15.0, QStringLiteral("°"));
    m_angle->setSpecialValueText(tr("pela profundidade do piso"));
    specialForm->addRow(tr("Degraus do 1º lance"), m_firstSteps);
    specialForm->addRow(tr("Profundidade do patamar"), m_landing);
    specialForm->addRow(tr("Folga entre lances (U)"), m_gap);
    specialForm->addRow(tr("Raio externo"), m_outerR);
    specialForm->addRow(tr("Raio interno"), m_innerR);
    specialForm->addRow(tr("Ângulo total"), m_angle);
    geometryLayout->addWidget(special);
    geometryLayout->addStretch(1);
    tabs->addTab(geometry, tr("Geometria"));

    // ---- perfis e apoios
    auto *props = new QWidget;
    auto *propsForm = new QFormLayout(props);
    m_stringer = namedCombo(sections);
    m_tread2 = namedCombo(sections);
    m_landingSection = namedCombo(sections);
    m_material = namedCombo(materials);
    if (sections.size() > 1) {
        m_tread2->setCurrentIndex(1);
        m_landingSection->setCurrentIndex(1);
    }
    m_baseSupport = new QComboBox;
    m_topSupport = new QComboBox;
    for (auto *box : {m_baseSupport, m_topSupport}) {
        for (const auto kindOfSupport : {StairGenerator::SupportKind::None, StairGenerator::SupportKind::Pinned, StairGenerator::SupportKind::Fixed}) {
            box->addItem(QString::fromUtf8(StairGenerator::supportName(kindOfSupport)), static_cast<int>(kindOfSupport));
        }
        box->setCurrentIndex(1);
    }
    propsForm->addRow(tr("Perfil das longarinas"), m_stringer);
    propsForm->addRow(tr("Perfil dos degraus"), m_tread2);
    propsForm->addRow(tr("Perfil do patamar"), m_landingSection);
    propsForm->addRow(tr("Material"), m_material);
    propsForm->addRow(tr("Apoio no início"), m_baseSupport);
    propsForm->addRow(tr("Apoio no fim"), m_topSupport);
    auto *supportHint = new QLabel(tr("Nós que coincidem com nós já existentes no modelo são reaproveitados e não recebem apoio novo. "
                                      "Com uma só longarina central, o apoio articulado também restringe as rotações (evita o giro em torno da longarina)."));
    supportHint->setWordWrap(true);
    propsForm->addRow(supportHint);
    tabs->addTab(props, tr("Perfis e apoios"));

    // ---- cargas
    auto *loadsTab = new QWidget;
    auto *loadsLayout = new QVBoxLayout(loadsTab);
    auto *loadsHint = new QLabel(tr("Cada carregamento pertence a um caso de carga. Valores positivos atuam para baixo. "
                                    "O peso próprio vem do caso de carga que o inclui."));
    loadsHint->setWordWrap(true);
    loadsLayout->addWidget(loadsHint);
    m_loads = new QTableWidget(0, 3);
    m_loads->setHorizontalHeaderLabels({tr("Carregamento"), tr("Valor"), tr("Caso de carga")});
    m_loads->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_loads->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_loads->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_loads->verticalHeader()->setVisible(false);
    m_loads->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_loads->setSelectionMode(QAbstractItemView::SingleSelection);
    loadsLayout->addWidget(m_loads, 1);
    auto *loadButtons = new QHBoxLayout;
    auto *addLoad = new QPushButton(tr("Adicionar"));
    auto *removeLoad = new QPushButton(tr("Remover"));
    loadButtons->addWidget(addLoad);
    loadButtons->addWidget(removeLoad);
    loadButtons->addStretch(1);
    loadsLayout->addLayout(loadButtons);
    tabs->addTab(loadsTab, tr("Cargas"));

    // ---- pré-visualização
    auto *rightLayout = new QVBoxLayout;
    m_preview = new StairPreview;
    rightLayout->addWidget(m_preview, 1);
    auto *legend = new QLabel(tr("<span style='color:#1f5f8b'>■ longarinas</span> &nbsp; <span style='color:#2e8b57'>■ degraus</span> &nbsp; "
                                 "<span style='color:#c0392b'>■ bordas do patamar</span> &nbsp; <span style='color:#d98c1f'>■ vigas do patamar</span> &nbsp; "
                                 "□ início &nbsp; ○ fim"));
    rightLayout->addWidget(legend);
    m_info = new QLabel;
    m_info->setWordWrap(true);
    m_info->setTextFormat(Qt::RichText);
    rightLayout->addWidget(m_info);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(initial ? tr("Aplicar") : tr("Gerar"));
    rightLayout->addWidget(buttons);
    root->addLayout(rightLayout, 1);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ---- valores iniciais (edição)
    if (initial) {
        const auto &p = initial->params;
        m_name->setText(initial->name);
        m_type->setCurrentIndex(std::max(0, m_type->findData(static_cast<int>(p.type))));
        m_layout->setCurrentIndex(std::max(0, m_layout->findData(static_cast<int>(p.layout))));
        m_baseZ->setValue(p.baseZ);
        m_topZ->setValue(p.topZ);
        m_riser->setValue(p.riser);
        m_tread->setValue(p.tread);
        m_width->setValue(p.width);
        m_x0->setValue(p.x0);
        m_y0->setValue(p.y0);
        m_azimuth->setValue(p.azimuthDeg);
        m_turn->setCurrentIndex(p.turnLeft ? 0 : 1);
        m_endByPoint->setChecked(p.endByPoint);
        m_x1->setValue(p.x1);
        m_y1->setValue(p.y1);
        m_firstSteps->setValue(p.firstFlightSteps);
        m_landing->setValue(p.landingLength);
        m_gap->setValue(p.gap);
        m_outerR->setValue(p.outerRadius);
        m_innerR->setValue(p.innerRadius);
        m_angle->setValue(p.totalAngleDeg);
        m_baseSupport->setCurrentIndex(std::max(0, m_baseSupport->findData(static_cast<int>(p.baseSupport))));
        m_topSupport->setCurrentIndex(std::max(0, m_topSupport->findData(static_cast<int>(p.topSupport))));
        m_stringer->setCurrentIndex(std::clamp(initial->stringerSection, 0, std::max(0, static_cast<int>(sections.size()) - 1)));
        m_tread2->setCurrentIndex(std::clamp(initial->treadSection, 0, std::max(0, static_cast<int>(sections.size()) - 1)));
        m_landingSection->setCurrentIndex(std::clamp(initial->landingSection, 0, std::max(0, static_cast<int>(sections.size()) - 1)));
        m_material->setCurrentIndex(std::clamp(initial->material, 0, std::max(0, static_cast<int>(materials.size()) - 1)));
        for (const auto &load : p.loads) {
            addLoadRow(load.kind, load.value, QString::fromStdString(load.loadCase));
        }
    } else {
        // sobrecarga de uso no primeiro caso que pareça variável; senão, no último
        QString live = m_loadCases.last();
        for (const auto &name : m_loadCases) {
            if (name.contains(QStringLiteral("sobre"), Qt::CaseInsensitive) || name.contains(QStringLiteral("var"), Qt::CaseInsensitive)
                || name.contains(QStringLiteral("uso"), Qt::CaseInsensitive)) {
                live = name;
                break;
            }
        }
        addLoadRow(StairGenerator::LoadKind::TreadArea, 3.0, live);
    }

    // ---- ligações
    auto syncLevel = [this](QComboBox *combo, QDoubleSpinBox *box) {
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, box](int index) {
            if (index > 0 && index - 1 < m_levels.size()) {
                box->setValue(m_levels[index - 1].elevation);
            }
        });
        connect(box, &QDoubleSpinBox::valueChanged, this, [this, combo](double value) {
            const int index = combo->currentIndex();
            if (index > 0 && index - 1 < m_levels.size() && std::abs(m_levels[index - 1].elevation - value) > 1e-9) {
                combo->blockSignals(true);
                combo->setCurrentIndex(0);
                combo->blockSignals(false);
            }
        });
    };
    syncLevel(m_baseLevel, m_baseZ);
    syncLevel(m_topLevel, m_topZ);
    // seleciona o nível cuja cota coincide com o valor inicial
    for (const auto &pair : {std::make_pair(m_baseLevel, m_baseZ), std::make_pair(m_topLevel, m_topZ)}) {
        for (int i = 0; i < m_levels.size(); ++i) {
            if (std::abs(m_levels[i].elevation - pair.second->value()) < 1e-9) {
                pair.first->blockSignals(true);
                pair.first->setCurrentIndex(i + 1);
                pair.first->blockSignals(false);
                break;
            }
        }
    }

    connect(addLoad, &QPushButton::clicked, this, [this]() {
        addLoadRow(StairGenerator::LoadKind::TreadArea, 0.0, m_loadCases.first());
        updateState();
    });
    connect(removeLoad, &QPushButton::clicked, this, [this]() {
        const int row = m_loads->currentRow();
        if (row >= 0) {
            m_loads->removeRow(row);
            updateState();
        }
    });
    for (auto *box : {m_baseZ, m_topZ, m_riser, m_tread, m_width, m_x0, m_y0, m_azimuth, m_x1, m_y1, m_landing, m_gap, m_outerR, m_innerR, m_angle}) {
        connect(box, &QDoubleSpinBox::valueChanged, this, [this]() { updateState(); });
    }
    connect(m_firstSteps, &QSpinBox::valueChanged, this, [this]() { updateState(); });
    connect(m_endByPoint, &QCheckBox::toggled, this, [this]() { updateState(); });
    for (auto *box : {m_type, m_layout, m_turn, m_baseSupport, m_topSupport}) {
        connect(box, &QComboBox::currentIndexChanged, this, [this]() { updateState(); });
    }
    updateState();
}

void StairDialog::addLoadRow(StairGenerator::LoadKind kind, double value, const QString &loadCase)
{
    const int row = m_loads->rowCount();
    m_loads->insertRow(row);
    auto *kinds = new QComboBox;
    for (const auto k : {StairGenerator::LoadKind::TreadArea, StairGenerator::LoadKind::LandingArea, StairGenerator::LoadKind::StringerLine,
                         StairGenerator::LoadKind::HandrailLine}) {
        kinds->addItem(QString::fromUtf8(StairGenerator::loadKindName(k)), static_cast<int>(k));
    }
    kinds->setCurrentIndex(std::max(0, kinds->findData(static_cast<int>(kind))));
    auto *amount = spin(-1e6, 1e6, value, 3, 0.5, QString());
    auto *cases = new QComboBox;
    cases->addItems(m_loadCases);
    int index = m_loadCases.indexOf(loadCase);
    if (index < 0 && !loadCase.isEmpty()) {
        cases->addItem(loadCase);   // caso citado pela escada que não existe mais no projeto: mantém o nome
        index = cases->count() - 1;
    }
    cases->setCurrentIndex(std::max(0, index));
    m_loads->setCellWidget(row, 0, kinds);
    m_loads->setCellWidget(row, 1, amount);
    m_loads->setCellWidget(row, 2, cases);
    connect(kinds, &QComboBox::currentIndexChanged, this, [this]() { updateState(); });
    connect(amount, &QDoubleSpinBox::valueChanged, this, [this]() { updateState(); });
}

StairGenerator::Params StairDialog::currentParams() const
{
    StairGenerator::Params p;
    p.type = static_cast<StairGenerator::Type>(m_type->currentData().toInt());
    p.layout = static_cast<StairGenerator::Layout>(m_layout->currentData().toInt());
    p.baseZ = m_baseZ->value();
    p.topZ = m_topZ->value();
    p.riser = m_riser->value();
    p.tread = m_tread->value();
    p.width = m_width->value();
    p.x0 = m_x0->value();
    p.y0 = m_y0->value();
    p.azimuthDeg = m_azimuth->value();
    p.turnLeft = m_turn->currentData().toInt() == 1;
    p.endByPoint = m_endByPoint->isChecked() && p.type == StairGenerator::Type::Straight;
    p.x1 = m_x1->value();
    p.y1 = m_y1->value();
    p.firstFlightSteps = m_firstSteps->value();
    p.landingLength = m_landing->value();
    p.gap = m_gap->value();
    p.outerRadius = m_outerR->value();
    p.innerRadius = m_innerR->value();
    p.totalAngleDeg = m_angle->value();
    p.baseSupport = static_cast<StairGenerator::SupportKind>(m_baseSupport->currentData().toInt());
    p.topSupport = static_cast<StairGenerator::SupportKind>(m_topSupport->currentData().toInt());
    for (int row = 0; row < m_loads->rowCount(); ++row) {
        const auto *kinds = qobject_cast<QComboBox *>(m_loads->cellWidget(row, 0));
        const auto *amount = qobject_cast<QDoubleSpinBox *>(m_loads->cellWidget(row, 1));
        const auto *cases = qobject_cast<QComboBox *>(m_loads->cellWidget(row, 2));
        if (!kinds || !amount || !cases) {
            continue;
        }
        StairGenerator::Load load;
        load.kind = static_cast<StairGenerator::LoadKind>(kinds->currentData().toInt());
        load.value = amount->value();
        load.loadCase = cases->currentText().toStdString();
        p.loads.push_back(load);
    }
    return p;
}

void StairDialog::updateState()
{
    const auto type = static_cast<StairGenerator::Type>(m_type->currentData().toInt());
    const bool straight = type == StairGenerator::Type::Straight;
    const bool landing = type == StairGenerator::Type::LShaped || type == StairGenerator::Type::UShaped;
    const bool spiral = type == StairGenerator::Type::Spiral;
    const bool byPoint = straight && m_endByPoint->isChecked();
    m_endByPoint->setEnabled(straight);
    m_x1->setEnabled(byPoint);
    m_y1->setEnabled(byPoint);
    m_azimuth->setEnabled(!byPoint);
    m_tread->setEnabled(!spiral && !byPoint);
    m_width->setEnabled(!spiral);
    m_turn->setEnabled(landing || spiral);
    m_firstSteps->setEnabled(landing);
    m_landing->setEnabled(landing);
    m_gap->setEnabled(type == StairGenerator::Type::UShaped);
    m_outerR->setEnabled(spiral);
    m_innerR->setEnabled(spiral);
    m_angle->setEnabled(spiral);

    const auto params = currentParams();
    m_preview->setParams(params);
    const auto &result = m_preview->result();
    QString text;
    if (!result.nodes.empty()) {
        const auto &top = result.nodes[static_cast<std::size_t>(result.topNodes.empty() ? 0 : result.topNodes.front())];
        text = tr("<b>%1 degraus</b> de %2 m (altura) × %3 m (piso) · %4 nós · %5 barras<br>Chegada no ponto (%6; %7; %8) m")
                   .arg(result.risers)
                   .arg(result.riser, 0, 'f', 3)
                   .arg(result.tread, 0, 'f', 3)
                   .arg(result.nodes.size())
                   .arg(result.bars.size())
                   .arg(top.x, 0, 'f', 3)
                   .arg(top.y, 0, 'f', 3)
                   .arg(top.z, 0, 'f', 3);
    }
    for (const auto &warning : result.warnings) {
        text += QStringLiteral("<br><span style='color:#b9770e'>⚠ %1</span>").arg(QString::fromStdString(warning).toHtmlEscaped());
    }
    m_info->setText(text);
}

StairRequest StairDialog::request() const
{
    StairRequest r;
    r.name = m_name->text().trimmed().isEmpty() ? tr("Escada") : m_name->text().trimmed();
    r.params = currentParams();
    r.stringerSection = m_stringer->currentIndex();
    r.treadSection = m_tread2->currentIndex();
    r.landingSection = m_landingSection->currentIndex();
    r.material = m_material->currentIndex();
    return r;
}

} // namespace Structura::UI
