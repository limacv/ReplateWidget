#ifndef GBASEWIDGET_H
#define GBASEWIDGET_H

#include <QWidget>
#include <QImage>
#include <QDebug>
#include <QMouseEvent>
#include <QMatrix>
#include <QPainter>
#include <QPainterPath>
#include "GRoi.h"

class GBaseWidget: public QWidget
{
    Q_OBJECT

public:
    explicit GBaseWidget(QWidget *parent = 0);

    bool pathMode() const;
    void setPathMode(bool b);

    QRect curSelectRect() const;
    QRectF curSelectRectF() const;
    QPainterPath curSelectPath() const;
    GRoiPtr curSelectRoi() const;

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void paintMouseSelection(QPainter &painter);
    virtual void resizeEvent(QResizeEvent *event);

    void updateCursor();
    QPointF getMousePosNorm(QPoint pos) const;
    QMatrix transMat() const;

public slots:
    virtual void clearMouseSelection();

protected:
    QMatrix transform_mat_;

    bool mouse_select_start_;
    bool select_path_mode_;

    QPointF mouse_pos_;
    QRectF rect_select_;

    QPainterPath path_select_;
    QPointF path_start_pos_;
};

#endif // GBASEWIDGET_H
