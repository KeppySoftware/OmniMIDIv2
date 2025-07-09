#include "sflistitem.h"

SFListItem::SFListItem(SoundFont *sf, QTreeWidget *parent)
    : QTreeWidgetItem(parent)
    , m_sf(sf)
{
    if (m_sf == nullptr) return;
    setCheckState(0, Qt::Checked);
    setText(1, QString::fromStdString(m_sf->path));
    setText(2, "-");
    setText(3, "-");
    setText(4, "-");
}

SoundFont *SFListItem::getSf() {
    return m_sf;
}
