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

#include "place_control_tool.h"

#include <cmath>

#include <Qt>
#include <QColor>
#include <QCursor>
#include <QKeyEvent>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>

#include "core/map.h"
#include "core/map_coord.h"
#include "core/map_view.h"
#include "course/course_database.h"
#include "course/course_undo.h"
#include "gui/map/map_widget.h"
#include "gui/util_gui.h"
#include "tools/tool_base.h"
#include "tools/tool_helpers.h"


namespace OpenOrienteering {

namespace {

const QColor course_purple { 148, 0, 211 };
constexpr qreal circle_diameter_mm = 5.0;
constexpr qreal line_width_mm      = 0.35;

}  // anonymous namespace


PlaceControlTool::PlaceControlTool(MapEditorController* editor,
                                   QAction* tool_action,
                                   CourseDatabase& db)
: MapEditorToolBase(QCursor(Qt::CrossCursor), MapEditorTool::Other, editor, tool_action)
, db(db)
{}

PlaceControlTool::~PlaceControlTool() = default;


void PlaceControlTool::initImpl()
{
    // No angle/snap helpers needed for course tools.
    angle_helper->setActive(false);
    snap_helper->setFilter(SnappingToolHelper::NoSnapping);

    updateStatusText();
}


// ============================================================
// Mouse events
// ============================================================

void PlaceControlTool::mouseMove()
{
    // Repaint so the cursor-circle preview follows the mouse.
    updateDirtyRect();
}

void PlaceControlTool::clickPress()
{
    // Determine at press time whether a drag will move an existing control.
    drag_target_id = controlAtViewportPos(click_pos, mapWidget());
}

void PlaceControlTool::clickRelease()
{
    const QString hit = controlAtViewportPos(click_pos, mapWidget());

    if (!hit.isEmpty())
    {
        // Select the clicked control without creating a new one.
        selectControl(hit);
    }
    else
    {
        // Create a new control at the click position.
        CourseControl ctrl;
        ctrl.id       = db.generateUniqueId(next_type);
        ctrl.position = MapCoord(static_cast<QPointF>(cur_pos_map));
        ctrl.type     = next_type;

        map()->push(new AddControlUndoStep(map(), ctrl));
        db.addControl(ctrl);

        selectControl(ctrl.id);
    }

    drag_target_id.clear();
    updateDirtyRect();
    updateStatusText();
}

void PlaceControlTool::dragStart()
{
    if (drag_target_id.isEmpty())
    {
        // Drag started on empty space — no control to move.
        return;
    }

    const auto* ctrl = db.findById(drag_target_id);
    if (!ctrl)
    {
        drag_target_id.clear();
        return;
    }

    drag_old_pos = ctrl->position;
    is_dragging  = true;
    setEditingInProgress(true);
    selectControl(drag_target_id);
    updateStatusText();
}

void PlaceControlTool::dragMove()
{
    if (!is_dragging || drag_target_id.isEmpty())
        return;

    // Update the control position live so CourseOverlay reflects the drag.
    const int idx = controlIndex(drag_target_id);
    if (idx < 0)
        return;

    auto ctrl = db.control(idx);
    ctrl.position = MapCoord(static_cast<QPointF>(cur_pos_map));
    db.updateControl(idx, std::move(ctrl));
    // CourseOverlay redraws automatically via controlChanged() signal.
}

void PlaceControlTool::dragFinish()
{
    if (!is_dragging || drag_target_id.isEmpty())
    {
        is_dragging = false;
        setEditingInProgress(false);
        return;
    }

    const int idx = controlIndex(drag_target_id);
    if (idx >= 0)
    {
        const MapCoord new_pos = db.control(idx).position;
        if (new_pos != drag_old_pos)
        {
            // Push undo step recording the position before the drag.
            map()->push(new MoveControlUndoStep(map(), drag_target_id,
                                                drag_old_pos, new_pos));
        }
    }

    drag_target_id.clear();
    is_dragging = false;
    setEditingInProgress(false);
    updateStatusText();
}

void PlaceControlTool::dragCanceled()
{
    if (!is_dragging || drag_target_id.isEmpty())
    {
        is_dragging = false;
        setEditingInProgress(false);
        return;
    }

    // Restore the original position.
    const int idx = controlIndex(drag_target_id);
    if (idx >= 0)
    {
        auto ctrl = db.control(idx);
        ctrl.position = drag_old_pos;
        db.updateControl(idx, std::move(ctrl));
    }

    drag_target_id.clear();
    is_dragging = false;
    setEditingInProgress(false);
}

bool PlaceControlTool::keyPress(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Escape:
        deactivate();
        return true;

    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        deleteSelectedControl();
        return true;

    default:
        return false;
    }
}


