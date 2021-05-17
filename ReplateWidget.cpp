#include "ReplateWidget.h"
#include "MLDataManager.h"
#include "MLConfigManager.h"
#include <qdebug.h>

ReplateWidget::ReplateWidget(QWidget *parent)
    : QMainWindow(parent),
    current_step(0)
{
    initConfig("./config.yaml");
    ui.setupUi(this);
    qDebug() << "ReplateWidget::start StartSelector";
    starter.exec();
    
    qDebug() << "ReplateWidget::load raw video from " << starter.filepath;
    if (!starter.isProjFile)
    {
        qDebug() << "ReplateWidget::load from video";
        MLDataManager::get().load_raw_video(starter.filepath);
    }
    else
    {
        qDebug() << "ReplateWidget::load from project file";
    }

    connect(ui.buttonStep1, &QPushButton::clicked, this, [this] { setStepTo(0); });
    connect(ui.buttonStep2, &QPushButton::clicked, this, [this] { setStepTo(1); });
    connect(ui.buttonStep3, &QPushButton::clicked, this, [this] { setStepTo(2); });

    connect(ui.buttonNextStep, &QPushButton::clicked, this, &ReplateWidget::nextStep);
    connect(ui.buttonLastStep, &QPushButton::clicked, this, &ReplateWidget::lastStep);

    // initialize the StepXWidget
    ui.step1Widget->initState();
    ui.step2Widget->initState();
    ui.step3Widget->initState();
    ui.step4Widget->initState();

    // initialize index 0 layout
    setStepTo(current_step);

    // for debug
    setStepTo(1);
    setStepTo(2);
    setStepTo(3);
}

void ReplateWidget::initConfig(const QString& cfgpath) const
{
    MLConfigManager::get().initFromFile(cfgpath);
}

void ReplateWidget::setStepTo(int step)
{
    step = step < 0 ? 0 : (step >= STEP_COUNT ? STEP_COUNT - 1 : step);
    ui.pipelineWidget->setCurrentIndex(step);
    switch (step)
    {
    case 0:
        ui.buttonLastStep->hide();
        ui.buttonNextStep->show();
        ui.step1Widget->onWidgetShowup();
        break;
    case 1:
        ui.buttonNextStep->show();
        ui.buttonLastStep->show();
        ui.step2Widget->onWidgetShowup();
        break;
    case 2:
        ui.buttonNextStep->show();
        ui.buttonLastStep->show();
        ui.step3Widget->onWidgetShowup();
        break;
    case 3:
        ui.buttonLastStep->show();
        ui.buttonNextStep->hide();
        ui.step4Widget->onWidgetShowup();
        break;
    }
}

void ReplateWidget::nextStep()
{
    setStepTo(++current_step);
}

void ReplateWidget::lastStep()
{
    setStepTo(--current_step);
}
