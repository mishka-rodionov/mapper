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

#include "iscd_symbol_library.h"

#include <algorithm>
#include <cstddef>

#include <QtGlobal>
#include <QCoreApplication>
#include <QHash>
#include <QLatin1String>
#include <QPainter>
#include <QRectF>

#include "core/map.h"
#include "core/objects/object.h"
#include "core/renderables/renderable.h"
#include "core/symbols/symbol.h"


namespace OpenOrienteering {

namespace IscdSymbolLibrary {

namespace {

const QString library_path = QStringLiteral(":/symbol-sets/Course_Design_15000.omap");

/// Side length of a description cell in the Course_Design symbol set, in mm.
constexpr qreal cell_size_mm = 6.0;

struct KeyCode { const char* key; const char* code; };

// ControlDescription::feature_part keys (see ControlPropertiesWidget) -> ISCD 2018 codes.
const KeyCode part_codes[] = {
    { "Northern", "0.1.1" },
    { "NE",       "0.2.1" },
    { "Eastern",  "0.1.2" },
    { "SE",       "0.2.2" },
    { "Southern", "0.1.3" },
    { "SW",       "0.2.3" },
    { "Western",  "0.1.4" },
    { "NW",       "0.2.4" },
    { "Upper",    "0.3"   },
    { "Lower",    "0.4"   },
    { "Middle",   "0.5"   },
};

// ControlDescription::feature keys -> ISCD 2018 codes.
const KeyCode feature_codes[] = {
    { "Spur",                    "1.2"  },
    { "Re-entrant",              "1.3"  },
    { "Earth bank",              "1.4"  },
    { "Earth wall",              "1.6"  },
    { "Erosion gully",           "1.7"  },
    { "Hill",                    "1.9"  },
    { "Knoll",                   "1.10" },
    { "Saddle",                  "1.11" },
    { "Depression",              "1.12" },
    { "Small depression",        "1.13" },
    { "Pit",                     "1.14" },
    { "Broken ground",           "1.15" },
    { "Anthill / termite mound", "1.16" },
    { "Cliff",                   "2.1"  },
    { "Rock face",               "2.1"  },
    { "Cave",                    "2.3"  },
    { "Boulder",                 "2.4"  },
    { "Boulder field",           "2.5"  },
    { "Boulder cluster",         "2.6"  },
    { "Lake / pond",             "3.1"  },
    { "River / stream",          "3.4"  },
    { "Ditch / channel",         "3.5"  },
    { "Narrow marsh",            "3.6"  },
    { "Marsh",                   "3.7"  },
    { "Firm ground in marsh",    "3.8"  },
    { "Well / water tank",       "3.9"  },
    { "Source / spring",         "3.10" },
    { "Open land",               "4.1"  },
    { "Forest corner",           "4.3"  },
    { "Clearing",                "4.4"  },
    { "Linear thicket",          "4.6"  },
    { "Copse",                   "4.8"  },
    { "Distinctive tree",        "4.9"  },
    { "Path / track",            "5.2"  },
    { "Bridge",                  "5.4"  },
    { "High-voltage line pylon", "5.6"  },
    { "Wall",                    "5.8"  },
    { "Fence",                   "5.9"  },
    { "Crossing point",          "5.10" },
    { "Building",                "5.11" },
    { "Paved area",              "5.12" },
    { "Ruin",                    "5.13" },
    { "Tower",                   "5.15" },
    { "Boundary stone / cairn",  "5.17" },
    { "Fodder rack",             "5.18" },
    { "Charcoal burning ground", "5.19" },
    { "Monument / statue",       "5.20" },
};

// ControlDescription::approach keys -> ISCD 2018 codes.
const KeyCode approach_codes[] = {
    { "Low",           "8.1"  },
    { "Shallow",       "8.2"  },
    { "Deep",          "8.3"  },
    { "Overgrown",     "8.4"  },
    { "Open",          "8.5"  },
    { "Rocky",         "8.6"  },
    { "Marshy",        "8.7"  },
    { "Sandy",         "8.8"  },
    { "Needle leaved", "8.9"  },
    { "Broad leaved",  "8.10" },
    { "Ruined",        "8.11" },
};

// ControlDescription::location_detail keys -> ISCD 2018 codes.
// "Side", "Corner (inside)" and "Corner (outside)" have no symbol without
// a direction in ISCD, so they are not mapped.
const KeyCode location_codes[] = {
    { "Top",                 "11.11"   },
    { "Upper part",          "11.9"    },
    { "Lower part",          "11.10"   },
    { "Beneath",             "11.12"   },
    { "Foot",                "11.13"   },
    { "N foot",              "11.14.1" },
    { "NE foot",             "11.14.2" },
    { "E foot",              "11.14.3" },
    { "SE foot",             "11.14.4" },
    { "S foot",              "11.14.5" },
    { "SW foot",             "11.14.6" },
    { "W foot",              "11.14.7" },
    { "NW foot",             "11.14.8" },
    { "N side",              "11.1.1"  },
    { "NE side",             "11.1.2"  },
    { "E side",              "11.1.3"  },
    { "SE side",             "11.1.4"  },
    { "S side",              "11.1.5"  },
    { "SW side",             "11.1.6"  },
    { "W side",              "11.1.7"  },
    { "NW side",             "11.1.8"  },
    { "N part",              "11.3.1"  },
    { "NE part",             "11.3.2"  },
    { "E part",              "11.3.3"  },
    { "SE part",             "11.3.4"  },
    { "S part",              "11.3.5"  },
    { "SW part",             "11.3.6"  },
    { "W part",              "11.3.7"  },
    { "NW part",             "11.3.8"  },
    { "N edge",              "11.2.1"  },
    { "NE edge",             "11.2.2"  },
    { "E edge",              "11.2.3"  },
    { "SE edge",             "11.2.4"  },
    { "S edge",              "11.2.5"  },
    { "SW edge",             "11.2.6"  },
    { "W edge",              "11.2.7"  },
    { "NW edge",             "11.2.8"  },
    { "N corner (inside)",   "11.4.1"  },
    { "NE corner (inside)",  "11.4.2"  },
    { "E corner (inside)",   "11.4.3"  },
    { "SE corner (inside)",  "11.4.4"  },
    { "S corner (inside)",   "11.4.5"  },
    { "SW corner (inside)",  "11.4.6"  },
    { "W corner (inside)",   "11.4.7"  },
    { "NW corner (inside)",  "11.4.8"  },
    { "N corner (outside)",  "11.5.1"  },
    { "NE corner (outside)", "11.5.2"  },
    { "E corner (outside)",  "11.5.3"  },
    { "SE corner (outside)", "11.5.4"  },
    { "S corner (outside)",  "11.5.5"  },
    { "SW corner (outside)", "11.5.6"  },
    { "W corner (outside)",  "11.5.7"  },
    { "NW corner (outside)", "11.5.8"  },
    { "N tip",               "11.6.1"  },
    { "NE tip",              "11.6.2"  },
    { "E tip",               "11.6.3"  },
    { "SE tip",              "11.6.4"  },
    { "S tip",               "11.6.5"  },
    { "SW tip",              "11.6.6"  },
    { "W tip",               "11.6.7"  },
    { "NW tip",              "11.6.8"  },
    { "N end",               "11.8.1"  },
    { "NE end",              "11.8.2"  },
    { "E end",               "11.8.3"  },
    { "SE end",              "11.8.4"  },
    { "S end",               "11.8.5"  },
    { "SW end",              "11.8.6"  },
    { "W end",               "11.8.7"  },
    { "NW end",              "11.8.8"  },
    { "Bend",                "11.7"    },
    { "Between",             "11.15"   },
    { "Junction",            "10.2"    },
};

template <std::size_t N>
QString lookup(const KeyCode (&table)[N], const QString& key)
{
    for (const auto& entry : table)
    {
        if (key.compare(QLatin1String(entry.key), Qt::CaseInsensitive) == 0)
            return QLatin1String(entry.code);
    }
    return {};
}


/**
 * Lazily loaded Course_Design symbol set, plus one single-object map per
 * drawn symbol (the same approach as Symbol::createIcon()).
 *
 * The maps are parented to the application object so that they are
 * destroyed together with it, not during static destruction.
 */
struct Library
{
    Map* map = nullptr;
    QHash<QString, const Symbol*> symbols;   ///< ISCD code -> point symbol
    QHash<QString, Map*> render_maps;        ///< ISCD code -> map with one object
};

Library& library()
{
    static Library lib;
    static bool loaded = false;
    if (loaded)
        return lib;
    loaded = true;

    auto* map = new Map();
    map->setParent(QCoreApplication::instance());
    if (!map->loadFrom(library_path))
    {
        qWarning("IscdSymbolLibrary: cannot load %s", qPrintable(library_path));
        delete map;
        return lib;
    }

    lib.map = map;
    for (int i = 0; i < map->getNumSymbols(); ++i)
    {
        const auto* symbol = map->getSymbol(i);
        if (symbol->getType() == Symbol::Point)
            lib.symbols.insert(symbol->getNumberAsString(), symbol);
    }
    return lib;
}

Map* renderMap(Library& lib, const QString& code)
{
    if (auto* map = lib.render_maps.value(code))
        return map;

    const auto* symbol = lib.symbols.value(code);
    if (!symbol)
        return nullptr;

    auto* map = new Map();
    map->setParent(QCoreApplication::instance());
    map->useColorsFrom(lib.map);
    map->setScaleDenominator(lib.map->getScaleDenominator());
    map->addObject(new PointObject(symbol));
    lib.render_maps.insert(code, map);
    return map;
}

}  // namespace


QString code(Column column, const QString& key)
{
    const auto trimmed = key.trimmed();
    if (trimmed.isEmpty())
        return {};
    if (trimmed.front().isDigit())
        return trimmed;
    switch (column)
    {
    case ColumnC: return lookup(part_codes, trimmed);
    case ColumnD: return lookup(feature_codes, trimmed);
    case ColumnE: return lookup(approach_codes, trimmed);
    case ColumnG: return lookup(location_codes, trimmed);
    }
    return {};
}


bool draw(QPainter* painter, const QString& code, const QRectF& cell)
{
    if (code.isEmpty())
        return false;

    auto& lib = library();
    if (!lib.map)
        return false;

    auto* map = renderMap(lib, code);
    if (!map)
        return false;

    const auto scaling = std::min(cell.width(), cell.height()) / cell_size_mm;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->translate(cell.center());
    painter->scale(scaling, scaling);
    // Generous bounding box: some pictograms slightly exceed the cell.
    const auto config = RenderConfig { *map, QRectF(-cell_size_mm, -cell_size_mm, 2 * cell_size_mm, 2 * cell_size_mm),
                                       scaling, RenderConfig::HelperSymbols, 1.0 };
    map->draw(painter, config);
    painter->restore();
    return true;
}

}  // namespace IscdSymbolLibrary

}  // namespace OpenOrienteering
