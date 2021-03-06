#include "imagesview.h"
#include "application.h"
#include "dscrollbar.h"
#include "controller/exporter.h"
#include "controller/importer.h"
#include "controller/popupmenumanager.h"
#include "controller/signalmanager.h"
#include "controller/wallpapersetter.h"
#include "utils/baseutils.h"
#include "utils/imageutils.h"
#include "widgets/importframe.h"
#include "widgets/thumbnaillistview.h"
#include "widgets/scrollbar.h"
#include "frame/deletedialog.h"

#include <QDebug>
#include <QFileInfo>
#include <QStandardItem>
#include <QStackedWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <math.h>

namespace {

const int TOP_TOOLBAR_HEIGHT = 40;
const QString SHORTCUT_SPLIT_FLAG = "@-_-@";
const QString MY_FAVORITES_ALBUM = "My favorites";
const QString RECENT_IMPORTED_ALBUM = "Recent imported";

DatabaseManager::ImageInfo genThumbnail(DatabaseManager::ImageInfo &info)
{
    using namespace utils::image;
    auto ni = info;
    ni.thumbnail = cutSquareImage(getThumbnail(ni.path, true));
    return ni;
}

}  // namespace

ImagesView::ImagesView(QWidget *parent)
    : QScrollArea(parent),
      m_popupMenu(new PopupMenuManager(this))
{
    setFrameStyle(QFrame::NoFrame);
    setWidgetResizable(true);
    setVerticalScrollBar(new ScrollBar(this));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    verticalScrollBar()->setContextMenuPolicy(Qt::PreventContextMenu);

    initContent();
    updateMenuContents();
    connect(m_popupMenu, &PopupMenuManager::menuItemClicked,
            this, &ImagesView::onMenuItemClicked);
}

void ImagesView::setAlbum(const QString &album)
{
    m_album = album;

    m_view->clearData();
    auto infos = dApp->databaseM->getImageInfosByAlbum(album);
    const int preloadCount = 100;
    // Load up to 100 images at initialization to accelerate rendering
    for (int i = 0; i < qMin(infos.length(), preloadCount); i ++) {
        insertItem(infos[i], false);
    }

    // The thumbnail is generated in the new thread
    QList<QFuture<DatabaseManager::ImageInfo>> fl;
    if (infos.length() >= preloadCount) {
        for (int i = preloadCount; i < infos.length(); i ++) {
            fl << QtConcurrent::run(QThreadPool::globalInstance(), genThumbnail, infos[i]);
        }
    }
    if (fl.length() > 0) {
        QTimer *t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [=] {
            if (fl.last().isFinished()) {
                for (auto f : fl) {
                    insertItem(f.result(), false);
                }
                t->deleteLater();
            }
        });
        t->start(500);
    }

    m_topTips->setAlbum(album);

    // Empty album, import first
    updateContent();

}

bool ImagesView::removeItem(const QString &name)
{
    const bool state = m_view->removeItem(name);

    m_topTips->setAlbum(m_album);
    updateContent();

    return state;
}

void ImagesView::removeItems(const QStringList &names)
{
    m_view->removeItems(names);

    m_topTips->setAlbum(m_album);
    updateContent();
}

int ImagesView::count() const
{
    return m_view->count();
}

void ImagesView::initListView()
{
    m_view = new ThumbnailListView();
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_contentLayout->addWidget(m_view);
    m_contentLayout->addStretch(1);

    connect(this, &ImagesView::rotated,
            m_view, &ThumbnailListView::updateThumbnails);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            m_view, &ThumbnailListView::updateThumbnails);
    connect(m_view, &ThumbnailListView::doubleClicked,
            this, [=] (const QModelIndex & index) {
        const QString path = m_view->itemInfo(index).path;
        emit viewImage(path, QStringList());
    });
    connect(m_view, &ThumbnailListView::customContextMenuRequested,
            this, [=] (const QPoint &pos) {
        if (m_view->indexAt(pos).isValid()) {
            updateMenuContents();
            m_popupMenu->showMenu();
        }
    });
    connect(m_view, &ThumbnailListView::clicked,
            this, &ImagesView::updateMenuContents);
}

