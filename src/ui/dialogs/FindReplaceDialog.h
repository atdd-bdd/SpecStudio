#pragma once

#include <QDialog>

class EditorTabWidget;
class PlainTextEditor;
class QCheckBox;
class QLabel;
class QLineEdit;

class FindReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindReplaceDialog(EditorTabWidget* tabs, QWidget* parent = nullptr);

    void showFind();
    void showReplace();

private slots:
    void onFindNext();
    void onFindPrev();
    void onReplace();
    void onReplaceAll();

private:
    PlainTextEditor* currentEditor() const;
    void             setStatus(const QString& msg, bool error = false);

    EditorTabWidget* m_tabs;
    QLineEdit*       m_findEdit;
    QLineEdit*       m_replaceEdit;
    QCheckBox*       m_caseSensitive;
    QCheckBox*       m_wrapAround;
    QLabel*          m_statusLabel;
};
