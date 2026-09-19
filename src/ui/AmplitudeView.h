#pragma once

#include <QByteArray>
#include <QPoint>
#include <QWidget>

// Panel 2.2: cửa sổ biên độ (A-scope) của đường quét MH.
//
// Chiều ngang là 600 điểm biên độ của gói VIDEO_I, tương ứng cự ly 0…360 km;
// chiều dọc là biên độ 0…255. Di chuột trong cửa sổ thì hiện giá trị và cự ly
// của đúng điểm dưới con trỏ.
class AmplitudeView : public QWidget
{
    Q_OBJECT
public:
    explicit AmplitudeView(QWidget *parent = nullptr);

    // 600 byte biên độ của một lần quét.
    void setTrace(const QByteArray &video);
    void clearTrace();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF plotArea() const;

    QByteArray m_video;
    QPoint m_cursor;
    bool m_hasCursor = false;
};
