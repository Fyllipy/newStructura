#include "LocalCoordinateSystem.h"

#include <cmath>
#include <stdexcept>

namespace Structura::Geometry {

namespace {
    constexpr double kMinBarLength = 1e-9;
}

DefaultLocalAxisProvider::DefaultLocalAxisProvider()
    : m_parallelEps(1e-5)
{
}

LCS DefaultLocalAxisProvider::computeLCS(const std::array<double, 3> &pointA,
                                          const std::array<double, 3> &pointB,
                                          const std::optional<std::array<double, 3>> &kPoint) const
{
    // Step 1: Compute x' = normalized(B - A)
    auto barVector = vectorAB(pointA, pointB);
    auto [xPrime, barLength] = normalize(barVector);
    
    if (barLength < kMinBarLength) {
        throw std::runtime_error("Bar length too small to compute LCS");
    }
    
    // Step 2: Determine auxiliary vector v
    std::array<double, 3> vAux;
    
    if (kPoint.has_value()) {
        // Use K point if provided
        vAux = vectorAB(pointA, kPoint.value());
        auto [vNorm, vLen] = normalize(vAux);
        
        // Check if K point creates a valid auxiliary vector
        if (vLen < kMinBarLength || areParallel(xPrime, vNorm)) {
            // K point is invalid (too close to A or parallel to bar)
            // Fall back to automatic selection
            vAux = selectAuxiliaryVector(xPrime);
        } else {
            vAux = vNorm;
        }
    } else {
        // No K point provided, use automatic selection
        vAux = selectAuxiliaryVector(xPrime);
    }
    
    // Step 3: Compute z' = normalized(x' × vAux)
    auto zCross = cross(xPrime, vAux);
    auto [zPrime, zLen] = normalize(zCross);
    
    if (zLen < kMinBarLength) {
        throw std::runtime_error("Failed to compute z-axis: vectors are parallel");
    }
    
    // Step 4: Compute y' = z' × x' (right-hand rule)
    auto yPrime = cross(zPrime, xPrime);
    // y' should already be normalized since z' and x' are orthonormal
    
    // Step 5: Compute origin at midpoint
    std::array<double, 3> origin = {
        (pointA[0] + pointB[0]) * 0.5,
        (pointA[1] + pointB[1]) * 0.5,
        (pointA[2] + pointB[2]) * 0.5
    };
    
    return LCS(xPrime, yPrime, zPrime, origin);
}

std::array<double, 3> DefaultLocalAxisProvider::vectorAB(const std::array<double, 3> &a,
                                                          const std::array<double, 3> &b) const
{
    return {
        b[0] - a[0],
        b[1] - a[1],
        b[2] - a[2]
    };
}

std::pair<std::array<double, 3>, double> DefaultLocalAxisProvider::normalize(
    const std::array<double, 3> &v) const
{
    double magnitude = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    
    if (magnitude < kMinBarLength) {
        return {{0.0, 0.0, 0.0}, 0.0};
    }
    
    return {{
        v[0] / magnitude,
        v[1] / magnitude,
        v[2] / magnitude
    }, magnitude};
}

std::array<double, 3> DefaultLocalAxisProvider::cross(const std::array<double, 3> &a,
                                                       const std::array<double, 3> &b) const
{
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]
    };
}

double DefaultLocalAxisProvider::dot(const std::array<double, 3> &a,
                                      const std::array<double, 3> &b) const
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

bool DefaultLocalAxisProvider::areParallel(const std::array<double, 3> &a,
                                            const std::array<double, 3> &b) const
{
    // Two unit vectors are parallel if |a · b| ≈ 1
    double dotProduct = dot(a, b);
    return std::abs(std::abs(dotProduct) - 1.0) < m_parallelEps;
}

std::array<double, 3> DefaultLocalAxisProvider::selectAuxiliaryVector(
    const std::array<double, 3> &xPrime) const
{
    // Try each fallback vector in order until we find one that's not parallel
    for (const auto &fallback : s_fallbackVectors) {
        if (!areParallel(xPrime, fallback)) {
            return fallback;
        }
    }
    
    // This should never happen with the three cardinal directions,
    // but return a safe default just in case
    return {0.0, 1.0, 0.0};
}

} // namespace Structura::Geometry
