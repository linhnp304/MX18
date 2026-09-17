#pragma once

#include <QVector>
#include <QWidget>

// Panel 2.2: cửa sổ biên độ (A-scope). Giai đoạn này mới dựng khung và lưới,
// đường biên độ thật sẽ nối vào ở giai đoạn sau.
class AmplitudeView : public QWidget
{
    Q_OBJECT
public:
    explicit AmplitudeView(QWidget *parent = nullptr);

    // Một lần quét: biên độ chuẩn hoá 0..1 theo cự ly.
    void setTrace(const QVector<float> &samples);
    void clearTrace();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<float> m_samples;
};
