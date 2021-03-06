#include "slideeffectplayer.h"
#include <QtCore/QTimerEvent>
#include <QtDebug>
#include <QPainter>

SlideEffectPlayer::SlideEffectPlayer(QObject *parent)
    : QObject(parent)
{

}

void SlideEffectPlayer::timerEvent(QTimerEvent *e)
{
    if (e->timerId() != m_tid)
        return;
    if (m_effect)
        m_effect->deleteLater();
    if (! startNext()) {
        stop();
    }
}

void SlideEffectPlayer::setFrameSize(int width, int height)
{
    m_w = width;
    m_h = height;
}

void SlideEffectPlayer::setImagePaths(const QStringList &paths)
{
    m_paths = paths;
    m_current = m_paths.constBegin();
}

void SlideEffectPlayer::setCurrentImage(const QString &path)
{
    m_current = std::find(m_paths.cbegin(), m_paths.cend(),  path);
    if (m_current == m_paths.constEnd())
        m_current = m_paths.constBegin();
}

QString SlideEffectPlayer::currentImagePath() const
{
    if (m_current == m_paths.constEnd())
        return *m_paths.constBegin();
    return *m_current;
}

bool SlideEffectPlayer::isRunning() const
{
    return m_running;
}

void SlideEffectPlayer::start()
{
    if (m_paths.isEmpty())
        return;
    m_running = true;
    m_tid = startTimer(2500);
}

bool SlideEffectPlayer::startNext()
{
    if (m_paths.isEmpty())
        return false;
    QSize fSize(m_w, m_h);
    if (! fSize.isValid()) {
        qWarning() << "Invalid frame size!";
        return false;
    }

    const QString oldPath = currentImagePath();
    m_current ++;
    if (m_current == m_paths.constEnd()) {
        m_current = m_paths.constBegin();
    }
    const QString newPath = currentImagePath();
    m_effect = SlideEffect::create();
    m_effect->setSize(fSize);
    m_effect->setImages(oldPath, newPath);
    if (!m_thread.isRunning())
        m_thread.start();

    m_effect->moveToThread(&m_thread);
    connect(m_effect, &SlideEffect::frameReady, this, [=] (const QImage &img) {
        if (m_running) {
            Q_EMIT frameReady(img);
        }
    }, Qt::DirectConnection);
    QMetaObject::invokeMethod(m_effect, "start");
    return true;
}

void SlideEffectPlayer::stop()
{
    if (!isRunning())
        return;
    killTimer(m_tid);
    m_tid = 0;
    m_running = false;
    Q_EMIT finished();
}
