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
#include <QCoreApplication>
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
const char* const iof_feature_part[] = {
    "",
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Northern"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "NE"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Eastern"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "SE"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Southern"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "SW"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Western"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "NW"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Upper"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Lower"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Middle"),
    nullptr
};

// IOF column D – main feature
const char* const iof_feature[] = {
    "",
    // Landforms
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Depression"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Small depression"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Pit"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Broken ground"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Re-entrant"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Spur"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Earth bank"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Erosion gully"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Hill"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Knoll"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Saddle"),
    // Rock
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Boulder"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Boulder cluster"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Boulder field"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Cliff"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Rock face"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Cave"),
    // Water
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Lake / pond"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Marsh"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Narrow marsh"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Firm ground in marsh"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Well / water tank"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "River / stream"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Ditch / channel"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Source / spring"),
    // Vegetation
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Open land"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Forest corner"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Clearing"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Copse"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Linear thicket"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Distinctive tree"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Charcoal burning ground"),
    // Man-made
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Building"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Ruin"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Wall"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Earth wall"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Fence"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Path / track"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Paved area"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Bridge"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Crossing point"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Tower"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "High-voltage line pylon"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Boundary stone / cairn"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Anthill / termite mound"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Monument / statue"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Fodder rack"),
    nullptr
};

// IOF column E – appearance / characteristic
const char* const iof_approach[] = {
    "",
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Shallow"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Deep"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Overgrown"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Open"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Rocky"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Marshy"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Sandy"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Ruined"),
    nullptr
};

// IOF column G – location of the control flag
const char* const iof_location[] = {
    "",
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Top"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Upper part"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Lower part"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Foot"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Side"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "N foot"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "NE foot"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "E foot"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "SE foot"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "S foot"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "SW foot"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "W foot"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "NW foot"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "N edge"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "E edge"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "S edge"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "W edge"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "N tip"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "E tip"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "S tip"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "W tip"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "N end"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "S end"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Corner (inside)"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Corner (outside)"),
    QT_TRANSLATE_NOOP("OpenOrienteering::ControlPropertiesWidget", "Junction"),
    nullptr
};

// Populates combo with translated display text; stores English key in Qt::UserRole.
// This keeps saved data locale-independent.
void populateCombo(QComboBox* combo, const char* const* items)
{
    static const char ctx[] = "OpenOrienteering::ControlPropertiesWidget";
    combo->clear();
    for (int i = 0; items[i] != nullptr; ++i)
    {
        const QLatin1String key(items[i]);
        const QString text = key.size() > 0
            ? QCoreApplication::translate(ctx, items[i])
            : QString{};
        combo->addItem(text, QString(key));
    }
}

// Sets combo to item matching the English key stored in Qt::UserRole.
// Falls back to setCurrentText for custom (user-typed) values.
void setComboByKey(QComboBox* combo, const QString& key)
{
    const int idx = combo->findData(key);
    if (idx >= 0)
        combo->setCurrentIndex(idx);
    else
        combo->setCurrentText(key);
}

// Returns the English key for storage: UserRole data for predefined items,
// currentText() for custom user-typed values.
QString comboKey(const QComboBox* combo)
{
    const QVariant data = combo->currentData();
    return data.isValid() ? data.toString() : combo->currentText();
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
        setComboByKey(feature_part_combo, d.feature_part);
        setComboByKey(feature_combo, d.feature);
        setComboByKey(approach_combo, d.approach);
        dimensions_edit->setText(d.dimensions);
        setComboByKey(location_combo, d.location_detail);
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
    new_desc.feature_part    = comboKey(feature_part_combo);
    new_desc.feature         = comboKey(feature_combo);
    new_desc.approach        = comboKey(approach_combo);
    new_desc.dimensions      = dimensions_edit->text().trimmed();
    new_desc.location_detail = comboKey(location_combo);
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
