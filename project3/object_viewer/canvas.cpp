#include "canvas.h"

#include <QPainter>
#include <QMouseEvent>
#include <QTextStream>

// 新增：mergePaths 用到 std::reverse
#include <algorithm>
#include <iostream>

Canvas::Canvas(QWidget *parent)
    : QWidget(parent)
    , _command(None)
    , _orthagonal(false)
    , _dragging(false)    // 新增
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
        // ===== 修改：多選、拖曳 =====
        // 按在已選取的圖形上就開始拖曳
        if (Qt::LeftButton == event->button() && Qt::NoModifier == event->modifiers())
        {
            foreach (ShapeObject * s, objectsAt(event->pos()))
            {
                if (_selectedObjects.contains(s))
                {
                    _dragging = true;
                    _dragStart = _dragPos = event->pos();
                    break;
                }
            }
        }

        if (!_dragging)
        {
            selectAt(event->pos(), event->modifiers());
        }
        // ===== 修改結束 =====
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

// ===== 新增：選取 / 移動 / 復原 =====
// 回傳 pos 底下所有圖形，最上層的在前面
QVector<ShapeObject*> Canvas::objectsAt(const QPoint & pos) const
{
    QVector<ShapeObject*> hits;

    for (QVector<ShapeObject*>::const_reverse_iterator it = _objects.rbegin(); it != _objects.rend(); it++)
    {
        if ((*it)->select(pos, _transform))
        {
            hits << *it;
        }
    }

    return hits;
}

// N / Shift+N：在上次點的位置，改選下一層（step = 1）或上一層（step = -1）的圖形
QString Canvas::selectNext(int step)
{
    const QString name = (step > 0) ? QString("Select Next") : QString("Select Previous");
    QVector<ShapeObject*> hits = objectsAt(_clickPos);

    if (_selectedObjects.isEmpty() || hits.isEmpty())
    {
        return name + QString(": no object selected");
    }

    ShapeObject * current = _selectedObjects.last();
    const int n = hits.size();
    int i = hits.indexOf(current);

    // 目前選的圖形不在這個點，就從最上層（下一層）或最底層（上一層）開始
    if (-1 == i && step < 0)
    {
        i = n;
    }

    // 跳過已經選取的，到頭就繞回來
    for (int k = 1; k <= n; k++)
    {
        ShapeObject * next = hits[((i + k * step) % n + n) % n];

        if (next == current || !_selectedObjects.contains(next))
        {
            _selectedObjects.last() = next;
            break;
        }
    }

    _connectedObjects.clear();
    update();

    current = _selectedObjects.last();
    return name + QString(": %1 on layer %2 (%3 of %4 shapes at this point)")
        .arg(current->getTypeString()).arg(current->getLayer())
        .arg(hits.indexOf(current) + 1).arg(hits.size());
}

void Canvas::clearSelection()
{
    _selectedObjects.clear();
    _connectedObjects.clear();
    update();
}

// 點選：一般 / Shift 加選 / Ctrl 取消
void Canvas::selectAt(const QPoint & pos, Qt::KeyboardModifiers modifiers)
{
    QVector<ShapeObject*> hits = objectsAt(pos);
    _clickPos = pos;

    // Shift + 點：加選最上層還沒選的圖形
    if (modifiers & Qt::ShiftModifier)
    {
        foreach (ShapeObject * s, hits)
        {
            if (!_selectedObjects.contains(s))
            {
                _selectedObjects << s;
                break;
            }
        }
    }
    // Ctrl + 點：取消最上層已選的圖形
    else if (modifiers & Qt::ControlModifier)
    {
        foreach (ShapeObject * s, hits)
        {
            if (_selectedObjects.contains(s))
            {
                _selectedObjects.remove(_selectedObjects.indexOf(s));
                break;
            }
        }
    }
    else
    {
        _selectedObjects.clear();

        if (!hits.isEmpty())
        {
            _selectedObjects << hits.first();
        }
    }

    _connectedObjects.clear();
    update();
}

void Canvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (_dragging)
    {
        _dragging = false;

        // 滑鼠幾乎沒動，當成一般點選
        if ((event->pos() - _dragStart).manhattanLength() < 3)
        {
            selectAt(event->pos(), event->modifiers());
        }
        else
        {
            moveSelected(_dragStart, event->pos());
        }
    }

    QWidget::mouseReleaseEvent(event);
}

// 移動選取的圖形（產生新的圖形取代，這樣 undo 才回得去）
void Canvas::moveSelected(const QPoint & from, const QPoint & to)
{
    const QPointF offset = _transform.screenToUser(to) - _transform.screenToUser(from);

    saveUndo();

    for (int i = 0; i < _selectedObjects.size(); i++)
    {
        ShapeObject * movedObject = _selectedObjects[i]->moved(offset);
        _objects[_objects.indexOf(_selectedObjects[i])] = movedObject;
        _selectedObjects[i] = movedObject;
    }

    _connectedObjects.clear();
    update();
}

// 修改前先存一份目前的圖形 list
// 圖形都沒有真的 delete，所以存起來的指標還能用
void Canvas::saveUndo()
{
    _undoStack << _objects;

    if (_undoStack.size() > 100)
    {
        _undoStack.remove(0);
    }
}

void Canvas::clearUndo()
{
    _undoStack.clear();
}

QString Canvas::undo()
{
    if (_undoStack.isEmpty())
    {
        return QString("Undo: nothing to undo");
    }

    _objects = _undoStack.last();
    _undoStack.removeLast();
    _selectedObjects.clear();
    _connectedObjects.clear();
    update();

    return QString("Undo: %1 step(s) left").arg(_undoStack.size());
}
// ===== 新增結束 =====

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

    // 新增：拖曳時重畫
    if (_dragging)
    {
        _dragPos = event->pos();
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

    // ===== 新增：connectivity 上色、拖曳預覽 =====
    // 相連的圖形畫成橘色
    painter.setPen(QPen(QColor(255, 140, 0), 2));
    painter.setBrush(QColor(255, 200, 0, 150));

    foreach (ShapeObject * s, _connectedObjects)
    {
        painter.drawPath(s->outline(_transform));
    }

    // 畫選取的圖形，拖曳中會跟著滑鼠移動
    if (_dragging)
    {
        painter.translate(_dragPos - _dragStart);
    }

    foreach (ShapeObject * s, _selectedObjects)
    {
        s->drawSelected(&painter, _transform);
    }
    // ===== 新增結束 =====

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

    _selectedObjects.clear();     // 修改
    _connectedObjects.clear();    // 新增
    _objects.clear();
    update();
}

// 修改：刪除所有選取的圖形，可以 undo
void Canvas::deleteObject()
{
    if (!_selectedObjects.isEmpty())
    {
        saveUndo();
    }

    foreach (ShapeObject * s, _selectedObjects)
    {
        int i = _objects.indexOf(s);

        if (-1 != i)
        {
//            delete s;
            _objects.remove(i);
        }
    }

    _selectedObjects.clear();
    _connectedObjects.clear();
    update();
}

// 修改：列出所有選取的圖形
QString Canvas::query() const
{
    if (_selectedObjects.isEmpty())
    {
        return QString("No object selected");
    }

    QString data;
    QTextStream ts(&data);
    ts.setRealNumberNotation(QTextStream::FixedNotation);
    ts.setRealNumberPrecision (3);

    foreach (ShapeObject * s, _selectedObjects)
    {
        if (s != _selectedObjects.first())
        {
            ts << "\n";
        }

        ts << "Object: " << s->getTypeString() << "\n";
        ts << "Layer: " << s->getLayer() << "\n";

        QString att = s->getAttributes();

        if (att.size())
        {
            ts << att << "\n";
        }

        ts << "User coordinates:\n";

        foreach (const QPointF & pt, s->getPoints())
        {
            QPoint roundPt = pt.toPoint();
            ts << "(" << roundPt.x() << ", " << roundPt.y() << ")\n";
        }
    }

    return data;
}

