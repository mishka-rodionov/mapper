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

#include "iscd_symbol_editor.h"

#include <cmath>

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QPushButton>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

#include "course/course_overlay.h"

namespace OpenOrienteering {

// ── Symbol table (same data as iscd_symbol_browser, duplicated to avoid
//    coupling to its anonymous-namespace structs) ────────────────────────────

namespace {

struct SymInfo { const char* key; int col; const char* ru; };

const SymInfo k_syms[] = {
    // C
    { "northern",   2, u8"Северная" },
    { "ne",         2, u8"СВ" },
    { "eastern",    2, u8"Восточная" },
    { "se",         2, u8"ЮВ" },
    { "southern",   2, u8"Южная" },
    { "sw",         2, u8"ЮЗ" },
    { "western",    2, u8"Западная" },
    { "nw",         2, u8"СЗ" },
    { "upper",      2, u8"Верхняя" },
    { "lower",      2, u8"Нижняя" },
    { "middle",     2, u8"Средняя" },
    // D – terrain
    { "re-entrant",              3, u8"Лощина" },
    { "spur",                    3, u8"Отрог" },
    { "earth bank",              3, u8"Земляной вал" },
    { "erosion gully",           3, u8"Промоина" },
    { "pit",                     3, u8"Яма" },
    { "hill",                    3, u8"Холм" },
    { "knoll",                   3, u8"Бугор" },
    { "saddle",                  3, u8"Седловина" },
    { "depression",              3, u8"Впадина" },
    { "small depression",        3, u8"Мал. впадина" },
    { "broken ground",           3, u8"Изрытая почва" },
    { "anthill / termite mound", 3, u8"Муравейник" },
    // D – rock
    { "cliff",                   3, u8"Обрыв" },
    { "rock face",               3, u8"Скальная стена" },
    { "cave",                    3, u8"Пещера" },
    { "boulder",                 3, u8"Валун" },
    { "boulder field",           3, u8"Каменное поле" },
    { "boulder cluster",         3, u8"Скопление валунов" },
    // D – water
    { "lake / pond",             3, u8"Озеро / пруд" },
    { "marsh",                   3, u8"Болото" },
    { "narrow marsh",            3, u8"Узкое болото" },
    { "firm ground in marsh",    3, u8"Твёрдый грунт" },
    { "well / water tank",       3, u8"Колодец / бак" },
    { "river / stream",          3, u8"Река / ручей" },
    { "ditch / channel",         3, u8"Канава" },
    { "source / spring",         3, u8"Источник" },
    // D – vegetation
    { "open land",               3, u8"Открытая местность" },
    { "forest corner",           3, u8"Угол леса" },
    { "clearing",                3, u8"Поляна" },
    { "copse",                   3, u8"Роща" },
    { "linear thicket",          3, u8"Лин. кустарник" },
    { "distinctive tree",        3, u8"Хар. дерево" },
    { "charcoal burning ground", 3, u8"Углежогня" },
    // D – man-made
    { "building",                3, u8"Здание" },
    { "ruin",                    3, u8"Руины" },
    { "wall",                    3, u8"Стена" },
    { "earth wall",              3, u8"Земляной вал" },
    { "fence",                   3, u8"Забор" },
    { "path / track",            3, u8"Тропа" },
    { "paved area",              3, u8"Мощёная пл." },
    { "bridge",                  3, u8"Мост" },
    { "crossing point",          3, u8"Переход" },
    { "tower",                   3, u8"Башня" },
    { "high-voltage line pylon", 3, u8"Опора ЛЭП" },
    { "boundary stone / cairn",  3, u8"Межевой камень" },
    { "monument / statue",       3, u8"Памятник" },
    { "fodder rack",             3, u8"Кормушка" },
    // E
    { "shallow",    4, u8"Мелкий" },
    { "deep",       4, u8"Глубокий" },
    { "overgrown",  4, u8"Заросший" },
    { "open",       4, u8"Открытый" },
    { "rocky",      4, u8"Каменистый" },
    { "marshy",     4, u8"Заболоченный" },
    { "sandy",      4, u8"Песчаный" },
    { "ruined",     4, u8"Разрушенный" },
    // G
    { "top",             6, u8"Вершина" },
    { "upper part",      6, u8"Верхняя часть" },
    { "lower part",      6, u8"Нижняя часть" },
    { "foot",            6, u8"Подножие" },
    { "side",            6, u8"Склон" },
    { "n foot",          6, u8"С подножие" },
    { "ne foot",         6, u8"СВ подножие" },
    { "e foot",          6, u8"В подножие" },
    { "se foot",         6, u8"ЮВ подножие" },
    { "s foot",          6, u8"Ю подножие" },
    { "sw foot",         6, u8"ЮЗ подножие" },
    { "w foot",          6, u8"З подножие" },
    { "nw foot",         6, u8"СЗ подножие" },
    { "n edge",          6, u8"С край" },
    { "e edge",          6, u8"В край" },
    { "s edge",          6, u8"Ю край" },
    { "w edge",          6, u8"З край" },
    { "n tip",           6, u8"С конец" },
    { "e tip",           6, u8"В конец" },
    { "s tip",           6, u8"Ю конец" },
    { "w tip",           6, u8"З конец" },
    { "n end",           6, u8"С оконечн." },
    { "s end",           6, u8"Ю оконечн." },
    { "corner (inside)", 6, u8"Угол (вн.)" },
    { "corner (outside)",6, u8"Угол (нар.)" },
    { "junction",        6, u8"Слияние" },
    { "between",         6, u8"Между" },
};
constexpr int k_sym_count = static_cast<int>(sizeof(k_syms)/sizeof(k_syms[0]));

// 3-point arc as polyline (screen coords).
QPolygonF arcPolylineEditor(QPointF p1, QPointF pmid, QPointF p3)
{
    const double ax=p1.x(), ay=p1.y(), bx=pmid.x(), by=pmid.y(), cx=p3.x(), cy=p3.y();
    const double D=2.0*(ax*(by-cy)+bx*(cy-ay)+cx*(ay-by));
    if (std::abs(D)<1e-4) { QPolygonF l; l<<p1<<p3; return l; }
    const double ux=((ax*ax+ay*ay)*(by-cy)+(bx*bx+by*by)*(cy-ay)+(cx*cx+cy*cy)*(ay-by))/D;
    const double uy=((ax*ax+ay*ay)*(cx-bx)+(bx*bx+by*by)*(ax-cx)+(cx*cx+cy*cy)*(bx-ax))/D;
    const double R=std::hypot(ax-ux,ay-uy);
    auto ang=[&](double px,double py){return std::atan2(py-uy,px-ux);};
    const double a1=ang(ax,ay),a2=ang(bx,by),a3=ang(cx,cy);
    double sw=a3-a1; if(sw<=0) sw+=2*M_PI;
    double a2r=a2-a1; if(a2r<=0) a2r+=2*M_PI;
    if(a2r>sw) sw-=2*M_PI;
    const int n=std::max(12,static_cast<int>(std::abs(sw)*R/3.0));
    QPolygonF poly;
    for(int i=0;i<=n;++i) poly<<QPointF(ux+R*std::cos(a1+sw*i/n),uy+R*std::sin(a1+sw*i/n));
    return poly;
}

}  // anonymous namespace

// ── EditorCanvas ─────────────────────────────────────────────────────────────

class EditorCanvas : public QWidget
{
    Q_OBJECT
public:
    enum Tool { LineTool, ArcTool, OvalTool, CircleTool, SquareTool, PointTool, DeleteTool };

