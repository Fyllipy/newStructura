#pragma once

#include <QString>

namespace Structura::Model {

inline bool isPositive(double value) noexcept
{
    return value > 0.0;
}

inline bool hasName(const QString &name) noexcept
{
    return !name.isEmpty();
}

} // namespace Structura::Model
