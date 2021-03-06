#include "albumsview.h"
#include "albumdelegate.h"
#include "application.h"
#include "controller/popupmenumanager.h"
#include "controller/exporter.h"
#include "controller/importer.h"
#include "utils/baseutils.h"
#include "utils/imageutils.h"
#include "frame/deletedialog.h"

#include <QDebug>
#include <QBuffer>
#include <QJsonDocument>
#include <QMouseEvent>

namespace {

const QString MY_FAVORITES_ALBUM = "My favorites";
const QString RECENT_IMPORTED_ALBUM = "Recent imported";
const int ITEM_SPACING = 61;
const QSize ITEM_DEFAULT_SIZE = QSize(152, 168);

}  // namespace

AlbumsView::AlbumsView(QWidget *parent)
    : QListView(parent),
      m_itemSize(ITEM_DEFAULT_SIZE),
      m_popupMenu(new PopupMenuManager(this))
{
    setMouseTracking(true);

    m_delegate = new AlbumDelegate(this);
    connect(m_delegate, &AlbumDelegate::editingFinished,
            this, [=](const QModelIndex &index) {
        closePersistentEditor(index);
        emit albumCreated();
    });

    setItemDelegate(m_delegate);
    m_model = new QStandardItemModel(this);
    setModel(m_model);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setResizeMode(QListView::Adjust);
    setViewMode(QListView::IconMode);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setUniformItemSizes(true);
    setSpacing(ITEM_SPACING);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragEnabled(false);

    installEventFilter(this);

    connect(this, &AlbumsView::doubleClicked,
            this, &AlbumsView::onDoubleClicked);
    connect(this, &AlbumsView::clicked, this, &AlbumsView::onClicked);
    connect(this, &AlbumsView::customContextMenuRequested,
            this, [=] (const QPoint &pos) {
        QModelIndex index = indexAt(pos);
        if (! isCreateIcon(index)) {
            m_popupMenu->setMenuContent(createMenuContent(indexAt(pos)));
            m_popupMenu->showMenu();
        }
    });
    connect(m_popupMenu, &PopupMenuManager::menuItemClicked,
            this, &AlbumsView::onMenuItemClicked);
    connect(Importer::instance(), &Importer::importProgressChanged,
            this, [=] (double progress) {
        if (progress == 1)
            updateView();
    });
}

QModelIndex AlbumsView::addAlbum(const DatabaseManager::AlbumInfo &info)
{
    // AlbumName ImageCount BeginTime EndTime Thumbnail
    QStringList imgNames = dApp->databaseM->getImageNamesByAlbum(info.name);
    if (imgNames.isEmpty()) {
        return QModelIndex();
    }

    QByteArray thumbnailByteArray;
    QBuffer inBuffer( &thumbnailByteArray );
    inBuffer.open( QIODevice::WriteOnly );
    // write inPixmap into inByteArray
    QString imageName = imgNames.first();
    if (imageName.isEmpty()) {
        for (QString name : imgNames) {
            if (! name.isEmpty()) {
                imageName = name;
                break;
            }
        }
    }
    auto imgInfo = dApp->databaseM->getImageInfoByName(imageName);
    if (! imageName.isEmpty() &&  ! imgInfo.thumbnail.save( &inBuffer, "JPG" )){
        QPixmap p = utils::image::getThumbnail(imgInfo.path);
        if (! p.save(&inBuffer, "JPG")) {
            qWarning() << "Can't get thumbnail for album: " << info.name;
        }
    }

    QVariantList datas;
    datas.append(QVariant(info.name));
    datas.append(QVariant(info.count));
    datas.append(QVariant(info.beginTime));
    datas.append(QVariant(info.endTime));
    datas.append(QVariant(thumbnailByteArray));

    removeCreateIcon();

    QModelIndex index;
    const int ti = indexOf(info.name);
    // Already exist, update the data
    if (ti != -1) {
        index = m_model->index(ti, 0);
    }
    // Not exist, create new item
    else {
        QStandardItem *item = new QStandardItem();
        QList<QStandardItem *> items;
        items.append(item);
        m_model->appendRow(items);

        index = m_model->index(m_model->rowCount() - 1, 0);
    }

    m_model->setData(index, QVariant(datas), Qt::DisplayRole);
    m_model->setData(index, QVariant(m_itemSize), Qt::SizeHintRole);
    appendCreateIcon();
    return index;
}

QSize AlbumsView::itemSize() const
{
    return m_itemSize;
}

void AlbumsView::setItemSizeMultiple(int multiple)
{
    const int w = ITEM_DEFAULT_SIZE.width() + multiple * 32;
    const int h = (1.0 *ITEM_DEFAULT_SIZE.height() / ITEM_DEFAULT_SIZE.width()) * w;
    m_itemSize = QSize(w, h);
    updateView();

    // SKILL
    // This signal must be emitted when the sizeHint() of index changed.
    // View has been set Uniform Item Sizes as true, so all of the items's size
    // is change with the last item on the view
    emit m_delegate->sizeHintChanged(m_model->index(m_model->rowCount() - 1, 0));
}

