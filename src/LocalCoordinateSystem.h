#pragma once

#include <array>
#include <optional>

namespace Structura::Geometry {

/**
 * @brief Local Coordinate System (LCS) representation
 * 
 * Represents an orthogonal coordinate system with three unit vectors
 * (x', y', z') and an origin point.
 */
struct LCS
{
    std::array<double, 3> xPrime; // Local x-axis (along bar direction)
    std::array<double, 3> yPrime; // Local y-axis
    std::array<double, 3> zPrime; // Local z-axis
    std::array<double, 3> origin; // Origin point (typically bar midpoint)
    
    LCS() = default;
    
    LCS(const std::array<double, 3> &x,
        const std::array<double, 3> &y,
        const std::array<double, 3> &z,
        const std::array<double, 3> &o)
        : xPrime(x), yPrime(y), zPrime(z), origin(o)
    {
    }
};

/**
 * @brief Interface for computing local coordinate systems for bars
 * 
 * This interface defines the contract for calculating the local axes
 * of structural bars based on their geometry and optional reference points.
 */
class ILocalAxisProvider
{
public:
    virtual ~ILocalAxisProvider() = default;
    
    /**
     * @brief Compute the local coordinate system for a bar
     * 
     * @param pointA Start point of the bar
     * @param pointB End point of the bar
     * @param kPoint Optional reference point K for defining the local z-axis orientation
     * @return LCS The computed local coordinate system
     */
    virtual LCS computeLCS(const std::array<double, 3> &pointA,
                           const std::array<double, 3> &pointB,
                           const std::optional<std::array<double, 3>> &kPoint) const = 0;
};

/**
 * @brief Default implementation of local axis provider
 * 
 * Implements the standard algorithm for computing bar local coordinate systems:
 * 1. x' = normalized(B - A) (along bar direction)
 * 2. Select auxiliary vector v from K point or fallback options
 * 3. z' = normalized(x' × v)
 * 4. y' = z' × x'
 * 5. Origin at midpoint (A + B) / 2
 */
class DefaultLocalAxisProvider : public ILocalAxisProvider
{
public:
    DefaultLocalAxisProvider();
    ~DefaultLocalAxisProvider() override = default;
    
    LCS computeLCS(const std::array<double, 3> &pointA,
                   const std::array<double, 3> &pointB,
                   const std::optional<std::array<double, 3>> &kPoint) const override;
    
    /**
     * @brief Set the epsilon tolerance for parallel vector detection
     * @param eps Tolerance value (default: 1e-5)
     */
    void setParallelEpsilon(double eps) { m_parallelEps = eps; }
    
    /**
     * @brief Get the current parallel epsilon tolerance
     */
    double parallelEpsilon() const { return m_parallelEps; }

private:
    /**
     * @brief Compute vector from A to B
     */
    std::array<double, 3> vectorAB(const std::array<double, 3> &a,
                                    const std::array<double, 3> &b) const;
    
    /**
     * @brief Normalize a vector
     * @return Normalized vector and its original magnitude
     */
    std::pair<std::array<double, 3>, double> normalize(const std::array<double, 3> &v) const;
    
    /**
     * @brief Compute cross product of two vectors
     */
    std::array<double, 3> cross(const std::array<double, 3> &a,
                                 const std::array<double, 3> &b) const;
    
    /**
     * @brief Compute dot product of two vectors
     */
    double dot(const std::array<double, 3> &a,
               const std::array<double, 3> &b) const;
    
    /**
     * @brief Check if two vectors are nearly parallel
     */
    bool areParallel(const std::array<double, 3> &a,
                     const std::array<double, 3> &b) const;
    
    /**
     * @brief Select an auxiliary vector that is not parallel to the given vector
     * @param xPrime The bar's x-axis direction
     * @return A suitable auxiliary vector
     */
    std::array<double, 3> selectAuxiliaryVector(const std::array<double, 3> &xPrime) const;
    
    double m_parallelEps; // Tolerance for parallel vector detection
    
    // Fallback auxiliary vectors to try in order
    static constexpr std::array<std::array<double, 3>, 3> s_fallbackVectors = {{
        {1.0, 0.0, 0.0},  // Global X
        {0.0, 1.0, 0.0},  // Global Y
        {0.0, 0.0, 1.0}   // Global Z
    }};
};

} // namespace Structura::Geometry
