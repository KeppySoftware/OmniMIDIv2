#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "synthsettings.h"
#include "omconfig.h"
#include "sflistconfig.h"
#include <vector>
#include "aboutwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void displayEngineSettings();
    void loadSettingsToUI();
    void resetSynthDefaults();
    void storeConfig();

private:
    Ui::MainWindow *ui;
    SynthSettings *m_engineSettings = nullptr;
    OMConfig m_cfg;
    std::vector<SFListConfig *> m_sfLists;
    AboutWindow m_aboutWin;
};
#endif // MAINWINDOW_H
