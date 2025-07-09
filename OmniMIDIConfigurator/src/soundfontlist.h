#ifndef SOUNDFONTLIST_H
#define SOUNDFONTLIST_H

#include <QTreeWidget>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include "sflistconfig.h"
#include "sfsettings.h"
#include <filesystem>

Q_DECLARE_METATYPE(SoundFont*)

namespace fs = std::filesystem;
using json = nlohmann::json;

class SoundFontList : public QTreeWidget {
    Q_OBJECT
public:
    SoundFontList(QWidget *parent = nullptr, SFListConfig *list = nullptr);
    void setUpHeader();
    void addSoundFont(QString path);
    void addFromObject(SoundFont *sf);
    void removeInvalid();
    void removeSelected();
    void removeAll();
    void moveDown();
    void moveUp();
    void replaceList(SFListConfig *list);
    void getData();
    void saveData();
    void openCfgs();
    QTreeWidgetItem *makeItem(SoundFont *sf);
protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dropEvent(QDropEvent *event);
    void paintEvent(QPaintEvent *e);
signals:
    void itemDroped();
    void modified();
private:
    fs::path m_listPath;
    SFListConfig *m_list;
    std::vector<SFSettings *> m_cfgWins;
};

#endif // SOUNDFONTLIST_H
