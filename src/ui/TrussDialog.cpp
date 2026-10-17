#include "TrussDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace Structura::UI {

using Structura::App::Services::TrussGenerator;

/// Desenho esquemático da treliça (banzos, montantes e diagonais em cores diferentes).
class TrussPreview final : public QWidget
{
public:
    explicit TrussPreview(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(420, 220);
    }

    void setShape(const TrussGenerator::Params &params)
    {
        m_result = TrussGenerator::generate(params);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), QColor(QStringLiteral("#f4f6f9")));
        if (m_result.nodes.empty()) {
            return;
        }
        double minX = 1e18, maxX = -1e18, minZ = 1e18, maxZ = -1e18;
        for (const auto &n : m_result.nodes) {
            minX = std::min(minX, n[0]);
            maxX = std::max(maxX, n[0]);
            minZ = std::min(minZ, n[1]);
            maxZ = std::max(maxZ, n[1]);
        }
        const QRectF area = QRectF(rect()).adjusted(18, 18, -18, -26);
        const double k = std::min(area.width() / std::max(maxX - minX, 1e-6), area.height() / std::max(maxZ - minZ, 1e-6));
        const double ox = area.center().x() - 0.5 * (maxX + minX) * k, oz = area.center().y() + 0.5 * (maxZ + minZ) * k;
        auto map = [&](const std::array<double, 2> &n) { return QPointF(ox + n[0] * k, oz - n[1] * k); };
        auto colorOf = [](TrussGenerator::Role role) {
            switch (role) {
            case TrussGenerator::Role::BottomChord:
            case TrussGenerator::Role::TopChord: return QColor(QStringLiteral("#1f5f8b"));
            case TrussGenerator::Role::Vertical: return QColor(QStringLiteral("#2e8b57"));
            default: return QColor(QStringLiteral("#c0392b"));
            }
        };
        for (const auto &b : m_result.bars) {
            const bool chord = b.role == TrussGenerator::Role::BottomChord || b.role == TrussGenerator::Role::TopChord;
            p.setPen(QPen(colorOf(b.role), chord ? 3.0 : 1.8));
            p.drawLine(map(m_result.nodes[static_cast<std::size_t>(b.a)]), map(m_result.nodes[static_cast<std::size_t>(b.b)]));
        }
        p.setPen(QPen(QColor(QStringLiteral("#1e232b")), 1));
        p.setBrush(Qt::white);
        for (const auto &n : m_result.nodes) {
            p.drawEllipse(map(n), 3.0, 3.0);
        }
        p.setPen(QColor(QStringLiteral("#5a6472")));
        p.drawText(rect().adjusted(8, 0, -8, -4), Qt::AlignLeft | Qt::AlignBottom,
                   QObject::tr("%1 nós · %2 barras · %3 painéis").arg(m_result.nodes.size()).arg(m_result.bars.size()).arg(m_result.panels));
    }

private:
    TrussGenerator::Result m_result;
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

} // namespace

