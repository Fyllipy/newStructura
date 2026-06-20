#pragma once

namespace Structura::Visualization {

/// Esforços internos em uma posição x ao longo do elemento.
struct InternalForces {
    double N{0};   ///< Esforço normal
    double Vy{0};  ///< Cortante Y local
    double Vz{0};  ///< Cortante Z local
    double Mt{0};  ///< Torção
    double My{0};  ///< Momento fletor Y
    double Mz{0};  ///< Momento fletor Z
};

/**
 * @brief Interface genérica de avaliação de esforços internos.
 *
 * DiagramMeshBuilder depende exclusivamente desta interface.
 * A implementação concreta pode ser substituída sem alterar o pipeline
 * de visualização.
 *
 * Expansão futura: criar implementação baseada em
 *     element->evaluateInternalForces(x)
 * quando o solver disponibilizar avaliação direta.
 */
class InternalForceEvaluator {
public:
    virtual ~InternalForceEvaluator() = default;

    /**
     * @brief Avalia os esforços internos na posição x ao longo do eixo local.
     * @param x Posição ao longo do eixo local, onde 0 ≤ x ≤ length().
     * @return Todos os seis componentes de esforço interno.
     */
    virtual InternalForces evaluate(double x) const = 0;

    /// Comprimento total do elemento.
    virtual double length() const = 0;
};

} // namespace Structura::Visualization
