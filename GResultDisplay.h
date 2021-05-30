#ifndef GRESULTDISPLAY_H
#define GRESULTDISPLAY_H

#include <QWidget>
#include <QLineEdit>
#include <QBoxLayout>
#include <QValidator>
#include <QDialogButtonBox>

#include "GBaseWidget.h"
//#include "GDataManager.h"
#include "GVideoPlayer.h"

class GResultDisplay : public GBaseWidget
{
    Q_OBJECT

public:
    explicit GResultDisplay(QWidget *parent = 0);

    void setStillState(bool b);
    void setInpaintState(bool b);

public slots:
    virtual void clearCurrentSelection();
    void play();
    void stop();
    void updateFrame(int id);
    void editFrameRate();
    void changeFrameRate();

protected:
    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const { return QSize(480, 200); }
    virtual void paintEvent(QPaintEvent *event);

protected:
    GVideoPlayer *video_player_;

    // Not sure whether this design is good or not
    int current_frame_id_;

    QLineEdit *frame_rate_edit_;
};

#endif // GRESULTDISPLAY_H
