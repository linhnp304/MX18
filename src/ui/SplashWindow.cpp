#include "ui/SplashWindow.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QTimer>

SplashWindow::SplashWindow(const QString &imagePath, const QString &caption, QWidget *parent)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_caption(caption)
{
    m_image.load(imagePath);
    if (m_image.isNull()) {
        // Thiếu ảnh thì vẫn phải chạy được: dựng nền tối cỡ vừa phải.
        m_image = QPixmap(640, 360);
        m_image.fill(QColor(0x10, 0x14, 0x18));
    }

    // Ảnh mẫu là 1200x1200 nhưng màn hình đích chỉ cao 1024, nên thu nhỏ cho vừa
    // màn hình mà vẫn giữ tỉ lệ; ảnh nhỏ hơn màn hình thì để nguyên cỡ thật.
    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        const QSize avail = scr->availableGeometry().size() * 0.92;
        if (m_image.width() > avail.width() || m_image.height() > avail.height())
            m_image = m_image.scaled(avail, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    setFixedSize(m_image.size());
    setAttribute(Qt::WA_DeleteOnClose, false);
}

void SplashWindow::showFor(int milliseconds)
{
    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        const QRect g = scr->geometry();
        move(g.center() - QPoint(width() / 2, height() / 2));
    }
    show();
    raise();
    QTimer::singleShot(milliseconds, this, [this] {
        close();
        emit finished();
    });
}

void SplashWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.drawPixmap(0, 0, m_image);

    if (m_caption.isEmpty())
        return;

    // Dòng chữ nằm đè lên ảnh, sát đáy và căn giữa.
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(qMax(14.0, height() * 0.032));
    p.setFont(f);

    const QFontMetrics fm(f);
    const int h = fm.height() + 14;
    const QRect band(0, height() - h - int(height() * 0.045), width(), h);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(band, QColor(0, 0, 0, 120));

    const QString text = m_caption.toUpper();
    p.setPen(QColor(0x20, 0x20, 0x20, 160));
    p.drawText(band.translated(1, 1), Qt::AlignCenter, text);
    p.setPen(QColor(0x4d, 0xb8, 0xff)); // xanh da trời theo yêu cầu
    p.drawText(band, Qt::AlignCenter, text);
}
