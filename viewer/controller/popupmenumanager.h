#ifndef SERVICE_POPUP_MENU_MANAGER_H_
#define SERVICE_POPUP_MENU_MANAGER_H_

/*
 *
        {
          "x": 0,
          "y": 0,
            "items": [
                {
                    "itemId": 10000,
                    "itemIcon": "",
                    "itemIconHover": "",
                    "itemIconInactive": "",
                    "itemText": "Menu Item Text",
                    "shortcut": "Ctrl+P",
                    "isSeparator": false,
                    "isActive": true,
                    "checked": false,
                    "itemSubMenu": {}
                },
                {
                    "itemId": 10001,
                    "itemIcon": "",
                    "itemIconHover": "",
                    "itemIconInactive": "",
                    "itemText": "Menu Item Text",
                    "shortcut": "Ctrl+P",
                    "isSeparator": false,
                    "isActive": true,
                    "checked": false,
                    "itemSubMenu": {}
                }
           ]
        }
 *
 */
#include <QObject>
#include <QVariant>
#include <QPoint>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

class QAction;
class QMenu;

// Construct and display a popup menu at specific position.
class PopupMenuManager : public QObject {
    Q_OBJECT

public:
    explicit PopupMenuManager(QWidget *parent);
    ~PopupMenuManager();
    const QSize sizeHint() const;

    static const QJsonObject createItemObj(const int id,
                                           const QString &text,
                                           const bool isSeparator = false,
                                           const QString &shortcut = "",
                                           const QJsonObject &subMenu = QJsonObject());

    // Note: you should set menu content on initialization for binding shortcut
    void setMenuContent(const QString &menuJsonContent);
    void showMenu();
    void hideMenu();

signals:
    void menuHided();
    void menuItemClicked(int menuId, const QString &text);

private:
    void initConnections();
    void handleMenuActionTriggered(QAction* action);
    void constructMenu(QMenu* menu, const QJsonArray& content);

private:
    QPoint m_pos;
    QMenu* m_menu;
    QWidget *m_parentWidget;
};


#endif  // SERVICE_POPUP_MENU_MANAGER_H_
