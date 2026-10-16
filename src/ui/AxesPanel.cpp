#include "AxesPanel.h"

#include "SvgIcons.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSet>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

using Structura::App::ProjectData::AxisLevel;
using Structura::App::ProjectData::ReferenceAxis;

namespace {

const QString kAxisNames[2] = {QStringLiteral("X"), QStringLiteral("Y")};
const QString kAxisColors[2] = {QStringLiteral("#d64545"), QStringLiteral("#2e9c5a")};

QString formatValue(double value)
{
    QString text = QString::number(value, 'f', 3);
    while (text.contains(QLatin1Char('.')) && (text.endsWith(QLatin1Char('0')) || text.endsWith(QLatin1Char('.')))) {
        text.chop(1);
    }
    return text == QLatin1String("-0") ? QStringLiteral("0") : text;
}

bool validLevelName(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_\\-]+$"));
    return pattern.match(name).hasMatch();
}

const QString kFieldStyle = QStringLiteral(
    "QLineEdit { background: #ffffff; border: 1px solid #cbd4e2; border-radius: 4px; padding: 3px 6px; color: #1e232b; }"
    "QLineEdit:focus { border: 1px solid #0d7fb9; background: #f4f9fd; }"
    "QLineEdit[invalid=\"true\"] { border: 1px solid #d64545; background: #fdf1f1; }");

void setInvalid(QLineEdit *edit, bool invalid)
{
    if (edit->property("invalid").toBool() == invalid) {
        return;
    }
    edit->setProperty("invalid", invalid);
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
}

QToolButton *flatButton(QWidget *parent, const QString &icon, const QString &tooltip)
{
    auto *button = new QToolButton(parent);
    button->setIcon(svgIcon(icon, QColor(QStringLiteral("#1e232b")), 16));
    button->setIconSize(QSize(16, 16));
    button->setFixedSize(26, 26);
    button->setToolTip(tooltip);
    button->setStyleSheet(QStringLiteral(
        "QToolButton { border: 1px solid #cbd4e2; border-radius: 4px; background: #ffffff; }"
        "QToolButton:hover { background: rgba(19, 147, 214, 0.14); border-color: #0d7fb9; }"
        "QToolButton::menu-indicator { image: none; }"));
    return button;
}

} // namespace