void ImagesView::initTopTips()
{
    m_topTips = new TopAlbumTips(this);
}

const QStringList ImagesView::paths()
{
    QStringList list;
    auto infos = dApp->databaseM->getImageInfosByAlbum(m_album);
    for (int i = 0; i < infos.length(); i ++) {
        list << infos[i].path;
    }
    return list;
}

QString ImagesView::createMenuContent()
{
    const QStringList nList = selectedImagesNameList();
    const QStringList pList = selectedImagesPathList();
    const int selectedCount = nList.length();
    bool canSave = true;
    for (QString p : pList) {
        if (! utils::image::imageSupportSave(p)) {
            canSave = false;
            break;
        }
    }

    QJsonArray items;
    if (selectedCount == 1) {
        items.append(createMenuItem(IdView, tr("View")));
        items.append(createMenuItem(IdFullScreen, tr("Fullscreen"),
                                    false, "F11"));
    }
    items.append(createMenuItem(IdStartSlideShow, tr("Start slide show"), false,
                                "F5"));
    const QJsonObject objF = createAlbumMenuObj();
    if (! objF.isEmpty()) {
        items.append(createMenuItem(IdAddToAlbum, tr("Add to album"),
                                    false, "", objF));
    }
    items.append(createMenuItem(IdSeparator, "", true));
    //Hide the export function
    //items.append(createMenuItem(IdExport, tr("Export"), false, ""));
    items.append(createMenuItem(IdCopy, tr("Copy"), false, "Ctrl+C"));
    items.append(createMenuItem(IdMoveToTrash, tr("Throw to Trash"), false,
                                "Delete"));
    items.append(createMenuItem(IdRemoveFromAlbum, tr("Remove from album"),
                                false, "Shift+Delete"));

    items.append(createMenuItem(IdSeparator, "", true));

    if (selectedCount == 1) {
        if (! dApp->databaseM->imageExistAlbum(nList.first(),
                                               MY_FAVORITES_ALBUM))
            items.append(createMenuItem(IdAddToFavorites,
                tr("Add to My favorites"), false, "Ctrl+K"));
        else
            items.append(createMenuItem(IdRemoveFromFavorites,
                tr("Unfavorite"), false, "Ctrl+Shift+K"));
    } else {
        bool addToFavor = false;
        foreach (const QString &img, nList) {
            if (!dApp->databaseM->imageExistAlbum(img, MY_FAVORITES_ALBUM)) {
                addToFavor = true;
                break;
            }
        }
        if (addToFavor)
            items.append(createMenuItem(IdAddToFavorites,
                        tr("Add to My favorites"), false, "Ctrl+K"));
    }

    if (canSave) {
    items.append(createMenuItem(IdSeparator, "", true));

    items.append(createMenuItem(IdRotateClockwise, tr("Rotate clockwise"),
                                false, "Ctrl+R"));
    items.append(createMenuItem(IdRotateCounterclockwise,
        tr("Rotate counterclockwise"), false, "Ctrl+Shift+R"));
    }
    items.append(createMenuItem(IdSeparator, "", true));

    if (selectedCount == 1) {
        if (canSave) {
        items.append(createMenuItem(IdSetAsWallpaper, tr("Set as wallpaper"),
                                    false, "Ctrl+F8"));
        }
        items.append(createMenuItem(IdDisplayInFileManager,
            tr("Display in file manager"), false, "Ctrl+D"));
        items.append(createMenuItem(IdImageInfo, tr("Image info"), false,
                                    "Alt+Return"));
    }

    QJsonObject contentObj;
    contentObj["x"] = 0;
    contentObj["y"] = 0;
    contentObj["items"] = QJsonValue(items);

    return QString(QJsonDocument(contentObj).toJson());
}

