#pragma once

#include <QtWidgets/QMainWindow>
#include "QtStartSelector.h"
#include "ui_ReplateWidget.h"
#include "QtStartSelector.h"

const int STEP_COUNT = 4;

class ReplateWidget : public QMainWindow
{
    Q_OBJECT

public:
    ReplateWidget(QWidget *parent = Q_NULLPTR);

private:
    void initConfig(const QString& cfgpath) const;
    
    void setStepTo(int step);
    void nextStep();
    void lastStep();

private:
    Ui::ReplateWidgetClass ui;
    QtStartSelector starter;
    int current_step;
};
