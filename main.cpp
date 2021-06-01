#include "ReplateWidget.h"
#include <QtWidgets/QApplication>
#include <qresource.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ReplateWidget w;
    w.show();
    return a.exec();
}
