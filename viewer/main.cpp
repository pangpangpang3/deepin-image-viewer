#include "application.h"
#include "controller/commandline.h"
#include "service/defaultimageviewer.h"
#include "dthememanager.h"

#include <QApplication>
#include <DLog>
#include <QTranslator>

using namespace Dtk::Util;

namespace {

const char kPlatformThemeName[] = "QT_QPA_PLATFORMTHEME";

}  // namespace

int main(int argc, char *argv[])
{
#if defined(STATIC_LIB)
    DWIDGET_INIT_RESOURCE();
#endif
    // If platform theme name is empty, fallback to gtk2.
    // gtk2 theme is included in libqt5libqgtk2 package.
    if (qgetenv(kPlatformThemeName).length() == 0) {
      qputenv(kPlatformThemeName, "gtk2");
    }
    Application::loadDXcbPlugin();
    Application a(argc, argv);


    if (!service::isDefaultImageViewer()) {
        service::setDefaultImageViewer(true);
    }

    CommandLine *cl = CommandLine::instance();

    if (cl->processOption()) {
        Dtk::Widget::DThemeManager::instance()->setTheme("dark");

        DLogManager::registerConsoleAppender();
        DLogManager::registerFileAppender();
        dInfo()<< "LogFile:" << DLogManager::getlogFilePath();

        if (Application::isDXcbPlatform()) {
            quick_exit(a.exec());
        } else {
            return a.exec();
        }
    }
    else {
        return 0;
    }
}