QJsonValue ImagesView::createMenuItem(const MenuItemId id,
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

void ImagesView::insertItem(const DatabaseManager::ImageInfo &info, bool update)
{
    using namespace utils::image;
    ThumbnailListView::ItemInfo vi;
    vi.name = info.name;
    vi.path = info.path;
    if (info.thumbnail.isNull())
        vi.thumb = cutSquareImage(getThumbnail(info.path, true));
    else
        vi.thumb = info.thumbnail;

    m_view->insertItem(vi);

    if (update) {
        m_view->updateThumbnails();
        m_topTips->setAlbum(m_album);
        updateContent();
    }
}

void ImagesView::updateMenuContents()
{
    m_popupMenu->setMenuContent(createMenuContent());
}

void ImagesView::onMenuItemClicked(int menuId, const QString &text)
{
    const QStringList nList = selectedImagesNameList();
    const QStringList pList = selectedImagesPathList();
    if (nList.isEmpty()) {
        return;
    }

    const QStringList viewPaths = (pList.length() == 1) ? paths() : pList;
    const QString cpath = pList.first();
    const QString cname = nList.first();

    switch (MenuItemId(menuId)) {
    case IdView:
        emit viewImage(cpath, viewPaths);
        break;
    case IdFullScreen:
        emit viewImage(cpath, viewPaths, true);
        break;
    case IdStartSlideShow:
        emit startSlideShow(viewPaths, cpath);
        break;
    case IdAddToAlbum:
    {
        const QString album = text.split(SHORTCUT_SPLIT_FLAG).first();
        for (QString name : nList) {
            const auto info = dApp->databaseM->getImageInfoByName(name);
            dApp->databaseM->insertImageIntoAlbum(
                        album, name, utils::base::timeToString(info.time));
        }
        break;
    }
    case IdExport:
        dApp->exporter->exportImage(selectedImagesPathList());
        break;
    case IdCopy:
        utils::base::copyImageToClipboard(pList);
        break;
    case IdMoveToTrash: {
        popupDelDialog(pList, nList);
        break;
    }
    case IdAddToFavorites: {
        foreach (QString cname, nList) {
            const auto info = dApp->databaseM->getImageInfoByName(cname);
            dApp->databaseM->insertImageIntoAlbum(
                MY_FAVORITES_ALBUM, cname, utils::base::timeToString(info.time));
        }
        updateMenuContents();
        break;
    }
    case IdRemoveFromFavorites:
        dApp->databaseM->removeImageFromAlbum(MY_FAVORITES_ALBUM, cname);
        updateMenuContents();
        break;
    case IdRemoveFromAlbum:
        dApp->databaseM->removeImagesFromAlbum(m_album, nList);
        break;
    case IdRotateClockwise:
        if (m_rotateList.isEmpty()) {
            m_rotateList = pList;
            for (QString path : pList) {
                QtConcurrent::run(this, &ImagesView::rotateImage, path, 90);
            }
        }
        break;
    case IdRotateCounterclockwise:
        if (m_rotateList.isEmpty()) {
            m_rotateList = pList;
            for (QString path : pList) {
                QtConcurrent::run(this, &ImagesView::rotateImage, path, -90);
            }
        }
        break;
    case IdSetAsWallpaper:
        dApp->wpSetter->setWallpaper(cpath);
        break;
    case IdDisplayInFileManager:
        utils::base::showInFileManager(cpath);
        break;
    case IdImageInfo:
        emit dApp->signalM->showImageInfo(cpath);
        break;
    default:
        break;
    }
}

void ImagesView::rotateImage(const QString &path, int degree)
{
    utils::image::rotate(path, degree);
    m_rotateList.removeAll(path);
    if (m_rotateList.isEmpty()) {
        qDebug() << "Rotate finish!";
        emit rotated();
    }
}

bool ImagesView::allInAlbum(const QStringList &names, const QString &album)
{
    const QStringList nl = dApp->databaseM->getImageNamesByAlbum(album);
    for (QString name : names) {
        // One of name is not in album
        if (nl.indexOf(name) == -1) {
            return false;
        }
    }
    return true;
}

void ImagesView::updateTopTipsRect()
{
    if (this->widget() != m_contentWidget) {
        m_topTips->setVisible(false);
        return;
    }

    m_topTips->move(0, TOP_TOOLBAR_HEIGHT);
    m_topTips->resize(width(), m_topTips->height());
    const int lm =
            - m_view->hOffset() + m_contentLayout->contentsMargins().left();
    m_topTips->setLeftMargin(lm);
    m_topTips->setVisible(true);
}

QString ImagesView::getCurrentAlbum() const
{
    return m_album;
}

QSize ImagesView::iconSize() const
{
    return m_view->iconSize();
}

void ImagesView::setIconSize(const QSize &iconSize)
{
    m_view->setIconSize(iconSize);
    updateTopTipsRect();
}

QStringList ImagesView::selectedImagesNameList() const
{
    QStringList names;
    const QList<ThumbnailListView::ItemInfo> infos =
            m_view->selectedItemInfos();
    for (ThumbnailListView::ItemInfo info : infos) {
        names << info.name;
    }

    return names;
}

QStringList ImagesView::selectedImagesPathList() const
{
    QStringList paths;
    const QList<ThumbnailListView::ItemInfo> infos =
            m_view->selectedItemInfos();
    for (ThumbnailListView::ItemInfo info : infos) {
        paths << info.path;
    }

    return paths;
}

void ImagesView::resizeEvent(QResizeEvent *e)
{
    QScrollArea::resizeEvent(e);
    updateTopTipsRect();
}

void ImagesView::showEvent(QShowEvent *e)
{
    // For import from timeline update
    if (count() != dApp->databaseM->getImagesCountByAlbum(m_album)) {
        const auto infos = dApp->databaseM->getImageInfosByAlbum(m_album);
        for (auto info : infos) {
            insertItem(info, false);
        }
        m_topTips->setAlbum(m_album);
        updateContent();
    }

    QScrollArea::showEvent(e);
}

void ImagesView::initContent()
{
    m_contentWidget = new QWidget;
    m_contentWidget->setObjectName("ImagesViewContent");
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(10, 70, 6, 20);

    m_importFrame = new ImportFrame(this);
    m_importFrame->setButtonText(tr("Import"));
    m_importFrame->setTitle(tr("Import or drag image to timeline"));
    connect(m_importFrame, &ImportFrame::clicked, this, [=] {
        dApp->importer->showImportDialog(m_album);
    });

    initListView();
    initTopTips();

    setWidget(m_contentWidget);
    m_importFrame->setVisible(false);
}

void ImagesView::updateContent()
{
    if (m_view->count() == 0) {
        // For avoid widget destroy
        takeWidget();
        m_contentWidget->setVisible(false);
        setWidget(m_importFrame);
        m_importFrame->setVisible(true);
    }
    else {
        // For avoid widget destroy
        takeWidget();
        m_importFrame->setVisible(false);
        setWidget(m_contentWidget);
        m_contentWidget->setVisible(true);
        updateTopTipsRect();
    }

}

QJsonObject ImagesView::createAlbumMenuObj()
{
    const QStringList albums = dApp->databaseM->getAlbumNameList();
    const QStringList selectNames = selectedImagesNameList();

    QJsonArray items;
    if (! selectNames.isEmpty()) {
        for (QString album : albums) {
            if (album == MY_FAVORITES_ALBUM || album == RECENT_IMPORTED_ALBUM) {
                continue;
            }
            else if (! allInAlbum(selectNames, album)) {
                items.append(createMenuItem(IdAddToAlbum, album));
            }
        }
    }

    QJsonObject contentObj;
    if (! items.isEmpty()) {
        contentObj[""] = QJsonValue(items);
    }

    return contentObj;
}

void ImagesView::popupDelDialog(const QStringList paths, const QStringList names) {
    DeleteDialog* delDialog = new DeleteDialog(paths, false, this);
    delDialog->show();
    delDialog->moveToCenter();
    connect(delDialog, &DeleteDialog::buttonClicked, [=](int index){
        if (index == 1) {
            removeItems(names);
            dApp->databaseM->removeImages(names);
            utils::base::trashFiles(paths);
        }
    });
    connect(delDialog, &DeleteDialog::closed,
            delDialog, &DeleteDialog::deleteLater);
}