AxesPanel::AxesPanel(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("axesPanel"));
    setFixedWidth(350);
    setStyleSheet(QStringLiteral(
        "QFrame#axesPanel { background: #f2f5fa; border: 1px solid #cbd4e2; border-radius: 8px; }"
        "QFrame#axesHeader { background: #0d7fb9; border: 0; border-top-left-radius: 8px; border-top-right-radius: 8px; }"
        "QLabel#axesTitle { color: #ffffff; font-size: 13px; font-weight: 700; background: transparent; }"
        "QLabel#axesHint { color: #5a6574; font-size: 11px; background: transparent; }"
        "QFrame#axisCard { background: #ffffff; border: 1px solid #d6dde8; border-radius: 6px; }"
        "QFrame#levelBox { background: #ffffff; border: 1px solid #d6dde8; border-radius: 6px; }"
        "QLabel#axisChip { color: #ffffff; font-weight: 700; border-radius: 3px; }"
        "QLabel#axisCount { color: #5a6574; font-size: 11px; background: transparent; }"
        "QLabel#axesStatus { color: #5a6574; font-size: 11px; background: transparent; }"
        "QLabel#fieldLabel { color: #5a6574; font-size: 11px; background: transparent; }"
        "QPushButton#axisAdd, QPushButton#levelCopy { background: transparent; color: #0d7fb9; border: 1px solid #0d7fb9; border-radius: 4px; padding: 2px 8px; font-weight: 600; }"
        "QPushButton#axisAdd:hover, QPushButton#levelCopy:hover { background: rgba(19, 147, 214, 0.14); }"
        "QPushButton#axesClear { background: transparent; color: #5a6574; border: 0; padding: 2px 4px; }"
        "QPushButton#axesClear:hover { color: #d64545; }"
        "QComboBox { background: #ffffff; border: 1px solid #cbd4e2; border-radius: 4px; padding: 3px 6px; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 10);
    root->setSpacing(8);

    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("axesHeader"));
    header->setFixedHeight(34);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 6, 0);
    auto *title = new QLabel(tr("Eixos de referencia"), header);
    title->setObjectName(QStringLiteral("axesTitle"));
    headerLayout->addWidget(title);
    headerLayout->addStretch(1);
    auto *close = new QToolButton(header);
    close->setIcon(svgIcon(QStringLiteral("close.svg"), QColor(Qt::white), 14));
    close->setIconSize(QSize(14, 14));
    close->setFixedSize(24, 24);
    close->setToolTip(tr("Fechar"));
    close->setStyleSheet(QStringLiteral(
        "QToolButton { border: 0; border-radius: 4px; background: transparent; }"
        "QToolButton:hover { background: rgba(255, 255, 255, 0.25); }"));
    connect(close, &QToolButton::clicked, this, &QWidget::hide);
    headerLayout->addWidget(close);
    root->addWidget(header);

    auto *hint = new QLabel(tr("Cada nivel (pavimento) tem seus proprios eixos. Os eixos tambem servem de alvo de snap."), this);
    hint->setObjectName(QStringLiteral("axesHint"));
    hint->setWordWrap(true);
    hint->setContentsMargins(12, 0, 12, 0);
    root->addWidget(hint);

    auto *levelWrapper = new QWidget(this);
    auto *levelWrapperLayout = new QVBoxLayout(levelWrapper);
    levelWrapperLayout->setContentsMargins(10, 0, 10, 0);
    levelWrapperLayout->addWidget(makeLevelBar());
    root->addWidget(levelWrapper);

    for (int axis = 0; axis < 2; ++axis) {
        auto *wrapper = new QWidget(this);
        auto *wrapperLayout = new QVBoxLayout(wrapper);
        wrapperLayout->setContentsMargins(10, 0, 10, 0);
        wrapperLayout->addWidget(makeCard(axis));
        root->addWidget(wrapper);
    }

    auto *footer = new QHBoxLayout();
    footer->setContentsMargins(12, 0, 12, 0);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("axesStatus"));
    footer->addWidget(m_status, 1);
    auto *clear = new QPushButton(tr("Limpar eixos do nivel"), this);
    clear->setObjectName(QStringLiteral("axesClear"));
    clear->setCursor(Qt::PointingHandCursor);
    connect(clear, &QPushButton::clicked, this, [this]() {
        m_building = true;
        clearRows();
        m_building = false;
        commit();
    });
    footer->addWidget(clear);
    root->addLayout(footer);

    m_axes.levels.append({QStringLiteral("N0"), 0.0, {}, {}});
    m_lastEmitted = m_axes;
    rebuildLevelCombo();
    loadCurrentLevel();
    adjustSize();
    hide();
}

