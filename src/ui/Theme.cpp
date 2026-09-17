#include "ui/Theme.h"

#include <QApplication>
#include <QPalette>

namespace Theme {

QString styleSheet()
{
    return QStringLiteral(R"QSS(
QWidget {
    background: #14171a;
    color: #d3dbe3;
}
QFrame#Panel, QWidget#Panel {
    background: #1b1f24;
}
QGroupBox {
    border: 1px solid #353d46;
    border-radius: 3px;
    margin-top: 14px;
    padding-top: 6px;
    background: #1b1f24;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 8px;
    padding: 0 4px;
    color: #7fc4ff;
    font-weight: bold;
}
QTabWidget::pane {
    border: 1px solid #353d46;
    background: #1b1f24;
    top: -1px;
}
QTabBar::tab {
    background: #21262c;
    border: 1px solid #353d46;
    border-bottom: none;
    padding: 5px 12px;
    color: #9aa5b1;
}
QTabBar::tab:selected {
    background: #2d343c;
    color: #e6edf4;
    border-bottom: 2px solid #3fa9f5;
}
QPushButton, QToolButton {
    background: #262c33;
    border: 1px solid #3c454f;
    border-radius: 3px;
    padding: 4px 10px;
    color: #d3dbe3;
}
QPushButton:hover, QToolButton:hover { background: #303841; }
QPushButton:pressed, QToolButton:pressed { background: #1d2228; }
QPushButton:disabled { color: #5d666f; border-color: #2c333a; }
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
    background: #101316;
    border: 1px solid #3c454f;
    border-radius: 2px;
    padding: 3px 5px;
    selection-background-color: #2f6ea5;
}
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled { color: #5d666f; background: #16191d; }
QComboBox::drop-down { border-left: 1px solid #3c454f; width: 16px; }
QHeaderView::section {
    background: #262c33;
    color: #b9c3cd;
    border: none;
    border-right: 1px solid #353d46;
    border-bottom: 1px solid #353d46;
    padding: 3px 6px;
}
QTableWidget, QTableView, QTreeWidget, QListWidget {
    background: #101316;
    alternate-background-color: #15191d;
    gridline-color: #2b323a;
    border: 1px solid #353d46;
}
QScrollBar:vertical { background: #14171a; width: 11px; margin: 0; }
QScrollBar::handle:vertical { background: #3c454f; min-height: 24px; border-radius: 5px; }
QScrollBar::handle:vertical:hover { background: #4d5863; }
QScrollBar:horizontal { background: #14171a; height: 11px; margin: 0; }
QScrollBar::handle:horizontal { background: #3c454f; min-width: 24px; border-radius: 5px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QSlider::groove:horizontal { height: 4px; background: #2b323a; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #2f6ea5; border-radius: 2px; }
QSlider::handle:horizontal {
    background: #9fb0c0; width: 11px; margin: -5px 0; border-radius: 5px;
}
QSlider::handle:horizontal:hover { background: #cfdae4; }
QCheckBox, QRadioButton { spacing: 6px; }
QCheckBox::indicator, QRadioButton::indicator { width: 13px; height: 13px; }
QCheckBox::indicator {
    border: 1px solid #55606b; background: #101316; border-radius: 2px;
}
QCheckBox::indicator:checked { background: #3fa9f5; border-color: #3fa9f5; }
QRadioButton::indicator {
    border: 1px solid #55606b; background: #101316; border-radius: 7px;
}
QRadioButton::indicator:checked { background: #3fa9f5; border-color: #3fa9f5; }
QSplitter::handle { background: #2b323a; }
QSplitter::handle:hover { background: #3fa9f5; }
QToolTip {
    background: #22272d; color: #d3dbe3; border: 1px solid #3fa9f5; padding: 3px;
}
QDialog, QMainWindow { background: #14171a; }
QLabel#SectionTitle { color: #7fc4ff; font-weight: bold; }
)QSS");
}

void apply()
{
    QPalette pal;
    pal.setColor(QPalette::Window, kBackground);
    pal.setColor(QPalette::WindowText, kText);
    pal.setColor(QPalette::Base, QColor(0x10, 0x13, 0x16));
    pal.setColor(QPalette::AlternateBase, kPanelAlt);
    pal.setColor(QPalette::Text, kText);
    pal.setColor(QPalette::Button, kPanelAlt);
    pal.setColor(QPalette::ButtonText, kText);
    pal.setColor(QPalette::Highlight, QColor(0x2f, 0x6e, 0xa5));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::ToolTipBase, kPanelAlt);
    pal.setColor(QPalette::ToolTipText, kText);
    pal.setColor(QPalette::Disabled, QPalette::Text, kTextDim);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, kTextDim);
    qApp->setPalette(pal);
    qApp->setStyleSheet(styleSheet());
}

} // namespace Theme
