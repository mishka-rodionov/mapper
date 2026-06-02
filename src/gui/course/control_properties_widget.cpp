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

#include "control_properties_widget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>

#include "core/map.h"
#include "course/course_control.h"
#include "course/course_database.h"
#include "course/course_undo.h"


namespace OpenOrienteering {

namespace {

// IOF column C – which part of the feature
const QStringList iof_feature_part = {
    {},
    QLatin1String("Northern"),
    QLatin1String("NE"),
    QLatin1String("Eastern"),
    QLatin1String("SE"),
    QLatin1String("Southern"),
    QLatin1String("SW"),
    QLatin1String("Western"),
    QLatin1String("NW"),
    QLatin1String("Upper"),
    QLatin1String("Lower"),
    QLatin1String("Middle"),
};

// IOF column D – main feature
const QStringList iof_feature = {
    {},
    // Landforms
    QLatin1String("Depression"),
    QLatin1String("Small depression"),
    QLatin1String("Pit"),
    QLatin1String("Broken ground"),
    QLatin1String("Re-entrant"),
    QLatin1String("Spur"),
    QLatin1String("Earth bank"),
    QLatin1String("Erosion gully"),
    QLatin1String("Hill"),
    QLatin1String("Knoll"),
    QLatin1String("Saddle"),
    // Rock
    QLatin1String("Boulder"),
    QLatin1String("Boulder cluster"),
    QLatin1String("Boulder field"),
    QLatin1String("Cliff"),
    QLatin1String("Rock face"),
    QLatin1String("Cave"),
    // Water
    QLatin1String("Lake / pond"),
    QLatin1String("Marsh"),
    QLatin1String("Narrow marsh"),
    QLatin1String("Firm ground in marsh"),
    QLatin1String("Well / water tank"),
    QLatin1String("River / stream"),
    QLatin1String("Ditch / channel"),
    QLatin1String("Source / spring"),
    // Vegetation
    QLatin1String("Open land"),
    QLatin1String("Forest corner"),
    QLatin1String("Clearing"),
    QLatin1String("Copse"),
    QLatin1String("Linear thicket"),
    QLatin1String("Distinctive tree"),
    QLatin1String("Charcoal burning ground"),
    // Man-made
    QLatin1String("Building"),
    QLatin1String("Ruin"),
    QLatin1String("Wall"),
    QLatin1String("Earth wall"),
    QLatin1String("Fence"),
    QLatin1String("Path / track"),
    QLatin1String("Paved area"),
    QLatin1String("Bridge"),
    QLatin1String("Crossing point"),
    QLatin1String("Tower"),
    QLatin1String("High-voltage line pylon"),
    QLatin1String("Boundary stone / cairn"),
    QLatin1String("Anthill / termite mound"),
    QLatin1String("Monument / statue"),
    QLatin1String("Fodder rack"),
};

// IOF column E – appearance / characteristic
const QStringList iof_approach = {
    {},
    QLatin1String("Shallow"),
    QLatin1String("Deep"),
    QLatin1String("Overgrown"),
    QLatin1String("Open"),
    QLatin1String("Rocky"),
    QLatin1String("Marshy"),
    QLatin1String("Sandy"),
    QLatin1String("Ruined"),
};

// IOF column G – location of the control flag
const QStringList iof_location = {
    {},
    QLatin1String("Top"),
    QLatin1String("Upper part"),
    QLatin1String("Lower part"),
    QLatin1String("Foot"),
    QLatin1String("Side"),
    QLatin1String("N foot"),
    QLatin1String("NE foot"),
    QLatin1String("E foot"),
    QLatin1String("SE foot"),
    QLatin1String("S foot"),
    QLatin1String("SW foot"),
    QLatin1String("W foot"),
    QLatin1String("NW foot"),
    QLatin1String("N edge"),
    QLatin1String("E edge"),
    QLatin1String("S edge"),
    QLatin1String("W edge"),
    QLatin1String("N tip"),
    QLatin1String("E tip"),
    QLatin1String("S tip"),
    QLatin1String("W tip"),
    QLatin1String("N end"),
    QLatin1String("S end"),
    QLatin1String("Corner (inside)"),
    QLatin1String("Corner (outside)"),
    QLatin1String("Junction"),
};

void populateCombo(QComboBox* combo, const QStringList& values)
{
    combo->clear();
    for (const auto& v : values)
        combo->addItem(v);
}

}  // anonymous namespace


ControlPropertiesWidget::ControlPropertiesWidget(Map& map, CourseDatabase& db, QWidget* parent)
: QWidget(parent)
, map(map)
, db(db)
{
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeft);

    // B – code
    code_edit = new QLineEdit;
    code_edit->setPlaceholderText(tr("e.g. 101"));
    form->addRow(tr("B – Code:"), code_edit);

    // C – feature part
    feature_part_combo = new QComboBox;
    feature_part_combo->setEditable(true);
    populateCombo(feature_part_combo, iof_feature_part);
    form->addRow(tr("C – Part:"), feature_part_combo);

