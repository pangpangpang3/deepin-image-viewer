#include "mainwidget.h"
#include "application.h"
#include "controller/importer.h"
#include "controller/configsetter.h"
#include "imageinfodialog.h"
#include "module/album/albumpanel.h"
//#include "module/edit/EditPanel.h"
#include "module/timeline/timelinepanel.h"
#include "module/slideshow/slideshowpanel.h"
#include "module/view/viewpanel.h"
#include "utils/baseutils.h"
#include "widgets/processtooltip.h"
#include <QDebug>
#include <QDesktopWidget>
#include <QFile>
#include <QHBoxLayout>

namespace {

const int TOP_TOOLBAR_HEIGHT = 40;
const int BOTTOM_TOOLBAR_HEIGHT = 22;
const int EXTENSION_PANEL_WIDTH = 240;

const QString SETTINGS_GROUP = "MAINWIDGET";
const QString SETTINGS_MAINPANEL_KEY = "MainPanel";

}  // namespace

MainWidget::MainWidget(bool manager, QWidget *parent)
    : QFrame(parent)
{
    initStyleSheet();
    initPanelStack(manager);
    initExtensionPanel();
    initTopToolbar();
    initBottomToolbar();

    initConnection();
}

MainWidget::~MainWidget()
{

}

void MainWidget::resizeEvent(QResizeEvent *)
{
    if (m_topToolbar) {
        m_topToolbar->resize(width(), TOP_TOOLBAR_HEIGHT);
//        m_topToolbar->move(0, 0);
    }
    if (m_bottomToolbar) {
        m_bottomToolbar->resize(width(), m_bottomToolbar->height());
        if (m_bottomToolbar->isVisible())
            m_bottomToolbar->move(0, height() - m_bottomToolbar->height());
    }
    if (m_extensionPanel) {
        m_extensionPanel->resize(m_extensionPanel->width(), height());
    }
}

void MainWidget::onGotoPanel(ModulePanel *panel)
{
    QPointer<ModulePanel> p(panel);
    if (p.isNull()) {
        return;
    }

    dApp->importer->nap();

    // Record the last panel for restore in the next time launch
    if (p->isMainPanel() && ! p->moduleName().isEmpty()) {
        dApp->setter->setValue(SETTINGS_GROUP, SETTINGS_MAINPANEL_KEY,
                               QVariant(p->moduleName()));
    }

    m_panelStack->setCurrentWidget(panel);
}

void MainWidget::onShowProcessTooltip(const QString &message, bool success)
{
    ProcessTooltip *t = new ProcessTooltip(this);
    t->showTooltip(message, success);
    t->move((width() - t->width()) / 2, height() * 4 / 5);
}

void MainWidget::onShowImageInfo(const QString &path)
{
    if (m_infoShowingList.indexOf(path) != -1)
        return;
    else
        m_infoShowingList << path;

    ImageInfoDialog *info = new ImageInfoDialog(this);
    info->setPath(path);
    info->move((width() - info->width()) / 2 +
               mapToGlobal(QPoint(0, 0)).x(),
               (window()->height() - info->sizeHint().height()) / 2 +
               mapToGlobal(QPoint(0, 0)).y());
    info->show();
    info->setWindowState(Qt::WindowActive);
    connect(info, &ImageInfoDialog::closed, this, [=] {
        info->deleteLater();
        m_infoShowingList.removeAll(path);
    });
    connect(dApp->signalM, &SignalManager::gotoPanel,
            info, &ImageInfoDialog::close);
}

void MainWidget::initPanelStack(bool manager)
{
    m_panelStack = new QStackedWidget(this);
    m_panelStack->setObjectName("PanelStack");

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_panelStack);

    // Init panel
    if (manager) {
        TimelinePanel *m_timelinePanel = new TimelinePanel;
        m_panelStack->addWidget(m_timelinePanel);
        AlbumPanel *m_albumPanel = new AlbumPanel;
        m_panelStack->addWidget(m_albumPanel);
//        EditPanel *m_editPanel = new EditPanel();
//        m_panelStack->addWidget(m_editPanel);
    }
    SlideShowPanel *m_slideShowPanel = new SlideShowPanel();
    m_panelStack->addWidget(m_slideShowPanel);
    ViewPanel *m_viewPanel = new ViewPanel();
    m_panelStack->addWidget(m_viewPanel);
}

void MainWidget::initTopToolbar()
{
    m_topToolbar = new TopToolbar(this);
    m_topToolbar->resize(width(), TOP_TOOLBAR_HEIGHT);
//    m_topToolbar->moveWithAnimation(0, 0);
    m_topToolbar->move(0, 0);
    connect(dApp->signalM, &SignalManager::updateTopToolbarLeftContent,
            this, [=](QWidget *c) {
        if (c != nullptr)
            m_topToolbar->setLeftContent(c);
    });
    connect(dApp->signalM, &SignalManager::updateTopToolbarMiddleContent,
            this, [=](QWidget *c) {
        if (c != nullptr)
            m_topToolbar->setMiddleContent(c);
    });
    connect(dApp->signalM, &SignalManager::showTopToolbar, this, [=] {
//        m_topToolbar->moveWithAnimation(0, 0);
        m_topToolbar->move(0, 0);
    });
    connect(dApp->signalM, &SignalManager::hideTopToolbar, this,
            [=](bool immediately) {
        Q_UNUSED(immediately)
        m_topToolbar->move(0, - TOP_TOOLBAR_HEIGHT);
//        if (immediately) {
//            m_topToolbar->move(0, - TOP_TOOLBAR_HEIGHT);
//        }
//        else {
//            m_topToolbar->moveWithAnimation(0, - TOP_TOOLBAR_HEIGHT);
//        }
    });
}

