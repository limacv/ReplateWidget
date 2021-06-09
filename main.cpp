#include "ReplateWidget.h"
#include <QtWidgets/QApplication>
#include <qresource.h>
#include <qfile.h>
#include <qtextstream.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFile file(":/dark.qss");
    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream stream(&file);
    a.setStyleSheet(stream.readAll());

    ReplateWidget w;
    w.show();
    return a.exec();
}
