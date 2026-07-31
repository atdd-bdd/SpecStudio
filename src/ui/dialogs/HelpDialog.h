#pragma once

#include <QDialog>

class QTextBrowser;
class QLineEdit;
class QLabel;
class QWidget;
class QComboBox;

// The user guide, readable without leaving SpecStudio.
//
// Deliberately modeless: help you cannot keep open beside the thing it describes
// is help you have to memorise first.
class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(QWidget* parent = nullptr);

    // Bring an already-open window forward rather than stacking another copy.
    void raiseAndFocus();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void loadDocument(const QString& resourceName);
    void showFindBar();
    void hideFindBar();
    void findNext(bool backwards = false);

    QComboBox*    m_docChooser = nullptr;
    QTextBrowser* m_view       = nullptr;
    QWidget*      m_findBar    = nullptr;
    QLineEdit*    m_findEdit   = nullptr;
    QLabel*       m_findStatus = nullptr;
};
