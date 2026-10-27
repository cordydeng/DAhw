#ifndef SHAPE_H
#define SHAPE_H

#include "transform.h"

#include <QPoint>
#include <QVector>
#include <QPen>
#include <QPainter>

// base clase for all shaped
class ShapeObject
{
public:
    ShapeObject(uint32_t layer = 0);

public:
    // draw the shape usingt he attributes from the defaines layer
    virtual void draw (QPainter * painter, const Transform & trans) const = 0;
    // draw the shape selected
    virtual void drawSelected (QPainter * painter, const Transform & trans) const = 0; 
    // test if shape can be selected i.e. mouse button click "pos" over shape area
    virtual bool select (const QPoint & pos, const Transform & trans) = 0;
    // Text represenation for this this object type
    virtual QString getTypeString() const = 0;

public:
    // get the points that define this shape
    const QVector<QPointF> & getPoints() const
    {
        return _pts;
    }

    // get the layer that defines this shape 
    uint32_t getLayer() const
    {
        return _layer;
    }

    // get shape specific attributes for this shape
    virtual QString getAttributes() const
    {
        return QString();
    }

protected:
    QVector<QPointF> _pts;
    uint32_t _layer;
};

class RectangleShape : public ShapeObject
{
public:
    RectangleShape()
        : ShapeObject()
    {
    }

    RectangleShape(const QPointF & pt1, const QPointF & pt2, uint32_t layer = 0)
        : ShapeObject(layer)
    {
        _pts << pt1;
        _pts << pt2;
    }

public:
    void draw (QPainter * painter, const Transform & trans) const override;
    void drawSelected (QPainter * painter, const Transform & trans) const override;
    QString getTypeString() const override;
    bool select (const QPoint & pos, const Transform & trans);
    static void drawInteractive (QPainter * painter, const QVector<QPoint> & pts, const QPoint & pos);
};

class BoundaryShape : public ShapeObject
{
public:
    BoundaryShape()
        : ShapeObject()
    {
    }

    BoundaryShape(const QVector<QPointF> & pts, uint32_t layer = 0)
        : ShapeObject(layer)
    {
        _pts << pts;
    }

public:
    void draw (QPainter * painter, const Transform & trans) const override;
    void drawSelected (QPainter * painter, const Transform & trans) const override;
    QString getTypeString() const override;
    bool select (const QPoint & pos, const Transform & trans);
    static void drawInteractive (QPainter * painter, const QVector<QPoint> & pts, const QPoint & pos);
};

#define INTERACTIVE_PATH_WIDTH 20
#define INTERACTIVE_PATH_ENDSTYLE 0

class PathShape : public ShapeObject
{
public:
    PathShape()
        : ShapeObject()
    {
    }

    // path has additional width and endStyle attributes
    PathShape(const QVector<QPointF> & pts, uint32_t width = 20,  uint32_t endStyle = 0, uint32_t layer = 0)
        : ShapeObject(layer)
        , _width(width)
        , _endStyle(endStyle)
    {
        _pts << pts;
    }

public:
    void draw (QPainter * painter, const Transform & trans) const override;
    void drawSelected (QPainter * painter, const Transform & trans) const override;
    QString getTypeString() const override;
    QString getAttributes() const override;
    bool select (const QPoint & pos, const Transform & trans);
    static void drawInteractive (QPainter * painter, const QVector<QPoint> & pts, const QPoint & pos);

private:
    static QPainterPath toPainterPath(const QPolygon & pathPts, uint32_t width, uint32_t endStyle);

private:
    uint32_t _width;
    uint32_t _endStyle;
};

#endif // SHAPE_H