void MainWidget::initConnection()
{
    connect(dApp->signalM, &SignalManager::backToMainPanel, this, [=] {
        window()->show();
        window()->raise();
        window()->activateWindow();
        QString name = dApp->setter->value(SETTINGS_GROUP,
                                           SETTINGS_MAINPANEL_KEY).toString();
        if (name.isEmpty()) {
            emit dApp->signalM->gotoTimelinePanel();
            return;
        }

        for (int i = 0; i < m_panelStack->count(); i++) {
            if (ModulePanel *p =
                    static_cast<ModulePanel *>(m_panelStack->widget(i))) {
                if ((p->moduleName() == name) && p->isMainPanel()) {
                    emit dApp->signalM->gotoPanel(p);
                    return;
                }
            }
        }
    });
    connect(dApp->signalM, &SignalManager::gotoPanel,
            this, &MainWidget::onGotoPanel);
    connect(dApp->signalM, &SignalManager::showInFileManager,
            this, [=] (const QString &path) {
        utils::base::showInFileManager(path);
    });
    connect(dApp->signalM, &SignalManager::showImageInfo,
            this, &MainWidget::onShowImageInfo);
    connect(dApp->importer, &Importer::importProgressChanged, this, [=] (double v) {
        if (v == 1) {
            onShowProcessTooltip(tr("Imported successfully"), true);
        }
    });
}

void MainWidget::initBottomToolbar()
{
    m_bottomToolbar = new BottomToolbar(this);
    m_bottomToolbar->resize(width(), BOTTOM_TOOLBAR_HEIGHT);
    m_bottomToolbar->move(0, height() - m_bottomToolbar->height());
    connect(dApp->signalM, &SignalManager::updateBottomToolbarContent,
            this, [=](QWidget *c, bool wideMode) {
        if (c == nullptr)
            return;
        m_bottomToolbar->setContent(c);
        if (wideMode) {
            m_bottomToolbar->setFixedHeight(38);
        }
        else {
            m_bottomToolbar->setFixedHeight(BOTTOM_TOOLBAR_HEIGHT);
        }
        m_bottomToolbar->move(0, height() - m_bottomToolbar->height());
    });
    connect(dApp->signalM, &SignalManager::showBottomToolbar, this, [=] {
        m_bottomToolbar->setVisible(true);
        m_bottomToolbar->move(0, height() - m_bottomToolbar->height());
//        // Make the bottom toolbar always stay at the bottom after windows resize
//        m_bottomToolbar->move(0, height());
//        m_bottomToolbar->moveWithAnimation(0, height() - m_bottomToolbar->height());
    });

    connect(dApp->signalM, &SignalManager::hideBottomToolbar,
            this, [=](bool immediately) {
        m_bottomToolbar->move(0, height());
        m_bottomToolbar->setVisible(false);
        Q_UNUSED(immediately)
//        if (immediately) {
//            m_bottomToolbar->move(0, height());
//            m_bottomToolbar->setVisible(false);
//        }
//        else {
//            m_bottomToolbar->moveWithAnimation(0, height());
//        }
    });
}

void MainWidget::initExtensionPanel()
{
    m_extensionPanel = new ExtensionPanel(this);
    m_extensionPanel->move(- EXTENSION_PANEL_WIDTH, 0);
    connect(dApp->signalM, &SignalManager::updateExtensionPanelContent,
            this, [=](QWidget *c) {
        if (c != nullptr)
            m_extensionPanel->setContent(c);
    });
    connect(dApp->signalM, &SignalManager::updateExtensionPanelRect,
            this, [=] {
        m_extensionPanel->updateRectWithContent();
    });
    connect(dApp->signalM, &SignalManager::showExtensionPanel, this, [=] {
        // Is visible
        if (m_extensionPanel->pos() == QPoint(0, 0)) {
            m_extensionPanel->moveWithAnimation(- qMax(m_extensionPanel->width(),
                                                   EXTENSION_PANEL_WIDTH), 0);
        }
        else {
            m_extensionPanel->moveWithAnimation(0, 0);
        }
    });
    connect(dApp->signalM, &SignalManager::hideExtensionPanel,
            this, [=] (bool immediately) {
        if (immediately) {
            m_extensionPanel->move(- qMax(m_extensionPanel->width(),
                                                   EXTENSION_PANEL_WIDTH), 0);
        }
        else {
            m_extensionPanel->moveWithAnimation(- qMax(m_extensionPanel->width(),
                                                   EXTENSION_PANEL_WIDTH), 0);
        }
    });
}

void MainWidget::initStyleSheet()
{
    QFile sf(":/qss/resources/qss/frame.qss");
    if (!sf.open(QIODevice::ReadOnly)) {
        qWarning() << "Open style-sheet file error:" << sf.errorString();
        return;
    }

    qApp->setStyleSheet(QString(sf.readAll()));
    sf.close();
}
