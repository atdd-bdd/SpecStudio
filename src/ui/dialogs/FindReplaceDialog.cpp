#include "FindReplaceDialog.h"
#include "../../editors/PlainTextEditor.h"
#include "../../ui/EditorTabWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

FindReplaceDialog::FindReplaceDialog(EditorTabWidget* tabs, QWidget* parent)
    : QDialog(parent, Qt::Tool)
    , m_tabs(tabs)
{
    setWindowTitle(tr("Find / Replace"));
    setMinimumWidth(440);

    m_findEdit      = new QLineEdit(this);
    m_replaceEdit   = new QLineEdit(this);
    m_caseSensitive = new QCheckBox(tr("Case sensitive"), this);
    m_wrapAround    = new QCheckBox(tr("Wrap around"), this);
    m_wrapAround->setChecked(true);
    m_statusLabel   = new QLabel(this);

    auto* findNextBtn   = new QPushButton(tr("Find Next"), this);
    auto* findPrevBtn   = new QPushButton(tr("Find Prev"), this);
    auto* replaceBtn    = new QPushButton(tr("Replace"), this);
    auto* replaceAllBtn = new QPushButton(tr("Replace All"), this);
    auto* closeBtn      = new QPushButton(tr("Close"), this);

    findNextBtn->setDefault(true);

    auto* form = new QFormLayout;
    form->addRow(tr("Find:"),    m_findEdit);
    form->addRow(tr("Replace:"), m_replaceEdit);

    auto* optRow = new QHBoxLayout;
    optRow->addWidget(m_caseSensitive);
    optRow->addWidget(m_wrapAround);
    optRow->addStretch();

    auto* leftCol = new QVBoxLayout;
    leftCol->addLayout(form);
    leftCol->addLayout(optRow);
    leftCol->addWidget(m_statusLabel);

    auto* btnCol = new QVBoxLayout;
    btnCol->addWidget(findNextBtn);
    btnCol->addWidget(findPrevBtn);
    btnCol->addWidget(replaceBtn);
    btnCol->addWidget(replaceAllBtn);
    btnCol->addStretch();
    btnCol->addWidget(closeBtn);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->addLayout(leftCol, 1);
    mainLayout->addLayout(btnCol);

    connect(findNextBtn,   &QPushButton::clicked, this, &FindReplaceDialog::onFindNext);
    connect(findPrevBtn,   &QPushButton::clicked, this, &FindReplaceDialog::onFindPrev);
    connect(replaceBtn,    &QPushButton::clicked, this, &FindReplaceDialog::onReplace);
    connect(replaceAllBtn, &QPushButton::clicked, this, &FindReplaceDialog::onReplaceAll);
    connect(closeBtn,      &QPushButton::clicked, this, &QDialog::hide);
    connect(m_findEdit,    &QLineEdit::returnPressed, this, &FindReplaceDialog::onFindNext);
}

void FindReplaceDialog::showFind()
{
    setWindowTitle(tr("Find"));
    show();
    raise();
    activateWindow();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void FindReplaceDialog::showReplace()
{
    setWindowTitle(tr("Find / Replace"));
    show();
    raise();
    activateWindow();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

PlainTextEditor* FindReplaceDialog::currentEditor() const
{
    return dynamic_cast<PlainTextEditor*>(m_tabs->currentEditor());
}

void FindReplaceDialog::setStatus(const QString& msg, bool error)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(error ? "color: red;" : "color: green;");
}

void FindReplaceDialog::onFindNext()
{
    auto* ed = currentEditor();
    if (!ed) { setStatus(tr("No editor open"), true); return; }
    const QString text = m_findEdit->text();
    if (text.isEmpty()) { setStatus({}); return; }
    bool found = ed->findNext(text, m_caseSensitive->isChecked(), m_wrapAround->isChecked());
    setStatus(found ? QString() : tr("Not found"), !found);
}

void FindReplaceDialog::onFindPrev()
{
    auto* ed = currentEditor();
    if (!ed) { setStatus(tr("No editor open"), true); return; }
    const QString text = m_findEdit->text();
    if (text.isEmpty()) { setStatus({}); return; }
    bool found = ed->findPrev(text, m_caseSensitive->isChecked(), m_wrapAround->isChecked());
    setStatus(found ? QString() : tr("Not found"), !found);
}

void FindReplaceDialog::onReplace()
{
    auto* ed = currentEditor();
    if (!ed) return;
    const QString findText    = m_findEdit->text();
    const QString replaceText = m_replaceEdit->text();
    if (findText.isEmpty()) return;
    // If something is already selected (from a prior Find), replace it then advance.
    // If not, just find next so the user can see what will be replaced.
    if (!ed->replaceCurrent(replaceText))
        onFindNext();
    else
        onFindNext();
}

void FindReplaceDialog::onReplaceAll()
{
    auto* ed = currentEditor();
    if (!ed) return;
    const QString findText    = m_findEdit->text();
    const QString replaceText = m_replaceEdit->text();
    if (findText.isEmpty()) return;
    int count = ed->replaceAll(findText, replaceText, m_caseSensitive->isChecked());
    if (count > 0)
        setStatus(tr("Replaced %1 occurrence(s)").arg(count), false);
    else
        setStatus(tr("Not found"), true);
}
