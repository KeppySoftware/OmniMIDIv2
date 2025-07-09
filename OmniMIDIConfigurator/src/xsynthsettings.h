#ifndef XSYNTHSETTINGS_H
#define XSYNTHSETTINGS_H

#include <QWidget>
#include "synthsettings.h"
#include "xsynthconfig.h"

namespace Ui {
class XSynthSettings;
}

class XSynthSettings : public QWidget, public SynthSettings
{
    Q_OBJECT

public:
    explicit XSynthSettings(QWidget *parent = nullptr, XSynthConfig *config = nullptr);
    ~XSynthSettings();
    void loadSettings() override;
    void storeSettings() override;
    QWidget *getWidget() override;

private:
    Ui::XSynthSettings *ui;
    XSynthConfig *m_cfg = nullptr;
};

#endif // XSYNTHSETTINGS_H
