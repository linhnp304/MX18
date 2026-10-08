#pragma once

#include <QFrame>
#include <QPoint>

class QLabel;
struct TrackEntry;

// Cửa sổ thông tin quỹ đạo (step-06 mục 6): con của panel 1, kéo đi được bằng
// chuột trong phạm vi panel. Nội dung bám theo quỹ đạo đang chọn; MapView
// đóng nó khi quỹ đạo đó bị xoá.
class TrackInfoBox : public QFrame
{
    Q_OBJECT
public:
    explicit TrackInfoBox(QWidget *parent);

    // Đổi sang quỹ đạo khác hoặc cập nhật quỹ đạo đang hiện.
    void setTrack(const TrackEntry &t);
    quint32 trackId() const { return m_id; }

    // Trắc thủ đã kéo cửa sổ đi chỗ khác: chọn quỹ đạo khác thì giữ nguyên chỗ.
    bool userMoved() const { return m_moved; }
    void closeBox();

    // Giữ cửa sổ nằm trọn trong panel (sau khi đổi nội dung hoặc panel co lại).
    void keepInside();

signals:
    void closed();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QLabel *m_body = nullptr;
    quint32 m_id = 0;
    bool m_moved = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
};
