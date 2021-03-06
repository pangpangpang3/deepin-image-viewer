#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QPixmap>
#include <QDateTime>
#include <QSqlDatabase>
#include <QMutex>

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    struct ImageInfo {
        QString name;
        QString path;
        QStringList albums; // Discard
        QStringList labels;  // Deprecated
        QDateTime time;
        QPixmap thumbnail; // Deprecated
    };
    struct AlbumInfo {
        QString name;
        int count;
        QDateTime beginTime;
        QDateTime endTime;
    };

    static DatabaseManager *instance();
    ~DatabaseManager();

    const QStringList getAllImagesName();
    const QStringList getAllImagesPath();
    const QList<ImageInfo> getAllImageInfos();
    QList<ImageInfo> getImageInfosByAlbum(const QString &album);
    QList<ImageInfo> getImageInfosByTimeline(const QString &timeline);
    ImageInfo getImageInfoByName(const QString &name);
    ImageInfo getImageInfoByPath(const QString &path);
    void insertImageInfos(const QList<ImageInfo> &infos);
    void updateImageInfo(const ImageInfo &info);
    void removeImages(const QStringList &names);
    bool imageExist(const QString &name);
    int getImagesCountByMonth(const QString &month);
    int imageCount();

    AlbumInfo getAlbumInfo(const QString &name);
    QStringList getAlbumNameList();
    QStringList getImageNamesByAlbum(const QString &album);
    void insertImageIntoAlbum(const QString &albumname,
                              const QString &filename,
                              const QString &time);
    void removeImageFromAlbum(const QString &albumname, const QString &filename);
    void removeImagesFromAlbum(const QString &album, const QStringList &names);
    void removeAlbum(const QString &name);
    void renameAlbum(const QString &oldName, const QString &newName);
    void clearRecentImported();
    bool imageExistAlbum(const QString &name, const QString &album);
    int getImagesCountByAlbum(const QString &album);
    int albumsCount();

    QStringList getTimeLineList(bool ascending = true);

private:
    explicit DatabaseManager(QObject *parent = 0);

    QList<ImageInfo> getImageInfos(const QString &key, const QString &value);
    QSqlDatabase getDatabase();
    void checkDatabase();

private:
    static DatabaseManager *m_databaseManager;
    QString m_connectionName;
    QMutex m_mutex;
};

#endif // DATABASEMANAGER_H
