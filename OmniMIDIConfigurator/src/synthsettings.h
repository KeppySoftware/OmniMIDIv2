#ifndef SYNTHSETTINGS_H
#define SYNTHSETTINGS_H

#include <QWidget>

class SynthSettings
{
public:
    virtual ~SynthSettings() {}
    virtual void loadSettings() = 0;
    virtual void storeSettings() = 0;
    virtual QWidget *getWidget() = 0;
};

#endif // SYNTHSETTINGS_H
