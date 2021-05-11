#pragma once

#include <QWidget>
#include "MLStep2Data.h"

namespace Ui { class Step2Widget; };

class Step2Widget : public QWidget
{
	Q_OBJECT

public:
	Step2Widget(QWidget *parent = Q_NULLPTR);
	~Step2Widget();
	void initState();

private slots:
	void runStitching();

private:
	Ui::Step2Widget *ui;

	MLStep2Data data;
};
