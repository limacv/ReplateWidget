#include "ReplateWidget.h"
#include "MLDataManager.h"
#include "MLConfigManager.h"
#include <qdebug.h>

ReplateWidget::ReplateWidget(QWidget *parent)
    : QMainWindow(parent)
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

    ui.step1Widget->initState();
    // initialize the StepXWidget
    ui.pipelineWidget->setCurrentIndex(0);
}

void ReplateWidget::initConfig(const QString& cfgpath) const
{
    MLConfigManager::get().initFromFile(cfgpath);
}