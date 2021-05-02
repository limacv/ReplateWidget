#pragma once

#include <QDialog>
#include "ui_QtStartSelector.h"
#include <qstring.h>

class QtStartSelector : public QDialog
{
	Q_OBJECT

public:
	QtStartSelector(QWidget *parent = Q_NULLPTR);
	~QtStartSelector();

private slots:
	void selectVideo();
	void selectProject();
private:
	Ui::QtStartSelector ui;

public:
	QString filepath;
	bool isProjFile;
};
