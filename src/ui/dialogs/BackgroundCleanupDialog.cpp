#include "BackgroundCleanupDialog.h"

#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QTextStream>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

BackgroundCleanupDialog::BackgroundCleanupDialog(const QString& filePath,
                                                 Mode mode,
                                                 QWidget* parent)
    : QDialog(parent, Qt::Tool | Qt::WindowCloseButtonHint)
    , m_filePath(filePath)
    , m_mode(mode)
{
    const QString title = (mode == Mode::Background ? tr("Background") : tr("Cleanup"))
                          + " — " + QFileInfo(filePath).fileName();
    setWindowTitle(title);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(640, 480);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenLinks(false);

    m_refresh = new QPushButton(tr("Refresh"), this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_refresh);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_browser);
    layout->addLayout(btnRow);

    connect(m_refresh, &QPushButton::clicked, this, &BackgroundCleanupDialog::refresh);

    m_watcher = new QFileSystemWatcher(this);
    if (QFile::exists(filePath))
        m_watcher->addPath(filePath);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &BackgroundCleanupDialog::refresh);

    refresh();
}

// ---------------------------------------------------------------------------
// Lightweight file parser
// ---------------------------------------------------------------------------

BackgroundCleanupDialog::ParsedFile BackgroundCleanupDialog::parseFile(const QString& filePath)
{
    ParsedFile result;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream in(&f);
    const QStringList lines = in.readAll().split('\n');

    enum class State {
        Top, InBackground, InCleanup, InDefineTable,
        AwaitStepTable, InStepTable, SkipBlock
    };

    State state = State::Top;
    bool  inCleanup = false;

    DisplayStep*   curStep   = nullptr;
    DisplayDefine* curDefine = nullptr;

    auto isPipeRow  = [](const QString& t) { return t.startsWith('|'); };
    auto splitPipe  = [](const QString& line) {
        QStringList parts = line.split('|');
        QStringList cells;
        for (int i = 1; i < parts.size() - 1; ++i)
            cells << parts[i].trimmed();
        return cells;
    };

    auto endStep = [&]() {
        curStep = nullptr;
        state   = inCleanup ? State::InCleanup : State::InBackground;
    };

    static QRegularExpression reStep(
        R"(^\s*(Given|When|Then|And|WhenThen)\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reAttr(
        R"(\s*:\s*(\w[\w\s]*\w|\w+)(?:\s+(Transposed))?\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reDef(R"(^=(\w+)\s*$)");
    static QRegularExpression reDefineDecl(
        R"(^Define\s+(\w+)\s*(?:=\s*(.*))?$)",
        QRegularExpression::CaseInsensitiveOption);

    QString lastKw;

    for (const QString& raw : lines) {
        const QString trimmed = raw.trimmed();

        // Blank line
        if (trimmed.isEmpty()) {
            if (state == State::InStepTable || state == State::AwaitStepTable)
                endStep();
            if (state == State::SkipBlock)
                state = State::Top;
            continue;
        }

        // Comment
        if (trimmed.startsWith('#'))
            continue;

        // Continuation line
        if ((raw.startsWith(' ') || raw.startsWith('\t')) && !isPipeRow(trimmed))
            continue;

        // Pipe row
        if (isPipeRow(trimmed)) {
            QStringList cells = splitPipe(trimmed);
            if (state == State::SkipBlock) {
                // discard
            } else if (state == State::InDefineTable && curDefine) {
                curDefine->tableRows << cells;
                if (curDefine->tableRows.size() == 1) {
                    QString h0 = cells.isEmpty() ? "" : cells[0].toLower();
                    curDefine->transposed = (h0 == "attribute" || h0 == "name");
                }
            } else if (state == State::AwaitStepTable && curStep) {
                curStep->tableRows << cells;
                state = State::InStepTable;
                if (!curStep->transposed && cells.size() >= 2) {
                    QString h0 = cells[0].toLower();
                    if (h0 == "attribute" || h0 == "name") {
                        curStep->transposed = true;
                        curStep->hasHeader  = true;
                        curStep->tableRows.clear(); // header consumed
                    } else {
                        curStep->hasHeader = true;
                    }
                } else if (curStep->transposed) {
                    curStep->hasHeader = false;
                } else {
                    curStep->hasHeader = true;
                }
            } else if (state == State::InStepTable && curStep) {
                curStep->tableRows << cells;
            }
            continue;
        }

        // Non-pipe: close any open step table
        if (state == State::InStepTable || state == State::AwaitStepTable)
            endStep();
        if (state == State::SkipBlock)
            state = State::Top;

        const QString firstWord = trimmed.split(QRegularExpression(R"(\s+)")).first();

        // Skip inline descriptors
        static const QStringList skipWords = {
            "Description","Details","Constraint","Notes","Import","Insert"
        };
        bool isSkip = false;
        for (const QString& k : skipWords)
            if (firstWord.startsWith(k, Qt::CaseInsensitive)) { isSkip = true; break; }
        if (isSkip) continue;

        // Background
        if (firstWord.compare("Background", Qt::CaseInsensitive) == 0
         || trimmed.startsWith("Background:", Qt::CaseInsensitive)) {
            curStep = nullptr; lastKw = {};
            inCleanup = false;
            state = State::InBackground;
            continue;
        }

        // Cleanup
        if (firstWord.compare("Cleanup", Qt::CaseInsensitive) == 0
         || trimmed.startsWith("Cleanup:", Qt::CaseInsensitive)) {
            curStep = nullptr; lastKw = {};
            inCleanup = true;
            state = State::InCleanup;
            continue;
        }

        // Scenario / other block-level keyword — exit Background/Cleanup
        static const QStringList blockWords = {
            "Scenario","ScenarioGroup","BusinessRule","Calculation",
            "DataType","DomainTerm","Attributes","Entity","Specification",
            "Examples"
        };
        bool isBlock = false;
        for (const QString& k : blockWords)
            if (firstWord.startsWith(k, Qt::CaseInsensitive)) { isBlock = true; break; }
        if (isBlock) {
            curStep   = nullptr;
            curDefine = nullptr;
            state = State::SkipBlock;
            continue;
        }

        // Define
        {
            auto dm = reDefineDecl.match(trimmed);
            if (dm.hasMatch()) {
                curStep = nullptr;
                DisplayDefine def;
                def.name = dm.captured(1);
                QString afterEq = dm.captured(2).trimmed();
                if (!afterEq.isEmpty()) {
                    def.scalarValue = afterEq;
                    def.isTable     = false;
                    result.defines << def;
                    curDefine = nullptr;
                    state     = State::Top;
                } else {
                    def.isTable = true;
                    result.defines << def;
                    curDefine = &result.defines.last();
                    state = State::InDefineTable;
                }
                continue;
            }
        }

        // Steps
        if (state == State::InBackground || state == State::InCleanup) {
            auto sm = reStep.match(trimmed);
            if (sm.hasMatch()) {
                QString kw   = sm.captured(1);
                QString rest = sm.captured(2).trimmed();
                if (kw.compare("And", Qt::CaseInsensitive) == 0)
                    kw = lastKw.isEmpty() ? "Given" : lastKw;
                else if (kw.compare("WhenThen", Qt::CaseInsensitive) == 0)
                    kw = QStringLiteral("WhenThen");
                else
                    kw = kw[0].toUpper() + kw.mid(1).toLower();
                lastKw = kw;

                DisplayStep st;
                st.keyword = kw;

                auto am = reAttr.match(rest);
                if (am.hasMatch()) {
                    st.attrSetName = am.captured(1).trimmed();
                    st.transposed  = !am.captured(2).isEmpty();
                    st.text        = rest.left(am.capturedStart()).trimmed();
                } else {
                    st.text = rest;
                }

                if (inCleanup) {
                    result.cleanupSteps << st;
                    curStep = &result.cleanupSteps.last();
                } else {
                    result.backgroundSteps << st;
                    curStep = &result.backgroundSteps.last();
                }

                state = st.attrSetName.isEmpty()
                    ? (inCleanup ? State::InCleanup : State::InBackground)
                    : State::AwaitStepTable;
                continue;
            }
        }

        // Define reference =Name
        {
            auto dm = reDef.match(trimmed);
            if (dm.hasMatch() && curStep
                && (state == State::InBackground || state == State::InCleanup
                    || state == State::AwaitStepTable)) {
                curStep->defineRef = dm.captured(1);
                endStep();
                continue;
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Define lookup
// ---------------------------------------------------------------------------

BackgroundCleanupDialog::DisplayDefine*
BackgroundCleanupDialog::findDefine(const QString& name, QVector<DisplayDefine>& defines)
{
    for (auto& d : defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0)
            return &d;
    return nullptr;
}

// ---------------------------------------------------------------------------
// HTML rendering helpers
// ---------------------------------------------------------------------------

QString BackgroundCleanupDialog::renderTable(const QVector<QStringList>& rows,
                                              bool transposed, bool hasHeader) const
{
    if (rows.isEmpty()) return {};

    QString html;
    html += "<table border='1' cellspacing='0' cellpadding='4' "
            "style='border-collapse:collapse; margin:4px 0 8px 0;'>\n";

    auto cellVal = [](const QString& raw) {
        return QString(raw).replace('~', ' ').toHtmlEscaped();
    };

    if (transposed) {
        for (const QStringList& row : rows) {
            html += "<tr>";
            for (int ci = 0; ci < row.size(); ++ci) {
                if (ci == 0)
                    html += "<th style='background:#3C3C3C; color:#DCDCDC; text-align:left; "
                            "padding:3px 6px;'>" + cellVal(row[ci]) + "</th>";
                else
                    html += "<td style='padding:3px 6px;'>" + cellVal(row[ci]) + "</td>";
            }
            html += "</tr>\n";
        }
    } else {
        for (int ri = 0; ri < rows.size(); ++ri) {
            html += "<tr>";
            const bool isHdr = (hasHeader && ri == 0);
            for (const QString& cell : rows[ri]) {
                if (isHdr)
                    html += "<th style='background:#3C3C3C; color:#DCDCDC; padding:3px 6px;'>"
                            + cellVal(cell) + "</th>";
                else
                    html += "<td style='padding:3px 6px;'>" + cellVal(cell) + "</td>";
            }
            html += "</tr>\n";
        }
    }

    html += "</table>\n";
    return html;
}

QString BackgroundCleanupDialog::resolveDefine(const QString& defName,
                                                QVector<DisplayDefine>& defines) const
{
    DisplayDefine* def = findDefine(defName, defines);
    if (!def) return "<i>(Define '" + defName.toHtmlEscaped() + "' not found)</i>";
    if (!def->isTable)
        return "<span style='color:#CE9178;'>" + def->scalarValue.toHtmlEscaped() + "</span>";
    return renderTable(def->tableRows, def->transposed, !def->transposed);
}

QString BackgroundCleanupDialog::buildHtml(const QVector<DisplayStep>& steps,
                                            QVector<DisplayDefine>& defines) const
{
    if (steps.isEmpty())
        return "<p style='color:#808080;'><i>(none)</i></p>";

    static const QMap<QString, QString> kwColors = {
        { "given", "#4EC9B0" },
        { "when",  "#569CD6" },
        { "then",  "#C586C0" }
    };

    QString html;
    for (const DisplayStep& step : steps) {
        const QString col = kwColors.value(step.keyword.toLower(), "#DCDCDC");
        html += "<p style='margin:8px 0 2px 0;'>"
                "<b style='color:" + col + ";'>" + step.keyword.toHtmlEscaped() + "</b>"
                " " + step.text.toHtmlEscaped();
        if (!step.attrSetName.isEmpty())
            html += ": <i style='color:#9CDCFE;'>" + step.attrSetName.toHtmlEscaped() + "</i>";
        if (step.transposed)
            html += " <span style='color:#C586C0;'>(Transposed)</span>";
        html += "</p>\n";

        if (!step.defineRef.isEmpty()) {
            html += "<div style='margin-left:16px;'>"
                    "<span style='color:#CE9178;'>=</span>"
                    "<b>" + step.defineRef.toHtmlEscaped() + "</b><br/>\n";
            html += resolveDefine(step.defineRef, const_cast<QVector<DisplayDefine>&>(defines));
            html += "</div>\n";
        } else if (!step.tableRows.isEmpty()) {
            html += "<div style='margin-left:16px;'>"
                    + renderTable(step.tableRows, step.transposed, step.hasHeader)
                    + "</div>\n";
        }
    }
    return html;
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

void BackgroundCleanupDialog::refresh()
{
    ParsedFile pf = parseFile(m_filePath);

    const QString sectionTitle = (m_mode == Mode::Background)
        ? tr("Background Steps") : tr("Cleanup Steps");
    const QVector<DisplayStep>& steps = (m_mode == Mode::Background)
        ? pf.backgroundSteps : pf.cleanupSteps;

    const QString html =
        "<html><body style='font-family:Consolas,\"Courier New\",monospace; "
        "font-size:10pt; background:#1E1E1E; color:#DCDCDC;'>"
        "<h3 style='color:#569CD6; margin-bottom:4px;'>" + sectionTitle + "</h3>"
        "<hr style='border-color:#3C3C3C;'/>"
        + buildHtml(steps, pf.defines)
        + "</body></html>";

    m_browser->setHtml(html);

    // Re-watch in case the file was replaced
    if (!m_watcher->files().contains(m_filePath) && QFile::exists(m_filePath))
        m_watcher->addPath(m_filePath);
}
