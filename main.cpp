#include "minibrowser.h"
#include <QApplication>

int main(int argc, char *argv[])
{
   QApplication a(argc, argv);
   //QFontDatabase::addApplicationFont("rarch.ttf");
   MiniBrowser w;
   w.show();

   return a.exec();
}
