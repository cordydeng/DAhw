#include "shape.h"
#include <QTextStream>

/* Define layers */

typedef struct LayerTable
{
    Qt::GlobalColor _color;
    Qt::PenStyle _style;
    qreal _width;
    Qt::BrushStyle _pattern;
} LayerTable;

LayerTable _layerTable[] = {
    {Qt::black, Qt::SolidLine, 1, Qt::NoBrush},
    {Qt::red, Qt::SolidLine, 1, Qt::NoBrush},
    {Qt::green, Qt::SolidLine, 1, Qt::NoBrush},
    {Qt::blue, Qt::SolidLine, 1, Qt::NoBrush},
    {Qt::black, Qt::DotLine, 1, Qt::NoBrush},
    {Qt::red, Qt::DotLine, 1, Qt::NoBrush},
    {Qt::green, Qt::DotLine, 1, Qt::NoBrush},
    {Qt::blue, Qt::DotLine, 1, Qt::NoBrush},
    {Qt::black, Qt::DashLine, 1, Qt::NoBrush},
    {Qt::red, Qt::DashLine, 1, Qt::NoBrush},
    {Qt::cyan, Qt::DashLine, 3, Qt::NoBrush},
    {Qt::blue, Qt::DashLine, 1, Qt::NoBrush},
    {Qt::black, Qt::SolidLine, 1, Qt::DiagCrossPattern},
    {Qt::red, Qt::SolidLine, 1, Qt::DiagCrossPattern},
    {Qt::green, Qt::SolidLine, 1, Qt::DiagCrossPattern},
    {Qt::blue, Qt::SolidLine, 1, Qt::DiagCrossPattern},
    {Qt::black, Qt::SolidLine, 1, Qt::Dense5Pattern},
    {Qt::red, Qt::SolidLine, 1, Qt::Dense5Pattern},
    {Qt::green, Qt::SolidLine, 1, Qt::Dense5Pattern},
    {Qt::blue, Qt::SolidLine, 1, Qt::Dense5Pattern},
    /* add more layers here */
};
    
static QBrush getBrush(uint32_t layer)
{
    return QBrush(_layerTable[layer]._color, _layerTable[layer]._pattern);
}

static QPen getPen(uint32_t layer)
{
    QPen pen;
    pen.setWidth(_layerTable[layer]._width);
    pen.setColor(_layerTable[layer]._color);
    pen.setStyle(_layerTable[layer]._style);
    return pen;
}

static QBrush getSelectedBrush()
{
    return QBrush();
}

static QPen getSelectedPen()
{
    return QPen(Qt::red, 2, Qt::DotLine);
}

static QBrush getInteractiveBrush()
{
    return QBrush();
}

static QPen getInteractivePen()
{
    return QPen(Qt::blue, 1, Qt::SolidLine);
}

/* base object for all shapes */

ShapeObject::ShapeObject(uint32_t layer)
{
    const uint32_t layerCount = sizeof(_layerTable) / sizeof(LayerTable);
    _layer = layer % layerCount;
}

/* Rectangle Shape */

QString RectangleShape::getTypeString() const
{
    return QString("Rectangle");
}

bool RectangleShape::select (const QPoint & pos, const Transform & trans)
{
    QRectF rect(trans.userToScreen(_pts[0]), trans.userToScreen(_pts[1]));
    return rect.contains(pos);
}

void RectangleShape::draw (QPainter * painter, const Transform & trans) const
{
    QRectF rect(trans.userToScreen(_pts[0]), trans.userToScreen(_pts[1]));
    
    painter->setPen (getPen(_layer));
    painter->setBrush(getBrush(_layer));
    painter->drawRect(rect);
}

void RectangleShape::drawSelected (QPainter * painter, const Transform & trans) const
{
    QRectF rect(trans.userToScreen(_pts[0]), trans.userToScreen(_pts[1]));
    painter->setBrush (getSelectedBrush());
    painter->setPen (getSelectedPen());
    painter->drawRect(rect);
}

void RectangleShape::drawInteractive (QPainter * painter, const QVector<QPoint> & pts, const QPoint & pos)
{
    if (!pts.isEmpty())
    {
        painter->setBrush (getInteractiveBrush());
        painter->setPen (getInteractivePen());
        painter->drawRect(QRect(pts[0], pos));
    }
}

/* Boundary Shape */

QString BoundaryShape::getTypeString() const
{
    return QString("Boundary");
}

