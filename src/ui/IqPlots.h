#pragma once

#include <QPoint>
#include <QRectF>
#include <QWidget>

#include <vector>

// Ba đồ thị của cửa sổ "ViewIQ - Vẽ cánh sóng", vẽ bằng QPainter như cửa sổ
// biên độ ở panel 2 (không dùng Qt Charts — xem ràng buộc "chỉ Qt base").

// Thang trục Y tự co giãn (anh Linh chốt 2026-09-24): nới ngay khi dữ liệu vượt
// khung, co lại từ từ để trục không nhảy liên tục ở nhịp 25 hình/giây.
class AutoRange
{
public:
    explicit AutoRange(double minSpan) : m_minSpan(minSpan) {}

    void reset() { m_valid = false; }
    // Min/max của khung hình mới. floorAtZero: dữ liệu không âm (biên độ, dB)
    // thì phần chừa lề không lấn xuống dưới 0.
    void feed(double lo, double hi, bool floorAtZero);

    bool valid() const { return m_valid; }
    double lo() const { return m_lo; }
    double hi() const { return m_hi; }

private:
    double m_minSpan;
    double m_lo = 0.0;
    double m_hi = 1.0;
    bool m_valid = false;
};

// Panel 1 "ViewIQ": 600 điểm IQ1 (đỏ) và IQ2 (xanh biển); trục ngang là chỉ số
// word từ StartWord đến StartWord + 600.
class IqScopeView : public QWidget
{
    Q_OBJECT
public:
    explicit IqScopeView(QWidget *parent = nullptr);

    void setTrace(int startWord, const std::vector<qint16> &iq1, const std::vector<qint16> &iq2);
    void clearTrace();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF plotArea() const;

    int m_start = 0;
    std::vector<qint16> m_iq1;
    std::vector<qint16> m_iq2;
    AutoRange m_range{16.0};
    QPoint m_cursor;
    bool m_hasCursor = false;
};

// Cánh sóng theo 4096 phương vị encoder, đã đổi đơn vị theo ViewType. Hai đồ
// thị cánh sóng dùng chung một thang để vòng tròn bên trái khớp với vạch chia
// trục Y bên phải.
struct BeamTrace {
    std::vector<double> sum;        // NaN = phương vị chưa có số liệu
    std::vector<double> sub;
    int sweep = -1;                 // ô phương vị của gói mới nhất
    bool valid = false;             // đã có thang (tức đã có ít nhất một điểm)
    double lo = 0.0;
    double hi = 1.0;
};

// Panel 2.1: đồ thị cực phương vị - biên độ; 0° hướng lên, chiều kim đồng hồ
// như trên nền bản đồ.
class BeamPolarView : public QWidget
{
    Q_OBJECT
public:
    explicit BeamPolarView(QWidget *parent = nullptr);

    void setTrace(const BeamTrace &trace);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    BeamTrace m_trace;
};

// Panel 2.2: biên độ theo phương vị, trục ngang 0..360°.
class BeamScopeView : public QWidget
{
    Q_OBJECT
public:
    explicit BeamScopeView(QWidget *parent = nullptr);

    void setTrace(const BeamTrace &trace);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF plotArea() const;

    BeamTrace m_trace;
    QPoint m_cursor;
    bool m_hasCursor = false;
};