QWidget *AxesPanel::makeLevelBar()
{
    auto *box = new QFrame(this);
    box->setObjectName(QStringLiteral("levelBox"));
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *top = new QHBoxLayout();
    top->setSpacing(6);
    auto *label = new QLabel(tr("Nivel"), box);
    label->setStyleSheet(QStringLiteral("font-weight: 600; color: #1e232b; background: transparent;"));
    top->addWidget(label);
    m_levelCombo = new QComboBox(box);
    m_levelCombo->setToolTip(tr("Nivel (pavimento) cujos eixos estao sendo editados"));
    top->addWidget(m_levelCombo, 1);

    auto *add = flatButton(box, QStringLiteral("plus.svg"), tr("Novo nivel"));
    auto *addMenu = new QMenu(add);
    addMenu->addAction(tr("Novo nivel vazio"), this, [this]() { addLevel(false); });
    addMenu->addAction(tr("Novo nivel copiando os eixos do nivel atual"), this, [this]() { addLevel(true); });
    add->setMenu(addMenu);
    add->setPopupMode(QToolButton::InstantPopup);
    top->addWidget(add);

    auto *remove = flatButton(box, QStringLiteral("trash.svg"), tr("Remover este nivel"));
    connect(remove, &QToolButton::clicked, this, &AxesPanel::removeCurrentLevel);
    top->addWidget(remove);
    layout->addLayout(top);

    auto *fields = new QHBoxLayout();
    fields->setSpacing(6);
    auto *nameLabel = new QLabel(tr("Nome"), box);
    nameLabel->setObjectName(QStringLiteral("fieldLabel"));
    fields->addWidget(nameLabel);
    m_levelName = new QLineEdit(box);
    m_levelName->setStyleSheet(kFieldStyle);
    m_levelName->setFixedHeight(26);
    m_levelName->setMaxLength(16);
    m_levelName->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z0-9_\\-]*")), m_levelName));
    fields->addWidget(m_levelName, 1);
    auto *heightLabel = new QLabel(tr("Altura Z (m)"), box);
    heightLabel->setObjectName(QStringLiteral("fieldLabel"));
    fields->addWidget(heightLabel);
    m_levelElevation = new QLineEdit(box);
    m_levelElevation->setStyleSheet(kFieldStyle);
    m_levelElevation->setFixedHeight(26);
    m_levelElevation->setFixedWidth(74);
    m_levelElevation->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[+-]?(?:\\d+(?:[.,]\\d{0,3})?)?")), m_levelElevation));
    fields->addWidget(m_levelElevation);
    layout->addLayout(fields);

    auto *copy = new QPushButton(tr("Copiar eixos para outras alturas..."), box);
    copy->setObjectName(QStringLiteral("levelCopy"));
    copy->setCursor(Qt::PointingHandCursor);
    copy->setToolTip(tr("Copia os eixos X/Y deste nivel para niveis existentes ou para novas alturas"));
    connect(copy, &QPushButton::clicked, this, &AxesPanel::openCopyDialog);
    layout->addWidget(copy);

    connect(m_levelCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_building || index < 0 || index >= m_axes.levels.size() || index == m_levelIndex) {
            return;
        }
        m_levelIndex = index;
        loadCurrentLevel();
        emit levelActivated(m_axes.levels.at(index).elevation);
    });
    connect(m_levelName, &QLineEdit::editingFinished, this, &AxesPanel::commit);
    connect(m_levelElevation, &QLineEdit::editingFinished, this, &AxesPanel::commit);
    return box;
}

QWidget *AxesPanel::makeCard(int axis)
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("axisCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *top = new QHBoxLayout();
    top->setSpacing(6);
    auto *chip = new QLabel(kAxisNames[axis], card);
    chip->setObjectName(QStringLiteral("axisChip"));
    chip->setAlignment(Qt::AlignCenter);
    chip->setFixedSize(20, 20);
    chip->setStyleSheet(QStringLiteral("background: %1;").arg(kAxisColors[axis]));
    top->addWidget(chip);
    auto *name = new QLabel(tr("Eixos em %1").arg(kAxisNames[axis]), card);
    name->setStyleSheet(QStringLiteral("font-weight: 600; color: #1e232b; background: transparent;"));
    top->addWidget(name);
    Card &data = m_cards[static_cast<std::size_t>(axis)];
    data.count = new QLabel(card);
    data.count->setObjectName(QStringLiteral("axisCount"));
    top->addWidget(data.count);
    top->addStretch(1);
    auto *add = new QPushButton(tr("+ Adicionar"), card);
    add->setObjectName(QStringLiteral("axisAdd"));
    add->setCursor(Qt::PointingHandCursor);
    add->setToolTip(tr("Adiciona um eixo %1 com rotulo e coordenada sugeridos").arg(kAxisNames[axis]));
    connect(add, &QPushButton::clicked, this, [this, axis]() { addSuggestedRow(axis); });
    top->addWidget(add);
    layout->addLayout(top);

    data.rows = new QVBoxLayout();
    data.rows->setSpacing(4);
    layout->addLayout(data.rows);
    return card;
}

