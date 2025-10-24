#pragma once

// Backward compatibility wrapper for existing code
// This file now includes the refactored core models from src/core/model/
#include "core/model/Vector3.h"
#include "core/model/ModelEntities.h"

// All classes (Node, Bar, Material, Section, GridLine) are now defined in
// src/core/model/ModelEntities.h and are accessible via this include.
// The API is backward compatible with the previous implementation.

