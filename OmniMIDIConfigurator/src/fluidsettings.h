#ifndef FLUIDSETTINGS_H
#define FLUIDSETTINGS_H

#include <QWidget>
#include "fluidconfig.h"
#include "synthsettings.h"

namespace Ui {
class FluidSettings;
}

class FluidSettings : public QWidget, public SynthSettings
{
    Q_OBJECT

public:
    explicit FluidSettings(QWidget *parent = nullptr, FluidConfig *config = nullptr);
    ~FluidSettings();
    void loadSettings() override;
    void storeSettings() override;
    QWidget *getWidget() override;

private:
    Ui::FluidSettings *ui;
    FluidConfig *m_cfg = nullptr;
    std::map<std::string, int> driverIndexes;
};

#endif // FLUIDSETTINGS_H
