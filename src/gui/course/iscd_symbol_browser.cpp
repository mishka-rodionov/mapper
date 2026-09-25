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

#include "iscd_symbol_browser.h"

#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRectF>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "course/course_overlay.h"
#include "gui/course/iscd_symbol_editor.h"


namespace OpenOrienteering {

// ── Symbol grid widget ───────────────────────────────────────────────────────

namespace {

struct SymbolEntry { const char* name; int column; const char* label_ru; };  // column: 2=C 3=D 4=E 6=G

const SymbolEntry k_symbols[] = {
    // ── Column C ─────────────────────────────────────────────
    { "Northern",   2, u8"Северная" },
    { "NE",         2, u8"СВ" },
    { "Eastern",    2, u8"Восточная" },
    { "SE",         2, u8"ЮВ" },
    { "Southern",   2, u8"Южная" },
    { "SW",         2, u8"ЮЗ" },
    { "Western",    2, u8"Западная" },
    { "NW",         2, u8"СЗ" },
    { "Upper",      2, u8"Верхняя" },
    { "Lower",      2, u8"Нижняя" },
    { "Middle",     2, u8"Средняя" },

    // ── Column D – Terrain ────────────────────────────────────
    { "Re-entrant",         3, u8"Лощина" },
    { "Spur",               3, u8"Отрог" },
    { "Earth bank",         3, u8"Земляной вал" },
    { "Erosion gully",      3, u8"Промоина" },
    { "Pit",                3, u8"Яма" },
    { "Hill",               3, u8"Холм" },
    { "Knoll",              3, u8"Бугор" },
    { "Saddle",             3, u8"Седловина" },
    { "Depression",         3, u8"Впадина" },
    { "Small depression",   3, u8"Мал. впадина" },
    { "Broken ground",      3, u8"Изрытая почва" },
    { "Anthill / termite mound", 3, u8"Муравейник" },
    // Column D – Rock
    { "Cliff",              3, u8"Обрыв" },
    { "Rock face",          3, u8"Скальная стена" },
    { "Cave",               3, u8"Пещера" },
    { "Boulder",            3, u8"Валун" },
    { "Boulder field",      3, u8"Каменное поле" },
    { "Boulder cluster",    3, u8"Скопление валунов" },
    // Column D – Water
    { "Lake / pond",        3, u8"Озеро / пруд" },
    { "Marsh",              3, u8"Болото" },
    { "Narrow marsh",       3, u8"Узкое болото" },
    { "Firm ground in marsh", 3, u8"Твёрдый грунт" },
    { "Well / water tank",  3, u8"Колодец / бак" },
    { "River / stream",     3, u8"Река / ручей" },
    { "Ditch / channel",    3, u8"Канава" },
    { "Source / spring",    3, u8"Источник" },
    // Column D – Vegetation
    { "Open land",          3, u8"Открытая местность" },
    { "Forest corner",      3, u8"Угол леса" },
    { "Clearing",           3, u8"Поляна" },
    { "Copse",              3, u8"Роща" },
    { "Linear thicket",     3, u8"Лин. кустарник" },
    { "Distinctive tree",   3, u8"Хар. дерево" },
    { "Charcoal burning ground", 3, u8"Углежогня" },
    // Column D – Man-made
    { "Building",           3, u8"Здание" },
    { "Ruin",               3, u8"Руины" },
    { "Wall",               3, u8"Стена" },
    { "Earth wall",         3, u8"Земляной вал" },
    { "Fence",              3, u8"Забор" },
    { "Path / track",       3, u8"Тропа" },
    { "Paved area",         3, u8"Мощёная пл." },
    { "Bridge",             3, u8"Мост" },
    { "Crossing point",     3, u8"Переход" },
    { "Tower",              3, u8"Башня" },
    { "High-voltage line pylon", 3, u8"Опора ЛЭП" },
    { "Boundary stone / cairn",  3, u8"Межевой камень" },
    { "Monument / statue",  3, u8"Памятник" },
    { "Fodder rack",        3, u8"Кормушка" },

    // ── Column E ─────────────────────────────────────────────
    { "Low",           4, u8"Низкий" },
    { "Shallow",       4, u8"Мелкий" },
    { "Deep",          4, u8"Глубокий" },
    { "Overgrown",     4, u8"Заросший" },
    { "Open",          4, u8"Открытый" },
    { "Rocky",         4, u8"Каменистый" },
    { "Marshy",        4, u8"Заболоченный" },
    { "Sandy",         4, u8"Песчаный" },
    { "Needle leaved", 4, u8"Хвойный" },
    { "Broad leaved",  4, u8"Лиственный" },
    { "Ruined",        4, u8"Разрушенный" },

    // ── Column G ─────────────────────────────────────────────
    { "Top",                 6, u8"Вершина" },
    { "Upper part",          6, u8"Верхняя часть" },
    { "Lower part",          6, u8"Нижняя часть" },
    { "Beneath",             6, u8"Под" },
    { "Foot",                6, u8"Подножие" },
    { "N foot",              6, u8"С подножие" },
    { "NE foot",             6, u8"СВ подножие" },
    { "E foot",              6, u8"В подножие" },
    { "SE foot",             6, u8"ЮВ подножие" },
    { "S foot",              6, u8"Ю подножие" },
    { "SW foot",             6, u8"ЮЗ подножие" },
    { "W foot",              6, u8"З подножие" },
    { "NW foot",             6, u8"СЗ подножие" },
    { "Side",                6, u8"Склон" },
    { "N side",              6, u8"С сторона" },
    { "NE side",             6, u8"СВ сторона" },
    { "E side",              6, u8"В сторона" },
    { "SE side",             6, u8"ЮВ сторона" },
    { "S side",              6, u8"Ю сторона" },
    { "SW side",             6, u8"ЮЗ сторона" },
    { "W side",              6, u8"З сторона" },
    { "NW side",             6, u8"СЗ сторона" },
    { "N part",              6, u8"С часть" },
    { "NE part",             6, u8"СВ часть" },
    { "E part",              6, u8"В часть" },
    { "SE part",             6, u8"ЮВ часть" },
    { "S part",              6, u8"Ю часть" },
    { "SW part",             6, u8"ЮЗ часть" },
    { "W part",              6, u8"З часть" },
    { "NW part",             6, u8"СЗ часть" },
    { "N edge",              6, u8"С край" },
    { "NE edge",             6, u8"СВ край" },
    { "E edge",              6, u8"В край" },
    { "SE edge",             6, u8"ЮВ край" },
    { "S edge",              6, u8"Ю край" },
    { "SW edge",             6, u8"ЮЗ край" },
    { "W edge",              6, u8"З край" },
    { "NW edge",             6, u8"СЗ край" },
    { "Corner (inside)",     6, u8"Угол (вн.)" },
    { "N corner (inside)",   6, u8"С угол вн." },
    { "NE corner (inside)",  6, u8"СВ угол вн." },
    { "E corner (inside)",   6, u8"В угол вн." },
    { "SE corner (inside)",  6, u8"ЮВ угол вн." },
    { "S corner (inside)",   6, u8"Ю угол вн." },
    { "SW corner (inside)",  6, u8"ЮЗ угол вн." },
    { "W corner (inside)",   6, u8"З угол вн." },
    { "NW corner (inside)",  6, u8"СЗ угол вн." },
    { "Corner (outside)",    6, u8"Угол (нар.)" },
    { "N corner (outside)",  6, u8"С угол нар." },
    { "NE corner (outside)", 6, u8"СВ угол нар." },
    { "E corner (outside)",  6, u8"В угол нар." },
    { "SE corner (outside)", 6, u8"ЮВ угол нар." },
    { "S corner (outside)",  6, u8"Ю угол нар." },
    { "SW corner (outside)", 6, u8"ЮЗ угол нар." },
    { "W corner (outside)",  6, u8"З угол нар." },
    { "NW corner (outside)", 6, u8"СЗ угол нар." },
    { "N tip",               6, u8"С конец" },
    { "NE tip",              6, u8"СВ конец" },
    { "E tip",               6, u8"В конец" },
    { "SE tip",              6, u8"ЮВ конец" },
    { "S tip",               6, u8"Ю конец" },
    { "SW tip",              6, u8"ЮЗ конец" },
    { "W tip",               6, u8"З конец" },
    { "NW tip",              6, u8"СЗ конец" },
    { "N end",               6, u8"С оконечн." },
    { "NE end",              6, u8"СВ оконечн." },
    { "E end",               6, u8"В оконечн." },
    { "SE end",              6, u8"ЮВ оконечн." },
    { "S end",               6, u8"Ю оконечн." },
    { "SW end",              6, u8"ЮЗ оконечн." },
    { "W end",               6, u8"З оконечн." },
    { "NW end",              6, u8"СЗ оконечн." },
    { "Bend",                6, u8"Изгиб" },
    { "Between",             6, u8"Между" },
    { "Junction",            6, u8"Слияние" },
};

static constexpr int k_cell_px  = 54;   // symbol cell size in pixels
static constexpr int k_cols     = 6;    // symbols per row
static constexpr int k_label_h  = 24;   // height of text label below each cell
static constexpr int k_gap      = 6;    // gap between cells
static constexpr int k_sect_h   = 26;   // section header height

// ── canvas widget ────────────────────────────────────────────────────────────

class SymbolCanvas : public QWidget
{
public:
    explicit SymbolCanvas(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    struct SectionInfo { QString title; int first_row; };
    QVector<SectionInfo> m_sections;
    int m_total_rows = 0;
};

SymbolCanvas::SymbolCanvas(QWidget* parent)
    : QWidget(parent)
{
    // Pre-compute section positions
    int col = 0, row = 0;
    int prev_col_idx = -1;
    const int n = static_cast<int>(sizeof(k_symbols) / sizeof(k_symbols[0]));
    for (int i = 0; i < n; ++i)
    {
        const int c = k_symbols[i].column;
        if (c != prev_col_idx)
        {
            if (col != 0) { ++row; col = 0; }  // flush partial row
            static const char* titles[] = {
                nullptr, nullptr, u8"C – Часть ориентира",
                u8"D – Ориентир (столбец D)", u8"E – Характер",
                nullptr, u8"G – Положение"
            };
            m_sections.append({QString::fromUtf8(titles[c]), row});
            ++row;  // section header occupies one "row"
            prev_col_idx = c;
        }
        ++col;
        if (col == k_cols) { col = 0; ++row; }
    }
    if (col != 0) ++row;
    m_total_rows = row;

    const int total_w = k_cols * (k_cell_px + k_gap) + k_gap;
    const int total_h = m_total_rows * (k_cell_px + k_label_h + k_gap) + k_gap;
    setMinimumSize(total_w, total_h);
    setFixedSize(total_w, total_h);
}

void SymbolCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    QFont label_font = painter.font();
    label_font.setPixelSize(10);
    painter.setFont(label_font);