TrussDialog::TrussDialog(const QStringList &sections, const QStringList &materials, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Gerar treliça"));
    resize(900, 640);
    auto *root = new QHBoxLayout(this);
    auto *left = new QVBoxLayout;

    auto *shape = new QGroupBox(tr("Forma"));
    auto *shapeForm = new QFormLayout(shape);
    m_profile = new QComboBox;
    for (const auto profile : {TrussGenerator::Profile::Parallel, TrussGenerator::Profile::Gable, TrussGenerator::Profile::Shed, TrussGenerator::Profile::Bowstring,
                               TrussGenerator::Profile::Scissors}) {
        m_profile->addItem(QString::fromUtf8(TrussGenerator::profileName(profile)), static_cast<int>(profile));
    }
    m_web = new QComboBox;
    for (const auto web : {TrussGenerator::Web::Pratt, TrussGenerator::Web::Howe, TrussGenerator::Web::Warren, TrussGenerator::Web::WarrenVerticals, TrussGenerator::Web::Fink,
                           TrussGenerator::Web::K}) {
        m_web->addItem(QString::fromUtf8(TrussGenerator::webName(web)), static_cast<int>(web));
    }
    m_span = spin(1.0, 500.0, 12.0, 2, 1.0, tr(" m"));
    m_height = spin(0.1, 100.0, 2.0, 2, 0.25, tr(" m"));
    m_endHeight = spin(0.0, 100.0, 0.0, 2, 0.25, tr(" m"));
    m_endHeight->setToolTip(tr("Altura nos apoios (duas águas, uma água, arco e tesoura). Nas treliças de banzos paralelos vale a altura total."));
    m_panels = new QSpinBox;
    m_panels->setRange(2, 60);
    m_panels->setValue(6);
    m_panels->setToolTip(tr("Número de painéis. Os arranjos simétricos (todos menos Warren) usam número par: ímpar é arredondado para cima."));
    shapeForm->addRow(tr("Perfil dos banzos"), m_profile);
    shapeForm->addRow(tr("Arranjo da alma"), m_web);
    shapeForm->addRow(tr("Vão"), m_span);
    shapeForm->addRow(tr("Altura máxima"), m_height);
    shapeForm->addRow(tr("Altura nos apoios"), m_endHeight);
    shapeForm->addRow(tr("Painéis"), m_panels);
    left->addWidget(shape);

    auto *place = new QGroupBox(tr("Posição"));
    auto *placeForm = new QFormLayout(place);
    m_plane = new QComboBox;
    m_plane->addItem(tr("Plano XZ (vão ao longo de X)"), 0);
    m_plane->addItem(tr("Plano YZ (vão ao longo de Y)"), 1);
    m_x0 = spin(-1e6, 1e6, 0.0, 3, 1.0, tr(" m"));
    m_y0 = spin(-1e6, 1e6, 0.0, 3, 1.0, tr(" m"));
    m_z0 = spin(-1e6, 1e6, 0.0, 3, 1.0, tr(" m"));
    placeForm->addRow(tr("Plano da treliça"), m_plane);
    placeForm->addRow(tr("Ponto inicial X"), m_x0);
    placeForm->addRow(tr("Ponto inicial Y"), m_y0);
    placeForm->addRow(tr("Ponto inicial Z"), m_z0);
    left->addWidget(place);

    auto *props = new QGroupBox(tr("Perfis e material"));
    auto *propsForm = new QFormLayout(props);
    m_chord = new QComboBox;
    m_vertical = new QComboBox;
    m_diagonal = new QComboBox;
    m_material = new QComboBox;
    for (auto *box : {m_chord, m_vertical, m_diagonal}) {
        box->addItems(sections);
    }
    m_material->addItems(materials);
    m_vertical->setCurrentIndex(m_vertical->count() > 1 ? 1 : 0);
    m_diagonal->setCurrentIndex(m_diagonal->count() > 1 ? 1 : 0);
    propsForm->addRow(tr("Banzos"), m_chord);
    propsForm->addRow(tr("Montantes"), m_vertical);
    propsForm->addRow(tr("Diagonais"), m_diagonal);
    propsForm->addRow(tr("Material"), m_material);
    m_truss = new QCheckBox(tr("Barras de treliça (biarticuladas, só esforço axial)"));
    m_truss->setChecked(true);
    m_supports = new QCheckBox(tr("Criar apoios (fixo à esquerda, móvel à direita)"));
    m_supports->setChecked(true);
    m_lock = new QCheckBox(tr("Travar o deslocamento fora do plano em todos os nós"));
    m_lock->setChecked(true);
    propsForm->addRow(m_truss);
    propsForm->addRow(m_supports);
    propsForm->addRow(m_lock);
    left->addWidget(props);
    left->addStretch(1);

    auto *leftWidget = new QWidget;
    leftWidget->setLayout(left);
    leftWidget->setFixedWidth(380);
    root->addWidget(leftWidget);

    auto *right = new QVBoxLayout;
    m_preview = new TrussPreview;
    right->addWidget(m_preview, 1);
    auto *legend = new QLabel(tr("<span style='color:#1f5f8b'>■ banzos</span> &nbsp; <span style='color:#2e8b57'>■ montantes</span> &nbsp; <span style='color:#c0392b'>■ diagonais</span>"));
    right->addWidget(legend);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Gerar"));
    right->addWidget(buttons);
    root->addLayout(right, 1);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    for (auto *box : {m_span, m_height, m_endHeight}) {
        connect(box, &QDoubleSpinBox::valueChanged, this, [this]() { updatePreview(); });
    }
    connect(m_panels, &QSpinBox::valueChanged, this, [this]() { updatePreview(); });
    connect(m_profile, &QComboBox::currentIndexChanged, this, [this]() { updatePreview(); });
    connect(m_web, &QComboBox::currentIndexChanged, this, [this]() { updatePreview(); });
    updatePreview();
}

void TrussDialog::updatePreview()
{
    const bool parallel = m_profile->currentData().toInt() == static_cast<int>(TrussGenerator::Profile::Parallel);
    m_endHeight->setEnabled(!parallel);
    TrussGenerator::Params params;
    params.profile = static_cast<TrussGenerator::Profile>(m_profile->currentData().toInt());
    params.web = static_cast<TrussGenerator::Web>(m_web->currentData().toInt());
    params.span = m_span->value();
    params.height = m_height->value();
    params.endHeight = m_endHeight->value();
    params.panels = m_panels->value();
    m_preview->setShape(params);
}

TrussRequest TrussDialog::request() const
{
    TrussRequest r;
    r.shape.profile = static_cast<TrussGenerator::Profile>(m_profile->currentData().toInt());
    r.shape.web = static_cast<TrussGenerator::Web>(m_web->currentData().toInt());
    r.shape.span = m_span->value();
    r.shape.height = m_height->value();
    r.shape.endHeight = m_endHeight->value();
    r.shape.panels = m_panels->value();
    r.planeYZ = m_plane->currentData().toInt() == 1;
    r.x0 = m_x0->value();
    r.y0 = m_y0->value();
    r.z0 = m_z0->value();
    r.chordSection = m_chord->currentIndex();
    r.verticalSection = m_vertical->currentIndex();
    r.diagonalSection = m_diagonal->currentIndex();
    r.material = m_material->currentIndex();
    r.trussBars = m_truss->isChecked();
    r.supports = m_supports->isChecked();
    r.lockOutOfPlane = m_lock->isChecked();
    return r;
}

} // namespace Structura::UI
