#pragma once

#include <QWidget>
#include <qtimer.h>
#include "MLStep1Data.h"

namespace Ui { class Step1Widget; };

class Step1Widget : public QWidget
{
	Q_OBJECT

public:
	Step1Widget(QWidget *parent = Q_NULLPTR);
	~Step1Widget();
	void initState();

private slots:
	void updateImage(int frameidx) const;
	void runDetect();
	void runTrack();

private:
	Ui::Step1Widget *ui;

	MLStep1Data data;
	QTimer playTimer;
};