#pragma once

#include <QVector3D>
#include <QUuid>

#include "core/model/GridLine.h"

class SceneController;

namespace Structura::UI {

class GridInsertController
{
public:
    struct State {
        Structura::Model::GridLine::Axis axis {Structura::Model::GridLine::Axis::X};
        bool active {false};
        bool pointerValid {false};
        double pointerCoord1 {0.0};
        double pointerCoord2 {0.0};
        double pointerAxisCoord {0.0};
        QUuid highlightedLineId;
        QUuid referenceLineId;
        double referenceCoord1 {0.0};
        double referenceCoord2 {0.0};
        bool referenceLocked {false};
        QString inputBuffer;
        double typedValue {0.0};
        bool hasTypedValue {false};
        double ghostCoord1 {0.0};
        double ghostCoord2 {0.0};
        bool ghostVisible {false};
    };

    static void resetState(State &state);
    static void beginInsert(State &state, Structura::Model::GridLine::Axis axis, SceneController *sceneController);
    static void cancelInsert(State &state, SceneController *sceneController);
    static void updateFromPoint(State &state, const QVector3D &worldPoint, SceneController *sceneController);
    static void refreshVisuals(State &state, SceneController *sceneController);
};

} // namespace Structura::UI