// ============================================================
// Drawing
// ============================================================

void PlaceControlTool::drawImpl(QPainter* painter, MapWidget* widget)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal lw = widget->getMapView()->lengthToPixel(line_width_mm * 1000.0);
    const qreal r  = widget->getMapView()->lengthToPixel(circle_diameter_mm * 1000.0 / 2.0);

    QColor preview_color = course_purple;
    preview_color.setAlphaF(preview_alpha);
    painter->setPen(QPen(preview_color, lw));
    painter->setBrush(Qt::NoBrush);

    // Draw a preview circle at the current cursor position.
    const QPointF center = widget->mapToViewport(constrained_pos_map);
    painter->drawEllipse(center, r, r);

    painter->restore();
}

int PlaceControlTool::updateDirtyRectImpl(QRectF& rect)
{
    // rect is in map coordinates (mm); return extra pixel border.
    const qreal r = circle_diameter_mm / 2.0;
    rect = QRectF(constrained_pos_map.x() - r,
                  constrained_pos_map.y() - r,
                  r * 2.0, r * 2.0);
    return 2;
}

void PlaceControlTool::updateStatusText()
{
    if (is_dragging)
    {
        setStatusBarText(tr("<b>Drag</b>: move control | <b>Escape</b>: cancel drag"));
    }
    else if (!selected_id.isEmpty())
    {
        setStatusBarText(tr("<b>Click</b>: place control | <b>Click control</b>: select"
                            " | <b>Delete</b>: remove selected | <b>Escape</b>: exit"));
    }
    else
    {
        setStatusBarText(tr("<b>Click</b>: place control | <b>Click control</b>: select"
                            " | <b>Drag control</b>: move | <b>Escape</b>: exit"));
    }
}


// ============================================================
// Private helpers
// ============================================================

QString PlaceControlTool::controlAtViewportPos(const QPoint& viewport_pos,
                                               MapWidget* widget) const
{
    for (int i = 0; i < db.numControls(); ++i)
    {
        const auto& ctrl = db.control(i);
        const QPointF screen_pos = widget->mapToViewport(MapCoordF(ctrl.position));
        const qreal dx = screen_pos.x() - viewport_pos.x();
        const qreal dy = screen_pos.y() - viewport_pos.y();
        if (std::sqrt(dx*dx + dy*dy) <= hit_radius_px)
            return ctrl.id;
    }
    return {};
}

int PlaceControlTool::controlIndex(const QString& id) const
{
    for (int i = 0; i < db.numControls(); ++i)
    {
        if (db.control(i).id == id)
            return i;
    }
    return -1;
}

void PlaceControlTool::selectControl(const QString& id)
{
    if (selected_id != id)
    {
        selected_id = id;
        emit selectedControlChanged(id);
        updateStatusText();
    }
}

void PlaceControlTool::deleteSelectedControl()
{
    if (selected_id.isEmpty())
        return;

    const int idx = controlIndex(selected_id);
    if (idx < 0)
        return;

    CourseControl ctrl = db.control(idx);
    map()->push(new RemoveControlUndoStep(map(), ctrl));
    db.removeControl(idx);

    selected_id.clear();
    emit selectedControlChanged({});
    updateStatusText();
}


}  // namespace OpenOrienteering