bool AlbumsView::eventFilter(QObject *obj, QEvent *e)
{
    Q_UNUSED(obj)
    if (e->type() == QEvent::Hide) {
//        m_model->clear();
    }
    else if (e->type() == QEvent::Show) {
        // Aways has Favorites and RecentImport album
        dApp->databaseM->insertImageIntoAlbum(MY_FAVORITES_ALBUM, "", "");
        updateView();
    }

    return false;
}

void AlbumsView::mousePressEvent(QMouseEvent *e)
{
    if (! indexAt(e->pos()).isValid()) {
        this->selectionModel()->clearSelection();
    }
    else {
        m_popupMenu->setMenuContent(createMenuContent(indexAt(e->pos())));
    }

    QListView::mousePressEvent(e);
}

int AlbumsView::horizontalOffset() const
{
    double spacing = 1.0 * (width() % (m_itemSize.width() + ITEM_SPACING) - ITEM_SPACING) / 2;
    // 0 is critical point, DO NOT use 0
    spacing = spacing == 0 ? -1 : spacing;
    // Not enought for item spacing
    if (spacing < 0) {
        spacing = 1.0 * (m_itemSize.width() + ITEM_SPACING) / 2 + spacing;
    }
    return -spacing;
}

bool AlbumsView::isCreateIcon(const QModelIndex &index) const
{
    return m_model->data(index, Qt::DisplayRole).toList().isEmpty();
}

const QStringList AlbumsView::paths(const QString &album) const
{
    const auto infos = dApp->databaseM->getImageInfosByAlbum(album);
    if (! infos.isEmpty()) {
        QStringList list;
        for (auto info : infos) {
            list << info.path;
        }
        return list;
    }
    return QStringList();
}

const QString AlbumsView::getAlbumName(const QModelIndex &index) const
{
    QString albumName = "";
    //TODO: if index isn't valid app will crashed,
    //index need be valid to access.
    if (!index.isValid())
        return "";

    QList<QVariant> datas =
            index.model()->data(index, Qt::DisplayRole).toList();
    if (! datas.isEmpty()) {
        albumName = datas[0].toString();
    }

    return albumName;
}

const QString AlbumsView::getNewAlbumName() const
{
    const QString nan = tr("Unnamed");
    const QStringList albums = dApp->databaseM->getAlbumNameList();
    QList<int> countList;
    for (QString album : albums) {
        if (album.startsWith(nan)) {
            countList << QString(album.split(nan).last()).toInt();
        }
    }

    if (countList.isEmpty()) {
        return nan;
    }
    else if (countList.length() == 1) {
        return nan + QString::number(2);
    }
    else {
        qSort(countList.begin(), countList.end());
        if (countList.first() != 0)
            return nan;
        for (int c : countList) {
            if (c == 0) {
                // Index start from 2.
                c = 1;
            }
            if (countList.indexOf(c + 1) == -1) {
                return nan + QString::number(c + 1);
            }
        }

        return nan;
    }
}

const QString AlbumsView::createMenuContent(const QModelIndex &index)
{
    QJsonArray items;
    if (index.isValid()) {
        bool isSpecial = false;
        QList<QVariant> datas =
                index.model()->data(index, Qt::DisplayRole).toList();
        if (! datas.isEmpty()) {
            const QString albumName = datas[0].toString();
            if (albumName == MY_FAVORITES_ALBUM
                    || albumName == RECENT_IMPORTED_ALBUM) {
                isSpecial = true;
            }
        }

        items.append(createMenuItem(IdView, tr("View")));
        items.append(createMenuItem(IdStartSlideShow, tr("Start slide show"),
                                    false, "F5"));
        items.append(createMenuItem(IdSeparator, "", true));
        if (! isSpecial)
            items.append(createMenuItem(IdRename, tr("Rename"), false, "F2"));
        // Hide the export function
        // items.append(createMenuItem(IdExport, tr("Export")));
        items.append(createMenuItem(IdCopy, tr("Copy"), false, "Ctrl+C"));
        if (! isSpecial)
            items.append(createMenuItem(IdDelete, tr("Delete"), false,
                                        "Delete"));
        items.append(createMenuItem(IdSeparator, "", true));
        //        items.append(createMenuItem(IdAlbumInfo, tr("Album info"), false,
        //                                    "Ctrl+Alt+Return"));
    }
    else {
        items.append(createMenuItem(IdCreate, tr("New album")));
    }

    QJsonObject contentObj;
    contentObj["x"] = 0;
    contentObj["y"] = 0;
    contentObj["items"] = QJsonValue(items);

    QJsonDocument document(contentObj);

    return QString(document.toJson());
}

