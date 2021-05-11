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
    // initialize index 0 layout
    setStepTo(current_step);

    // for debug
    setStepTo(1);
}

void ReplateWidget::initConfig(const QString& cfgpath) const
{
    MLConfigManager::get().initFromFile(cfgpath);
}

void ReplateWidget::setStepTo(int step)
{
    ui.pipelineWidget->setCurrentIndex(step);
    if (step == 0)
    {
        ui.buttonLastStep->hide();
    }
    else
    {
        ui.buttonLastStep->show();
    }
}

void ReplateWidget::nextStep()
{
    if (MLDataManager::get().is_prepared(current_step + 1))
    {
        current_step++;
        ui.pipelineWidget->setCurrentIndex(current_step);
    }
}

void ReplateWidget::lastStep()
{
    current_step--;
    ui.pipelineWidget->setCurrentIndex(current_step);
}
