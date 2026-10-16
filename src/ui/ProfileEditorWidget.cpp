#include "ProfileEditorWidget.h"

#include "SectionPreviewWidget.h"
#include "viz/structure/SectionCatalog.h"
#include "viz/structure/SectionShapes.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Structura::UI {

using Category = ProfileLibrary::Category;
using Structura::Visualization::SectionCatalog;
using Structura::Visualization::SectionShapes;

namespace {

constexpr int kFamilyRole = Qt::UserRole;

QDoubleSpinBox *makePropertySpin(const QString &suffix, double value, QWidget *parent)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(0.0001, 1.0e9);
    spin->setDecimals(4);
    spin->setSingleStep(1.0);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setGroupSeparatorShown(false);
    return spin;
}

QString stepTitle(const QString &number, const QString &text)
{
    return QStringLiteral("<span style='color:#f2992e;font-weight:700;'>%1</span>&nbsp; <b>%2</b>").arg(number, text);
}

} // namespace

ProfileEditorWidget::ProfileEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_categoryGroup(new QButtonGroup(this))
    , m_typeList(new QListWidget(this))
    , m_inputStack(new QStackedWidget(this))
    , m_catalogCombo(nullptr)
    , m_parameterHost(new QWidget(this))
    , m_parameterLayout(new QFormLayout())
    , m_mirror(new QCheckBox(tr("Espelhar em torno do eixo local z"), this))
    , m_areaSpin(makePropertySpin(tr(" cm²"), 50.0, this))
    , m_izSpin(makePropertySpin(tr(" cm⁴"), 5000.0, this))
    , m_iySpin(makePropertySpin(tr(" cm⁴"), 5000.0, this))
    , m_jSpin(makePropertySpin(tr(" cm⁴"), 1000.0, this))
    , m_nameEdit(new QLineEdit(this))
    , m_preview(new SectionPreviewWidget(this))
    , m_properties(new QLabel(this))
    , m_message(new QLabel(this))
    , m_createButton(new QPushButton(tr("Criar perfil"), this))
    , m_createAssignButton(new QPushButton(this))
{
    setStyleSheet(QStringLiteral(
        "QPushButton[chip=\"true\"] { background: #ffffff; border: 1px solid #cbd4e2; border-radius: 12px;"
        "  padding: 4px 14px; color: #1e232b; }"
        "QPushButton[chip=\"true\"]:hover { border-color: #0d7fb9; }"
        "QPushButton[chip=\"true\"]:checked { background: #0d7fb9; border-color: #0d7fb9; color: #ffffff; }"
        "QListWidget#typeTiles { background: #ffffff; border: 1px solid #cbd4e2; border-radius: 6px; }"
        "QListWidget#typeTiles::item { border-radius: 6px; padding: 4px; }"
        "QListWidget#typeTiles::item:selected { background: #dcecf6; color: #0d4f74; border: 1px solid #0d7fb9; }"
        "QListWidget#typeTiles::item:hover { background: #eef5fa; }"));

    // ---- 1. categoria
    auto *categoryLabel = new QLabel(stepTitle(QStringLiteral("1"), tr("Categoria")), this);
    auto *categoryRow = new QHBoxLayout();
    categoryRow->setSpacing(6);
    for (Category category : ProfileLibrary::categories()) {
        auto *button = new QPushButton(ProfileLibrary::categoryName(category), this);
        button->setCheckable(true);
        button->setProperty("chip", true);
        button->setCursor(Qt::PointingHandCursor);
        m_categoryGroup->addButton(button, static_cast<int>(category));
        categoryRow->addWidget(button);
    }
    categoryRow->addStretch(1);
    m_categoryGroup->setExclusive(true);
    m_categoryGroup->button(static_cast<int>(Category::SemGeometria))
        ->setToolTip(tr("Define o perfil apenas por A, Iz, Iy e J (sem contorno na exibição extrudada)."));

    // ---- 2. tipo
    auto *typeLabel = new QLabel(stepTitle(QStringLiteral("2"), tr("Tipo de perfil")), this);
    m_typeList->setObjectName(QStringLiteral("typeTiles"));
    m_typeList->setViewMode(QListView::IconMode);
    m_typeList->setMovement(QListView::Static);
    m_typeList->setResizeMode(QListView::Adjust);
    m_typeList->setWrapping(true);
    m_typeList->setIconSize(QSize(40, 40));
    m_typeList->setGridSize(QSize(100, 88));
    m_typeList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_typeList->setUniformItemSizes(true);
    m_typeList->setWordWrap(true);
    m_typeList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_typeList->setFixedHeight(190);

    // ---- 3. dimensoes / propriedades
    m_parameterLayout->setContentsMargins(0, 0, 0, 0);
    m_parameterLayout->setLabelAlignment(Qt::AlignLeft);
    m_parameterHost->setLayout(m_parameterLayout);

    auto *dimensionPage = new QWidget(this);
    auto *dimensionLayout = new QVBoxLayout(dimensionPage);
    dimensionLayout->setContentsMargins(0, 0, 0, 0);
    dimensionLayout->addWidget(m_parameterHost);
    dimensionLayout->addWidget(m_mirror);
    dimensionLayout->addStretch(1);

    auto *propertyPage = new QWidget(this);
    auto *propertyLayout = new QFormLayout(propertyPage);
    propertyLayout->setContentsMargins(0, 0, 0, 0);
    propertyLayout->addRow(tr("Área (A)"), m_areaSpin);
    propertyLayout->addRow(tr("Inércia Iz (eixo forte)"), m_izSpin);
    propertyLayout->addRow(tr("Inércia Iy"), m_iySpin);
    propertyLayout->addRow(tr("Constante de torção (J)"), m_jSpin);
    auto *propertyHint = new QLabel(tr("Use esta opção para seções sem contorno conhecido. "
                                       "Na exibição extrudada a barra aparece como linha."), propertyPage);
    propertyHint->setWordWrap(true);
    propertyHint->setStyleSheet(QStringLiteral("color: #6e7781;"));
    propertyLayout->addRow(propertyHint);

    m_inputStack->addWidget(dimensionPage);
    m_inputStack->addWidget(propertyPage);

    auto *dimensionLabel = new QLabel(stepTitle(QStringLiteral("3"), tr("Dimensões")), this);

    // ---- nome
    auto *nameLabel = new QLabel(stepTitle(QStringLiteral("4"), tr("Nome")), this);
    m_nameEdit->setPlaceholderText(tr("Nome do perfil"));
    m_nameEdit->setClearButtonEnabled(true);

    auto *left = new QVBoxLayout();
    left->setSpacing(6);
    left->addWidget(categoryLabel);
    left->addLayout(categoryRow);
    left->addWidget(typeLabel);
    left->addWidget(m_typeList);
    left->addWidget(dimensionLabel);
    left->addWidget(m_inputStack, 1);
    left->addWidget(nameLabel);
    left->addWidget(m_nameEdit);
    m_saveToLibrary = new QCheckBox(tr("Salvar no banco de perfis do programa (disponível em todos os projetos)"), this);
    m_saveToLibrary->setChecked(true);
    left->addWidget(m_saveToLibrary);

    // ---- pre-visualizacao
    auto *previewLabel = new QLabel(tr("<b>Pré-visualização</b>"), this);
    m_properties->setTextFormat(Qt::RichText);
    m_properties->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_message->setWordWrap(true);
    m_message->setStyleSheet(QStringLiteral("color: #b3261e;"));

    auto *right = new QVBoxLayout();
    right->setSpacing(6);
    right->addWidget(previewLabel);
    right->addWidget(m_preview, 1);
    right->addWidget(m_properties);
    right->addWidget(m_message);

    auto *columns = new QHBoxLayout();
    columns->setSpacing(16);
    columns->addLayout(left, 6);
    columns->addLayout(right, 4);

    auto *cancel = new QPushButton(tr("Cancelar"), this);
    m_createButton->setDefault(true);
    auto *buttons = new QHBoxLayout();
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(m_createAssignButton);
    buttons->addWidget(m_createButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(columns, 1);
    layout->addLayout(buttons);

    connect(m_categoryGroup, &QButtonGroup::idClicked, this, [this](int id) { setCategory(static_cast<Category>(id)); });
    connect(m_typeList, &QListWidget::currentItemChanged, this, [this]() { onTypeChanged(); });
    connect(m_mirror, &QCheckBox::toggled, this, [this]() { recalculate(); });
    for (QDoubleSpinBox *spin : {m_areaSpin, m_izSpin, m_iySpin, m_jSpin}) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this]() {
            recalculate();
            updateAutoName();
            updateState();
        });
    }
    connect(m_nameEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        m_autoName = text.trimmed().isEmpty();
        if (m_autoName) {
            updateAutoName();
        }
        updateState();
    });
    connect(cancel, &QPushButton::clicked, this, &ProfileEditorWidget::cancelled);
    connect(m_createButton, &QPushButton::clicked, this, [this]() { emit createRequested(buildInfo(), false, m_saveToLibrary->isChecked()); });
    connect(m_createAssignButton, &QPushButton::clicked, this, [this]() { emit createRequested(buildInfo(), true, m_saveToLibrary->isChecked()); });

    setSelectedBarCount(0);
    reset();
}

