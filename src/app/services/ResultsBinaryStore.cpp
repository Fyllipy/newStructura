#include "ResultsBinaryStore.h"

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QMap>

#include <cstring>

namespace Structura::App::Services {

using namespace Structura::App::ProjectData;

namespace {

constexpr char kMagic[4] = {'S', 'T', 'R', 'R'};

template <std::size_t N>
QDataStream &putArray(QDataStream &out, const std::array<double, N> &values)
{
    for (const double v : values) {
        out << v;
    }
    return out;
}

template <std::size_t N>
QDataStream &getArray(QDataStream &in, std::array<double, N> &values)
{
    for (double &v : values) {
        in >> v;
    }
    return in;
}

QDataStream &putNodes(QDataStream &out, const QVector<StoredNodeResult> &nodes)
{
    out << static_cast<qint32>(nodes.size());
    for (const auto &n : nodes) {
        out << static_cast<qint32>(n.nodeId);
        putArray(out, n.displacements);
        putArray(out, n.reactions);
    }
    return out;
}

QDataStream &getNodes(QDataStream &in, QVector<StoredNodeResult> &nodes)
{
    qint32 count = 0;
    in >> count;
    nodes.clear();
    if (count < 0 || count > 50'000'000) {
        in.setStatus(QDataStream::ReadCorruptData);
        return in;
    }
    nodes.reserve(count);
    for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        StoredNodeResult n;
        qint32 id = 0;
        in >> id;
        n.nodeId = id;
        getArray(in, n.displacements);
        getArray(in, n.reactions);
        nodes.push_back(n);
    }
    return in;
}

QByteArray encodeRun(const StoredRun &run)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out.setFloatingPointPrecision(QDataStream::DoublePrecision);
    out << run.name;
    putNodes(out, run.nodes);
    out << static_cast<qint32>(run.members.size());
    for (const auto &m : run.members) {
        out << static_cast<qint32>(m.memberId) << m.hasWarping;
        putArray(out, m.forces);
        putArray(out, m.auxK);
        putArray(out, m.warpingRate);
        putArray(out, m.warpingForce);
    }
    putNodes(out, run.shellNodes);
    out << static_cast<qint32>(run.shellElements.size());
    for (const auto &s : run.shellElements) {
        out << static_cast<qint32>(s.shellId);
        putArray(out, s.values);
        putArray(out, s.axes);
    }
    return data;
}

bool decodeRun(const QByteArray &data, StoredRun &run)
{
    QDataStream in(data);
    in.setVersion(QDataStream::Qt_6_0);
    in.setFloatingPointPrecision(QDataStream::DoublePrecision);
    run = StoredRun {};
    in >> run.name;
    getNodes(in, run.nodes);
    qint32 count = 0;
    in >> count;
    if (count < 0 || count > 50'000'000) {
        return false;
    }
    for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        StoredMemberResult m;
        qint32 id = 0;
        in >> id >> m.hasWarping;
        m.memberId = id;
        getArray(in, m.forces);
        getArray(in, m.auxK);
        getArray(in, m.warpingRate);
        getArray(in, m.warpingForce);
        run.members.push_back(m);
    }
    getNodes(in, run.shellNodes);
    in >> count;
    if (count < 0 || count > 50'000'000) {
        return false;
    }
    for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        StoredShellResult s;
        qint32 id = 0;
        in >> id;
        s.shellId = id;
        getArray(in, s.values);
        getArray(in, s.axes);
        run.shellElements.push_back(s);
    }
    return in.status() == QDataStream::Ok;
}

QByteArray encodeDesign(const StoredResults &r)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out.setFloatingPointPrecision(QDataStream::DoublePrecision);
    out << static_cast<qint32>(r.design.size());
    for (const auto &d : r.design) {
        out << static_cast<qint32>(d.memberId) << d.section << d.eta << d.check << d.combination << d.lengthM;
    }
    return data;
}

