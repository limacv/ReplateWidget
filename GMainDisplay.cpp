#include "GMainDisplay.h"
#include "Step3Widget.h"
#include "MLDataManager.h"
#include "MLDataStructure.h"

GMainDisplay::GMainDisplay(Step3Widget* step3widget, QWidget *parent)
    : GBaseWidget(parent)
    , step3widget(step3widget)
    , is_adjust_path_(false)
    , is_modify_(false)
{
}

bool GMainDisplay::toggleModifyMode()
{
    is_modify_ = !is_modify_;
    setPathSelectionMode(is_modify_);
    if (!is_modify_) {
        step3widget->setPathRoi(curSelectRoi());
        clearMouseSelection();
    }
    return is_modify_;
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
    
    if (step3widget->cur_tracked_path)
        step3widget->cur_tracked_path->paint(painter, step3widget->cur_frameidx);
}

void GMainDisplay::mousePressEvent(QMouseEvent *event)
{
    mouse_pos_ = getMousePosNorm(event->pos());

    if (step3widget->is_auto_selection())
    {
        rect_select_ = rect_pre_select_.normalized();
    }
    else if (!is_modify_ 
        && step3widget->cur_tracked_path 
        && step3widget->cur_tracked_path->checkInside(step3widget->cur_frameidx, mouse_pos_))
    {
        is_adjust_path_ = true;
        adjust_path_start_pos_ = mouse_pos_;
    }
    else
        GBaseWidget::mousePressEvent(event);
    update();
}

void GMainDisplay::mouseMoveEvent(QMouseEvent *event)
{
    mouse_pos_ = getMousePosNorm(event->pos());

    if (step3widget->is_auto_selection())
    {
        const auto& global_data = MLDataManager::get();
        static unsigned int selection_count = 0;
        QVector<BBox*> boxes = global_data.queryBoxes(step3widget->cur_frameidx, mouse_pos_);
        rect_pre_select_ = boxes.empty() ? QRect() : global_data.toPaintROI(boxes[selection_count++ / 5 % boxes.size()]->rect_global, QRect(), true);
    }
    else if (is_adjust_path_) // function to drag box to modify
    {
        QPointF off = mouse_pos_ - adjust_path_start_pos_;
        adjust_path_start_pos_ = mouse_pos_;
        step3widget->cur_tracked_path->translateRect(step3widget->cur_frameidx, off);
        //dataManager()->translatePathRect(off);
    }
    else
        GBaseWidget::mouseMoveEvent(event);
    update();
}

void GMainDisplay::mouseReleaseEvent(QMouseEvent *event)
{
    mouse_pos_ = getMousePosNorm(event->pos());

    if (step3widget->is_auto_selection())
    {
    }
    else if (is_adjust_path_) {
        is_adjust_path_ = false;
        if (event->button() == Qt::RightButton) {
            //dataManager()->movePathRectCenter(mouse_pos_);
            step3widget->cur_tracked_path->moveRectCenter(step3widget->cur_frameidx, mouse_pos_);
        }
        step3widget->trackPath();
    }
    else {
        if (event->button() == Qt::LeftButton)
            GBaseWidget::mouseReleaseEvent(event);
        else
            clearMouseSelection();
    }
    update();
}
