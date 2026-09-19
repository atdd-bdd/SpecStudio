#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;
class QPlainTextEdit;
class QSplitter;
class QStackedWidget;
class QTextEdit;
class QToolButton;

// Two versions of a specification, whole, with the differences marked.
//
// Two ways of looking, on a toggle, both from the same line comparison:
//
//   Inline -- one document, the way Word's Compare shows it: text that went
//   away is struck through in red, text that arrived is underlined in green,
//   and a line that changed shows both in place, word by word, so a table row
//   keeps its shape and its neighbours.
//
//   Side by side -- the earlier version on the left and the current one on
//   the right, scrolled together, a changed line beside its replacement and a
//   blank row where one side has nothing.
//
// The old Diff tab showed git's patch: the changed hunks alone, with + and -
// in the margin, which is a programmer's convention and lost the shape of a
// table around the change.
class DiffView : public QWidget
{
    Q_OBJECT
public:
    explicit DiffView(QWidget* parent = nullptr);

    enum class Mode { Inline, SideBySide };

    // Compare two whole texts. The titles head the two sides.
    void setTexts(const QString& oldText, const QString& newText,
                  const QString& oldTitle, const QString& newTitle);
    // Show a message instead of a comparison (no history, identical, ...).
    void setMessage(const QString& text);
    void clear();

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setDiffFont(const QFont& font);

    // One line of the comparison, after the two texts are lined up.
    struct Row {
        enum Kind { Same, Removed, Added, Changed };
        Kind    kind = Same;
        QString left;    // the earlier version's line, or empty
        QString right;   // the current version's line, or empty
    };
    // Lines of the two texts, aligned: a longest-common-subsequence of lines,
    // with each run of removed lines paired against the run of added lines
    // that follows it. Public so that it can be exercised without a window.
    static QVector<Row> compare(const QString& oldText, const QString& newText);

private:
    void render();
    void renderInline();
    void renderSideBySide();

    Mode            m_mode = Mode::Inline;
    QVector<Row>    m_rows;
    QString         m_oldTitle, m_newTitle;
    bool            m_showingMessage = false;

    QToolButton*    m_inlineButton     = nullptr;
    QToolButton*    m_sideBySideButton = nullptr;
    QStackedWidget* m_stack            = nullptr;
    QTextEdit*      m_inlineView       = nullptr;
    QWidget*        m_sidePage         = nullptr;
    QLabel*         m_leftTitle        = nullptr;
    QLabel*         m_rightTitle       = nullptr;
    QPlainTextEdit* m_leftView         = nullptr;
    QPlainTextEdit* m_rightView        = nullptr;
    QFont           m_font;
};