// ===== 新增：合併 path（M） =====
static double distance(const QPointF & a, const QPointF & b)
{
    return qAbs(a.x() - b.x()) + qAbs(a.y() - b.y());
}

// 端點線段 p0-p1 在螢幕上的範圍，往外多 1 pixel，剛好碰到也算重疊
static QPainterPath endArea(const QPointF & p0, const QPointF & p1, uint32_t width, const Transform & trans)
{
    QVector<QPointF> pts;
    pts << p0 << p1;
    QPainterPath pp = PathShape(pts, width).outline(trans);
    QPainterPathStroker stroker;
    stroker.setWidth(2);
    return pp.united(stroker.createStroke(pp));
}

// 把選取的 path 合併成一條
// 端點有重疊才合併，距離近的端點先接
QString Canvas::mergePaths()
{
    QVector<PathShape*> paths;

    foreach (ShapeObject * s, _selectedObjects)
    {
        PathShape * p = dynamic_cast<PathShape*>(s);

        if (p)
        {
            paths << p;
        }
    }

    if (paths.size() < 2 || paths.size() != _selectedObjects.size())
    {
        return QString("Merge Paths: select 2 or more paths with Shift + click");
    }

    foreach (PathShape * p, paths)
    {
        if (p->getLayer() != paths[0]->getLayer())
        {
            return QString("Merge Paths: paths must be on the same layer");
        }
    }

    QVector<QPointF> merged = paths[0]->getPoints();
    uint32_t width = paths[0]->getWidth();
    QVector<PathShape*> rest = paths.mid(1);

    while (!rest.isEmpty())
    {
        // 找端點和 merged 端點重疊的 path
        // 0：尾接頭  1：尾接尾  2：頭接尾  3：頭接頭
        const int n = merged.size();
        QPainterPath mergedEnd[2] = {endArea(merged[n - 2], merged[n - 1], width, _transform),
                                     endArea(merged[1], merged[0], width, _transform)};
        int best = -1;
        int bestCase = 0;
        double bestDist = -1;

        for (int i = 0; i < rest.size(); i++)
        {
            const QVector<QPointF> & pts = rest[i]->getPoints();
            const int m = pts.size();
            QPainterPath pathEnd[2] = {endArea(pts[0], pts[1], rest[i]->getWidth(), _transform),
                                       endArea(pts[m - 1], pts[m - 2], rest[i]->getWidth(), _transform)};
            double dist[4] = {distance(merged.last(), pts.first()),
                              distance(merged.last(), pts.last()),
                              distance(merged.first(), pts.last()),
                              distance(merged.first(), pts.first())};

            for (int k = 0; k < 4; k++)
            {
                bool overlap = mergedEnd[k < 2 ? 0 : 1].intersects(pathEnd[(0 == k || 3 == k) ? 0 : 1]);

                if (overlap && (bestDist < 0 || dist[k] < bestDist))
                {
                    best = i;
                    bestCase = k;
                    bestDist = dist[k];
                }
            }
        }

        if (-1 == best)
        {
            return QString("Merge Paths: path ends do not overlap, nothing merged");
        }

        // 把點反轉，統一變成接在 merged 的尾巴
        QVector<QPointF> pts = rest[best]->getPoints();

        if (bestCase >= 2)
        {
            std::reverse(merged.begin(), merged.end());
        }

        if (1 == bestCase || 2 == bestCase)
        {
            std::reverse(pts.begin(), pts.end());
        }

        // 兩段在同一直線上而且重疊，就拿掉 merged 的最後一點
        {
            const QPointF & a = merged[merged.size() - 2];
            const QPointF & b = merged.last();
            const QPointF dir = b - a;
            const QPointF & c = pts[0];
            const QPointF & d = pts[1];
            bool sameLine = (a.y() == b.y() && c.y() == b.y() && d.y() == b.y()) ||
                            (a.x() == b.x() && c.x() == b.x() && d.x() == b.x());
            double along = QPointF::dotProduct(c - a, dir);

            if (sameLine && along >= 0 && along <= QPointF::dotProduct(dir, dir) && QPointF::dotProduct(d - b, dir) >= 0)
            {
                merged.removeLast();
            }
        }

        // x、y 都不同時補一個轉角點，保持垂直水平
        if (merged.last().x() != pts.first().x() && merged.last().y() != pts.first().y())
        {
            const QPointF & end = merged.last();
            bool horizontal = (merged[merged.size() - 2].y() == end.y());
            merged << (horizontal ? QPointF(pts.first().x(), end.y()) : QPointF(end.x(), pts.first().y()));
        }

        merged << pts;
        width = qMax(width, rest[best]->getWidth());
        rest.remove(best);
    }

    // 拿掉重複的點和直線中間多餘的點
    QVector<QPointF> cleaned;

    foreach (const QPointF & pt, merged)
    {
        if (!cleaned.isEmpty() && cleaned.last() == pt)
        {
            continue;
        }

        if (cleaned.size() >= 2)
        {
            const QPointF & a = cleaned[cleaned.size() - 2];
            const QPointF & b = cleaned.last();

            if ((a.x() == b.x() && b.x() == pt.x() && (b.y() - a.y()) * (pt.y() - b.y()) >= 0) ||
                (a.y() == b.y() && b.y() == pt.y() && (b.x() - a.x()) * (pt.x() - b.x()) >= 0))
            {
                cleaned.removeLast();
            }
        }

        cleaned << pt;
    }

    // 刪掉原本的 path，加入合併後的
    saveUndo();
    PathShape * mergedPath = new PathShape(cleaned, width, paths[0]->getEndStyle(), paths[0]->getLayer());

    foreach (PathShape * p, paths)
    {
        _objects.remove(_objects.indexOf(p));
//        delete p;
    }

    _objects << mergedPath;
    _selectedObjects.clear();
    _selectedObjects << mergedPath;
    _connectedObjects.clear();
    update();

    return QString("Merge Paths: %1 paths merged into one path with %2 points").arg(paths.size()).arg(cleaned.size());
}

