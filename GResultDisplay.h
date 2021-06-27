#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QBoxLayout>
#include <QValidator>
#include <QDialogButtonBox>
#include <QTimer>

#include "GBaseWidget.h"
#include "GVideoPlayer.h"
#include "MLPlayerHandler.hpp"

class GResultDisplay;
namespace Ui { class GResultWidget; }
class Step3Widget;

class GResultWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GResultWidget(QWidget* parent = 0, Step3Widget* step3widget = 0);

    void setPathSelectModel(bool b);
    void clearMouseSelection();

    void setInpaintState(bool b);
    void play();
public slots:
    void editFrameRate();
    void changeFrameRate();

private:
    friend class GResultDisplay;
    QTimer timer;
    Ui::GResultWidget* ui;
    Step3Widget* step3widget;

    int current_frame_id_;
    
    QLineEdit* frame_rate_edit_;
    int* duration;
    MLPlayerHandler* player_manager;
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