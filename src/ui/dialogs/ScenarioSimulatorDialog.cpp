#include "ScenarioSimulatorDialog.h"

#include <QColor>
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

ScenarioSimulatorDialog::ScenarioSimulatorDialog(const QString& filePath,
                                                 int cursorLine,
                                                 QWidget* parent)
    : QDialog(parent, Qt::Tool | Qt::WindowCloseButtonHint)
    , m_filePath(filePath)
    , m_cursorLine(cursorLine)
{
    setWindowTitle(tr("Scenario Simulator — %1").arg(QFileInfo(filePath).fileName()));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(700, 520);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenLinks(false);

    m_refresh = new QPushButton(tr("Refresh"), this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_refresh);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_browser);
    layout->addLayout(btnRow);

    connect(m_refresh, &QPushButton::clicked, this, &ScenarioSimulatorDialog::refresh);

    m_watcher = new QFileSystemWatcher(this);
    if (QFile::exists(filePath))
        m_watcher->addPath(filePath);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &ScenarioSimulatorDialog::refresh);

    refresh();
}

// ---------------------------------------------------------------------------
// Lightweight parser — collects Background, all Scenarios, Cleanup, Defines
// ---------------------------------------------------------------------------

ScenarioSimulatorDialog::ParsedFile ScenarioSimulatorDialog::parseFile(const QString& filePath)
{
    ParsedFile result;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream in(&f);
    const QStringList lines = in.readAll().split('\n');

    enum class State {
        Top, InBackground, InCleanup, InScenario,
        InDefineTable, AwaitStepTable, InStepTable, SkipBlock
    };

    State state       = State::Top;
    bool  inCleanup   = false;
    int   lineNum     = 0;

    ParsedScenario* curScenario = nullptr;
    DisplayStep*    curStep     = nullptr;
    DisplayDefine*  curDefine   = nullptr;
    QString         lastKw;

    static const QStringList skipWords = {
        "Description","Details","Constraint","Notes","Import","Insert"
    };
    static const QStringList blockWords = {
        "Specification","Entity","Collection","DomainTerm","DataType","Attributes",
        "Examples","BusinessRule","Calculation","DataType"
    };
    static const QStringList scenarioWords = { "Scenario","ScenarioGroup" };
    static QRegularExpression reStep(
        R"(^\s*(Given|When|Then|And|WhenThen)\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reAttr(
        R"(\s*:\s*(\w[\w\s]*\w|\w+)(?:\s+(Vertical))?\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reDef(R"(^=(\w+)\s*$)");
    static QRegularExpression reDefineDecl(
        R"(^Define\s+(\w+)\s*(?:=\s*(.*))?$)",
        QRegularExpression::CaseInsensitiveOption);

    auto isPipeRow = [](const QString& t) { return t.startsWith('|'); };
    auto splitPipe = [](const QString& line) {
        QStringList parts = line.split('|');
        QStringList cells;
        for (int i = 1; i < parts.size() - 1; ++i)
            cells << parts[i].trimmed();
        return cells;
    };
    auto endStep = [&]() {
        curStep = nullptr;
        if (inCleanup)       state = State::InCleanup;
        else if (curScenario) state = State::InScenario;
        else                  state = State::InBackground;
    };
    auto addStep = [&](DisplayStep st) -> DisplayStep* {
        if (inCleanup) {
            result.cleanupSteps << st;
            return &result.cleanupSteps.last();
        } else if (curScenario) {
            curScenario->steps << st;
            return &curScenario->steps.last();
        } else {
            result.backgroundSteps << st;
            return &result.backgroundSteps.last();
        }
    };

    for (const QString& raw : lines) {
        ++lineNum;
        const QString trimmed = raw.trimmed();

        if (trimmed.isEmpty()) {
            if (state == State::InStepTable || state == State::AwaitStepTable) endStep();
            if (state == State::SkipBlock) state = State::Top;
            continue;
        }
        if (trimmed.startsWith('#')) continue;
        if ((raw.startsWith(' ') || raw.startsWith('\t')) && !isPipeRow(trimmed)) continue;

        // Pipe row
        if (isPipeRow(trimmed)) {
            QStringList cells = splitPipe(trimmed);
            if (state == State::SkipBlock) {
            } else if (state == State::InDefineTable && curDefine) {
                curDefine->tableRows << cells;
                if (curDefine->tableRows.size() == 1) {
                    QString h0 = cells.isEmpty() ? "" : cells[0].toLower();
                    curDefine->vertical = (h0 == "attribute" || h0 == "name");
                }
            } else if (state == State::AwaitStepTable && curStep) {
                curStep->tableRows << cells;
                state = State::InStepTable;
                if (!curStep->vertical && cells.size() >= 2) {
                    QString h0 = cells[0].toLower();
                    if (h0 == "attribute" || h0 == "name") {
                        curStep->vertical = true;
                        curStep->hasHeader  = true;
                        curStep->tableRows.clear();
                    } else {
                        curStep->hasHeader = true;
                    }
                } else if (curStep->vertical) {
                    curStep->hasHeader = false;
                } else {
                    curStep->hasHeader = true;
                }
            } else if (state == State::InStepTable && curStep) {
                curStep->tableRows << cells;
            }
            continue;
        }

        if (state == State::InStepTable || state == State::AwaitStepTable) endStep();
        if (state == State::SkipBlock) state = State::Top;

        const QString firstWord = trimmed.split(QRegularExpression(R"(\s+)")).first();

        bool isSkip = false;
        for (const QString& k : skipWords)
            if (firstWord.startsWith(k, Qt::CaseInsensitive)) { isSkip = true; break; }
        if (isSkip) continue;

        // Background
        if (firstWord.compare("Background", Qt::CaseInsensitive) == 0
         || trimmed.startsWith("Background:", Qt::CaseInsensitive)) {
            curStep = nullptr; lastKw = {}; curScenario = nullptr; inCleanup = false;
            state = State::InBackground;
            continue;
        }

        // Cleanup
        if (firstWord.compare("Cleanup", Qt::CaseInsensitive) == 0
         || trimmed.startsWith("Cleanup:", Qt::CaseInsensitive)) {
            curStep = nullptr; lastKw = {}; curScenario = nullptr; inCleanup = true;
            state = State::InCleanup;
            continue;
        }

        // Scenario / ScenarioGroup
        bool isScenario = false;
        for (const QString& k : scenarioWords)
            if (firstWord.startsWith(k, Qt::CaseInsensitive)) { isScenario = true; break; }
        if (isScenario) {
            curStep = nullptr; lastKw = {}; inCleanup = false;
            ParsedScenario sc;
            sc.keyword   = firstWord;
            sc.name      = trimmed.mid(firstWord.length()).trimmed();
            sc.startLine = lineNum;
            result.scenarios << sc;
            curScenario = &result.scenarios.last();
            state = State::InScenario;
            continue;
        }

        // Other block keywords → skip block
        bool isBlock = false;
        for (const QString& k : blockWords)
            if (firstWord.startsWith(k, Qt::CaseInsensitive)) { isBlock = true; break; }
        if (isBlock) {
            curStep = nullptr; curDefine = nullptr; curScenario = nullptr;
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
                    state = State::Top;
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
        if (state == State::InBackground || state == State::InCleanup
         || state == State::InScenario) {
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
                    st.vertical  = !am.captured(2).isEmpty();
                    st.text        = rest.left(am.capturedStart()).trimmed();
                } else {
                    st.text = rest;
                }

                curStep = addStep(st);
                state = st.attrSetName.isEmpty()
                    ? (inCleanup ? State::InCleanup
                       : (curScenario ? State::InScenario : State::InBackground))
                    : State::AwaitStepTable;
                continue;
            }
        }

        // Define reference =Name
        {
            auto dm = reDef.match(trimmed);
            if (dm.hasMatch() && curStep) {
                curStep->defineRef = dm.captured(1);
                endStep();
                continue;
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

ScenarioSimulatorDialog::DisplayDefine*
ScenarioSimulatorDialog::findDefine(const QString& name, QVector<DisplayDefine>& defines)
{
    for (auto& d : defines)
        if (d.name.compare(name, Qt::CaseInsensitive) == 0)
            return &d;
    return nullptr;
}

QString ScenarioSimulatorDialog::renderTable(const QVector<QStringList>& rows,
                                              bool vertical, bool hasHeader) const
{
    if (rows.isEmpty()) return {};

    auto cellVal = [](const QString& raw) {
        return QString(raw).replace('~', ' ').toHtmlEscaped();
    };

    QString html;
    html += "<table border='1' cellspacing='0' cellpadding='4' "
            "style='border-collapse:collapse; margin:4px 0 8px 0;'>\n";

    if (vertical) {
        for (const QStringList& row : rows) {
            html += "<tr>";
            for (int ci = 0; ci < row.size(); ++ci) {
                if (ci == 0)
                    html += "<th style='background:#3C3C3C; color:#DCDCDC; text-align:left; padding:3px 6px;'>"
                            + cellVal(row[ci]) + "</th>";
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

QString ScenarioSimulatorDialog::resolveDefine(const QString& defName,
                                                QVector<DisplayDefine>& defines) const
{
    DisplayDefine* def = findDefine(defName, defines);
    if (!def)
        return "<i>(Define '" + defName.toHtmlEscaped() + "' not found)</i>";
    if (!def->isTable)
        return "<span style='color:#CE9178;'>" + def->scalarValue.toHtmlEscaped() + "</span>";
    return renderTable(def->tableRows, def->vertical, !def->vertical);
}

QString ScenarioSimulatorDialog::renderSteps(const QVector<DisplayStep>& steps,
                                              QVector<DisplayDefine>& defines) const
{
    if (steps.isEmpty())
        return "<p style='color:#808080; margin:4px 0;'><i>(none)</i></p>";

    static const QMap<QString, QString> kwColors = {
        { "given", "#4EC9B0" },
        { "when",  "#569CD6" },
        { "then",  "#C586C0" }
    };

    QString html;
    for (const DisplayStep& step : steps) {
        const QString col = kwColors.value(step.keyword.toLower(), "#DCDCDC");
        html += "<p style='margin:6px 0 2px 0;'>"
                "<b style='color:" + col + ";'>" + step.keyword.toHtmlEscaped() + "</b>"
                " " + step.text.toHtmlEscaped();
        if (!step.attrSetName.isEmpty())
            html += ": <i style='color:#9CDCFE;'>" + step.attrSetName.toHtmlEscaped() + "</i>";
        if (step.vertical)
            html += " <span style='color:#C586C0;'>(Vertical)</span>";
        html += "</p>\n";

        if (!step.defineRef.isEmpty()) {
            html += "<div style='margin-left:16px;'>"
                    "<span style='color:#CE9178;'>=</span>"
                    "<b>" + step.defineRef.toHtmlEscaped() + "</b><br/>\n"
                    + resolveDefine(step.defineRef, const_cast<QVector<DisplayDefine>&>(defines))
                    + "</div>\n";
        } else if (!step.tableRows.isEmpty()) {
            html += "<div style='margin-left:16px;'>"
                    + renderTable(step.tableRows, step.vertical, step.hasHeader)
                    + "</div>\n";
        }
    }
    return html;
}

QString ScenarioSimulatorDialog::renderSection(const QString& title,
                                                const QColor& titleColor,
                                                const QVector<DisplayStep>& steps,
                                                QVector<DisplayDefine>& defines) const
{
    QString html;
    html += "<h3 style='color:" + titleColor.name() + "; margin:12px 0 4px 0;'>"
            + title.toHtmlEscaped() + "</h3>"
            "<hr style='border-color:#3C3C3C; margin-bottom:4px;'/>";
    html += renderSteps(steps, defines);
    return html;
}

// ---------------------------------------------------------------------------
// Build HTML
// ---------------------------------------------------------------------------

QString ScenarioSimulatorDialog::buildHtml(const ParsedFile& pf, int cursorLine) const
{
    // Find the scenario the cursor is in (or nearest above)
    const ParsedScenario* target = nullptr;
    for (int i = pf.scenarios.size() - 1; i >= 0; --i) {
        if (pf.scenarios[i].startLine <= cursorLine) {
            target = &pf.scenarios[i];
            break;
        }
    }

    QString html;
    html += "<html><body style='font-family:Consolas,\"Courier New\",monospace; "
            "font-size:10pt; background:#1E1E1E; color:#DCDCDC; margin:8px;'>";

    QVector<DisplayDefine> defines = pf.defines;

    if (!pf.backgroundSteps.isEmpty())
        html += renderSection("Background", QColor("#4EC9B0"), pf.backgroundSteps, defines);

    if (target) {
        const QString scenTitle = target->keyword + ": " + target->name;
        html += renderSection(scenTitle, QColor("#569CD6"), target->steps, defines);
    } else {
        html += "<p style='color:#F48771;'><i>No Scenario found at cursor position.</i></p>";
    }

    if (!pf.cleanupSteps.isEmpty())
        html += renderSection("Cleanup", QColor("#C586C0"), pf.cleanupSteps, defines);

    html += "</body></html>";
    return html;
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

void ScenarioSimulatorDialog::refresh()
{
    const ParsedFile pf = parseFile(m_filePath);
    m_browser->setHtml(buildHtml(pf, m_cursorLine));

    if (!m_watcher->files().contains(m_filePath) && QFile::exists(m_filePath))
        m_watcher->addPath(m_filePath);
}