    explicit EditorCanvas(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedSize(320, 320);
        setMouseTracking(true);
        setCursor(Qt::CrossCursor);
    }

    void setTool(Tool t)
    {
        m_tool = t;
        m_pending.clear();
        m_hoveredStroke = -1;
        setCursor(t == DeleteTool ? Qt::PointingHandCursor : Qt::CrossCursor);
        update();
    }
    Tool tool() const { return m_tool; }

    void setStrokes(const QJsonArray& s) { m_strokes=s; m_pending.clear(); update(); }
    const QJsonArray& strokes() const    { return m_strokes; }

    void setPointDiameter(double d) { m_pointDiameter = d; update(); }

    void undoLast()
    {
        if (!m_pending.isEmpty()) { m_pending.removeLast(); }
        else if (!m_strokes.isEmpty()) { m_strokes.removeLast(); emit strokesChanged(); }
        update();
    }

    void clear()
    {
        m_strokes=QJsonArray(); m_pending.clear(); m_hoveredStroke=-1;
        emit strokesChanged(); update();
    }

signals:
    void strokesChanged();

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::white);
        drawGrid(p);
        drawStoredStrokes(p);
        drawInProgress(p);
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::RightButton) { m_pending.clear(); update(); return; }
        if (e->button() != Qt::LeftButton) return;

        if (m_tool == DeleteTool) {
            if (m_hoveredStroke >= 0 && m_hoveredStroke < m_strokes.size()) {
                m_strokes.removeAt(m_hoveredStroke);
                m_hoveredStroke = -1;
                emit strokesChanged();
            }
            update();
            return;
        }

        const QPointF u = snap(pixToUnit(e->pos()));
        switch (m_tool) {
        case LineTool:
            if (m_pending.isEmpty()) { m_pending << u; }
            else { addStroke(QLatin1String("L"), {m_pending[0], u}); m_pending.clear(); }
            break;
        case ArcTool:
            m_pending << u;
            if (m_pending.size() == 3) {
                addStroke(QLatin1String("A"), {m_pending[0], m_pending[1], m_pending[2]});
                m_pending.clear();
            }
            break;
        case OvalTool:
            if (m_pending.isEmpty()) { m_pending << u; }
            else { addStroke(QLatin1String("O"), {m_pending[0], u}); m_pending.clear(); }
            break;
        case CircleTool:
            if (m_pending.isEmpty()) { m_pending << u; }
            else { addStroke(QLatin1String("C"), {m_pending[0], u}); m_pending.clear(); }
            break;
        case SquareTool:
            if (m_pending.isEmpty()) { m_pending << u; }
            else { addStroke(QLatin1String("R"), {m_pending[0], constrainSquare(m_pending[0], u)}); m_pending.clear(); }
            break;
        case PointTool:
            addStroke(QLatin1String("P"), {u}, m_pointDiameter);
            break;
        case DeleteTool:
            break;  // handled above
        }
        update();
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        const QPointF px = e->pos();
        m_cursor = snap(pixToUnit(px));
        m_hasCursor = true;
        if (m_tool == DeleteTool)
            m_hoveredStroke = nearestStroke(px);
        update();
    }

    void leaveEvent(QEvent*) override { m_hasCursor=false; update(); }

