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

#ifndef OPENORIENTEERING_COURSE_TEXT_SUBSTITUTION_H
#define OPENORIENTEERING_COURSE_TEXT_SUBSTITUTION_H

#include <utility>
#include <vector>

#include <QString>

namespace OpenOrienteering {

class Map;
class TextObject;


/**
 * Temporarily fills course placeholders in the map's text objects.
 *
 * A text object on the map may contain "{course}" (e.g. "Classes: {course}").
 * While an instance of this class exists, every such placeholder is replaced
 * by the given course name, so that printing or exporting the map shows the
 * text of the course being drawn. The destructor restores the original texts.
 *
 * The substitution bypasses the undo system and does not mark the map as
 * modified: the placeholder text is what stays in the map file.
 */
class CourseTextSubstitution
{
public:
	/** Returns the placeholder which is replaced by the course name. */
	static QString courseNamePlaceholder();

	/**
	 * Replaces the placeholders in all text objects of the map.
	 * Does nothing if course_name is null (no course is shown).
	 */
	CourseTextSubstitution(Map& map, const QString& course_name);

	/** Restores the original texts. */
	~CourseTextSubstitution();

	CourseTextSubstitution(const CourseTextSubstitution&) = delete;
	CourseTextSubstitution& operator=(const CourseTextSubstitution&) = delete;

private:
	std::vector<std::pair<TextObject*, QString>> original_texts;
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_COURSE_TEXT_SUBSTITUTION_H
