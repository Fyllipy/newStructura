#include "app/services/DstvExporter.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

namespace {

QString n2(double v)
{
    return QString::number(v, 'f', 2);
}

QString line(const QString &text)
{
    return QStringLiteral("  ") + text + QLatin1Char('\n');
}

} // namespace

QString DstvExporter::plateToNc1(const FabPart &part, const QString &pieceMark, const QString &steelGrade, const Options &options)
{
    // comprimento = maior dimensao da chapa; largura = menor. Os furos sao reescritos nesse referencial (x ao longo do comprimento, y da borda inferior)
    const bool swap = part.width < part.height;
    const double length = swap ? part.height : part.width;
    const double width = swap ? part.width : part.height;
    auto mapped = [&](double a, double b) { return swap ? std::array<double, 2> {b, a} : std::array<double, 2> {a, b}; };

    QString out;
    out += QStringLiteral("ST\n");
    out += line(options.order.isEmpty() ? QStringLiteral("-") : options.order);
    out += line(options.drawing.isEmpty() ? QStringLiteral("-") : options.drawing);
    out += line(options.phase);
    out += line(pieceMark);
    out += line(steelGrade.isEmpty() ? QStringLiteral("-") : steelGrade);
    out += line(QString::number(part.quantity));
    out += line(QStringLiteral("PL%1*%2").arg(n2(part.thickness), n2(width)));
    out += line(QStringLiteral("B"));
    out += line(n2(length));
    out += line(n2(width));          // altura do perfil (largura da chapa)
    out += line(n2(width));          // largura da mesa
    out += line(n2(part.thickness)); // espessura da mesa
    out += line(n2(part.thickness)); // espessura da alma
    out += line(n2(0.0));            // raio
    out += line(n2(width * part.thickness * 7850.0e-6));   // kg/m (largura mm x espessura mm x 7850 kg/m3 -> kg por metro)
    out += line(n2(2.0 * (width + part.thickness) * 1.0e-3));   // area de pintura por metro (m2/m)
    for (int i = 0; i < 4; ++i) {
        out += line(n2(0.0));        // angulos de corte (alma e mesa, inicio e fim)
    }
    if (!part.holes.empty()) {
        out += QStringLiteral("BO\n");
        for (const FabHole &h : part.holes) {
            const auto p = mapped(h.a, h.b);
            out += line(QStringLiteral("v %1 %2 %3").arg(n2(p[0]), n2(p[1]), n2(h.diameter)));
        }
    }
    if (!part.rectangular && part.outline.size() >= 3) {
        out += QStringLiteral("AK\n");
        for (const auto &v : part.outline) {
            const auto p = mapped(v[0], v[1]);
            out += line(QStringLiteral("v %1 %2 %3").arg(n2(p[0]), n2(p[1]), n2(0.0)));
        }
        const auto first = mapped(part.outline.front()[0], part.outline.front()[1]);
        out += line(QStringLiteral("v %1 %2 %3").arg(n2(first[0]), n2(first[1]), n2(0.0)));
    }
    out += QStringLiteral("EN\n");
    return out;
}

QStringList DstvExporter::exportConnections(const QVector<Structura::App::ProjectData::NodeConnection> &connections, const QString &directory, const Options &options, QString *error)
{
    QStringList written;
    if (!QDir().mkpath(directory)) {
        if (error) {
            *error = QStringLiteral("Nao foi possivel criar a pasta %1.").arg(directory);
        }
        return written;
    }
    for (const auto &connection : connections) {
        const FabAssembly fab = FabricationModel::fromConnection(connection);
        for (const FabPart &part : fab.parts) {
            if (part.kind != FabPart::Kind::Plate) {
                continue;
            }
            const QString mark = QStringLiteral("L%1-%2").arg(connection.id).arg(part.mark);
            const QString grade = part.material.isEmpty() ? QString() : QString(part.material).replace(QLatin1Char(' '), QLatin1Char('_'));
            const QString path = QDir(directory).filePath(QStringLiteral("lig%1_%2.nc1").arg(connection.id).arg(part.mark));
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                if (error) {
                    *error = QStringLiteral("Falha ao gravar %1.").arg(path);
                }
                return written;
            }
            file.write(plateToNc1(part, mark, grade, options).toLatin1());
            written << path;
        }
    }
    return written;
}

} // namespace Structura::App::Services
