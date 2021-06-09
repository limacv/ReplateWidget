#ifndef GMAINDISPLAY_H
#define GMAINDISPLAY_H

#include <QWidget>

#include "GBaseWidget.h"

class Step3Widget;

class GMainDisplay : public GBaseWidget
{
    Q_OBJECT

public:
    explicit GMainDisplay(Step3Widget* step3widget, QWidget *parent = 0);

    void toggleModifyMode(bool ismodify);
protected:
    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;

    virtual void paintEvent(QPaintEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private:
    Step3Widget* step3widget;

protected:
    // mouse related
    bool is_adjust_path_;
    //QPointF adjust_path_start_pos_;
    bool is_modify_;
};

#endif // GMAINDISPLAY_H
