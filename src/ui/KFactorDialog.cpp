#include "KFactorDialog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <cmath>
#include <functional>

namespace Structura::UI {

namespace {

enum class End { Fixed, Pinned, Free, Guided };

struct Drawing {
    End bottom;
    End top;
    std::function<double(double)> shape;   ///< deslocamento lateral normalizado (0 a 1) em funcao da posicao t (0 = base, 1 = topo)
};

constexpr double kPi = 3.14159265358979323846;

const QVector<Drawing> &drawings()
{
    static const QVector<Drawing> list = {
        {End::Fixed, End::Fixed, [](double t) { return (1.0 - std::cos(2.0 * kPi * t)) / 2.0; }},
        {End::Fixed, End::Pinned, [](double t) { return t * t * (1.0 - t) / (4.0 / 27.0); }},
        {End::Pinned, End::Pinned, [](double t) { return std::sin(kPi * t); }},
        {End::Fixed, End::Guided, [](double t) { return (1.0 - std::cos(kPi * t)) / 2.0; }},
        {End::Fixed, End::Free, [](double t) { return 1.0 - std::cos(kPi * t / 2.0); }},
        {End::Pinned, End::Guided, [](double t) { return std::sin(kPi * t / 2.0); }},
    };
    return list;
}

QString comma(double v)
{
    return QString::number(v, 'f', 2).replace(QLatin1Char('.'), QLatin1Char(','));
}

void hatchBlock(QPainter &p, const QRectF &block)
{
    p.save();
    p.setPen(QPen(QColor(QStringLiteral("#46505c")), 1.0));
    p.setBrush(QColor(QStringLiteral("#dfe4ea")));
    p.drawRect(block);
    p.setClipRect(block);
    for (double x = block.left() - block.height(); x < block.right(); x += 5.0) {
        p.drawLine(QPointF(x, block.bottom()), QPointF(x + block.height(), block.top()));
    }
    p.restore();
}

/// `down`: o apoio fica abaixo do ponto (base da coluna); senao, acima (topo).
void drawEnd(QPainter &p, End end, const QPointF &at, bool down)
{
    const double s = down ? 1.0 : -1.0;
    const QColor line(QStringLiteral("#46505c"));
    p.setPen(QPen(line, 1.2));
    switch (end) {
    case End::Free:
        break;
    case End::Fixed:
        hatchBlock(p, QRectF(at.x() - 15.0, down ? at.y() : at.y() - 8.0, 30.0, 8.0));
        break;
    case End::Pinned: {
        QPolygonF tri;
        tri << at << QPointF(at.x() - 8.0, at.y() + s * 12.0) << QPointF(at.x() + 8.0, at.y() + s * 12.0);
        p.setBrush(QColor(QStringLiteral("#dfe4ea")));
        p.drawPolygon(tri);
        p.drawLine(QPointF(at.x() - 13.0, at.y() + s * 12.0), QPointF(at.x() + 13.0, at.y() + s * 12.0));
        break;
    }
    case End::Guided: {
        // rotacao impedida e translacao livre: bloco apoiado em dois roletes
        p.setBrush(Qt::white);
        p.drawEllipse(QPointF(at.x() - 6.0, at.y() + s * 3.0), 2.6, 2.6);
        p.drawEllipse(QPointF(at.x() + 6.0, at.y() + s * 3.0), 2.6, 2.6);
        hatchBlock(p, QRectF(at.x() - 15.0, down ? at.y() + 6.0 : at.y() - 14.0, 30.0, 8.0));
        break;
    }
    }
}

} // namespace

const QVector<KType> &kTypes()
{
    // valores teoricos e recomendados (coluna isolada) para o coeficiente de flambagem por flexao K
    static const QVector<KType> list = {
        {"Engastada–engastada\n(sem translação)", 0.50, 0.65},
        {"Engastada–articulada\n(sem translação)", 0.70, 0.80},
        {"Articulada–articulada\n(sem translação)", 1.00, 1.00},
        {"Engastada–engastada\n(com translação)", 1.00, 1.20},
        {"Engastada–livre\n(balanço)", 2.00, 2.10},
        {"Articulada–engastada\n(com translação)", 2.00, 2.00},
    };
    return list;
}

// ------------------------------------------------------------------------------------------------------------------------------

KTypeCard::KTypeCard(int index, QWidget *parent)
    : QWidget(parent)
    , m_index(index)
{
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(sizeHint());
    setToolTip(tr("Clique para escolher este tipo de vinculação"));
}

void KTypeCard::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void KTypeCard::setRecommendedHighlight(bool recommended)
{
    m_recommended = recommended;
    update();
}

void KTypeCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_index);
    }
}