bool BoundaryShape::select (const QPoint & pos, const Transform & trans)
{
    QPolygon screenPts;
    trans.userToScreenPts(_pts, screenPts);
    return screenPts.containsPoint(pos, Qt::OddEvenFill);
}

void BoundaryShape::draw (QPainter * painter, const Transform & trans) const
{
    QPolygon screenPts;
    trans.userToScreenPts(_pts, screenPts);
    painter->setPen (getPen(_layer));
    painter->setBrush(getBrush(_layer));
    painter->drawPolygon(screenPts.data(), screenPts.size());
}

void BoundaryShape::drawSelected (QPainter * painter, const Transform & trans) const
{
    QPolygon screenPts;
    trans.userToScreenPts(_pts, screenPts);
    painter->setBrush (getSelectedBrush());
    painter->setPen (getSelectedPen());
    painter->drawPolygon(screenPts.data(), screenPts.size());
}

void BoundaryShape::drawInteractive (QPainter * painter, const QVector<QPoint> & pts, const QPoint & pos)
{
    if (0 == pts.size())
    {
        return;
    }

    painter->setBrush (getInteractiveBrush());
    painter->setPen (getInteractivePen());
    painter->drawPolyline(pts.data(), pts.size());
    painter->drawLine(pts[pts.size() - 1], pos);
}

/* PAth Shape */

QString PathShape::getTypeString() const
{
    return QString("Path");
}

bool PathShape::select (const QPoint & pos, const Transform & trans)
{
    QPolygon screenPts;
    trans.userToScreenPts(_pts, screenPts);
    QPainterPath pp = toPainterPath(screenPts, _width, _endStyle);
    return pp.contains(pos);
}

void PathShape::draw (QPainter * painter, const Transform & trans) const
{
    QPolygon screenPts;
    trans.userToScreenPts(_pts, screenPts);
    QPainterPath pp = toPainterPath(screenPts, _width, _endStyle);
    painter->setPen(getPen(_layer));
    painter->setBrush(getBrush(_layer));
    painter->drawPath(pp);
    painter->drawPolyline(screenPts);
}

void PathShape::drawSelected (QPainter * painter, const Transform & trans) const
{
    QPolygon screenPts;
    trans.userToScreenPts(_pts, screenPts);
    QPainterPath pp = toPainterPath(screenPts, _width, _endStyle);
    painter->setBrush (getSelectedBrush());
    painter->setPen (getSelectedPen());
    painter->drawPath(pp);
    painter->drawPolyline(screenPts);
}

void PathShape::drawInteractive (QPainter * painter, const QVector<QPoint> & pathPts, const QPoint & pos)
{
    if (0 == pathPts.size())
    {
        return;
    }

    // we want to show the path to the current mouse pointer so add the mouse pos 
    QVector<QPoint> pts(pathPts);
    pts << pos;
    QPainterPath pp = toPainterPath(pts, INTERACTIVE_PATH_WIDTH, INTERACTIVE_PATH_ENDSTYLE);

    // draw the bulked out path and center line.
    painter->setBrush (getInteractiveBrush());
    painter->setPen (getInteractivePen());
    painter->drawPath(pp);
    painter->drawPolyline(pts);
}

QString PathShape::getAttributes() const
{
    // get attributes specific to path
    QString attr;
    QTextStream ts(&attr);
    ts.setRealNumberNotation(QTextStream::FixedNotation);
    ts.setRealNumberPrecision (3);
    ts << "Width: " << _width << "\n";
    ts << "End Style: " << _endStyle;

    return attr;
}

QPainterPath PathShape::toPainterPath(const QPolygon & pathPts, uint32_t pathWidth, uint32_t endStyle)
{
    QPainterPath pp;
    QPolygon pts(pathPts);
    QPoint lastPt, thisPt;

    // TODO: support different _endStyle setting for path

    // adjust points to add the path width
    foreach (const QPoint & pt, pts)
    {
        if (!lastPt.isNull())
        {
            thisPt = pt;
            int32_t distX = abs(lastPt.x() - thisPt.x());
            int32_t distY = abs(lastPt.y() - thisPt.y());

            // path segment is horizontal
            if (distX > distY)
            {
                lastPt.ry() -= pathWidth / 2;
                thisPt.ry() += pathWidth / 2;
            }
            // path segment is vertical 
            else
            {
                lastPt.rx() -= pathWidth / 2;
                thisPt.rx() += pathWidth / 2;
            }

            // add path segment
            pp.addRect(QRect(lastPt, thisPt));
        }

        lastPt = pt;
    }

    return pp.simplified();
}