AxisLevel *AxesPanel::currentLevel()
{
    if (m_levelIndex < 0 || m_levelIndex >= m_axes.levels.size()) {
        return nullptr;
    }
    return &m_axes.levels[m_levelIndex];
}

QString AxesPanel::levelText(const AxisLevel &level)
{
    return QStringLiteral("%1   (%2 m)").arg(level.name, formatValue(level.elevation));
}

void AxesPanel::rebuildLevelCombo()
{
    const bool wasBuilding = m_building;
    m_building = true;
    m_levelCombo->clear();
    for (const AxisLevel &level : m_axes.levels) {
        m_levelCombo->addItem(levelText(level));
    }
    m_levelIndex = std::clamp(m_levelIndex, 0, static_cast<int>(m_axes.levels.size()) - 1);
    m_levelCombo->setCurrentIndex(m_levelIndex);
    m_building = wasBuilding;
}

void AxesPanel::addRow(int axis, const QString &label, const QString &value, bool focus)
{
    Card &card = m_cards[static_cast<std::size_t>(axis)];
    auto *rowWidget = new QWidget(this);
    rowWidget->setFixedHeight(28);
    auto *layout = new QHBoxLayout(rowWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *labelEdit = new QLineEdit(label, rowWidget);
    labelEdit->setPlaceholderText(tr("Rotulo"));
    labelEdit->setMaxLength(8);
    labelEdit->setFixedSize(84, 26);
    labelEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z0-9]*")), labelEdit));
    labelEdit->setStyleSheet(kFieldStyle);
    connect(labelEdit, &QLineEdit::textEdited, labelEdit, [labelEdit](const QString &text) {
        const QString upper = text.toUpper();
        if (upper != text) {
            const int cursor = labelEdit->cursorPosition();
            labelEdit->setText(upper);
            labelEdit->setCursorPosition(cursor);
        }
    });
    layout->addWidget(labelEdit);

    auto *valueEdit = new QLineEdit(value, rowWidget);
    valueEdit->setPlaceholderText(tr("Coordenada (m)"));
    valueEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[+-]?(?:\\d+(?:[.,]\\d{0,3})?)?")), valueEdit));
    valueEdit->setStyleSheet(kFieldStyle);
    valueEdit->setFixedHeight(26);
    layout->addWidget(valueEdit, 1);

    auto *unit = new QLabel(QStringLiteral("m"), rowWidget);
    unit->setStyleSheet(QStringLiteral("color: #5a6574; background: transparent;"));
    layout->addWidget(unit);

    auto *remove = new QToolButton(rowWidget);
    remove->setIcon(svgIcon(QStringLiteral("trash.svg"), QColor(QStringLiteral("#5a6574")), 16));
    remove->setIconSize(QSize(16, 16));
    remove->setFixedSize(24, 24);
    remove->setToolTip(tr("Remover este eixo"));
    remove->setStyleSheet(QStringLiteral(
        "QToolButton { border: 0; border-radius: 4px; background: transparent; }"
        "QToolButton:hover { background: rgba(214, 69, 69, 0.15); }"));
    connect(remove, &QToolButton::clicked, this, [this, axis, rowWidget]() { removeRow(axis, rowWidget); });
    layout->addWidget(remove);

    connect(labelEdit, &QLineEdit::returnPressed, valueEdit, [valueEdit]() { valueEdit->setFocus(); valueEdit->selectAll(); });
    connect(labelEdit, &QLineEdit::editingFinished, this, &AxesPanel::commit);
    connect(valueEdit, &QLineEdit::editingFinished, this, &AxesPanel::commit);
    connect(labelEdit, &QLineEdit::textChanged, this, [this]() { if (!m_building) updateStatus(); });
    connect(valueEdit, &QLineEdit::textChanged, this, [this]() { if (!m_building) updateStatus(); });

    card.rows->addWidget(rowWidget);
    card.items.push_back({rowWidget, labelEdit, valueEdit});
    if (focus) {
        valueEdit->setFocus();
        valueEdit->selectAll();
    }
}

