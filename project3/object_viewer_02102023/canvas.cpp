#include "canvas.h"

#include <QPainter>
#include <QMouseEvent>
#include <QTextStream>

#include <iostream>

Canvas::Canvas(QWidget *parent)
    : QWidget(parent)
    , _selectedObject(nullptr)
    , _command(None)
    , _orthagonal(false)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void Canvas::setUserRect(const QRect & userRect)
{
    _transform.setUserRect(userRect);
    _transform.setScale(rect().size());
}

QSize Canvas::minimumSizeHint() const
{
    return QSize(100, 100);
}

QSize Canvas::sizeHint() const
{
    return QSize(400, 200);
}

bool Canvas::start(CommandType type, bool orthagonal)
{
    if (None != _command)
    {
        return false;
    }

    // iitialize point collection for interative shape creation
    _command = type;
    _orthagonal = orthagonal;
    _pts.clear();

    return true;
}

void Canvas::stop(bool lastPos)
{
    if (lastPos)
    {
        _pts << _pos;
    }

    _command = None;
    update();
}

QVector<QPointF> Canvas::getUserPoints() const
{
    QVector<QPointF> userPoints;
    _transform.screenToUserPts(_pts, userPoints);
    return userPoints;
}

void Canvas::zoomIn()
{
    _transform.zoomIn(rect().size());
    update();
}

void Canvas::zoomOut()
{
    _transform.zoomOut(rect().size());
    update();
}

void Canvas::zoomFit()
{
    _transform.zoomFit(rect().size());
    update();
}

void Canvas::panUp()
{
    _transform.panUp(rect().size());
    update();
}

void Canvas::panDown()
{
    _transform.panDown(rect().size());
    update();
}

void Canvas::panLeft()
{
    _transform.panLeft(rect().size());
    update();
}

void Canvas::panRight()
{
    _transform.panRight(rect().size());
    update();
}

// convert point to orthoganal coordinates
QPoint Canvas::toOrthogonal(const QPoint & pt) const
{
    if (!_pts.empty())
    {
        QPoint thisPt = pt;
        QPoint lastPt = _pts[_pts.size() - 1];
        int32_t distX = abs(lastPt.x() - thisPt.x());
        int32_t distY = abs(lastPt.y() - thisPt.y());

        if (distX > distY)
        {
            thisPt.setY(lastPt.y());
        }
        else
        {
            thisPt.setX(lastPt.x());
        }

        return thisPt;
    }

    return pt;
}

void Canvas::mousePressEvent(QMouseEvent *event)
{
    // lets see if a shape can be selected not currently creating a shape
    if (None == _command)
    {
        _selectedObject = nullptr;

        for (QVector<ShapeObject*>::reverse_iterator it = _objects.rbegin(); it != _objects.rend(); it++)
        {
            if ((*it)->select(event->pos(), _transform))
            {
                _selectedObject = *it;
                break;
            }
        }

        update();
    }
    // interactive shape creatoin .. collect current point 
    else
    {
        if (_orthagonal)
        {
            _pts << toOrthogonal(event->pos());
        }
        else
        {
            _pts << event->pos();
        }

        // tall the command loop that the mouse button was pressed
        emit ButtonPress();
    }

    QWidget::mousePressEvent(event);
}

void Canvas::keyPressEvent(QKeyEvent * event)
{
    if (Qt::Key_Enter == event->key() || Qt::Key_Return == event->key())
    {
        // tall the command loop that the enter/return key was pressed
        emit EnterKeyPress();
    }

    QWidget::keyPressEvent(event);
}

void Canvas::resizeEvent(QResizeEvent *event)
{
    // adjust the transformation based on the new screen size
    _transform.setScale(event->size());
    QWidget::resizeEvent(event);
}

void Canvas::mouseMoveEvent(QMouseEvent *event)
{
    // track the current cursor location and adjust for orthagonal if needed
    if (_orthagonal)
    {
        _pos = toOrthogonal(event->pos());
    }
    else
    {
        _pos = event->pos();
    }

    if (None != _command)
    {
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void Canvas::paintEvent(QPaintEvent * event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    painter.save();

    /* draw any existing shapes */
    foreach (ShapeObject * s, _objects)
    {
        s->draw(&painter, _transform);
    }

    /* handle interactive shape creation */

    switch (_command)
    {
        case Rectangle:
            RectangleShape::drawInteractive(&painter, _pts, _pos);
            break;
        case Boundary:
            BoundaryShape::drawInteractive(&painter, _pts, _pos);
            break;
        case Path:
            PathShape::drawInteractive(&painter, _pts, _pos);
            break;
        default:
            break; 
    }

    /* draw the selected shape */
    if (_selectedObject)
    {
        _selectedObject->drawSelected(&painter, _transform);
    }

    painter.restore();
}

void Canvas::addRectangle(const QPointF & pt1, const QPointF & pt2, uint32_t layer)
{
    _objects << new RectangleShape (pt1, pt2, layer);
}

void Canvas::addBoundary(const QVector<QPointF> & pts, uint32_t layer)
{
    _objects << new BoundaryShape (pts, layer);
}

void Canvas::addPath(const QVector<QPointF> & pts, uint32_t width, uint32_t endStyle, uint32_t layer)
{
    _objects << new PathShape (pts, width, endStyle, layer);
}

void Canvas::clear()
{
    foreach (ShapeObject * s, _objects)
    {
//        delete s;
    }

    _selectedObject = nullptr;
    _objects.clear();
    update();
}

void Canvas::deleteObject()
{
    if (_selectedObject)
    {
        int i = _objects.indexOf(_selectedObject);

        if (-1 != i)
        {
//            delete _selectedObject;
            _selectedObject = nullptr;
            _objects.remove(i);
            update();
        }
    }
}

QString Canvas::query() const
{
    if (nullptr == _selectedObject)
    {
        return QString("No object selected");
    }

    QString data;
    QTextStream ts(&data);
    ts.setRealNumberNotation(QTextStream::FixedNotation);
    ts.setRealNumberPrecision (3);
    ts << "Object: " << _selectedObject->getTypeString() << "\n";
    ts << "Layer: " << _selectedObject->getLayer() << "\n";

    QString att = _selectedObject->getAttributes();

    if (att.size())
    {
        ts << att << "\n";
    }

    ts << "User coordinates:\n";

    foreach (const QPointF & pt, _selectedObject->getPoints())
    {
        QPoint roundPt = pt.toPoint();
        ts << "(" << roundPt.x() << ", " << roundPt.y() << ")\n";
    }

    return data;
}
