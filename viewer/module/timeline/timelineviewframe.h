#ifndef TIMELINEVIEWFRAME_H
#define TIMELINEVIEWFRAME_H

#include "controller/databasemanager.h"
#include "widgets/thumbnaillistview.h"
#include <QLabel>
#include <QWidget>
#include <QFrame>
#include <QListView>
#include <QStandardItem>
#include <QVBoxLayout>

class TimelineViewFrame : public QFrame
{
    Q_OBJECT
public:
    explicit TimelineViewFrame(const QString &timeline, QWidget *parent);
    ~TimelineViewFrame();
    void insertItem(const DatabaseManager::ImageInfo &info);
    bool removeItem(const QString &name);
    void removeItems(const QStringList &names);
    void clearSelection() const;
    void updateThumbnails();
    void setMultiSelection(bool multiple);
    bool isMultiSelection() const;
    bool posInSelected(const QPoint &pos);

    QMap<QString, QString> selectedImages() const;
    QString timeline() const;
    bool isEmpty() const;
    int hOffset() const;

    void setIconSize(const QSize &iconSize);

signals:
    void clicked(const QModelIndex &index);
    void singleClicked(QMouseEvent *e);
    void showMenuRequested();
    void viewImage(const QString &path, const QStringList &paths);

protected:
    void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;

private:
    void initListView();

private:
    QString m_timeline;
    ThumbnailListView *m_view;
    QLabel *m_title;
    QLabel *m_separator;
};

#endif // TIMELINEVIEWFRAME_H
