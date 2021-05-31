#include "GBaseWidget.h"

GBaseWidget::GBaseWidget(QWidget *parent)
    : QWidget(parent)
    , mouse_select_start_(false)
    , select_path_mode_(false)
{
    
}

bool GBaseWidget::pathMode() const
{
    return select_path_mode_;
}

void GBaseWidget::setPathMode(bool b)
{
    select_path_mode_ = b;
    updateCursor();
    if (b) clearMouseSelection();
}

QRect GBaseWidget::curSelectRect() const
{
    return transMat().mapRect(curSelectRectF()).toRect();
}

QRectF GBaseWidget::curSelectRectF() const
{
    if (pathMode())
        return curSelectPath().boundingRect();
    else
        return rect_select_.normalized();
}

void GBaseWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    paintMouseSelection(painter);
}

void GBaseWidget::mousePressEvent(QMouseEvent *event)
{
    mouse_pos_ = getMousePosNorm(event->pos());
//    qDebug() << "Mouse Press." << event->pos() << "norm:" << mouse_pos_;

    if (event->button() == Qt::LeftButton) {
        mouse_select_start_ = true;
        if (!pathMode()) {
            clearMouseSelection();

            rect_select_.setTopLeft(mouse_pos_);
            rect_select_.setBottomRight(mouse_pos_);
            rect_select_ = rect_select_.normalized();
        }
    }
    else if (event->button() == Qt::RightButton) {
        clearMouseSelection();
    }

    update();
}

void GBaseWidget::mouseMoveEvent(QMouseEvent *event)
{
    mouse_pos_ = getMousePosNorm(event->pos());

    if (!this->rect().contains(event->pos())) {
        //qDebug() << "[Discard] Mouse Move." << event->pos() << "norm:" << mouse_pos_;
        return;
    }

    if (mouse_select_start_) {
        if(!pathMode())
            rect_select_.setBottomRight(mouse_pos_);
    }

    update();
}

void GBaseWidget::mouseReleaseEvent(QMouseEvent *event)
{
    mouse_pos_ = getMousePosNorm(event->pos());
//    qDebug() << "Mouse Release." << event->pos() << "norm:" << mouse_pos_;

    // TODO: add check for pos outside of rect
    if (mouse_select_start_) {
        if (pathMode()) {
            if (path_select_.elementCount() == 0) {
                path_start_pos_ = mouse_pos_;
                path_select_.moveTo(mouse_pos_);
                path_select_.lineTo(mouse_pos_);
            }
            else
                path_select_.lineTo(mouse_pos_);
        }
        else {
            rect_select_ = rect_select_.normalized();
        }
        mouse_select_start_ = false;
    }
    update();
}

void GBaseWidget::updateCursor()
{
    if (pathMode())
        this->setCursor(Qt::CrossCursor);
    else
        this->setCursor(Qt::ArrowCursor);
}

QPainterPath GBaseWidget::curSelectPath() const
{
    return path_select_;
}

GRoiPtr GBaseWidget::curSelectRoi() const
{
    if (path_select_.isEmpty())
        return GRoiPtr(new GRoi(rect_select_));
    else
        return GRoiPtr(new GRoi(path_select_));
}

QPointF GBaseWidget::getMousePosNorm(QPoint pos) const
{
    return QPointF(pos.x() / (qreal)this->width(),
                   pos.y() / (qreal)this->height());
}

QMatrix GBaseWidget::transMat() const
{
    return transform_mat_;
}

void GBaseWidget::paintMouseSelection(QPainter &painter)
{
    if (pathMode() && path_select_.elementCount() > 0) {
        QPainterPath pp = transMat().map(path_select_);
        pp.closeSubpath();
        painter.strokePath(pp, QPen(Qt::black, 1, Qt::DashLine));

        // draw start point
        painter.setBrush(QBrush(Qt::black));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(transMat().map(path_start_pos_), 2, 2);
        painter.setBrush(QBrush(Qt::NoBrush));
    }
    else {
        QRectF rect = transMat().mapRect(rect_select_);
        painter.setPen(QPen(Qt::black, 1, Qt::DashLine));
        painter.drawRect(rect);
    }
}

void GBaseWidget::resizeEvent(QResizeEvent *event)
{
    transform_mat_ = QMatrix();
    transform_mat_.scale(this->width(), this->height());
    update();
}

void GBaseWidget::clearMouseSelection()
{
    path_start_pos_ = QPoint();
    rect_select_ = QRect();
    path_select_ = QPainterPath();
    update();
}