void ProfileEditorWidget::setExistingNames(const QStringList &names)
{
    m_existingNames = names;
    updateState();
}

void ProfileEditorWidget::setSelectedBarCount(int count)
{
    m_barCount = count;
    m_createAssignButton->setText(count > 0 ? tr("Criar e atribuir a %1 barra(s)").arg(count)
                                            : tr("Criar e atribuir (nenhuma barra selecionada)"));
    updateState();
}

void ProfileEditorWidget::reset()
{
    m_autoName = true;
    m_nameEdit->clear();
    m_mirror->setChecked(false);
    setCategory(Category::Laminado);
}

void ProfileEditorWidget::startFrom(const SectionInfo &section)
{
    const Category category = ProfileLibrary::categoryOf(section.family);
    setCategory(category);
    if (category == Category::SemGeometria) {
        m_updating = true;
        m_areaSpin->setValue(section.area * 1e4);
        m_izSpin->setValue(section.iz * 1e8);
        m_iySpin->setValue(section.iy * 1e8);
        m_jSpin->setValue(section.j * 1e8);
        m_updating = false;
    } else {
        for (int i = 0; i < m_typeList->count(); ++i) {
            if (m_typeList->item(i)->data(kFamilyRole).toString() == section.family) {
                m_typeList->setCurrentRow(i);
                break;
            }
        }
        for (auto it = section.geometry.constBegin(); it != section.geometry.constEnd(); ++it) {
            if (auto *spin = m_parameterSpins.value(it.key(), nullptr)) {
                const QSignalBlocker blocker(spin);
                spin->setValue(it.value());
            }
        }
        if (m_catalogCombo) {
            const QSignalBlocker blocker(m_catalogCombo);
            m_catalogCombo->setCurrentIndex(0);
        }
        const QSignalBlocker mirrorBlocker(m_mirror);
        m_mirror->setChecked(section.mirrored);
    }
    m_autoName = false;
    m_nameEdit->setText(tr("%1 (variação)").arg(section.name));
    recalculate();
    updateState();
}

