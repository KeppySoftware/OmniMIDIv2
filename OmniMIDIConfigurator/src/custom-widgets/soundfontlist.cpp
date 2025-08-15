#include "soundfontlist.h"
#include <QMessageBox>
#include <filesystem>
#include "../utils.h"
#include <QHeaderView>

SoundFontList::SoundFontList(QWidget *parent, SFListConfig *list)
    : QTreeWidget(parent) {
    replaceList(list);

    this->setAcceptDrops(true);
    this->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(this, &SoundFontList::modified, this, &SoundFontList::saveData);
    connect(this, &QTreeWidget::itemChanged, this, &SoundFontList::modified);
    connect(this, &SoundFontList::modified, this, &SoundFontList::removeInvalid);
}

void SoundFontList::setUpHeader() {
    this->setHeaderLabels(QStringList() << "" << "Filepath" << "Port" << "Type" << "Size");
    this->header()->resizeSections(QHeaderView::ResizeToContents);
    this->header()->resizeSection(1, 450);
}

void SoundFontList::dropEvent(QDropEvent *event) {
    for (const QUrl &url : event->mimeData()->urls()) {
        QString fileName = url.toLocalFile();
        this->addSoundFont(fileName);
    }
}

void SoundFontList::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())  event->acceptProposedAction();
}

void SoundFontList::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void SoundFontList::dragLeaveEvent(QDragLeaveEvent *event) {
    event->accept();
}

void SoundFontList::paintEvent(QPaintEvent *e) {
    QTreeView::paintEvent(e);
    if (model() && model()->rowCount(rootIndex()) > 0)
        return;

    QPainter p(this->viewport());
    p.setOpacity(0.5);
    p.drawText(rect(), Qt::AlignCenter, "Click + to select or\nDrag & Drop");
}

void SoundFontList::removeInvalid() {
    std::vector<QTreeWidgetItem *> toRem;

    int count = this->topLevelItemCount();
    for (int i = 0; i < count; i++) {
        auto item = this->topLevelItem(i);
        auto path = item->text(1).toStdString();
        // ToDo: Maybe also check if the file is valid here
        if (!std::filesystem::exists(path)) {
            toRem.push_back(item);
        }
    }

    bool modified = !toRem.empty();
    for (auto item : toRem) {
        delete this->takeTopLevelItem(this->indexOfTopLevelItem(item));
    }
    if (modified) {
        this->saveData();
    }
}

void SoundFontList::removeSelected() {
    auto items = this->selectedItems();
    for (auto item : items)
        delete this->takeTopLevelItem(this->indexOfTopLevelItem(item));
    emit modified();
}

void SoundFontList::removeAll() {
    int count = this->topLevelItemCount();
    for (int i = 0; i < count; i++) {
        auto item = this->topLevelItem(0);
        this->removeItemWidget(item, 0);
        delete item;
    }
    emit modified();
}

void SoundFontList::addSoundFont(QString path) {
    if (std::filesystem::exists(path.toStdString())) {
        SoundFont *sf = new SoundFont;
        sf->path = path.toStdString();
        this->addTopLevelItem(makeItem(sf));
        emit modified();
    } else {
        QMessageBox::warning(this, "OmniMIDI | Error", path + ": File not found");
    }
}

void SoundFontList::moveDown() {
    auto selected = this->selectedItems();
    if (selected.empty()) {
        return;
    } else {
        QTreeWidgetItem *current = selected[0];
        if (current == nullptr)
            return;
        int currIndex = this->indexOfTopLevelItem(current);

        QTreeWidgetItem *next = this->topLevelItem(this->indexOfTopLevelItem(current) + 1);
        int nextIndex = this->indexOfTopLevelItem(next);

        QTreeWidgetItem *temp = this->takeTopLevelItem(nextIndex);
        this->insertTopLevelItem(currIndex, temp);
        this->insertTopLevelItem(nextIndex, current);
    }
    emit modified();
}

void SoundFontList::moveUp() {
    auto selected = this->selectedItems();
    if (selected.empty()) {
        return;
    } else {
        QTreeWidgetItem *current = selected[0];
        if (current == nullptr)
            return;
        int currIndex = this->indexOfTopLevelItem(current);

        QTreeWidgetItem *prev = this->topLevelItem(this->indexOfTopLevelItem(current) - 1);
        int prevIndex = this->indexOfTopLevelItem(prev);

        QTreeWidgetItem *temp = this->takeTopLevelItem(prevIndex);
        this->insertTopLevelItem(prevIndex, current);
        this->insertTopLevelItem(currIndex, temp);
    }
    emit modified();
}

void SoundFontList::replaceList(SFListConfig *list) {
    m_list = list;
    if (m_list == nullptr) return;
    for (std::vector<SFSettings*>::iterator it = m_cfgWins.begin(); it != m_cfgWins.end();) {
        delete *it;
        ++it;
    }
    m_cfgWins.clear();
    getData();
}

void SoundFontList::getData() {
    this->clear();
    std::vector<SoundFont*> list;

    try {
        list = m_list->loadFromDisk();
    } catch (const std::exception &e) {
        QString s = "An error occured while loading the soundfont list:\n";
        QMessageBox::warning(this, WARNING_TITLE, s + e.what());
        return;
    }

    for (std::vector<SoundFont *>::reverse_iterator sf = list.rbegin();
         sf != list.rend(); ++sf)
    {
        SoundFont *s = *sf;
        if (!s->path.empty())
            this->addTopLevelItem(makeItem(s));
    }
}

void SoundFontList::saveData() {
    std::vector<SoundFont*> list;

    int count = this->topLevelItemCount();
    for (int i = count - 1; i >= 0; i--) {
        auto item = this->topLevelItem(i);
        SoundFont *sf = (item->data(0, Qt::UserRole).value<SoundFont*>());
        sf->enabled = item->checkState(0) == Qt::Checked;
        list.push_back(sf);
    }

    try {
        m_list->storeToDisk(list);
    } catch (const std::exception &e) {
        QString s = "An error occured while saving the soundfont list:\n";
        QMessageBox::warning(this, WARNING_TITLE, s + e.what());
        return;
    }
}

void SoundFontList::openCfgs() {
    auto items = this->selectedItems();
    for (auto item : items) {
        SoundFont *sf = (item->data(0, Qt::UserRole).value<SoundFont*>());
        SFSettings *n = new SFSettings(sf);
        QString title = "Config for ";
        title += QString::fromStdString(sf->path.substr(sf->path.find_last_of("/\\") + 1));
        n->setWindowTitle(title);
        n->show();
        m_cfgWins.push_back(n);
        connect(n, &SFSettings::modified, this, &SoundFontList::saveData);
        connect(n, &SFSettings::closed, this, [this](SFSettings* self) {
            for (std::vector<SFSettings*>::iterator it = m_cfgWins.begin(); it != m_cfgWins.end();) {
                if (*it == self) {
                    it = m_cfgWins.erase(it);
                    delete self;
                    break;
                }
                ++it;
            }
        });
    }
}

QTreeWidgetItem *SoundFontList::makeItem(SoundFont *sf) {
    auto n = new QTreeWidgetItem();
    QVariant data;
    data.setValue(sf);
    n->setData(0, Qt::UserRole, data);
    n->setCheckState(0, CCS(sf->enabled));
    n->setText(1, QString::fromStdString(sf->path));
    n->setText(2, "-");
    n->setText(3, "-");
    n->setText(4, "-");
    return n;
}
