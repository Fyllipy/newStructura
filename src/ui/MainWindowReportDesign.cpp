#include "MainWindow.h"

#include "ConnectionForces.h"

#include "DesignRunData.h"
#include "ReportDialogs.h"
#include "SelectionModel.h"
#include "app/services/DesignService.h"
#include "app/services/ReportService.h"
#include "viz/SceneController.h"
#include "app/services/SectionPropertiesService.h"

#include <QDate>
#include <QMessageBox>

#include <algorithm>
#include <cmath>

using Structura::App::Services::DesignService;
using Structura::App::Services::SectionPropertiesService;
using Structura::App::Services::ReportService;

void MainWindow::fillDesignReportInput(ReportService::Input &input)
{
        DesignRunData run;
    if (buildDesignRun(run)) {
            input.hasDesign = true;
            input.designSettings = run.settings;
            input.verdicts = DesignService::verifyAll(run.effects, run.refs, run.settings, run.lengthFactor);
            input.deflections = run.deflections;
            input.stories = run.elsStories.stories;
            input.storyResults = run.elsStories.storyResults;
            input.designWarnings = run.warnings;
            for (const auto &connection : m_extras.connections) {
                ReportService::Input::ConnectionEntry entry;
                entry.connection = connection;
                const auto forces = Structura::UI::connectionForces(run, connection, m_extras.connections);
                entry.hasForces = forces.has_value();
                entry.forces = forces.value_or(Structura::App::Services::ConnectionService::Forces {});
                entry.evaluation = Structura::App::Services::ConnectionService::evaluate(connection, entry.forces);
                input.connections.append(entry);
            }
        }
}