void AxesPanel::removeRow(int axis, QWidget *widget)
{
    Card &card = m_cards[static_cast<std::size_t>(axis)];
    card.items.erase(std::remove_if(card.items.begin(), card.items.end(),
                                    [widget](const Row &row) { return row.widget == widget; }),
                     card.items.end());
    widget->hide();
    widget->deleteLater();
    commit();
}

void AxesPanel::clearRows()
{
    for (auto &card : m_cards) {
        for (auto &row : card.items) {
            row.widget->hide();
            row.widget->deleteLater();
        }
        card.items.clear();
    }
}

QString AxesPanel::suggestedLabel(int axis) const
{
    QSet<QString> used;
    for (const Row &row : m_cards[static_cast<std::size_t>(axis)].items) {
        used.insert(row.label->text().trimmed().toUpper());
    }
    for (int i = 0; i < 1000; ++i) {
        QString candidate;
        if (axis == 0) {
            int n = i;
            do {
                candidate.prepend(QChar(QLatin1Char('A' + n % 26)));
                n = n / 26 - 1;
            } while (n >= 0);
        } else {
            candidate = QString::number(i + 1);
        }
        if (!used.contains(candidate)) {
            return candidate;
        }
    }
    return QString();
}

void AxesPanel::addSuggestedRow(int axis)
{
    const auto &items = m_cards[static_cast<std::size_t>(axis)].items;
    double next = 0.0;
    bool any = false;
    double last = 0.0;
    for (const Row &row : items) {
        bool ok = false;
        const double value = row.value->text().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
        if (ok && (!any || value > last)) {
            last = value;
            any = true;
        }
    }
    if (any) {
        next = last + (m_suggestedStep > 0.0 ? m_suggestedStep : 1.0);
    }
    addRow(axis, suggestedLabel(axis), formatValue(next), true);
    commit();
}

void AxesPanel::loadCurrentLevel()
{
    m_building = true;
    clearRows();
    if (const AxisLevel *level = currentLevel()) {
        m_levelName->setText(level->name);
        m_levelElevation->setText(formatValue(level->elevation));
        for (const auto &entry : level->x) {
            addRow(0, entry.label, formatValue(entry.value), false);
        }
        for (const auto &entry : level->y) {
            addRow(1, entry.label, formatValue(entry.value), false);
        }
    }
    m_building = false;
    updateStatus();
    scheduleResize();
}

