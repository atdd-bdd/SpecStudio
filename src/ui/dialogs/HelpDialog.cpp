#include "HelpDialog.h"

#include <QComboBox>
#include <QFile>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextCursor>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("SpecStudio Help"));
    resize(900, 700);
    // A dialog you can leave open while working, and that gets a maximise button
    // rather than only a close cross.
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setSizeGripEnabled(true);

    auto* layout = new QVBoxLayout(this);

    auto* topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel(tr("Document:"), this));
    m_docChooser = new QComboBox(this);
    m_docChooser->addItem(tr("User Guide"),      QStringLiteral("User Guide.md"));
    m_docChooser->addItem(tr("Getting Started"), QStringLiteral("Getting Started.md"));
    topBar->addWidget(m_docChooser);
    topBar->addStretch(1);
    auto* findHint = new QLabel(tr("Ctrl+F to search"), this);
    findHint->setEnabled(false);
    topBar->addWidget(findHint);
    layout->addLayout(topBar);

    m_view = new QTextBrowser(this);
    m_view->setOpenExternalLinks(true);
    layout->addWidget(m_view, 1);

    // ---- find bar, hidden until asked for ----
    m_findBar = new QWidget(this);
    auto* findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(0, 0, 0, 0);
    findLayout->addWidget(new QLabel(tr("Find:"), m_findBar));
    m_findEdit = new QLineEdit(m_findBar);
    findLayout->addWidget(m_findEdit, 1);
    auto* prevBtn  = new QPushButton(tr("Previous"), m_findBar);
    auto* nextBtn  = new QPushButton(tr("Next"),     m_findBar);
    auto* closeBtn = new QPushButton(tr("Close"),    m_findBar);
    findLayout->addWidget(prevBtn);
    findLayout->addWidget(nextBtn);
    m_findStatus = new QLabel(m_findBar);
    m_findStatus->setEnabled(false);
    findLayout->addWidget(m_findStatus);
    findLayout->addWidget(closeBtn);
    m_findBar->hide();
    layout->addWidget(m_findBar);

    connect(m_docChooser, &QComboBox::currentIndexChanged, this, [this](int i) {
        loadDocument(m_docChooser->itemData(i).toString());
    });
    // Search as you type, from the top, so the first match is found without
    // pressing anything -- but keep Enter for stepping through the rest.
    connect(m_findEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.isEmpty()) { m_findStatus->clear(); return; }
        QTextCursor c = m_view->textCursor();
        c.setPosition(c.selectionStart());
        m_view->setTextCursor(c);
        findNext();
    });
    connect(m_findEdit, &QLineEdit::returnPressed, this, [this] { findNext(); });
    connect(nextBtn,  &QPushButton::clicked, this, [this] { findNext(false); });
    connect(prevBtn,  &QPushButton::clicked, this, [this] { findNext(true); });
    connect(closeBtn, &QPushButton::clicked, this, [this] { hideFindBar(); });

    loadDocument(QStringLiteral("User Guide.md"));
}

void HelpDialog::loadDocument(const QString& resourceName)
{
    QFile f(QStringLiteral(":/help/") + resourceName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_view->setPlainText(tr("Could not open %1.").arg(resourceName));
        return;
    }
    // setMarkdown, not setPlainText: the guide is written as Markdown and its
    // tables are most of what makes it readable.
    m_view->setMarkdown(QString::fromUtf8(f.readAll()));
    m_view->moveCursor(QTextCursor::Start);
}

void HelpDialog::raiseAndFocus()
{
    show();
    raise();
    activateWindow();
}

void HelpDialog::showFindBar()
{
    m_findBar->show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void HelpDialog::hideFindBar()
{
    m_findBar->hide();
    m_findStatus->clear();
    m_view->setFocus();
}

void HelpDialog::findNext(bool backwards)
{
    const QString text = m_findEdit->text();
    if (text.isEmpty()) return;

    QTextDocument::FindFlags flags;
    if (backwards) flags |= QTextDocument::FindBackward;

    if (m_view->find(text, flags)) {
        m_findStatus->clear();
        return;
    }

    // Nothing further in that direction: wrap once, so searching never dead-ends
    // partway through a document the length of the user guide.
    QTextCursor c = m_view->textCursor();
    c.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
    m_view->setTextCursor(c);

    m_findStatus->setText(m_view->find(text, flags) ? tr("wrapped")
                                                    : tr("not found"));
}

void HelpDialog::keyPressEvent(QKeyEvent* event)
{
    // Ctrl+F is the reason this dialog handles keys at all: the request was that
    // the usual find shortcut search *this* document, not the editor behind it.
    if (event->matches(QKeySequence::Find)) {
        showFindBar();
        return;
    }
    if (event->key() == Qt::Key_F3) {
        findNext(event->modifiers() & Qt::ShiftModifier);
        return;
    }
    if (event->key() == Qt::Key_Escape && m_findBar->isVisible()) {
        hideFindBar();       // first Escape closes the find bar, not the window
        return;
    }
    QDialog::keyPressEvent(event);
}
