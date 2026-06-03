/*
 *    Copyright 2026 OpenOrienteering contributors
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "course_feature.h"

#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <Qt>

#include "core/map.h"
#include "course/course_database.h"
#include "gui/main_window.h"
#include "course/course_overlay.h"
#include "fileformats/file_format_registry.h"
#include "fileformats/file_import_export.h"
#include "gui/course/course_panel.h"
#include "gui/map/map_editor.h"
#include "gui/map/map_editor_p.h"
#include "gui/map/map_widget.h"
#include "tools/place_control_tool.h"


namespace OpenOrienteering {


CourseFeature::CourseFeature(MapEditorController& controller)
: QObject(nullptr)
, controller(controller)
{
    // Create the overlay immediately — MapWidget exists by the time
    // this feature is constructed (called from createActions()).
    course_overlay = std::make_unique<CourseOverlay>(
        controller.getMainWidget(),
        controller.getMap()->courseDatabase());

    // "Course Planning" panel toggle action
    show_panel_act = new QAction(tr("Course &Planning"), this);
    show_panel_act->setCheckable(true);
    show_panel_act->setMenuRole(QAction::NoRole);
    connect(show_panel_act, &QAction::toggled,
            this, &CourseFeature::showPanelToggled);

    // "Place Control" tool action
    place_control_act = new QAction(tr("Place &Control"), this);
    place_control_act->setCheckable(true);
    place_control_act->setMenuRole(QAction::NoRole);
    connect(place_control_act, &QAction::triggered,
            this, &CourseFeature::activatePlaceControlTool);

    // "Export IOF (Full)" action
    export_iof_act = new QAction(tr("&Export IOF (full course database)…"), this);
    export_iof_act->setMenuRole(QAction::NoRole);
    connect(export_iof_act, &QAction::triggered,
            this, &CourseFeature::exportIofFull);
}


CourseFeature::~CourseFeature()
{
    if (dock_widget)
        dock_widget->deleteLater();
}


void CourseFeature::setEnabled(bool enabled)
{
    show_panel_act->setEnabled(enabled);
    place_control_act->setEnabled(enabled);
    export_iof_act->setEnabled(enabled);
}


// ── Private slots ──────────────────────────────────────────────────────────────

void CourseFeature::showPanelToggled(bool show)
{
    if (show && !dock_widget)
        createDockWidget();

    if (dock_widget)
        dock_widget->setVisible(show);
}

void CourseFeature::activatePlaceControlTool()
{
    auto* tool = new PlaceControlTool(&controller, place_control_act,
                                      controller.getMap()->courseDatabase());
    tool->setNextControlType(next_control_type);
    connect(tool, &PlaceControlTool::selectedControlChanged,
            this, &CourseFeature::onControlSelectedInTool);
    current_tool = tool;
    controller.setTool(tool);
}

void CourseFeature::onNextControlTypeChangeRequested(ControlType type)
{
    next_control_type = type;
    if (current_tool)
        current_tool->setNextControlType(type);
}

void CourseFeature::onControlSelectedInTool(const QString& control_id)
{
    if (panel)
        panel->selectControl(control_id);
}


void CourseFeature::exportIofFull()
{
    const auto& db = controller.getMap()->courseDatabase();
    if (db.numControls() == 0 && db.numCourses() == 0)
    {
        QMessageBox::information(
            controller.getWindow(),
            tr("Export IOF"),
            tr("The course database is empty. "
               "Add controls and courses in the Course Planning panel first."));
        return;
    }

    const auto* format = FileFormats.findFormat("full-iof-course");
    if (!format)
    {
        QMessageBox::warning(controller.getWindow(), tr("Export IOF"),
                             tr("Export format not available."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        controller.getWindow(),
        tr("Export IOF Course Data"),
        {},
        tr("IOF Data Standard 3.0 (*.xml)"));

    if (path.isEmpty())
        return;

    auto exporter = format->makeExporter(path, controller.getMap(), nullptr);
    if (!exporter->doExport())
    {
        const auto& warnings = exporter->warnings();
        if (!warnings.empty())
        {
            QMessageBox::warning(controller.getWindow(),
                                 tr("Export IOF"),
                                 warnings.back());
        }
        return;
    }

    // Show any non-fatal warnings
    const auto& warnings = exporter->warnings();
    if (!warnings.empty())
    {
        QString msg;
        for (const auto& w : warnings)
            msg += w + QLatin1Char('\n');
        QMessageBox::information(controller.getWindow(),
                                 tr("Export IOF"), msg);
    }
}


// ── Dock widget ────────────────────────────────────────────────────────────────

void CourseFeature::createDockWidget()
{
    Q_ASSERT(!dock_widget);

    panel = new CoursePanelWidget(
        *controller.getMap(),
        controller.getMap()->courseDatabase(),
        course_overlay.get());

    connect(panel, &CoursePanelWidget::controlSelected,
            this, [this](const QString& id) {
                Q_UNUSED(id)
            });
    connect(panel, &CoursePanelWidget::nextControlTypeChangeRequested,
            this, &CourseFeature::onNextControlTypeChangeRequested);

    auto* main_window = controller.getWindow();
    dock_widget = new EditorDockWidget(tr("Course Planning"),
                                       show_panel_act, &controller, main_window);
    dock_widget->setWidget(panel);
    dock_widget->setObjectName(QStringLiteral("Course Planning dock widget"));

    if (!main_window->restoreDockWidget(dock_widget))
        main_window->addDockWidget(Qt::RightDockWidgetArea, dock_widget, Qt::Vertical);

    dock_widget->setVisible(true);
}


}  // namespace OpenOrienteering
