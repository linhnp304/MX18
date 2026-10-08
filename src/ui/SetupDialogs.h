#pragma once

#include "core/Settings.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;

// Cửa sổ "Màu sắc và thiết lập khác": luôn nổi trên cùng khi đang mở.
class ColorSetupDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ColorSetupDialog(QWidget *parent = nullptr);

    void loadFromSettings();

signals:
    void applied();   // thiết lập đã ghi vào file, panel 1 vẽ lại

private:
    // Thứ tự ô màu, cũng là thứ tự dòng trên tab "Màu sắc"; tia báo động có
    // hai ô trên cùng một dòng (hai màu nhấp nháy).
    enum ColorIndex {
        CGrid, CTrail, CTrack, CTrackMh, CProfile, CPlot, CAlarm1, CAlarm2,
        kColorCount
    };

    void applyToSettings();
    void loadValues(const Setups &s);
    void updateSwatch(int index);
    QColor &colorRef(DisplayColors &c, int index) const;

    QPushButton *m_swatch[kColorCount] = {nullptr};
    QColor m_colors[kColorCount];

    QSpinBox *m_plotHold = nullptr;
    QSpinBox *m_trackDrop = nullptr;
    QRadioButton *m_trackSize[5] = {nullptr};
    QRadioButton *m_plotSize[5] = {nullptr};
    QCheckBox *m_mhTrackInit = nullptr;
    QDoubleSpinBox *m_mergeAzimuth = nullptr;
    QDoubleSpinBox *m_mergeRange = nullptr;
};

// Cửa sổ "Nhập mật khẩu kỹ sư".
class EngineerPasswordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EngineerPasswordDialog(QWidget *parent = nullptr);

private:
    QLineEdit *m_password = nullptr;
};