void KTypeCard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF frame = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
    p.setPen(QPen(m_selected ? QColor(QStringLiteral("#0d7fb9")) : QColor(QStringLiteral("#c8d0da")), m_selected ? 2.4 : 1.0));
    p.setBrush(m_selected ? QColor(QStringLiteral("#eaf5fb")) : QColor(Qt::white));
    p.drawRoundedRect(frame, 6, 6);

    const KType &type = kTypes().at(m_index);
    QFont small = font();
    small.setPointSizeF(7.5);
    p.setFont(small);
    p.setPen(QColor(QStringLiteral("#2c3642")));
    p.drawText(QRectF(4, 4, width() - 8, 34), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, tr(type.name));

    // coluna
    const Drawing &d = drawings().at(m_index);
    const double x0 = width() / 2.0 - 12.0;
    const double yb = 128.0, yt = 58.0, amplitude = 24.0;
    p.setPen(QPen(QColor(QStringLiteral("#a7b0bb")), 1.0, Qt::DashLine));
    p.drawLine(QPointF(x0, yb), QPointF(x0, yt));
    QPainterPath curve;
    const int steps = 48;
    for (int i = 0; i <= steps; ++i) {
        const double t = static_cast<double>(i) / steps;
        const QPointF point(x0 + amplitude * d.shape(t), yb + (yt - yb) * t);
        i == 0 ? curve.moveTo(point) : curve.lineTo(point);
    }
    p.setPen(QPen(QColor(QStringLiteral("#d9480f")), 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(curve);
    drawEnd(p, d.bottom, QPointF(x0, yb), true);
    drawEnd(p, d.top, QPointF(x0 + amplitude * d.shape(1.0), yt), false);

    // valores de K
    QFont bold = small;
    bold.setBold(true);
    const QRectF line1(4, height() - 44, width() - 8, 18), line2(4, height() - 26, width() - 8, 18);
    p.setFont(m_recommended ? small : bold);
    p.setPen(m_recommended ? QColor(QStringLiteral("#59636f")) : QColor(QStringLiteral("#0b4f78")));
    p.drawText(line1, Qt::AlignCenter, tr("K teórico = %1").arg(comma(type.theoretical)));
    p.setFont(m_recommended ? bold : small);
    p.setPen(m_recommended ? QColor(QStringLiteral("#0b4f78")) : QColor(QStringLiteral("#59636f")));
    p.drawText(line2, Qt::AlignCenter, tr("K recomendado = %1").arg(comma(type.recommended)));
}

// ------------------------------------------------------------------------------------------------------------------------------

KFactorDialog::KFactorDialog(const QVector<int> &memberIds, const Structura::App::ProjectData::DesignMemberParams &current, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Comprimentos de flambagem (K)"));
    auto *root = new QVBoxLayout(this);

    QStringList names;
    for (int i = 0; i < memberIds.size() && i < 12; ++i) {
        names << QStringLiteral("B%1").arg(memberIds.at(i));
    }
    auto *target = new QLabel(tr("<b>Barras:</b> %1%2 (%3)").arg(names.join(QStringLiteral(", ")), memberIds.size() > 12 ? QStringLiteral(" …") : QString()).arg(memberIds.size()), this);
    target->setWordWrap(true);
    root->addWidget(target);

    auto *intro = new QLabel(tr("Escolha o tipo de vinculação nas extremidades (os valores teóricos e recomendados de K estão em cada quadro), informe um K qualquer ou "
                                "defina diretamente o comprimento efetivo de flambagem. Cada eixo pode usar uma forma diferente."),
                             this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto *cards = new QHBoxLayout;
    for (int i = 0; i < kTypes().size(); ++i) {
        auto *card = new KTypeCard(i, this);
        connect(card, &KTypeCard::clicked, this, [this](int index) { selectType(index); });
        cards->addWidget(card);
        m_cards.append(card);
    }
    root->addLayout(cards);

    auto *variant = new QHBoxLayout;
    variant->addWidget(new QLabel(tr("Valor de K do tipo escolhido:"), this));
    m_theoretical = new QRadioButton(tr("teórico"), this);
    m_recommended = new QRadioButton(tr("recomendado"), this);
    m_recommended->setChecked(true);
    variant->addWidget(m_theoretical);
    variant->addWidget(m_recommended);
    variant->addStretch();
    root->addLayout(variant);
    connect(m_theoretical, &QRadioButton::toggled, this, [this]() { refreshTypeValue(); });

    auto *grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    m_x = makeRow(tr("Kx — flexão em torno de X (plano da altura)"), current.kx, current.lx, grid, 0);
    m_y = makeRow(tr("Ky — flexão em torno de Y"), current.ky, current.ly, grid, 1);
    m_z = makeRow(tr("Kz — torção"), current.kz, current.lz, grid, 2);
    root->addLayout(grid);

    auto *note = new QLabel(tr("Comprimento efetivo de flambagem = K · L. Ao informar o comprimento efetivo, K fica igual a 1,0 e o comprimento informado é usado "
                               "diretamente na verificação."),
                            this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:#59636f; font-size:8.5pt;"));
    root->addWidget(note);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Aplicar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    selectType(m_type);
}

KFactorDialog::AxisRow KFactorDialog::makeRow(const QString &title, double currentK, double currentL, QGridLayout *grid, int row)
{
    AxisRow r;
    auto *label = new QLabel(title, this);
    r.mode = new QComboBox(this);
    r.mode->addItem(tr("Não alterar"), static_cast<int>(KAxisSetting::Mode::Keep));
    r.mode->addItem(tr("Tipo escolhido no desenho"), static_cast<int>(KAxisSetting::Mode::Type));
    r.mode->addItem(tr("K arbitrário"), static_cast<int>(KAxisSetting::Mode::Coefficient));
    r.mode->addItem(tr("Comprimento efetivo"), static_cast<int>(KAxisSetting::Mode::Length));
    r.value = new QDoubleSpinBox(this);
    r.value->setProperty("k0", currentK);
    r.value->setProperty("l0", currentL);
    r.unit = new QLabel(this);
    r.unit->setMinimumWidth(40);
    const QString current = currentL > 0.0 ? tr("atual: comprimento efetivo %1 m").arg(comma(currentL)) : tr("atual: K = %1").arg(comma(currentK));
    auto *currentLabel = new QLabel(current, this);
    currentLabel->setStyleSheet(QStringLiteral("color:#59636f; font-size:8.5pt;"));
    grid->addWidget(label, row, 0);
    grid->addWidget(r.mode, row, 1);
    grid->addWidget(r.value, row, 2);
    grid->addWidget(r.unit, row, 3);
    grid->addWidget(currentLabel, row, 4);
    AxisRow *slot = &r;
    Q_UNUSED(slot)
    connect(r.mode, &QComboBox::currentIndexChanged, this, [this, combo = r.mode]() {
        for (AxisRow *row : {&m_x, &m_y, &m_z}) {
            if (row->mode == combo) {
                updateRow(*row);
            }
        }
    });
    updateRow(r);
    return r;
}

void KFactorDialog::updateRow(AxisRow &row)
{
    const auto mode = static_cast<KAxisSetting::Mode>(row.mode->currentData().toInt());
    row.value->setEnabled(mode == KAxisSetting::Mode::Coefficient || mode == KAxisSetting::Mode::Length);
    row.value->blockSignals(true);
    switch (mode) {
    case KAxisSetting::Mode::Keep:
    case KAxisSetting::Mode::Type:
        row.value->setDecimals(3);
        row.value->setRange(0.0, 1000.0);
        row.value->setValue(mode == KAxisSetting::Mode::Type ? typeValue() : row.value->property("k0").toDouble());
        row.unit->setText(mode == KAxisSetting::Mode::Type ? QStringLiteral("K") : QString());
        break;
    case KAxisSetting::Mode::Coefficient:
        row.value->setDecimals(3);
        row.value->setRange(0.05, 20.0);
        row.value->setSingleStep(0.05);
        row.value->setValue(std::max(0.05, row.value->property("k0").toDouble()));
        row.unit->setText(QStringLiteral("K"));
        break;
    case KAxisSetting::Mode::Length:
        row.value->setDecimals(3);
        row.value->setRange(0.001, 10000.0);
        row.value->setSingleStep(0.1);
        row.value->setValue(row.value->property("l0").toDouble() > 0.0 ? row.value->property("l0").toDouble() : 1.0);
        row.unit->setText(QStringLiteral("m"));
        break;
    }
    row.value->blockSignals(false);
}

double KFactorDialog::typeValue() const
{
    const KType &type = kTypes().at(m_type);
    return m_theoretical && m_theoretical->isChecked() ? type.theoretical : type.recommended;
}

void KFactorDialog::selectType(int index)
{
    m_type = index;
    for (int i = 0; i < m_cards.size(); ++i) {
        m_cards.at(i)->setSelected(i == index);
    }
    // escolher um tipo no desenho passa os eixos que estavam "Nao alterar" para "Tipo escolhido" no primeiro uso: o usuario ve o efeito
    for (AxisRow *row : {&m_x, &m_y, &m_z}) {
        if (static_cast<KAxisSetting::Mode>(row->mode->currentData().toInt()) == KAxisSetting::Mode::Keep && sender() != nullptr) {
            row->mode->setCurrentIndex(1);
        }
    }
    refreshTypeValue();
}

void KFactorDialog::refreshTypeValue()
{
    const bool recommended = !(m_theoretical && m_theoretical->isChecked());
    for (auto *card : m_cards) {
        card->setRecommendedHighlight(recommended);
    }
    for (AxisRow *row : {&m_x, &m_y, &m_z}) {
        if (static_cast<KAxisSetting::Mode>(row->mode->currentData().toInt()) == KAxisSetting::Mode::Type) {
            row->value->setValue(typeValue());
        }
    }
}

KAxisSetting KFactorDialog::settingOf(const AxisRow &row) const
{
    KAxisSetting s;
    s.mode = static_cast<KAxisSetting::Mode>(row.mode->currentData().toInt());
    s.value = s.mode == KAxisSetting::Mode::Type ? typeValue() : row.value->value();
    return s;
}

KFactorSettings KFactorDialog::settings() const
{
    return {settingOf(m_x), settingOf(m_y), settingOf(m_z)};
}

} // namespace Structura::UI
