#pragma once

#include "app/ProjectDataTypes.h"
#include "app/SolverBridge.h"

#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Forcas nocionais (NBR 8800:2024, 4.9.7.2): forcas horizontais iguais a 0,3 % das cargas gravitacionais de cada no.
 *
 * Como o solver e linear, cada caso de carga ganha dois "casos nocionais" (X e Y) com forcas horizontais proporcionais as suas
 * cargas verticais descendentes (peso proprio, cargas nodais e distribuidas globais em -Z). Cada combinacao ultima gera quatro
 * variantes (+X, -X, +Y, -Y) que somam o caso nocional de cada parcela ponderado pelo fator dessa parcela na combinacao.
 */
class NotionalLoadService
{
public:
    static constexpr double kDefaultFactor = 0.003;

    /// Chave do caso nocional da direcao `axis` (0 = X, 1 = Y) e do caso de carga `caseName`.
    [[nodiscard]] static QString key(int axis, const QString &caseName);
    [[nodiscard]] static bool isNotionalKey(const QString &key);

    /// Modelo do caso nocional: so forcas horizontais nodais; carregamentos originais removidos.
    [[nodiscard]] static Structura::App::SolverBridge::ModelData
    notionalModel(const Structura::App::SolverBridge::ModelData &caseModel, int axis, double factor);

    /// Combinacoes originais mais as variantes com forcas nocionais das combinacoes ultimas (nomes nao iniciados por "ELS").
    [[nodiscard]] static QVector<Structura::App::ProjectData::LoadCombination>
    withVariants(const QVector<Structura::App::ProjectData::LoadCombination> &combinations);
};

} // namespace Structura::App::Services
