#pragma once

#include <QWidget>
namespace Ui { class Step4Widget; };

class Step4Widget : public QWidget
{
	Q_OBJECT

public:
	Step4Widget(QWidget *parent = Q_NULLPTR);
	~Step4Widget();

private:
	Ui::Step4Widget *ui;
};
