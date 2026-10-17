#pragma once

#include "design/CompositeFloor.h"
#include "design/DesignSection.h"

#include <QDialog>
#include <QMap>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QPlainTextEdit;
class QTabWidget;
class QTableWidget;

namespace Structura::UI {

/// Dados de uma barra candidata a viga mista (perfil I laminado) e seus esforcos da analise.
struct CompositeCandidate {
    int memberId {0};
    QString name;
    double lengthMm {0.0};
    Structura::Design::DesignSection section;
    double d {0.0}, bf {0.0}, tf {0.0}, tw {0.0};
    double fy {345.0}, fu {450.0};
    bool hasAnalysis {false};
    double msdAnalysis {0.0};   ///< N*mm, maior momento positivo de calculo das combinacoes ultimas
    double vsdAnalysis {0.0};   ///< N
    QMap<QString, double> saved;   ///< entradas gravadas no projeto
};

/// Vigas mistas (NBR 8800, Anexo L) e lajes mistas com forma de aco incorporada (Anexo N).
class CompositeDialog final : public QDialog
{
    Q_OBJECT

public:
    CompositeDialog(const QVector<CompositeCandidate> &beams, int initialMember, const QMap<QString, double> &deckSaved, QWidget *parent = nullptr);

signals:
    void beamInputsChanged(int memberId, const QMap<QString, double> &values);
    void deckInputsChanged(const QMap<QString, double> &values);

private:
    struct Field {
        QString key;
        QDoubleSpinBox *box {nullptr};
    };

    void buildBeamTab(QTabWidget *tabs);
    void buildDeckTab(QTabWidget *tabs);
    void loadBeam(int index);
    void runBeam();
    void proposeBeamSpacing();
    void runDeck();
    [[nodiscard]] QMap<QString, double> beamValues() const;
    [[nodiscard]] QMap<QString, double> deckValues() const;
    Field addField(QFormLayout *form, QVector<Field> &list, const QString &key, const QString &label, double value, double min, double max, int decimals,
                   double step, const QString &suffix = QString(), const QString &tip = QString());
    void showChecks(QTableWidget *table, const std::vector<Structura::Design::Connections::Check> &checks);

    QVector<CompositeCandidate> m_beams;
    QMap<QString, double> m_deckSaved;
    // viga
    QComboBox *m_member {nullptr};
    QComboBox *m_slabKind {nullptr}, *m_demandSource {nullptr};
    QCheckBox *m_shored {nullptr};
    QVector<Field> m_beamFields;
    QTableWidget *m_beamTable {nullptr};
    QLabel *m_beamSummary {nullptr};
    QPlainTextEdit *m_beamMemo {nullptr};
    QDoubleSpinBox *m_msdManual {nullptr}, *m_vsdManual {nullptr};
    // laje
    QComboBox *m_continuity {nullptr};
    QVector<Field> m_deckFields;
    QTableWidget *m_deckTable {nullptr};
    QLabel *m_deckSummary {nullptr};
    QPlainTextEdit *m_deckMemo {nullptr};
    bool m_loading {false};
};

} // namespace Structura::UI