bool decodeDesign(const QByteArray &data, StoredResults &r)
{
    QDataStream in(data);
    in.setVersion(QDataStream::Qt_6_0);
    in.setFloatingPointPrecision(QDataStream::DoublePrecision);
    qint32 count = 0;
    in >> count;
    if (count < 0 || count > 50'000'000) {
        return false;
    }
    r.design.clear();
    for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        StoredDesignResult d;
        qint32 id = 0;
        in >> id >> d.section >> d.eta >> d.check >> d.combination >> d.lengthM;
        d.memberId = id;
        r.design.push_back(d);
    }
    return in.status() == QDataStream::Ok;
}

QByteArray encodeModes(const QString &source, const QVector<double> &values, const QVector<std::array<double, 3>> &ratios)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out.setFloatingPointPrecision(QDataStream::DoublePrecision);
    out << source << static_cast<qint32>(values.size());
    for (const double v : values) {
        out << v;
    }
    out << static_cast<qint32>(ratios.size());
    for (const auto &r : ratios) {
        putArray(out, r);
    }
    return data;
}

bool decodeModes(const QByteArray &data, QString &source, QVector<double> &values, QVector<std::array<double, 3>> &ratios)
{
    QDataStream in(data);
    in.setVersion(QDataStream::Qt_6_0);
    in.setFloatingPointPrecision(QDataStream::DoublePrecision);
    qint32 count = 0;
    in >> source >> count;
    if (count < 0 || count > 1'000'000) {
        return false;
    }
    values.clear();
    for (qint32 i = 0; i < count; ++i) {
        double v = 0.0;
        in >> v;
        values.push_back(v);
    }
    in >> count;
    if (count < 0 || count > 1'000'000) {
        return false;
    }
    ratios.clear();
    for (qint32 i = 0; i < count; ++i) {
        std::array<double, 3> r {};
        getArray(in, r);
        ratios.push_back(r);
    }
    return in.status() == QDataStream::Ok;
}

struct Entry {
    QString name;
    qint64 offset {0};
    qint64 size {0};
    quint16 checksum {0};
};

struct Header {
    QString fingerprint;
    QVector<Entry> entries;
    qint64 dataStart {0};
};

bool readHeader(QIODevice &device, Header &header, QString *error)
{
    auto fail = [&](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };
    if (!device.seek(0)) {
        return fail(QStringLiteral("Arquivo de resultados nao posicionavel."));
    }
    QDataStream in(&device);
    in.setVersion(QDataStream::Qt_6_0);
    char magic[4] = {};
    if (in.readRawData(magic, 4) != 4 || std::memcmp(magic, kMagic, 4) != 0) {
        return fail(QStringLiteral("Arquivo de resultados invalido (assinatura)."));
    }
    quint32 version = 0;
    in >> version;
    if (version == 0 || version > ResultsBinaryStore::kVersion) {
        return fail(QStringLiteral("Versao %1 do arquivo de resultados nao suportada (maxima %2).").arg(version).arg(ResultsBinaryStore::kVersion));
    }
    qint32 count = 0;
    in >> header.fingerprint >> count;
    if (in.status() != QDataStream::Ok || count < 0 || count > 10'000'000) {
        return fail(QStringLiteral("Indice de resultados corrompido."));
    }
    for (qint32 i = 0; i < count; ++i) {
        Entry e;
        in >> e.name >> e.offset >> e.size >> e.checksum;
        header.entries.push_back(e);
    }
    if (in.status() != QDataStream::Ok) {
        return fail(QStringLiteral("Indice de resultados corrompido."));
    }
    header.dataStart = device.pos();
    return true;
}

bool readSection(QIODevice &device, const Header &header, const Entry &entry, QByteArray &data, QString *error)
{
    if (!device.seek(header.dataStart + entry.offset)) {
        if (error) {
            *error = QStringLiteral("Secao fora do arquivo: %1").arg(entry.name);
        }
        return false;
    }
    data = device.read(entry.size);
    if (data.size() != entry.size || qChecksum(QByteArrayView(data), Qt::ChecksumIso3309) != entry.checksum) {
        if (error) {
            *error = QStringLiteral("Secao corrompida (soma de verificacao): %1").arg(entry.name);
        }
        return false;
    }
    return true;
}

} // namespace

