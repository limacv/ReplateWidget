#pragma once

#include <QtWidgets/QMainWindow>
#include "QtStartSelector.h"
#include "ui_ReplateWidget.h"
#include "QtStartSelector.h"

class ReplateWidget : public QMainWindow
{
    Q_OBJECT

public:
    ReplateWidget(QWidget *parent = Q_NULLPTR);

private:
    void initConfig(const QString& cfgpath) const;

private:
    Ui::ReplateWidgetClass ui;
    QtStartSelector starter;
};
