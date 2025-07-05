#ifndef SFSETTINGS_H
#define SFSETTINGS_H

#include <QWidget>
#include "sflistconfig.h"
#include <QTreeWidgetItem>

namespace Ui {
class SFSettings;
}

class SFSettings : public QWidget
{
    Q_OBJECT

public:
    explicit SFSettings(SoundFont *sf, QWidget *parent = nullptr);
    ~SFSettings();

    void loadSettings();
    void saveSettings();

protected:
    inline void closeEvent(QCloseEvent *event) override {
        emit closed(this);
    }

signals:
    void modified();
    void closed(SFSettings* self);

private:
    Ui::SFSettings *ui;
    SoundFont *m_sf;
};

#endif // SFSETTINGS_H