    // D – feature
    feature_combo = new QComboBox;
    feature_combo->setEditable(true);
    populateCombo(feature_combo, iof_feature);
    form->addRow(tr("D – Feature:"), feature_combo);

    // E – appearance
    approach_combo = new QComboBox;
    approach_combo->setEditable(true);
    populateCombo(approach_combo, iof_approach);
    form->addRow(tr("E – Appearance:"), approach_combo);

    // F – dimensions
    dimensions_edit = new QLineEdit;
    dimensions_edit->setPlaceholderText(tr("e.g. 2x1"));
    form->addRow(tr("F – Dimensions:"), dimensions_edit);

    // G – location
    location_combo = new QComboBox;
    location_combo->setEditable(true);
    populateCombo(location_combo, iof_location);
    form->addRow(tr("G – Location:"), location_combo);

    // H – other info
    other_edit = new QLineEdit;
    form->addRow(tr("H – Other:"), other_edit);

    auto* group = new QGroupBox(tr("Control description"), this);
    group->setLayout(form);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(group);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    // Connect signals — all go through the same change/commit pathway
    connect(code_edit,           &QLineEdit::editingFinished,
            this, &ControlPropertiesWidget::onCodeEdited);
    connect(feature_part_combo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlPropertiesWidget::onFeaturePartChanged);
    connect(feature_combo,       QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlPropertiesWidget::onFeatureChanged);
    connect(approach_combo,      QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlPropertiesWidget::onApproachChanged);
    connect(dimensions_edit,     &QLineEdit::editingFinished,
            this, &ControlPropertiesWidget::onDimensionsEdited);
    connect(location_combo,      QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlPropertiesWidget::onLocationDetailChanged);
    connect(other_edit,          &QLineEdit::editingFinished,
            this, &ControlPropertiesWidget::onOtherInfoEdited);

    connect(&db, &CourseDatabase::controlChanged,
            this, &ControlPropertiesWidget::onDatabaseChanged);

    setEnabled(false);
}


void ControlPropertiesWidget::setControl(const QString& id)
{
    control_id = id;
    setEnabled(!id.isEmpty());

    loading = true;

    if (id.isEmpty())
    {
        code_edit->clear();
        feature_part_combo->setCurrentIndex(0);
        feature_combo->setCurrentIndex(0);
        approach_combo->setCurrentIndex(0);
        dimensions_edit->clear();
        location_combo->setCurrentIndex(0);
        other_edit->clear();
    }
    else if (const auto* ctrl = db.findById(id))
    {
        const auto& d = ctrl->description;
        code_edit->setText(d.code);
        feature_part_combo->setCurrentText(d.feature_part);
        feature_combo->setCurrentText(d.feature);
        approach_combo->setCurrentText(d.approach);
        dimensions_edit->setText(d.dimensions);
        location_combo->setCurrentText(d.location_detail);
        other_edit->setText(d.other_info);
    }

    loading = false;
}


// ── Change handlers ───────────────────────────────────────────────────────────

void ControlPropertiesWidget::onCodeEdited()           { if (!loading) commit(); }
void ControlPropertiesWidget::onFeaturePartChanged()   { if (!loading) commit(); }
void ControlPropertiesWidget::onFeatureChanged()       { if (!loading) commit(); }
void ControlPropertiesWidget::onApproachChanged()      { if (!loading) commit(); }
void ControlPropertiesWidget::onDimensionsEdited()     { if (!loading) commit(); }
void ControlPropertiesWidget::onLocationDetailChanged(){ if (!loading) commit(); }
void ControlPropertiesWidget::onOtherInfoEdited()      { if (!loading) commit(); }

void ControlPropertiesWidget::onDatabaseChanged(int /*index*/)
{
    // If the control we're editing was updated externally (e.g. by undo),
    // reload the fields — but don't re-trigger commit.
    if (!control_id.isEmpty())
        setControl(control_id);
}


// ── Commit ────────────────────────────────────────────────────────────────────

void ControlPropertiesWidget::commit()
{
    if (control_id.isEmpty()) return;

    int index = -1;
    for (int i = 0; i < db.numControls(); ++i)
    {
        if (db.control(i).id == control_id)
        {
            index = i;
            break;
        }
    }
    if (index < 0) return;

    ControlDescription new_desc;
    new_desc.code            = code_edit->text().trimmed();
    new_desc.feature_part    = feature_part_combo->currentText();
    new_desc.feature         = feature_combo->currentText();
    new_desc.approach        = approach_combo->currentText();
    new_desc.dimensions      = dimensions_edit->text().trimmed();
    new_desc.location_detail = location_combo->currentText();
    new_desc.other_info      = other_edit->text().trimmed();

    const ControlDescription old_desc = db.control(index).description;
    if (new_desc == old_desc)
        return;  // nothing changed

    map.push(new ModifyControlDescriptionUndoStep(&map, control_id, old_desc, new_desc));

    auto updated = db.control(index);
    updated.description = new_desc;

    // Suppress reload during our own updateControl() call
    loading = true;
    db.updateControl(index, std::move(updated));
    loading = false;
}


}  // namespace OpenOrienteering
