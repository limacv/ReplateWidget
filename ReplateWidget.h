#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ReplateWidget.h"
#include "StepWidgetBase.h"

const int STEP_COUNT = 4;

class ReplateWidget : public QMainWindow
{
    Q_OBJECT

public:
    ReplateWidget(QWidget *parent = Q_NULLPTR);

protected:
    void initConfig(const QString& cfgpath) const;
    
    void setStepTo(int step);
    void nextStep();
    void lastStep();
    
private:
    void setButtonActive(QPushButton& button);
    void setButtonInvalid(QPushButton& button);
    void setButtonValid(QPushButton& button);

private:
    Ui::ReplateWidgetClass ui;
    int current_step;
    std::array<StepWidgetBase*, STEP_COUNT> stepwidgets;
    std::array<QPushButton*, STEP_COUNT> stepbuttons;
};
