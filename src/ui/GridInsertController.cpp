#include "GridInsertController.h"

#include "viz/SceneController.h"

namespace Structura::UI {

void GridInsertController::resetState(State &state)
{
    Structura::Model::GridLine::Axis axis = state.axis;
    state = State{};
    state.axis = axis;
}

void GridInsertController::beginInsert(State &state,
                                       Structura::Model::GridLine::Axis axis,
                                       SceneController *sceneController)
{
    resetState(state);
    state.axis = axis;
    state.active = true;
    state.pointerValid = false;
    state.ghostVisible = false;

    if (sceneController) {
        sceneController->hideGridGhostLine();
        sceneController->clearHighlightedGridLine();
    }
}

void GridInsertController::cancelInsert(State &state, SceneController *sceneController)
{
    if (sceneController) {
        sceneController->hideGridGhostLine();
        sceneController->clearHighlightedGridLine();
    }
    resetState(state);
    state.active = false;
}

void GridInsertController::updateFromPoint(State &state,
                                           const QVector3D &worldPoint,
                                           SceneController *sceneController)
{
    if (!state.active) {
        return;
    }

    state.pointerValid = true;

    switch (state.axis) {
    case Structura::Model::GridLine::Axis::X:
        state.pointerCoord1 = worldPoint.y();
        state.pointerCoord2 = worldPoint.z();
        state.pointerAxisCoord = worldPoint.x();
        break;
    case Structura::Model::GridLine::Axis::Y:
        state.pointerCoord1 = worldPoint.x();
        state.pointerCoord2 = worldPoint.z();
        state.pointerAxisCoord = worldPoint.y();
        break;
    case Structura::Model::GridLine::Axis::Z:
        state.pointerCoord1 = worldPoint.x();
        state.pointerCoord2 = worldPoint.y();
        state.pointerAxisCoord = worldPoint.z();
        break;
    }

    if (!state.referenceLocked) {
        state.highlightedLineId = QUuid();
        if (sceneController) {
            const auto nearest = sceneController->nearestGridLineId(state.axis, state.pointerCoord1, state.pointerCoord2);
            if (nearest.has_value()) {
                state.highlightedLineId = *nearest;
                state.referenceLineId = *nearest;
                if (const auto *line = sceneController->findGridLine(*nearest)) {
                    state.referenceCoord1 = line->coordinate1();
                    state.referenceCoord2 = line->coordinate2();
                }
            } else {
                state.referenceLineId = QUuid();
                state.referenceCoord1 = state.pointerCoord1;
                state.referenceCoord2 = state.pointerCoord2;
            }
        }
    }
}

void GridInsertController::refreshVisuals(State &state, SceneController *sceneController)
{
    if (!state.active) {
        return;
    }

    if (!state.pointerValid) {
        if (sceneController) {
            sceneController->hideGridGhostLine();
            sceneController->clearHighlightedGridLine();
        }
        state.ghostVisible = false;
        return;
    }

    const bool useReferenceHighlight = state.referenceLocked && !state.referenceLineId.isNull();
    const QUuid highlightId = useReferenceHighlight ? state.referenceLineId : state.highlightedLineId;
    if (sceneController) {
        if (highlightId.isNull()) {
            sceneController->clearHighlightedGridLine();
        } else {
            sceneController->setHighlightedGridLine(highlightId);
        }
    }

    double coord1 = state.pointerCoord1;
    double coord2 = state.pointerCoord2;

    if (state.referenceLocked) {
        coord1 = state.referenceCoord1;
    }
    if (state.hasTypedValue) {
        coord1 = state.referenceCoord1 + state.typedValue;
    }

    state.ghostCoord1 = coord1;
    state.ghostCoord2 = coord2;

    if (sceneController) {
        sceneController->showGridGhostLine(state.axis, coord1, coord2);
    }
    state.ghostVisible = true;
}

} // namespace Structura::UI
