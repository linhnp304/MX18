#pragma once

#include "ui/IconFactory.h"

#include <QWidget>

class QLabel;
class QToolButton;

// Panel 3: thanh trạng thái dưới cùng.
//
// Trái: các nút mở/đóng cửa sổ trạng thái (biểu tượng đổi màu theo tình trạng).
// Giữa: thời gian hệ thống, toạ độ con trỏ, góc đường quét.
// Phải: nút toạ độ tâm đài và nút ẩn/hiện panel 2.
class StatusPanel : public QWidget
{
    Q_OBJECT
public:
    enum PopupId { Notify = 0, Network, MhStatus, ScnStatus, SvrStatus, RadarCenter, PopupCount };

    // Màu biểu tượng trạng thái.
    enum StateColor { Idle = 0, Ok, Warn, Error };

    explicit StatusPanel(QWidget *parent = nullptr);

    void setPopupState(PopupId id, StateColor color);
    void setPopupChecked(PopupId id, bool checked);
    void setPanelHidden(bool hidden);

    void setCursorInfo(bool valid, double lat, double lon, double bearing, double range);
    void setSweepAngles(bool valid, double radarAz, double mhAz);

    // Toạ độ trái-trên (theo cửa sổ chính) để neo cửa sổ popup của nút.
    QPoint anchorFor(PopupId id, QWidget *reference) const;

signals:
    void popupToggled(int id);
    void panelToggleRequested();

private:
    QToolButton *makeButton(PopupId id, IconFactory::Glyph glyph, const QString &tooltip);
    void updateTime();

    QToolButton *m_buttons[PopupCount] = {nullptr};
    IconFactory::Glyph m_glyphs[PopupCount] = {};
    QToolButton *m_panelBtn = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_cursorLabel = nullptr;
    QLabel *m_angleLabel = nullptr;
    bool m_panelHidden = false;
};
