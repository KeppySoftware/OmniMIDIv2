#ifndef SFLISTITEM_H
#define SFLISTITEM_H

#include <QTreeWidgetItem>
#include "sflistconfig.h"

class SFListItem : public QTreeWidgetItem
{
    Q_OBJECT

public:
    SFListItem(SoundFont *sf, QTreeWidget *parent = nullptr);

    SoundFont *getSf();

private:
    SoundFont *m_sf;
};

#endif // SFLISTITEM_H
