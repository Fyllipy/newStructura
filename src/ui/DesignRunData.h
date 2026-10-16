#pragma once

#include "app/ProjectDataTypes.h"
#include "app/services/DesignService.h"
#include "app/services/StructuralChecks.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <vector>

/// Esforcos (ELU), B2, flechas e deslocamentos por andar (ELS) da ultima analise.
struct DesignRunData {
    QString error;
    double lengthFactor {1000.0};
    double forceFactor {1.0};
    QVector<Structura::App::Services::DesignService::MemberEffects> effects;
    QVector<Structura::App::Services::StructuralChecks::DeflectionResult> deflections;
    Structura::App::Services::StructuralChecks::Output elsStories;
    QStringList warnings;
    Structura::App::ProjectData::DesignSettings settings;   ///< com o aco das barras resolvido pelos materiais
    std::vector<Structura::App::Services::DesignService::MemberRef> refs;
    QHash<int, double> weightKg;
};
