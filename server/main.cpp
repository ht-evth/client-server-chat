#include <QCoreApplication>
#include "myserver.h"
#include <locale>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    setlocale(LC_CTYPE, "rus");
    _setmode(_fileno(stdout), _O_U16TEXT);
    QCoreApplication a(argc, argv);

    myserver Server;

    return a.exec();
}