bool ResultsBinaryStore::write(QIODevice &device, const StoredResults &results, QString *error)
{
    QVector<QPair<QString, QByteArray>> sections;
    for (const auto &run : results.runs) {
        sections.push_back({QStringLiteral("run:") + run.name, encodeRun(run)});
    }
    sections.push_back({QStringLiteral("design"), encodeDesign(results)});
    sections.push_back({QStringLiteral("buckling"), encodeModes(results.bucklingCombination, results.bucklingFactors, {})});
    sections.push_back({QStringLiteral("modal"), encodeModes(results.modalSource, results.modalOmega, results.modalMassRatio)});

    QByteArray head;
    {
        QDataStream out(&head, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out.writeRawData(kMagic, 4);
        out << kVersion << results.fingerprint << static_cast<qint32>(sections.size());
        qint64 offset = 0;
        for (const auto &s : sections) {
            out << s.first << offset << static_cast<qint64>(s.second.size()) << qChecksum(QByteArrayView(s.second), Qt::ChecksumIso3309);
            offset += s.second.size();
        }
    }
    if (device.write(head) != head.size()) {
        if (error) {
            *error = QStringLiteral("Falha ao gravar o cabecalho dos resultados.");
        }
        return false;
    }
    for (const auto &s : sections) {
        if (device.write(s.second) != s.second.size()) {
            if (error) {
                *error = QStringLiteral("Falha ao gravar a secao %1.").arg(s.first);
            }
            return false;
        }
    }
    return true;
}

bool ResultsBinaryStore::read(QIODevice &device, StoredResults &results, QString *error)
{
    Header header;
    if (!readHeader(device, header, error)) {
        return false;
    }
    results = StoredResults {};
    results.fingerprint = header.fingerprint;
    for (const Entry &entry : header.entries) {
        QByteArray data;
        if (!readSection(device, header, entry, data, error)) {
            return false;
        }
        bool ok = true;
        if (entry.name.startsWith(QLatin1String("run:"))) {
            StoredRun run;
            ok = decodeRun(data, run);
            results.runs.push_back(std::move(run));
        } else if (entry.name == QLatin1String("design")) {
            ok = decodeDesign(data, results);
        } else if (entry.name == QLatin1String("buckling")) {
            QVector<std::array<double, 3>> unused;
            ok = decodeModes(data, results.bucklingCombination, results.bucklingFactors, unused);
        } else if (entry.name == QLatin1String("modal")) {
            ok = decodeModes(data, results.modalSource, results.modalOmega, results.modalMassRatio);
        }   // secoes desconhecidas (versoes futuras compativeis) sao ignoradas
        if (!ok) {
            if (error) {
                *error = QStringLiteral("Secao ilegivel: %1").arg(entry.name);
            }
            return false;
        }
    }
    return true;
}

QStringList ResultsBinaryStore::sections(QIODevice &device, QString *fingerprint, QString *error)
{
    Header header;
    if (!readHeader(device, header, error)) {
        return {};
    }
    if (fingerprint) {
        *fingerprint = header.fingerprint;
    }
    QStringList names;
    for (const Entry &e : header.entries) {
        names << e.name;
    }
    return names;
}

bool ResultsBinaryStore::readRun(QIODevice &device, const QString &name, StoredRun &run, QString *error)
{
    Header header;
    if (!readHeader(device, header, error)) {
        return false;
    }
    for (const Entry &entry : header.entries) {
        if (entry.name == QLatin1String("run:") + name) {
            QByteArray data;
            if (!readSection(device, header, entry, data, error)) {
                return false;
            }
            if (!decodeRun(data, run)) {
                if (error) {
                    *error = QStringLiteral("Secao ilegivel: %1").arg(entry.name);
                }
                return false;
            }
            return true;
        }
    }
    if (error) {
        *error = QStringLiteral("Hipotese nao encontrada: %1").arg(name);
    }
    return false;
}

} // namespace Structura::App::Services
