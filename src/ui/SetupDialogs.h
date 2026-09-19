#pragma once

#include "core/Settings.h"

#include <QDialog>

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
    void applyToSettings();
    void restoreDefaults();
    void updateSwatch(int index);

    static constexpr int kColorCount = 4;
    QPushButton *m_swatch[kColorCount] = {nullptr};
    QColor m_colors[kColorCount];

    QSpinBox *m_plotHold = nullptr;
    QRadioButton *m_trackSize[5] = {nullptr};
    QRadioButton *m_plotSize[5] = {nullptr};
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