void AxesPanel::updateStatus()
{
    int total = 0;
    for (int axis = 0; axis < 2; ++axis) {
        Card &card = m_cards[static_cast<std::size_t>(axis)];
        QSet<QString> labels;
        QSet<qint64> values;
        int valid = 0;
        for (Row &row : card.items) {
            const QString label = row.label->text().trimmed().toUpper();
            QString valueText = row.value->text().trimmed();
            valueText.replace(QLatin1Char(','), QLatin1Char('.'));
            bool ok = false;
            const double raw = valueText.toDouble(&ok);
            const bool labelEmpty = label.isEmpty();
            const bool valueEmpty = valueText.isEmpty() || !ok;
            const qint64 key = static_cast<qint64>(std::llround(raw * 1000.0));
            const bool labelDup = !labelEmpty && labels.contains(label);
            const bool valueDup = !valueEmpty && values.contains(key);
            setInvalid(row.label, labelDup || (labelEmpty && !valueEmpty));
            setInvalid(row.value, valueDup || (valueEmpty && !labelEmpty));
            if (!(labelEmpty || valueEmpty || labelDup || valueDup)) {
                labels.insert(label);
                values.insert(key);
                ++valid;
            }
        }
        card.count->setText(valid == 0 ? QString() : QStringLiteral("(%1)").arg(valid));
        total += valid;
    }

    // Nome e elevacao do nivel (unicos entre os niveis).
    bool nameInvalid = false;
    bool elevationInvalid = false;
    const QString name = m_levelName->text().trimmed();
    bool elevationOk = false;
    const double elevation = m_levelElevation->text().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&elevationOk);
    for (int i = 0; i < m_axes.levels.size(); ++i) {
        if (i == m_levelIndex) {
            continue;
        }
        nameInvalid = nameInvalid || m_axes.levels.at(i).name == name;
        elevationInvalid = elevationInvalid || (elevationOk && std::abs(m_axes.levels.at(i).elevation - elevation) < 1e-6);
    }
    setInvalid(m_levelName, nameInvalid || !validLevelName(name));
    setInvalid(m_levelElevation, elevationInvalid || !elevationOk);

    m_status->setText(tr("%1 eixo(s) | %2 nivel(is) | vermelho = ignorado")
                          .arg(total)
                          .arg(m_axes.levels.size()));
}

void AxesPanel::commit()
{
    if (m_building) {
        return;
    }
    AxisLevel *level = currentLevel();
    if (!level) {
        return;
    }

    // Eixos do nivel atual: so linhas validas e sem repeticao.
    auto collect = [this](int axis) {
        QVector<ReferenceAxis> result;
        QSet<QString> labels;
        QSet<qint64> values;
        for (const Row &row : m_cards[static_cast<std::size_t>(axis)].items) {
            const QString label = row.label->text().trimmed().toUpper();
            QString valueText = row.value->text().trimmed();
            valueText.replace(QLatin1Char(','), QLatin1Char('.'));
            bool ok = false;
            const double raw = valueText.toDouble(&ok);
            if (label.isEmpty() || !ok) {
                continue;
            }
            const double value = std::round(raw * 1000.0) / 1000.0;
            const qint64 key = static_cast<qint64>(std::llround(value * 1000.0));
            if (labels.contains(label) || values.contains(key)) {
                continue;
            }
            labels.insert(label);
            values.insert(key);
            result.append({label, value});
        }
        return result;
    };
    level->x = collect(0);
    level->y = collect(1);

    // Nome e elevacao (unicos entre os niveis).
    bool activate = false;
    const QString name = m_levelName->text().trimmed();
    bool elevationOk = false;
    const double elevation = std::round(m_levelElevation->text().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&elevationOk) * 1000.0) / 1000.0;
    bool nameFree = validLevelName(name);
    bool elevationFree = elevationOk;
    for (int i = 0; i < m_axes.levels.size(); ++i) {
        if (i == m_levelIndex) {
            continue;
        }
        nameFree = nameFree && m_axes.levels.at(i).name != name;
        elevationFree = elevationFree && std::abs(m_axes.levels.at(i).elevation - elevation) >= 1e-6;
    }
    if (nameFree) {
        level->name = name;
    }
    if (elevationFree && std::abs(level->elevation - elevation) > 1e-9) {
        level->elevation = elevation;
        activate = true;
    }
    const double activeElevation = level->elevation;
    sortLevels(level->name);

    m_building = true;
    rebuildLevelCombo();
    m_building = false;
    updateStatus();

    if (!(m_axes == m_lastEmitted)) {
        m_lastEmitted = m_axes;
        emit axesCommitted(m_axes);
    }
    if (activate) {
        emit levelActivated(activeElevation);
    }
    scheduleResize();
}