    QFont section_font = label_font;
    section_font.setPixelSize(12);
    section_font.setBold(true);

    // Recompute layout
    int col = 0, row = 0;
    int prev_col_idx = -1;
    int sect_idx = 0;

    auto rowTop = [&](int r) {
        return k_gap + r * (k_cell_px + k_label_h + k_gap);
    };
    auto colLeft = [&](int c) {
        return k_gap + c * (k_cell_px + k_gap);
    };

    const int n = static_cast<int>(sizeof(k_symbols) / sizeof(k_symbols[0]));
    for (int i = 0; i < n; ++i)
    {
        const int c_idx = k_symbols[i].column;
        if (c_idx != prev_col_idx)
        {
            if (col != 0) { ++row; col = 0; }
            // Draw section header
            const QRectF header_rect(k_gap, rowTop(row),
                                     k_cols * (k_cell_px + k_gap),
                                     k_sect_h);
            painter.fillRect(header_rect, QColor(220, 220, 240));
            painter.setFont(section_font);
            painter.setPen(Qt::black);
            painter.drawText(header_rect.adjusted(6, 0, 0, 0),
                             Qt::AlignVCenter | Qt::AlignLeft,
                             m_sections[sect_idx].title);
            ++sect_idx;
            ++row;
            prev_col_idx = c_idx;
        }

        // Draw cell background
        const QRectF cell(colLeft(col), rowTop(row), k_cell_px, k_cell_px);
        painter.fillRect(cell, QColor(245, 245, 245));
        painter.setPen(QPen(Qt::gray, 0.5));
        painter.drawRect(cell);

        // Draw symbol
        painter.save();
        CourseOverlay::drawISCDCell(&painter, c_idx, QLatin1String(k_symbols[i].name), cell, 3);
        painter.restore();

        // Draw label
        painter.setFont(label_font);
        painter.setPen(Qt::black);
        const QRectF label_rect(colLeft(col), rowTop(row) + k_cell_px, k_cell_px, k_label_h);
        painter.drawText(label_rect, Qt::AlignTop | Qt::AlignHCenter | Qt::TextWordWrap,
                         QString::fromUtf8(k_symbols[i].label_ru));

        ++col;
        if (col == k_cols) { col = 0; ++row; }
    }
}

}  // anonymous namespace

// ── ISCDSymbolBrowser ────────────────────────────────────────────────────────

ISCDSymbolBrowser::ISCDSymbolBrowser(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("ISCD Symbol Reference"));
    resize(420, 620);

    auto* canvas = new SymbolCanvas;
    auto* scroll = new QScrollArea;
    scroll->setWidget(canvas);
    scroll->setWidgetResizable(false);

    auto* editBtn = new QPushButton(tr("Symbol Editor…"), this);
    connect(editBtn, &QPushButton::clicked, this, [this, canvas]{
        auto* ed = new ISCDSymbolEditor(this);
        ed->setAttribute(Qt::WA_DeleteOnClose);
        connect(ed, &QDialog::finished, canvas, [canvas]{ canvas->update(); });
        ed->show();
    });

    auto* btnBar = new QHBoxLayout;
    btnBar->addStretch();
    btnBar->addWidget(editBtn);
    btnBar->setContentsMargins(4, 4, 4, 4);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll, 1);
    layout->addLayout(btnBar);
}

}  // namespace OpenOrienteering
