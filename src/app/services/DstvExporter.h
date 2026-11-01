#pragma once

#include "app/ProjectDataTypes.h"
#include "app/services/FabricationModel.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Exporta chapas de ligacao no formato DSTV (arquivos .nc1) para maquinas de corte e furacao CNC.
 *
 * Cada chapa vira um arquivo com os blocos ST (cabecalho, codigo de perfil B = chapa), BO (furos, face v) e, para contornos nao retangulares, AK (contorno).
 * O formato segue a estrutura publica do DSTV (Stahlbau-Verband, NC-Schnittstelle): linhas com dois espacos iniciais, ponto decimal e blocos terminados por EN.
 * Variantes de maquina/importador diferem em detalhes (colunas, sufixos de referencia); valide o arquivo no software da maquina antes de produzir.
 */
class DstvExporter
{
public:
    struct Options {
        QString order;         ///< identificacao do pedido/obra
        QString drawing;       ///< identificacao do desenho
        QString phase {QStringLiteral("1")};
    };

    /// Conteudo NC1 de uma chapa (`part.kind` deve ser Plate).
    static QString plateToNc1(const FabPart &part, const QString &pieceMark, const QString &steelGrade, const Options &options);

    /// Grava um arquivo por chapa marcada de cada ligacao em `directory` (nome `lig<id>_<marca>.nc1`). Devolve os caminhos gravados.
    static QStringList exportConnections(const QVector<Structura::App::ProjectData::NodeConnection> &connections, const QString &directory, const Options &options,
                                         QString *error = nullptr);
};

} // namespace Structura::App::Services