void AxesPanel::sortLevels(const QString &keepName)
{
    std::sort(m_axes.levels.begin(), m_axes.levels.end(),
              [](const AxisLevel &a, const AxisLevel &b) { return a.elevation < b.elevation; });
    for (int i = 0; i < m_axes.levels.size(); ++i) {
        if (m_axes.levels.at(i).name == keepName) {
            m_levelIndex = i;
        }
    }
}

QString AxesPanel::suggestedLevelName() const
{
    QSet<QString> used;
    for (const AxisLevel &level : m_axes.levels) {
        used.insert(level.name);
    }
    for (int i = 1; i < 10000; ++i) {
        const QString candidate = QStringLiteral("N%1").arg(i);
        if (!used.contains(candidate)) {
            return candidate;
        }
    }
    return QStringLiteral("N");
}

void AxesPanel::addLevel(bool copyCurrentAxes)
{
    commit();
    double elevation = 0.0;
    if (!m_axes.levels.isEmpty()) {
        const double last = m_axes.levels.last().elevation;
        double step = 3.0;
        if (m_axes.levels.size() > 1) {
            const double previous = m_axes.levels.at(m_axes.levels.size() - 2).elevation;
            if (last - previous > 1e-9) {
                step = last - previous;
            }
        }
        elevation = last + step;
    }
    AxisLevel level;
    level.name = suggestedLevelName();
    level.elevation = elevation;
    if (copyCurrentAxes) {
        if (const AxisLevel *source = currentLevel()) {
            level.x = source->x;
            level.y = source->y;
        }
    }
    m_axes.levels.append(level);
    sortLevels(level.name);
    rebuildLevelCombo();
    loadCurrentLevel();
    commit();
    emit levelActivated(level.elevation);
}

void AxesPanel::removeCurrentLevel()
{
    if (m_axes.levels.size() <= 1) {
        // O ultimo nivel nao e removido: apenas esvaziado.
        m_building = true;
        clearRows();
        m_building = false;
        commit();
        return;
    }
    if (QMessageBox::question(this, tr("Remover nivel"),
                              tr("Remover o nivel \"%1\" e seus eixos?").arg(m_axes.levels.at(m_levelIndex).name))
        != QMessageBox::Yes) {
        return;
    }
    m_axes.levels.removeAt(m_levelIndex);
    m_levelIndex = std::clamp(m_levelIndex, 0, static_cast<int>(m_axes.levels.size()) - 1);
    rebuildLevelCombo();
    loadCurrentLevel();
    m_lastEmitted = m_axes;
    emit axesCommitted(m_axes);
    emit levelActivated(m_axes.levels.at(m_levelIndex).elevation);
}