private:
    void drawGrid(QPainter& p) const
    {
        // Dot grid: one dot per snap position (every 0.25 units)
        p.setPen(Qt::NoPen);
        for (int ix = -4; ix <= 4; ++ix) {
            for (int iy = -4; iy <= 4; ++iy) {
                const QPointF pt = u2px(QPointF(ix * 0.25, iy * 0.25));
                const bool major = (ix % 2 == 0) && (iy % 2 == 0);  // 0.5-unit grid
                p.setBrush(major ? QColor(160, 160, 210) : QColor(200, 200, 210));
                p.drawEllipse(pt, major ? 2.0 : 1.2, major ? 2.0 : 1.2);
            }
        }
        // Axes (thin)
        p.setPen(QPen(QColor(160, 160, 210), 0.6));
        p.setBrush(Qt::NoBrush);
        p.drawLine(u2px(QPointF(0, -1.1)), u2px(QPointF(0, 1.1)));
        p.drawLine(u2px(QPointF(-1.1, 0)), u2px(QPointF(1.1, 0)));
        // Unit border
        p.setPen(QPen(QColor(140, 140, 190), 1.0));
        const QPointF tl=u2px(QPointF(-1,-1)), br=u2px(QPointF(1,1));
        p.drawRect(QRectF(tl, br));
    }

    void drawStoredStrokes(QPainter& p) const
    {
        for (int i = 0; i < m_strokes.size(); ++i) {
            const bool hovered = (m_tool == DeleteTool && i == m_hoveredStroke);
            const QColor col = hovered ? QColor(210, 40, 0) : Qt::black;
            p.setPen(QPen(col, hovered ? 2.5 : 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            const QJsonObject s   = m_strokes[i].toObject();
            const QJsonArray  pts = s[QLatin1String("p")].toArray();
            const QString     t   = s[QLatin1String("t")].toString();
            if (t==QLatin1String("L") && pts.size()>=2) {
                p.drawLine(u2pj(pts[0].toArray()), u2pj(pts[1].toArray()));
            } else if (t==QLatin1String("A") && pts.size()>=3) {
                p.drawPolyline(arcPolylineEditor(u2pj(pts[0].toArray()),
                                                  u2pj(pts[1].toArray()),
                                                  u2pj(pts[2].toArray())));
            } else if (t==QLatin1String("O") && pts.size()>=2) {
                p.drawEllipse(QRectF(u2pj(pts[0].toArray()), u2pj(pts[1].toArray())).normalized());
            } else if (t==QLatin1String("C") && pts.size()>=2) {
                const QPointF c = u2pj(pts[0].toArray());
                const QPointF e = u2pj(pts[1].toArray());
                const double  r = std::hypot(e.x()-c.x(), e.y()-c.y());
                p.drawEllipse(c, r, r);
            } else if (t==QLatin1String("R") && pts.size()>=2) {
                p.drawRect(QRectF(u2pj(pts[0].toArray()), u2pj(pts[1].toArray())).normalized());
            } else if (t==QLatin1String("P") && pts.size()>=1) {
                const QPointF c = u2pj(pts[0].toArray());
                const double  r = s[QLatin1String("d")].toDouble(0.1) * scale() * 0.5;
                p.setPen(Qt::NoPen);
                p.setBrush(col);
                p.drawEllipse(c, r, r);
            }
        }
    }

    void drawInProgress(QPainter& p) const
    {
        // Pending point dots
        p.setPen(QPen(QColor(200,60,0), 1.5));
        p.setBrush(QColor(200,60,0,160));
        for (const QPointF& u : m_pending)
            p.drawEllipse(u2px(u), 3.5, 3.5);

        // Ghost preview of the point about to be placed
        if (m_hasCursor && m_tool == PointTool) {
            p.setPen(QPen(QColor(0,100,200), 1.0, Qt::DashLine));
            p.setBrush(QColor(0,100,200,70));
            const double r = m_pointDiameter * 0.5 * scale();
            p.drawEllipse(u2px(m_cursor), r, r);
        }

        // Cursor snap dot
        if (m_hasCursor) {
            p.setPen(QPen(Qt::blue, 1.0));
            p.setBrush(QColor(0,0,220,100));
            p.drawEllipse(u2px(m_cursor), 3.0, 3.0);
        }
        if (!m_hasCursor || m_pending.isEmpty()) return;
        // Rubber band
        p.setPen(QPen(QColor(0,100,200), 1.5, Qt::DashLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        if (m_tool == LineTool) {
            p.drawLine(u2px(m_pending[0]), u2px(m_cursor));
        } else if (m_tool == ArcTool) {
            if (m_pending.size()==1)
                p.drawLine(u2px(m_pending[0]), u2px(m_cursor));
            else if (m_pending.size()==2)
                p.drawPolyline(arcPolylineEditor(u2px(m_pending[0]),
                                                  u2px(m_pending[1]),
                                                  u2px(m_cursor)));
        } else if (m_tool == OvalTool) {
            p.drawEllipse(QRectF(u2px(m_pending[0]), u2px(m_cursor)).normalized());
        } else if (m_tool == CircleTool) {
            const QPointF c = u2px(m_pending[0]);
            const double  r = std::hypot(u2px(m_cursor).x()-c.x(), u2px(m_cursor).y()-c.y());
            p.drawEllipse(c, r, r);
        } else if (m_tool == SquareTool) {
            p.drawRect(QRectF(u2px(m_pending[0]), u2px(constrainSquare(m_pending[0], m_cursor))).normalized());
        }
    }

    void addStroke(const QString& type, const QVector<QPointF>& pts, double diameter = -1.0)
    {
        QJsonArray arr;
        for (const QPointF& u : pts) {
            QJsonArray pt;
            pt << std::round(u.x()*1000)/1000.0 << std::round(u.y()*1000)/1000.0;
            arr << pt;
        }
        QJsonObject s; s[QLatin1String("t")]=type; s[QLatin1String("p")]=arr;
        if (diameter >= 0.0)
            s[QLatin1String("d")] = std::round(diameter*1000)/1000.0;
        m_strokes.append(s);
        emit strokesChanged();
    }

    // Snaps p2 so that (p1,p2) form opposite corners of a square.
    static QPointF constrainSquare(QPointF p1, QPointF p2)
    {
        const double dx = p2.x()-p1.x(), dy = p2.y()-p1.y();
        const double side = std::max(std::abs(dx), std::abs(dy));
        return QPointF(p1.x() + std::copysign(side, dx), p1.y() + std::copysign(side, dy));
    }

    // Coordinate helpers
    double scale() const { return std::min(width(), height()) * 0.44; }

    QPointF pixToUnit(QPointF px) const
    { return QPointF((px.x()-width()*0.5)/scale(), (px.y()-height()*0.5)/scale()); }

    QPointF u2px(QPointF u) const
    { return QPointF(width()*0.5+u.x()*scale(), height()*0.5+u.y()*scale()); }

    QPointF u2pj(const QJsonArray& a) const
    { return u2px(QPointF(a[0].toDouble(), a[1].toDouble())); }

    // Snap to 0.05-unit grid
    static QPointF snap(QPointF u)
    { return QPointF(std::round(u.x()*20)/20.0, std::round(u.y()*20)/20.0); }

    static double distToSeg(QPointF p, QPointF a, QPointF b)
    {
        const double dx=b.x()-a.x(), dy=b.y()-a.y();
        const double len2=dx*dx+dy*dy;
        if (len2 < 1e-6) return std::hypot(p.x()-a.x(), p.y()-a.y());
        const double t=std::max(0.0, std::min(1.0, ((p.x()-a.x())*dx+(p.y()-a.y())*dy)/len2));
        return std::hypot(p.x()-(a.x()+t*dx), p.y()-(a.y()+t*dy));
    }

    static double distToEllipseOutline(QPointF p, const QRectF& r)
    {
        const QPointF c = r.center();
        const double rx = r.width()*0.5, ry = r.height()*0.5;
        constexpr int n = 48;
        double minD = 1e9;
        QPointF prev(c.x()+rx, c.y());
        for (int i = 1; i <= n; ++i) {
            const double a = 2*M_PI*i/n;
            const QPointF cur(c.x()+rx*std::cos(a), c.y()+ry*std::sin(a));
            minD = std::min(minD, distToSeg(p, prev, cur));
            prev = cur;
        }
        return minD;
    }

    static double distToRectOutline(QPointF p, const QRectF& r)
    {
        const QPointF tl=r.topLeft(), tr=r.topRight(), br=r.bottomRight(), bl=r.bottomLeft();
        return std::min({distToSeg(p,tl,tr), distToSeg(p,tr,br), distToSeg(p,br,bl), distToSeg(p,bl,tl)});
    }

    int nearestStroke(QPointF cursorPx) const
    {
        constexpr double kThresh = 10.0;
        double minD = kThresh;
        int best = -1;
        for (int i = 0; i < m_strokes.size(); ++i) {
            const QJsonObject s   = m_strokes[i].toObject();
            const QJsonArray  pts = s[QLatin1String("p")].toArray();
            const QString     t   = s[QLatin1String("t")].toString();
            double d = 1e9;
            if (t==QLatin1String("L") && pts.size()>=2) {
                d = distToSeg(cursorPx, u2pj(pts[0].toArray()), u2pj(pts[1].toArray()));
            } else if (t==QLatin1String("A") && pts.size()>=3) {
                const QPolygonF poly = arcPolylineEditor(u2pj(pts[0].toArray()),
                                                          u2pj(pts[1].toArray()),
                                                          u2pj(pts[2].toArray()));
                for (int j = 0; j+1 < poly.size(); ++j)
                    d = std::min(d, distToSeg(cursorPx, poly[j], poly[j+1]));
            } else if (t==QLatin1String("O") && pts.size()>=2) {
                d = distToEllipseOutline(cursorPx, QRectF(u2pj(pts[0].toArray()), u2pj(pts[1].toArray())).normalized());
            } else if (t==QLatin1String("C") && pts.size()>=2) {
                const QPointF c = u2pj(pts[0].toArray());
                const QPointF e = u2pj(pts[1].toArray());
                const double  r = std::hypot(e.x()-c.x(), e.y()-c.y());
                d = std::abs(std::hypot(cursorPx.x()-c.x(), cursorPx.y()-c.y()) - r);
            } else if (t==QLatin1String("R") && pts.size()>=2) {
                d = distToRectOutline(cursorPx, QRectF(u2pj(pts[0].toArray()), u2pj(pts[1].toArray())).normalized());
            } else if (t==QLatin1String("P") && pts.size()>=1) {
                const QPointF c = u2pj(pts[0].toArray());
                const double  r = s[QLatin1String("d")].toDouble(0.1) * scale() * 0.5;
                d = std::max(0.0, std::hypot(cursorPx.x()-c.x(), cursorPx.y()-c.y()) - r);
            }
            if (d < minD) { minD = d; best = i; }
        }
        return best;
    }

    Tool             m_tool = LineTool;
    double           m_pointDiameter = 0.15;
    QJsonArray       m_strokes;
    QVector<QPointF> m_pending;
    QPointF          m_cursor;
    bool             m_hasCursor  = false;
    int              m_hoveredStroke = -1;
};

// ── ISCDSymbolEditor ──────────────────────────────────────────────────────────

ISCDSymbolEditor::ISCDSymbolEditor(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("ISCD Symbol Editor"));
    resize(640, 480);

    m_jsonPath = QCoreApplication::applicationDirPath() + QLatin1String("/iscd_symbols.json");

    // ── Toolbar ──────────────────────────────────────────────────────────────
    m_btnLine = new QToolButton(this);
    m_btnLine->setText(tr("Линия"));
    m_btnLine->setCheckable(true);
    m_btnLine->setChecked(true);

    m_btnArc = new QToolButton(this);
    m_btnArc->setText(tr("Дуга (3 точки)"));
    m_btnArc->setCheckable(true);

    m_btnOval = new QToolButton(this);
    m_btnOval->setText(tr("Овал"));
    m_btnOval->setCheckable(true);

    m_btnCircle = new QToolButton(this);
    m_btnCircle->setText(tr("Круг"));
    m_btnCircle->setCheckable(true);

    m_btnSquare = new QToolButton(this);
    m_btnSquare->setText(tr("Квадрат"));
    m_btnSquare->setCheckable(true);

    m_btnPoint = new QToolButton(this);
    m_btnPoint->setText(tr("Точка"));
    m_btnPoint->setCheckable(true);

    m_pointDiameterSpin = new QDoubleSpinBox(this);
    m_pointDiameterSpin->setRange(0.05, 1.0);
    m_pointDiameterSpin->setSingleStep(0.05);
    m_pointDiameterSpin->setDecimals(2);
    m_pointDiameterSpin->setValue(0.15);
    m_pointDiameterSpin->setPrefix(tr("⌀ "));
    m_pointDiameterSpin->setToolTip(tr("Диаметр точки (доля от размера символа)"));

    m_btnDelete = new QToolButton(this);
    m_btnDelete->setText(tr("Удалить"));
    m_btnDelete->setCheckable(true);

    auto* btnUndo = new QPushButton(tr("Отмена"), this);
    auto* btnClear = new QPushButton(tr("Очистить"), this);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(m_btnLine);
    toolbar->addWidget(m_btnArc);
    toolbar->addWidget(m_btnOval);
    toolbar->addWidget(m_btnCircle);
    toolbar->addWidget(m_btnSquare);
    toolbar->addWidget(m_btnPoint);
    toolbar->addWidget(m_pointDiameterSpin);
    toolbar->addWidget(m_btnDelete);
    toolbar->addSpacing(12);
    toolbar->addWidget(btnUndo);
    toolbar->addWidget(btnClear);
    toolbar->addStretch();

    // ── Canvas ───────────────────────────────────────────────────────────────
    m_canvas = new EditorCanvas(this);

    m_statusLabel = new QLabel(tr("Выберите символ из списка слева"), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    auto* canvasBox = new QVBoxLayout;
    canvasBox->addLayout(toolbar);
    canvasBox->addWidget(m_canvas);
    canvasBox->addWidget(m_statusLabel);

    // ── Symbol list ──────────────────────────────────────────────────────────
    m_list = new QListWidget(this);
    m_list->setMaximumWidth(190);
    populateSymbolList();

    // ── Bottom bar ───────────────────────────────────────────────────────────
    m_pathLabel = new QLabel(m_jsonPath, this);
    m_pathLabel->setStyleSheet(QLatin1String("font-size: 10px; color: gray;"));
    m_pathLabel->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    auto* saveBtn = buttons->addButton(tr("Сохранить"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Закрыть"), QDialogButtonBox::RejectRole);

    auto* bottomBar = new QHBoxLayout;
    bottomBar->addWidget(m_pathLabel, 1);
    bottomBar->addWidget(buttons);

    // ── Main layout ──────────────────────────────────────────────────────────
    auto* hbox = new QHBoxLayout;
    hbox->addWidget(m_list);
    hbox->addLayout(canvasBox, 1);

    auto* vbox = new QVBoxLayout(this);
    vbox->addLayout(hbox, 1);
    vbox->addLayout(bottomBar);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_list,   &QListWidget::currentRowChanged, this, &ISCDSymbolEditor::onSymbolSelected);
    auto selectTool = [this](EditorCanvas::Tool t) {
        m_btnLine->setChecked(t == EditorCanvas::LineTool);
        m_btnArc->setChecked(t == EditorCanvas::ArcTool);
        m_btnOval->setChecked(t == EditorCanvas::OvalTool);
        m_btnCircle->setChecked(t == EditorCanvas::CircleTool);
        m_btnSquare->setChecked(t == EditorCanvas::SquareTool);
        m_btnPoint->setChecked(t == EditorCanvas::PointTool);
        m_btnDelete->setChecked(t == EditorCanvas::DeleteTool);
        m_canvas->setTool(t);
    };
    connect(m_btnLine,   &QToolButton::clicked, this, [selectTool]{ selectTool(EditorCanvas::LineTool); });
    connect(m_btnArc,    &QToolButton::clicked, this, [selectTool]{ selectTool(EditorCanvas::ArcTool); });
    connect(m_btnOval,   &QToolButton::clicked, this, [selectTool]{ selectTool(EditorCanvas::OvalTool); });
    connect(m_btnCircle, &QToolButton::clicked, this, [selectTool]{ selectTool(EditorCanvas::CircleTool); });
    connect(m_btnSquare, &QToolButton::clicked, this, [selectTool]{ selectTool(EditorCanvas::SquareTool); });
    connect(m_btnPoint,  &QToolButton::clicked, this, [selectTool]{ selectTool(EditorCanvas::PointTool); });
    connect(m_btnDelete, &QToolButton::clicked, this, [selectTool]{ selectTool(EditorCanvas::DeleteTool); });
    connect(m_pointDiameterSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_canvas, &EditorCanvas::setPointDiameter);
    connect(btnUndo,    &QPushButton::clicked, this, &ISCDSymbolEditor::onUndo);
    connect(btnClear,   &QPushButton::clicked, this, &ISCDSymbolEditor::onClear);
    connect(saveBtn,    &QPushButton::clicked, this, &ISCDSymbolEditor::onSave);
    connect(buttons,    &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_canvas,   &EditorCanvas::strokesChanged, this, &ISCDSymbolEditor::onStrokesChanged);

    loadFromDisk();
}

void ISCDSymbolEditor::populateSymbolList()
{
    int prevCol = -1;
    const char* colNames[] = {nullptr, nullptr, u8"C – Часть ориентира",
                               u8"D – Ориентир", u8"E – Характер",
                               nullptr, u8"G – Положение"};
    for (int i = 0; i < k_sym_count; ++i) {
        const int col = k_syms[i].col;
        if (col != prevCol) {
            auto* hdr = new QListWidgetItem(QString::fromUtf8(colNames[col]));
            hdr->setFlags(Qt::NoItemFlags);
            hdr->setBackground(QColor(220, 220, 240));
            m_list->addItem(hdr);
            prevCol = col;
        }
        auto* item = new QListWidgetItem(QString::fromUtf8(k_syms[i].ru));
        item->setData(Qt::UserRole, QString::fromLatin1(k_syms[i].key));
        m_list->addItem(item);
    }
}

void ISCDSymbolEditor::loadFromDisk()
{
    QFile f(m_jsonPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject())
        m_paths = doc.object();
    // Mark items with custom paths
    for (int i = 0; i < m_list->count(); ++i) {
        auto* item = m_list->item(i);
        const QString key = item->data(Qt::UserRole).toString();
        if (!key.isEmpty() && m_paths.contains(key) && !m_paths[key].toArray().isEmpty())
            item->setForeground(QColor(0, 120, 0));
    }
}

void ISCDSymbolEditor::onSymbolSelected(int row)
{
    if (row < 0) return;
    const QString key = m_list->item(row)->data(Qt::UserRole).toString();
    if (key.isEmpty()) return;  // section header

    m_currentKey = key;
    m_canvas->setStrokes(m_paths.value(key).toArray());

    const QString ruName = m_list->item(row)->text();
    const int strokeCount = m_canvas->strokes().size();
    m_statusLabel->setText(tr("%1  |  %2 штрихов  |  ЛКМ — точка, ПКМ — отмена")
                               .arg(ruName).arg(strokeCount));
}

void ISCDSymbolEditor::onUndo()
{
    m_canvas->undoLast();
}

void ISCDSymbolEditor::onClear()
{
    m_canvas->clear();
}

void ISCDSymbolEditor::onSave()
{
    // Flush current symbol
    if (!m_currentKey.isEmpty()) {
        if (m_canvas->strokes().isEmpty())
            m_paths.remove(m_currentKey);
        else
            m_paths[m_currentKey] = m_canvas->strokes();
    }

    QFile f(m_jsonPath);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось сохранить:\n%1").arg(m_jsonPath));
        return;
    }
    f.write(QJsonDocument(m_paths).toJson());

    // Reload into CourseOverlay so changes are visible immediately
    CourseOverlay::setCustomSymbolPaths(m_paths);

    // Update list item colour
    for (int i = 0; i < m_list->count(); ++i) {
        auto* item = m_list->item(i);
        const QString key = item->data(Qt::UserRole).toString();
        if (!key.isEmpty()) {
            const bool has = m_paths.contains(key) && !m_paths[key].toArray().isEmpty();
            item->setForeground(has ? QColor(0,120,0) : QColor(Qt::black));
        }
    }
    QMessageBox::information(this, tr("Сохранено"),
                             tr("Символы сохранены в:\n%1").arg(m_jsonPath));
}

void ISCDSymbolEditor::onStrokesChanged()
{
    if (m_currentKey.isEmpty()) return;
    if (m_canvas->strokes().isEmpty())
        m_paths.remove(m_currentKey);
    else
        m_paths[m_currentKey] = m_canvas->strokes();

    const int n = m_canvas->strokes().size();
    const auto cur = m_list->currentItem();
    const QString ru = cur ? cur->text() : m_currentKey;
    m_statusLabel->setText(tr("%1  |  %2 штрихов  |  ЛКМ — точка, ПКМ — отмена")
                               .arg(ru).arg(n));
}

}  // namespace OpenOrienteering

#include "iscd_symbol_editor.moc"