// ===== 新增結束 =====

// ===== 新增：connectivity（C） =====
// 從選取的圖形開始用 BFS，找出同一 layer 上所有重疊或相碰的圖形
QString Canvas::traceConnectivity()
{
    _connectedObjects.clear();

    if (_selectedObjects.isEmpty())
    {
        update();
        return QString("Connectivity: no object selected");
    }

    ShapeObject * seed = _selectedObjects.last();

    // 同 layer 圖形的範圍，往外多 1 pixel，剛好碰到也算相連
    QVector<ShapeObject*> shapes;
    QVector<QPainterPath> outlines;
    QPainterPathStroker stroker;
    stroker.setWidth(2);

    foreach (ShapeObject * s, _objects)
    {
        if (s->getLayer() == seed->getLayer())
        {
            QPainterPath pp = s->outline(_transform);
            shapes << s;
            outlines << pp.united(stroker.createStroke(pp));
        }
    }

    QVector<bool> visited(shapes.size(), false);
    QVector<int> queue;
    int start = shapes.indexOf(seed);
    visited[start] = true;
    queue << start;

    for (int head = 0; head < queue.size(); head++)
    {
        const QPainterPath & current = outlines[queue[head]];

        for (int i = 0; i < shapes.size(); i++)
        {
            if (!visited[i] && current.intersects(outlines[i]))
            {
                visited[i] = true;
                queue << i;
            }
        }
    }

    foreach (int i, queue)
    {
        _connectedObjects << shapes[i];
    }

    update();

    return QString("Connectivity: %1 shape(s) connected on layer %2").arg(_connectedObjects.size()).arg(seed->getLayer());
}
// ===== 新增結束 =====
