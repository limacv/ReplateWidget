#ifndef GRESULTDISPLAY_H
#define GRESULTDISPLAY_H

#include <QWidget>
#include <QLineEdit>
#include <QBoxLayout>
#include <QValidator>
#include <QDialogButtonBox>
#include <QTimer>

#include "GBaseWidget.h"
#include "GVideoPlayer.h"

namespace Ui { class GResultWidget; };
class GResultDisplay;

class GResultWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GResultWidget(QWidget* parent = 0);

    void setStillState(bool b);
    void setInpaintState(bool b);

    

public slots:
    void play();
    void stop();
    void updateFrame();
    void editFrameRate();
    void changeFrameRate();

private:
    friend class GResultDisplay;
    QTimer timer;
    Ui::GResultWidget* ui;

    int current_frame_id_;
    
    QLineEdit* frame_rate_edit_;
    int* duration;
public:
    GResultDisplay* display_widget;
};


class GResultDisplay : public GBaseWidget
{
    Q_OBJECT

public:
    explicit GResultDisplay(QWidget *parent = 0);
    void setparent(GResultWidget* parent) { parent_widget = parent; }

protected:
    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const { return QSize(480, 200); }
    virtual void paintEvent(QPaintEvent *event);
    
private:
    GResultWidget* parent_widget;
};

#endif // GRESULTDISPLAY_H