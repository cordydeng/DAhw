#ifndef TRANSFORM_H
#define TRANSFORM_H 

#include <QRectF>
#include <QPolygon>
#include <QVector>

// Transform used to mape from user to screen and back. Performs transformations 
// for zoom, pan ...
class Transform
{
public:
    Transform()
        : _scaleX(1.0)
        , _scaleY(1.0)
    {
        // set a default user coordinate rectasnge. used if setUserRect is not called
        _origRect = _userRect = QRectF(0, 0, 1000, 1000);
    }

    // bounds of all gds shapes in user coordinates
    void setUserRect(const QRectF & rect)
    {
        _userRect = _origRect = rect;
    }

    // scale in X and Y used for coordinate mappsings
    void setScale(const QSize & size)
    {
        _scaleX = double(size.width()) / _userRect.width();
        _scaleY = double(size.height()) / _userRect.height();
    }

    // map single user point to screen coordinates
    QPointF userToScreen(const QPointF & userPt) const
    {
        return QPointF((userPt.x() - _userRect.left()) * _scaleX, (userPt.y() - _userRect.top()) * _scaleY);
    }

    // map single screen point to user creen coordinates
    QPointF screenToUser(const QPointF & screenPt) const
    {
        return QPointF(screenPt.x() / _scaleX + _userRect.left(), (screenPt.y() / _scaleY + _userRect.top()));
    }

    void zoomIn(const QSize & size)
    {
        const double adjX = _userRect.width() / 4;
        const double adjY = _userRect.height() / 4;

        _userRect.adjust(adjX, adjY, -adjX, -adjY);
        setScale(size);
    }

    void zoomOut(const QSize & size)
    {
        const double adjX = _userRect.width() / 2;
        const double adjY = _userRect.height() / 2;

        _userRect.adjust(-adjX, -adjY, adjX, adjY);
        setScale(size);
    }

    void zoomFit(const QSize & size)
    {
        _userRect = _origRect;
        setScale(size);
    }

    void panUp(const QSize & size)
    {
        // pan is one third of scrceen size (pixels)
        const double adjY = (size.height() / 3) / _scaleY;
        _userRect.adjust(0, -adjY, 0, -adjY);
        setScale(size);
    }

    void panDown(const QSize & size)
    {
        // pan is one third of scrceen size (pixels)
        const double adjY = (size.height() / 3) / _scaleY;
        _userRect.adjust(0, adjY, 0, adjY);
        setScale(size);
    }

    void panLeft(const QSize & size)
    {
        // pan is one third of scrceen size (pixels)
        const double adjX = (size.width() / 3) / _scaleX;
        _userRect.adjust(-adjX, 0, -adjX, 0);
        setScale(size);
    }

    void panRight(const QSize & size)
    {
        // pan is one third of scrceen size (pixels)
        const double adjX = (size.width() / 3) / _scaleX;
        _userRect.adjust(adjX, 0, adjX, 0);
        setScale(size);
    }

    void userToScreenPts(const QVector<QPointF> & userPts, QVector<QPointF> & screenPts) const
    {
        foreach (const QPointF & pt, userPts)
        {
            screenPts << userToScreen(pt);
        }
    }

    // map user coordinates to screen coordinates
    void userToScreenPts(const QVector<QPointF> & userPts, QPolygon & screenPts) const
    {
        foreach (const QPointF & pt, userPts)
        {
            screenPts << userToScreen(pt).toPoint();
        }
    }

    // map screen coordinates to user coordinates
    void screenToUserPts(const QVector<QPoint> & screenPts, QVector<QPointF> & userPts) const
    {
        foreach (const QPointF & pt, screenPts)
        {
            userPts << screenToUser(pt);
        }
    }

private:
    QRectF _origRect;
    QRectF _userRect;
    double _scaleX;
    double _scaleY;
};

#endif // TRANSFORM_H
