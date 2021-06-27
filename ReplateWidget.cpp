#include "ReplateWidget.h"
#include "MLDataManager.h"
#include "MLConfigManager.h"
#include <qdebug.h>
#include "Step1Widget.h"

ReplateWidget::ReplateWidget(QWidget *parent)
    : QMainWindow(parent),
    current_step(0)
{
    initConfig("./config.yaml");
    ui.setupUi(this);
    MLConfigManager::get().setup_ui(this);

    stepwidgets[0] = ui.step1Widget; stepbuttons[0] = ui.buttonStep1;
    stepwidgets[1] = ui.step2Widget; stepbuttons[1] = ui.buttonStep2;
    stepwidgets[2] = ui.step3Widget; stepbuttons[2] = ui.buttonStep3;
    stepwidgets[3] = ui.step4Widget; stepbuttons[3] = ui.buttonStep4;
    connect(ui.buttonStep1, &QPushButton::clicked, this, [this] { setStepTo(0); });
    connect(ui.buttonStep2, &QPushButton::clicked, this, [this] { setStepTo(1); });
    connect(ui.buttonStep3, &QPushButton::clicked, this, [this] { setStepTo(2); });
    connect(ui.buttonStep4, &QPushButton::clicked, this, [this] { setStepTo(3); });

    connect(ui.buttonNextStep, &QPushButton::clicked, this, &ReplateWidget::nextStep);
    connect(ui.buttonLastStep, &QPushButton::clicked, this, &ReplateWidget::lastStep);

    connect(ui.actionClear_Cache, &QAction::toggled, this, &ReplateWidget::clearCache);
    connect(ui.actionClear_All_Cache, &QAction::toggled, this, &ReplateWidget::clearAllCache);
    ui.menuFile->addAction(MLConfigManager::get().config_widget_view_action());
    
    connect(ui.step1Widget, &Step1Widget::videoloaded, this, [this]() {ui.buttonNextStep->setEnabled(true); });
    
    ui.buttonNextStep->setEnabled(false);

    ui.menuWindow->addAction(ui.step3Widget->showMainDisplayAction());
    ui.menuWindow->addAction(ui.step3Widget->showResultAction());
    ui.menuWindow->addAction(ui.step3Widget->showControlAction());
    ui.menuWindow->addAction(ui.step3Widget->showTimeLineAction());

    // initialize the StepXWidget
    for (int i = 0; i < STEP_COUNT; ++i)
    {
        stepwidgets[i]->hide();
    }

    // initialize index 0 layout
    setStepTo(current_step);
}

void ReplateWidget::initConfig(const QString& cfgpath) const
{
    MLConfigManager::get().readFromFile(cfgpath);
}

void ReplateWidget::setStepTo(int step)
{
    step = step < 0 ? 0 : (step >= STEP_COUNT ? STEP_COUNT - 1 : step);
    stepwidgets[current_step]->hide();
    ui.pipelineWidget->setCurrentIndex(step);
    current_step = step;
    stepwidgets[current_step]->show();

    if (step == 0)
    {
        ui.buttonLastStep->hide();
        ui.buttonNextStep->show();
    }
    else if (step == STEP_COUNT - 1)
    {
        ui.buttonLastStep->show();
        ui.buttonNextStep->hide();
    }
    else
    {
        ui.buttonNextStep->show();
        ui.buttonLastStep->show();
    }

    if (step == 2)
        ui.menuWindow->setEnabled(true);
    else
        ui.menuWindow->setEnabled(false);

    // set button states
    int i = 0;
    for (; i < step; ++i)
        setButtonValid(*stepbuttons[i]);
    setButtonActive(*stepbuttons[i++]);
    for (; i < STEP_COUNT; ++i)
        setButtonInvalid(*stepbuttons[i]);
}

void ReplateWidget::nextStep()
{
    setStepTo(current_step + 1);
}

void ReplateWidget::lastStep()
{
    setStepTo(current_step - 1);
}

void ReplateWidget::setButtonActive(QPushButton& button)
{
    QFont font = button.font();
    font.setBold(true);
    button.setFont(font);
    button.setEnabled(true);
}

void ReplateWidget::setButtonInvalid(QPushButton& button)
{
    QFont font = button.font();
    font.setBold(false);
    button.setFont(font);
    button.setDisabled(true);
}

void ReplateWidget::setButtonValid(QPushButton& button)
{
    QFont font = button.font();
    font.setBold(false);
    button.setFont(font);
    button.setEnabled(true);
}

void ReplateWidget::clearCache()
{
    
}

void ReplateWidget::clearAllCache()
{
}