void ProfileEditorWidget::setCategory(Category category)
{
    m_category = category;
    if (auto *button = m_categoryGroup->button(static_cast<int>(category))) {
        button->setChecked(true);
    }
    const bool direct = category == Category::SemGeometria;
    m_typeList->setVisible(!direct);
    m_inputStack->setCurrentIndex(direct ? 1 : 0);

    {
        const QSignalBlocker blocker(m_typeList);
        m_typeList->clear();
        if (!direct) {
            for (const QString &family : ProfileLibrary::familiesIn(category)) {
                SectionShapes::Parameters defaults;
                if (const auto *info = SectionShapes::findFamily(family)) {
                    for (const auto &parameter : info->parameters) {
                        defaults.insert(parameter.key, parameter.defaultValue);
                    }
                }
                auto *item = new QListWidgetItem(QIcon(SectionPreviewWidget::thumbnail(family, defaults, QSize(40, 40))),
                                                 ProfileLibrary::typeLabel(family), m_typeList);
                item->setData(kFamilyRole, family);
                item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
                item->setToolTip(family);
            }
            m_typeList->setCurrentRow(0);
        }
    }
    onTypeChanged();
}

QString ProfileEditorWidget::currentFamily() const
{
    if (directMode()) {
        return {};
    }
    const auto *item = m_typeList->currentItem();
    return item ? item->data(kFamilyRole).toString() : QString();
}