void AxesPanel::openCopyDialog()
{
    commit();
    const AxisLevel *source = currentLevel();
    if (!source) {
        return;
    }
    const AxisLevel sourceCopy = *source;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Copiar eixos do nivel %1").arg(sourceCopy.name));
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Copiar os eixos X/Y de \"%1\" (%2 m) para:").arg(sourceCopy.name, formatValue(sourceCopy.elevation)), &dialog));

    auto *list = new QListWidget(&dialog);
    for (const AxisLevel &level : m_axes.levels) {
        if (level.name == sourceCopy.name) {
            continue;
        }
        auto *item = new QListWidgetItem(levelText(level), list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, level.name);
    }
    list->setMaximumHeight(140);
    layout->addWidget(new QLabel(tr("Niveis existentes:"), &dialog));
    layout->addWidget(list);

    layout->addWidget(new QLabel(tr("Novas alturas (m) - separe por espaco ou ponto e virgula:"), &dialog));
    auto *heights = new QLineEdit(&dialog);
    heights->setPlaceholderText(tr("ex.: 3 6 9 12"));
    layout->addWidget(heights);
    auto *replace = new QCheckBox(tr("Substituir os eixos ja existentes nos niveis de destino"), &dialog);
    replace->setChecked(true);
    layout->addWidget(replace);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto applyAxes = [&](AxisLevel &target) {
        if (replace->isChecked()) {
            target.x = sourceCopy.x;
            target.y = sourceCopy.y;
            return;
        }
        auto merge = [](QVector<ReferenceAxis> &into, const QVector<ReferenceAxis> &from) {
            for (const ReferenceAxis &entry : from) {
                bool present = false;
                for (const ReferenceAxis &existing : into) {
                    present = present || existing.label == entry.label || std::abs(existing.value - entry.value) < 1e-9;
                }
                if (!present) {
                    into.append(entry);
                }
            }
        };
        merge(target.x, sourceCopy.x);
        merge(target.y, sourceCopy.y);
    };

    QSet<QString> destinations;
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->checkState() == Qt::Checked) {
            destinations.insert(list->item(i)->data(Qt::UserRole).toString());
        }
    }
    for (AxisLevel &level : m_axes.levels) {
        if (destinations.contains(level.name)) {
            applyAxes(level);
        }
    }

    const QStringList tokens = heights->text().split(QRegularExpression(QStringLiteral("[;\\s]+")), Qt::SkipEmptyParts);
    for (QString token : tokens) {
        token.replace(QLatin1Char(','), QLatin1Char('.'));
        bool ok = false;
        const double elevation = std::round(token.toDouble(&ok) * 1000.0) / 1000.0;
        if (!ok) {
            continue;
        }
        bool existed = false;
        for (AxisLevel &level : m_axes.levels) {
            if (std::abs(level.elevation - elevation) < 1e-6) {
                existed = true;
                if (level.name != sourceCopy.name) {
                    applyAxes(level);
                }
            }
        }
        if (existed) {
            continue;
        }
        AxisLevel level;
        level.elevation = elevation;
        QString name = QStringLiteral("N%1").arg(formatValue(elevation));
        name.replace(QLatin1Char('.'), QLatin1Char('_'));
        while (std::any_of(m_axes.levels.begin(), m_axes.levels.end(), [&name](const AxisLevel &l) { return l.name == name; })) {
            name += QLatin1Char('x');
        }
        level.name = name;
        level.x = sourceCopy.x;
        level.y = sourceCopy.y;
        m_axes.levels.append(level);
    }

    sortLevels(sourceCopy.name);
    rebuildLevelCombo();
    loadCurrentLevel();
    m_lastEmitted = m_axes;
    emit axesCommitted(m_axes);
    updateStatus();
}

void AxesPanel::scheduleResize()
{
    // As linhas novas so tem tamanho valido apos o proximo ciclo de layout.
    QTimer::singleShot(0, this, [this]() {
        layout()->invalidate();
        layout()->activate();
        adjustSize();
    });
}

void AxesPanel::setAxes(const ReferenceAxes &axes, double elevation)
{
    m_axes = axes;
    if (m_axes.levels.isEmpty()) {
        m_axes.levels.append({QStringLiteral("N0"), 0.0, {}, {}});
    }
    std::sort(m_axes.levels.begin(), m_axes.levels.end(),
              [](const AxisLevel &a, const AxisLevel &b) { return a.elevation < b.elevation; });
    m_lastEmitted = m_axes;
    m_levelIndex = 0;
    double best = std::abs(m_axes.levels.first().elevation - elevation);
    for (int i = 1; i < m_axes.levels.size(); ++i) {
        const double distance = std::abs(m_axes.levels.at(i).elevation - elevation);
        if (distance < best) {
            best = distance;
            m_levelIndex = i;
        }
    }
    rebuildLevelCombo();
    loadCurrentLevel();
}

void AxesPanel::hideEvent(QHideEvent *event)
{
    commit();
    QFrame::hideEvent(event);
}

void AxesPanel::discard()
{
    m_building = true;
    hide();
    m_building = false;
}

void AxesPanel::reposition()
{
    if (!parentWidget()) {
        return;
    }
    adjustSize();
    move(parentWidget()->width() - width() - 20, 12);
    raise();
}

} // namespace Structura::UI