QJsonValue AlbumsView::createMenuItem(const MenuItemId id,
                                      const QString &text,
                                      const bool isSeparator,
                                      const QString &shortcut,
                                      const QJsonObject &subMenu)
{
    return QJsonValue(m_popupMenu->createItemObj(id,
                                                 text,
                                                 isSeparator,
                                                 shortcut,
                                                 subMenu));
}

void AlbumsView::onClicked(const QModelIndex &index)
{
    if (isCreateIcon(index)) {
        createAlbum();
    }
}

void AlbumsView::onMenuItemClicked(int menuId)
{
    const QString albumName = getAlbumName(currentIndex());
    switch (MenuItemId(menuId)) {
    case IdCreate:
        createAlbum();
        break;
    case IdView:
        emit openAlbum(albumName);
        break;
    case IdStartSlideShow:
        emit startSlideShow(paths(albumName));
        break;
    case IdRename:
        openPersistentEditor(this->currentIndex());
        break;
    case IdExport:
        dApp->exporter->exportAlbum(albumName);
        break;
    case IdCopy:
    {
        const auto infos = dApp->databaseM->getImageInfosByAlbum(albumName);
        QStringList paths;
        for (int i = 0; i < infos.length(); i ++) {
            paths << infos[i].path;
        }
        utils::base::copyImageToClipboard(paths);
        break;
    }
    case IdDelete: {
        if (!albumName.isEmpty())
            popupDelDialog(albumName);
        break;
    }
        //    case IdAlbumInfo:
        //        break;
    default:
        break;
    }
}

void AlbumsView::onDoubleClicked(const QModelIndex &index)
{
    if (! isCreateIcon(index)) {
        emit openAlbum(getAlbumName(index));
    }
}

void AlbumsView::removeCreateIcon()
{
    // The create icon must in the end of rows
    const QModelIndex li = m_model->index(m_model->rowCount() - 1, 0);
    if (m_model->rowCount() > 0 &&
            isCreateIcon(li)) {
        m_model->removeRow(m_model->rowCount() - 1);
    }
}

void AlbumsView::appendCreateIcon()
{
    const QModelIndex li = m_model->index(m_model->rowCount() - 1, 0);
    if (! isCreateIcon(li)) {
        QStandardItem *item = new QStandardItem();
        QList<QStandardItem *> items;
        items.append(item);
        m_model->appendRow(items);

        QModelIndex index = m_model->index(m_model->rowCount() - 1, 0);
        m_model->setData(index, QVariant(m_itemSize), Qt::SizeHintRole);
    }
}

void AlbumsView::createAlbum()
{
    const QString name = getNewAlbumName();
    dApp->databaseM->insertImageIntoAlbum(name, "", "");
    QModelIndex index = addAlbum(dApp->databaseM->getAlbumInfo(name));
    openPersistentEditor(index);
    scrollTo(index);
    this->selectionModel()->clearSelection();
}

void AlbumsView::updateView()
{
    if (! isVisible())
        return;

    // Make those special album always show at front
//    addAlbum(dApp->databaseM->getAlbumInfo(RECENT_IMPORTED_ALBUM));
    addAlbum(dApp->databaseM->getAlbumInfo(MY_FAVORITES_ALBUM));
    QStringList albums = dApp->databaseM->getAlbumNameList();
    albums.removeAll(MY_FAVORITES_ALBUM);
    albums.removeAll(RECENT_IMPORTED_ALBUM);
    for (const QString name : albums) {
        addAlbum(dApp->databaseM->getAlbumInfo(name));
    }
}

int AlbumsView::indexOf(const QString &name) const
{
    for (int i = 0; i < m_model->rowCount(); i ++) {
        const QVariantList datas =
            m_model->data(m_model->index(i, 0), Qt::DisplayRole).toList();
        if (! datas.isEmpty() && datas[0].toString() == name) {
            return i;
        }
    }
    return -1;
}

void AlbumsView::popupDelDialog(const QString &albumName) {
    const auto infos = dApp->databaseM->getImageInfosByAlbum(albumName);
    QStringList paths;
    for (int i = 0; i < infos.length(); i++) {
        paths << infos[i].path;
    }

    DeleteDialog* delDialog = new DeleteDialog(paths, true, this);
    delDialog->show();
    delDialog->moveToCenter();

    connect(delDialog, &DeleteDialog::buttonClicked, [=](int index){
        if (index == 1) {
            if (albumName != MY_FAVORITES_ALBUM
                    && albumName != RECENT_IMPORTED_ALBUM
                    && ! isCreateIcon(currentIndex())) {
                dApp->databaseM->removeAlbum(albumName);
                m_model->removeRow(currentIndex().row());
                emit albumRemoved();
            }
        }
    });
    connect(delDialog, &DeleteDialog::closed,
            delDialog, &DeleteDialog::deleteLater);

}