bool ProfileEditorWidget::directMode() const
{
    return m_category == Category::SemGeometria;
}

QMap<QString, double> ProfileEditorWidget::currentParameters() const
{
    QMap<QString, double> result;
    for (auto it = m_parameterSpins.constBegin(); it != m_parameterSpins.constEnd(); ++it) {
        result.insert(it.key(), it.value()->value());
    }
    return result;
}

void ProfileEditorWidget::onTypeChanged()
{
    rebuildParameterFields();
    m_autoName = m_nameEdit->text().trimmed().isEmpty() || m_autoName;
    recalculate();
    updateAutoName();
    updateState();
}

void ProfileEditorWidget::rebuildParameterFields()
{
    while (m_parameterLayout->rowCount() > 0) {
        m_parameterLayout->removeRow(0);
    }
    m_parameterSpins.clear();
    m_catalogCombo = nullptr;

    const auto *family = SectionShapes::findFamily(currentFamily());
    if (!family) {
        return;
    }
    if (SectionCatalog::hasCatalog(family->name)) {
        m_catalogCombo = new QComboBox(m_parameterHost);
        m_catalogCombo->setMaxVisibleItems(18);
        m_catalogCombo->addItem(tr("(personalizado)"));
        for (const auto &profile : SectionCatalog::profiles(family->name)) {
            m_catalogCombo->addItem(profile.name);
        }
        m_parameterLayout->addRow(tr("Partir de perfil da biblioteca"), m_catalogCombo);
        connect(m_catalogCombo, &QComboBox::currentIndexChanged, this, [this](int index) { onCatalogChosen(index); });
    }
    for (const auto &parameter : family->parameters) {
        auto *spin = new QDoubleSpinBox(m_parameterHost);
        spin->setDecimals(3);
        spin->setRange(parameter.optional ? 0.0 : 0.001, 100000.0);
        spin->setValue(parameter.defaultValue);
        spin->setSingleStep(1.0);
        spin->setSuffix(tr(" mm"));
        if (parameter.optional) {
            spin->setSpecialValueText(tr("padrão"));
        }
        // O rotulo ja traz "(mm)"; usa so o nome da dimensao.
        QString label = parameter.label;
        const int paren = label.indexOf(QLatin1String(" (mm)"));
        if (paren > 0) {
            label.truncate(paren);
        }
        m_parameterLayout->addRow(label, spin);
        m_parameterSpins.insert(parameter.key, spin);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this](double) { onParameterEdited(); });
    }
}

void ProfileEditorWidget::onParameterEdited()
{
    if (m_updating) {
        return;
    }
    if (m_catalogCombo && m_catalogCombo->currentIndex() != 0) {
        const QSignalBlocker blocker(m_catalogCombo);
        m_catalogCombo->setCurrentIndex(0);
    }
    recalculate();
    updateAutoName();
    updateState();
}

void ProfileEditorWidget::onCatalogChosen(int index)
{
    const auto *family = SectionShapes::findFamily(currentFamily());
    if (index <= 0 || !family) {
        updateAutoName();
        return;
    }
    const auto &profile = SectionCatalog::profiles(family->name).at(index - 1);
    for (auto it = profile.parameters.constBegin(); it != profile.parameters.constEnd(); ++it) {
        if (auto *spin = m_parameterSpins.value(it.key(), nullptr)) {
            const QSignalBlocker blocker(spin);
            spin->setValue(it.value());
        }
    }
    recalculate();
    updateAutoName();
    updateState();
}

