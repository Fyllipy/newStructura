#include <QtTest/QtTest>

#include "ui/KFactorDialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QFontDatabase>

using namespace Structura::UI;
using Mode = KAxisSetting::Mode;

class TestKFactorDialog : public QObject
{
    Q_OBJECT

private slots:
    void typesCarryTheoreticalAndRecommendedValues()
    {
        const auto &types = kTypes();
        QCOMPARE(types.size(), 6);
        QCOMPARE(types.at(0).theoretical, 0.5);
        QCOMPARE(types.at(0).recommended, 0.65);
        QCOMPARE(types.at(2).theoretical, 1.0);
        QCOMPARE(types.at(4).theoretical, 2.0);
        QCOMPARE(types.at(4).recommended, 2.1);
    }

    void eachAxisCanUseADifferentForm()
    {
        Structura::App::ProjectData::DesignMemberParams current;
        KFactorDialog dialog({3, 4}, current);
        dialog.show();
        const auto cards = dialog.findChildren<KTypeCard *>();
        QCOMPARE(cards.size(), 6);
        QTest::mouseClick(cards.at(4), Qt::LeftButton);   // engastada-livre (balanco): K recomendado = 2,1

        const auto combos = dialog.findChildren<QComboBox *>();
        const auto spins = dialog.findChildren<QDoubleSpinBox *>();
        QCOMPARE(combos.size(), 3);
        QCOMPARE(spins.size(), 3);
        // ao escolher um tipo, os eixos "Nao alterar" passam a usar o tipo
        auto settings = dialog.settings();
        QCOMPARE(settings.x.mode, Mode::Type);
        QCOMPARE(settings.x.value, 2.1);

        combos.at(1)->setCurrentIndex(2);   // Ky: K arbitrario
        spins.at(1)->setValue(1.35);
        combos.at(2)->setCurrentIndex(3);   // Kz: comprimento efetivo
        spins.at(2)->setValue(3.25);
        settings = dialog.settings();
        QCOMPARE(settings.y.mode, Mode::Coefficient);
        QCOMPARE(settings.y.value, 1.35);
        QCOMPARE(settings.z.mode, Mode::Length);
        QCOMPARE(settings.z.value, 3.25);

        const QByteArray shot = qgetenv("STRUCTURA_K_SHOT");
        if (!shot.isEmpty()) {
            dialog.grab().save(QString::fromLocal8Bit(shot));
        }
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    // O plugin offscreen nao traz fontes: carrega uma do sistema para as capturas terem texto legivel.
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf"));
    TestKFactorDialog test;
    return QTest::qExec(&test, argc, argv);
}
#include "TestKFactorDialog.moc"
