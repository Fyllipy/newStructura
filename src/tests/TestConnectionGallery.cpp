// Galeria das miniaturas da tela de selecao de ligacoes: todas as tipologias devem ter imagens distintas.
#include <QtTest/QtTest>

#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionService.h"
#include "ui/ConnectionPreview.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QImage>
#include <QPainter>

using Structura::App::ProjectData::NodeConnection;
using namespace Structura::App::Services;

class TestConnectionGallery : public QObject
{
    Q_OBJECT

private slots:
    void everyTypeHasADistinctThumbnail()
    {
        QVector<int> types = {NodeConnection::kShearTab, NodeConnection::kDoubleAngle, NodeConnection::kGusset, NodeConnection::kTubular,
                              NodeConnection::kEndPlate, NodeConnection::kSplice, NodeConnection::kBasePlate, NodeConnection::kColdFormed};
        for (const auto &entry : ConnectionCatalog::entries()) {
            types.append(entry.id);
        }
        const QSize size(300, 200);
        const int columns = 4;
        QImage sheet(columns * size.width(), ((types.size() + columns - 1) / columns) * (size.height() + 18), QImage::Format_ARGB32);
        sheet.fill(Qt::white);
        QPainter painter(&sheet);
        QHash<QByteArray, int> seen;
        for (int i = 0; i < types.size(); ++i) {
            const QImage image = Structura::UI::ConnectionPreview::example(types[i], size).toImage().convertToFormat(QImage::Format_ARGB32);
            const QByteArray hash = QCryptographicHash::hash(QByteArray(reinterpret_cast<const char *>(image.constBits()), static_cast<qsizetype>(image.sizeInBytes())), QCryptographicHash::Md5);
            if (seen.contains(hash)) {
                QFAIL(qPrintable(QStringLiteral("miniaturas identicas: %1 e %2").arg(ConnectionService::typeName(seen.value(hash)), ConnectionService::typeName(types[i]))));
            }
            seen.insert(hash, types[i]);
            const QPoint at((i % columns) * size.width(), (i / columns) * (size.height() + 18));
            painter.drawImage(at, image);
            painter.setPen(Qt::black);
            painter.drawText(QRect(at.x() + 2, at.y() + size.height(), size.width() - 4, 18), Qt::AlignLeft | Qt::AlignVCenter, QString::number(types[i]));
        }
        painter.end();
        const QByteArray shots = qgetenv("STRUCTURA_SHOT_DIR");
        if (!shots.isEmpty()) {
            sheet.save(QString::fromLocal8Bit(shots) + QStringLiteral("/connection_gallery.png"));
        }
    }
};

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(resources);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestConnectionGallery test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestConnectionGallery.moc"
