#include "GMainDisplay.h"
#include "Step3Widget.h"
#include "MLDataManager.h"
#include "MLDataStructure.h"
#include "GUtil.h"
#include "MLPathTracker.h"

GMainDisplay::GMainDisplay(Step3Widget* step3widget, QSlider* slider, QWidget* parent)
    : GBaseWidget(parent)
    , step3widget(step3widget)
    , is_adjust_path_(false)
    , is_modify_(false)
    , controlslider(slider)
{
}

void GMainDisplay::toggleModifyMode(bool ismodify)
{
    is_modify_ = ismodify;
    clearMouseSelection();
}

QSize GMainDisplay::sizeHint() const
{
    //const auto& global_data = MLDataManager::get();
    //return QSize(global_data.VideoWidth(), global_data.VideoHeight());
    return QWidget::sizeHint();
}

QSize GMainDisplay::minimumSizeHint() const
{
    // TODO: I don't know how to implement this function
    return QSize(480, 200);
}

void GMainDisplay::paintEvent(QPaintEvent *event)
{
    if (NULL == event) return;

    QPainter painter(this);
    
    MLDataManager::get().paintWarpedFrames(painter, step3widget->cur_frameidx);
    paintMouseSelection(painter);
    
    if (!rect_select_.isEmpty() || !path_select_.isEmpty())
    {
        step3widget->setCurrentEffect(nullptr);
    }

    if (step3widget->cur_tracked_path)
        step3widget->cur_tracked_path->paint(painter, step3widget->cur_frameidx);
}

void GMainDisplay::wheelEvent(QWheelEvent* event)
{
    auto dp = event->angleDelta();
    int dt = dp.y() > 0 ? 1 : -1;
    controlslider->setValue(controlslider->value() - dt);
}

void GMainDisplay::mousePressEvent(QMouseEvent *event)
{
    mouse_pos_ = getMousePosNorm(event->pos());

    if (is_modify_ && event->button() == Qt::LeftButton && step3widget->cur_tracked_path)
    {
        step3widget->cur_tracked_path->moveRectCenter(step3widget->cur_frameidx, mouse_pos_, true);
        is_adjust_path_ = true;
        clearMouseSelection();
        setCursor(Qt::SizeAllCursor);
    }
    else if (is_modify_ && event->button() == Qt::RightButton)
    {
        auto& flag = step3widget->cur_tracked_path->manual_adjust_[step3widget->cur_frameidx];
        flag = !flag;
    }
    else if (step3widget->is_auto_selection())
    {
        rect_select_ = rect_pre_select_.normalized();
    }
    else
        GBaseWidget::mousePressEvent(event);
    update();
}

void GMainDisplay::mouseMoveEvent(QMouseEvent *event)
{
    mouse_pos_ = getMousePosNorm(event->pos());

    if (is_modify_ && is_adjust_path_)
    {
        step3widget->cur_tracked_path->moveRectCenter(step3widget->cur_frameidx, mouse_pos_);
    }
    else if (step3widget->is_auto_selection())
    {
        const auto& global_data = MLDataManager::get();
        static unsigned int selection_count = 0;
        QVector<BBox*> boxes = global_data.queryBoxes(step3widget->cur_frameidx, mouse_pos_);
        rect_pre_select_ = boxes.empty() 
            ? QRect() 
            : global_data.toPaintROI(
                GUtil::addMarginToRect(boxes[selection_count++ / 5 % boxes.size()]->rect_global, RECT_MARGIN), 
                QRect(), true);
    }
    else
        GBaseWidget::mouseMoveEvent(event);
    update();
}

void GMainDisplay::mouseReleaseEvent(QMouseEvent *event)
{
    mouse_pos_ = getMousePosNorm(event->pos());

    if (is_modify_ && is_adjust_path_)
    {
        step3widget->trackPath();
        is_adjust_path_ = false;
        setCursor(Qt::ArrowCursor);
    }
    else if (step3widget->is_auto_selection())
    {
    }
    else 
    {
        if (event->button() == Qt::LeftButton)
            GBaseWidget::mouseReleaseEvent(event);
        else
            clearMouseSelection();
    }
    update();
}
