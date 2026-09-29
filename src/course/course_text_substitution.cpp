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

#include "course_text_substitution.h"

#include <Qt>

#include "core/map.h"
#include "core/map_part.h"
#include "core/objects/object.h"
#include "core/objects/text_object.h"


namespace OpenOrienteering {


QString CourseTextSubstitution::courseNamePlaceholder()
{
	return QStringLiteral("{course}");
}


CourseTextSubstitution::CourseTextSubstitution(Map& map, const QString& course_name)
{
	if (course_name.isNull())
		return;

	const auto placeholder = courseNamePlaceholder();
	for (int p = 0; p < map.getNumParts(); ++p)
	{
		auto* part = map.getPart(std::size_t(p));
		for (int i = 0; i < part->getNumObjects(); ++i)
		{
			auto* object = part->getObject(i);
			if (object->getType() != Object::Text)
				continue;

			auto* text_object = object->asText();
			const auto& text = text_object->getText();
			if (!text.contains(placeholder, Qt::CaseInsensitive))
				continue;

			original_texts.emplace_back(text_object, text);
			text_object->setText(QString(text).replace(placeholder, course_name, Qt::CaseInsensitive));
			text_object->update();
		}
	}
}


CourseTextSubstitution::~CourseTextSubstitution()
{
	for (auto& entry : original_texts)
	{
		entry.first->setText(entry.second);
		entry.first->update();
	}
}


}  // namespace OpenOrienteering
