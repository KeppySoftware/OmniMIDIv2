#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QStyleFactory>
#include "utils.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Important to generate/fix config before starting the configurator
    // Temporary until a new API is implemented
    try {
        Utils::InitializeKdmapi();
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, FATAL_ERROR_TITLE, e.what());
        exit(0);
    }

    int r = 0;
    try {
        MainWindow w;
        w.show();
        r = a.exec();
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, FATAL_ERROR_TITLE, e.what());
    }
    return r;
}
