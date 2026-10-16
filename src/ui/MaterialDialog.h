#pragma once

#include <QDialog>

class QLineEdit;
class QDoubleSpinBox;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QCheckBox;

/// Cadastro de material com biblioteca padrao (acos estruturais, formados a frio e concretos) e opcao personalizada.
/// `pascalUnits`: o projeto usa E em Pa (forca em N); caso contrario kN/m2 (forca em kN).
class MaterialDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MaterialDialog(bool pascalUnits = false, QWidget *parent = nullptr);

    QString name() const;
    /// E e G na unidade do projeto (Pa ou kN/m2).
    double youngModulus() const;
    double shearModulus() const;
    /// Peso especifico em kN/m3 (usado no peso proprio).
    double unitWeight() const;
    /// Entrada da biblioteca escolhida (vazio = personalizado).
    QString libraryName() const { return m_library; }
    double fy() const;
    double fu() const;
    /// Salvar no banco de materiais do programa.
    bool saveToLibrary() const;
    double youngModulusGPa() const;
    double shearModulusGPa() const;

private:
    void selectEntry(const QString &libraryName);
    void applyFilter(const QString &text);
    void updateFormEnabled();
    double unitFactor() const { return m_pascal ? 1e9 : 1e6; }

    bool m_pascal;
    QString m_library;
    QLineEdit *m_search {nullptr};
    QTreeWidget *m_tree {nullptr};
    QLineEdit *m_nameEdit {nullptr};
    QDoubleSpinBox *m_eSpin {nullptr};
    QDoubleSpinBox *m_gSpin {nullptr};
    QDoubleSpinBox *m_fySpin {nullptr};
    QDoubleSpinBox *m_fuSpin {nullptr};
    QDoubleSpinBox *m_weightSpin {nullptr};
    QLabel *m_note {nullptr};
    QLabel *m_unitsNote {nullptr};
    QCheckBox *m_saveToLibrary {nullptr};
};