void ProfileEditorWidget::recalculate()
{
    if (directMode()) {
        m_area = m_areaSpin->value() * 1e-4;
        m_iz = m_izSpin->value() * 1e-8;
        m_iy = m_iySpin->value() * 1e-8;
        m_j = m_jSpin->value() * 1e-8;
        m_valid = true;
        m_preview->clear();
        m_preview->setPlaceholder(tr("Perfil definido só por propriedades:\nsem contorno para exibir."));
        m_message->clear();
    } else {
        QString error;
        const auto properties = SectionShapes::properties(currentFamily(), currentParameters(), &error);
        QString shapeError;
        const bool shapeOk = m_preview->setSection(currentFamily(), currentParameters(), m_mirror->isChecked(), &shapeError);
        m_valid = properties.has_value() && shapeOk;
        if (properties) {
            m_area = properties->area;
            m_iz = properties->iz;
            m_iy = properties->iy;
            m_j = properties->j;
        } else {
            m_area = m_iz = m_iy = m_j = 0.0;
        }
        if (!m_valid) {
            m_preview->setPlaceholder(tr("Dimensões inválidas"));
            m_message->setText(!error.isEmpty() ? error : (!shapeError.isEmpty() ? shapeError : tr("Dimensões inválidas.")));
        } else {
            m_message->clear();
        }
    }
    if (m_valid) {
        m_properties->setText(
            tr("<b>A</b> = %1 cm&sup2;<br><b>Iz</b> = %2 cm&#8308;<br><b>Iy</b> = %3 cm&#8308;<br>"
               "<b>J</b> = %4 cm&#8308;%5")
                .arg(m_area * 1e4, 0, 'f', 2)
                .arg(m_iz * 1e8, 0, 'f', 1)
                .arg(m_iy * 1e8, 0, 'f', 1)
                .arg(m_j * 1e8, 0, 'f', 2)
                .arg(directMode() ? QString() : tr("<br><span style='color:#6e7781'>J é aproximado (pré-dimensionamento).</span>")));
    } else {
        m_properties->clear();
    }
}

void ProfileEditorWidget::updateAutoName()
{
    if (!m_autoName) {
        return;
    }
    QString name;
    if (directMode()) {
        name = tr("Personalizado A=%1 cm²").arg(m_areaSpin->value(), 0, 'g', 6);
    } else if (m_catalogCombo && m_catalogCombo->currentIndex() > 0) {
        name = m_catalogCombo->currentText();
    } else if (const auto *family = SectionShapes::findFamily(currentFamily())) {
        QStringList dims;
        for (const auto &parameter : family->parameters) {
            const double value = m_parameterSpins.value(parameter.key)->value();
            if (value > 0.0) {
                dims << QString::number(value, 'g', 6);
            }
        }
        name = QStringLiteral("%1 %2").arg(family->name, dims.join(QLatin1Char('x')));
    }
    const QSignalBlocker blocker(m_nameEdit);
    m_nameEdit->setText(name);
}

void ProfileEditorWidget::updateState()
{
    const QString name = m_nameEdit->text().trimmed();
    QString problem;
    if (!m_valid) {
        problem = m_message->text();
    } else if (name.isEmpty()) {
        problem = tr("Informe um nome para o perfil.");
    } else if (m_existingNames.contains(name, Qt::CaseInsensitive)) {
        problem = tr("Já existe um perfil com esse nome no projeto.");
    }
    if (m_valid) {
        m_message->setText(problem);
    }
    const bool ok = m_valid && problem.isEmpty();
    m_createButton->setEnabled(ok);
    m_createAssignButton->setEnabled(ok && m_barCount > 0);
}

ProfileEditorWidget::SectionInfo ProfileEditorWidget::buildInfo() const
{
    SectionInfo info;
    info.name = m_nameEdit->text().trimmed();
    info.area = m_area;
    info.iz = m_iz;
    info.iy = m_iy;
    info.j = m_j;
    if (!directMode()) {
        info.family = currentFamily();
        info.geometry = currentParameters();
        info.mirrored = m_mirror->isChecked();
    }
    return info;
}

} // namespace Structura::UI
