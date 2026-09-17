#pragma once

#include <QPixmap>
#include <QString>
#include <QWidget>

// Màn hình giới thiệu: đúng kích thước ảnh, không có ControlBox, tự đóng sau 2 giây.
class SplashWindow : public QWidget
{
    Q_OBJECT
public:
    explicit SplashWindow(const QString &imagePath, const QString &caption,
                          QWidget *parent = nullptr);

    void showFor(int milliseconds);

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_image;
    QString m_caption;
};
