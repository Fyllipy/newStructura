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

    /// Variantes das combinacoes ultimas (nomes nao iniciados por "ELS") com imperfeicao geometrica inicial: mesmas parcelas, geometria com desaprumo
    /// (+X, -X, +Y, -Y); resolvidas pela analise de 2a ordem de cada combinacao.
    [[nodiscard]] static QVector<Structura::App::ProjectData::LoadCombination>
    withGeometricVariants(const QVector<Structura::App::ProjectData::LoadCombination> &combinations);

    /// Variantes conforme as opcoes do projeto: geometricas (2a ordem com imperfeicao direta) ou nocionais; sem variantes se nenhuma estiver ativa.
    [[nodiscard]] static QVector<Structura::App::ProjectData::LoadCombination>
    variantsFor(const Structura::App::ProjectData::DesignSettings &settings, const QVector<Structura::App::ProjectData::LoadCombination> &combinations);

    /// Desaprumo inicial: cada no se desloca na direcao `axis` (0 = X, 1 = Y), no sentido `sign`, de `ratio` x (z - z da base); a base e o menor z entre os nos
    /// com apoio (ou entre todos). Nos de casca acompanham.
    static void applyImperfection(Structura::App::SolverBridge::ModelData &model, int axis, int sign, double ratio);

    /// Combinacoes originais mais as variantes com forcas nocionais das combinacoes ultimas (nomes nao iniciados por "ELS").
    [[nodiscard]] static QVector<Structura::App::ProjectData::LoadCombination>
    withVariants(const QVector<Structura::App::ProjectData::LoadCombination> &combinations);
};

} // namespace Structura::App::Services
