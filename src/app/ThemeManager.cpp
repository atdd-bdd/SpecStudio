#include "ThemeManager.h"
#include <QApplication>

static const char* kDarkStyleSheet = R"(
QWidget {
    background-color: #1e1e1e;
    color: #d4d4d4;
}
QMainWindow, QDialog {
    background-color: #1e1e1e;
}
QMenuBar {
    background-color: #2d2d2d;
    color: #cccccc;
}
QMenuBar::item:selected {
    background-color: #094771;
}
QMenu {
    background-color: #252526;
    color: #cccccc;
    border: 1px solid #454545;
}
QMenu::item:selected {
    background-color: #094771;
}
QToolBar {
    background-color: #2d2d2d;
    border: none;
}
QDockWidget {
    color: #cccccc;
    titlebar-close-icon: none;
}
QDockWidget::title {
    background-color: #2d2d2d;
    padding: 4px;
}
QTabWidget::pane {
    border: 1px solid #454545;
    background-color: #1e1e1e;
}
QTabBar::tab {
    background-color: #2d2d2d;
    color: #969696;
    padding: 4px 12px;
    border-right: 1px solid #1e1e1e;
}
QTabBar::tab:selected {
    background-color: #1e1e1e;
    color: #ffffff;
    border-top: 1px solid #007acc;
}
QPlainTextEdit, QTextEdit {
    background-color: #1e1e1e;
    color: #d4d4d4;
    selection-background-color: #264f78;
    border: none;
}
QListWidget {
    background-color: #252526;
    color: #cccccc;
    border: none;
}
QListWidget::item:selected {
    background-color: #094771;
}
QTreeView {
    background-color: #252526;
    color: #cccccc;
    border: none;
}
QTreeView::item:selected {
    background-color: #094771;
}
QTreeView::branch {
    background-color: #252526;
}
QLineEdit, QComboBox {
    background-color: #3c3c3c;
    color: #cccccc;
    border: 1px solid #555555;
    padding: 2px 4px;
    border-radius: 2px;
}
QComboBox::drop-down {
    border: none;
}
QPushButton {
    background-color: #0e639c;
    color: #ffffff;
    border: none;
    padding: 4px 12px;
    border-radius: 2px;
}
QPushButton:hover {
    background-color: #1177bb;
}
QPushButton:pressed {
    background-color: #0a4d78;
}
QScrollBar:vertical {
    background-color: #1e1e1e;
    width: 12px;
}
QScrollBar::handle:vertical {
    background-color: #424242;
    min-height: 20px;
    border-radius: 3px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal {
    background-color: #1e1e1e;
    height: 12px;
}
QScrollBar::handle:horizontal {
    background-color: #424242;
    min-width: 20px;
    border-radius: 3px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QSplitter::handle {
    background-color: #454545;
}
QStatusBar {
    background-color: #007acc;
    color: #ffffff;
}
QHeaderView::section {
    background-color: #2d2d2d;
    color: #cccccc;
    border: 1px solid #454545;
    padding: 2px 4px;
}
QTableWidget {
    background-color: #252526;
    color: #cccccc;
    gridline-color: #454545;
}
QCheckBox {
    color: #cccccc;
}
QLabel {
    color: #cccccc;
}
QGroupBox {
    color: #cccccc;
    border: 1px solid #454545;
    margin-top: 6px;
    padding-top: 4px;
}
QDialogButtonBox QPushButton {
    min-width: 64px;
}
)";

void ThemeManager::applyDark(QApplication* app)
{
    app->setStyleSheet(kDarkStyleSheet);
}

void ThemeManager::applyLight(QApplication* app)
{
    app->setStyleSheet({});
}

void ThemeManager::apply(QApplication* app, bool dark)
{
    dark ? applyDark(app) : applyLight(app);
}
