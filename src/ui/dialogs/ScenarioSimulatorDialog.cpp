#include "ScenarioSimulatorDialog.h"

#include "../../analyzer/SpecTableIndex.h"
#include "SpectableParser.h"

#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ScenarioSimulatorDialog::ScenarioSimulatorDialog(const QString& filePath,
                                                 int cursorLine,
                                                 const SpecTableIndex* index,
                                                 QWidget* parent)
    : QDialog(parent, Qt::Tool | Qt::WindowCloseButtonHint)
    , m_filePath(filePath)
    , m_cursorLine(cursorLine)
    , m_index(index)
{
    setWindowTitle(tr("Scenario Simulator — %1").arg(QFileInfo(filePath).fileName()));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(700, 520);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenLinks(false);
    // Defines can expand into sizeable nested tables (see renderTable/resolveDefine),
    // so let the document grow to its natural width/height and scroll both ways
    // instead of squeezing wide tables to fit.
    m_browser->setLineWrapMode(QTextEdit::NoWrap);
    m_browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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
// The file, as the parser reads it
// ---------------------------------------------------------------------------
//
// Until 2026-09-18 this dialog read the specification itself, with regular
// expressions -- the same second reading Analyze once had, with the same
// consequence: a table whose orientation the parser inferred was shown the
// other way round here. Now it takes the converter's parse tree, merged with
// the project's other files when an index is at hand, so a Define declared in
// a sibling resolves and every table lies the way the generators read it.

ScenarioSimulatorDialog::ParsedFile
ScenarioSimulatorDialog::fromModel(const SpectableFile& file)
{
    ParsedFile result;

    auto toDisplay = [](const Step& st) {
        DisplayStep d;
        d.keyword     = st.keyword;
        d.text        = st.text;
        d.attrSetName = st.attrSetName;
        d.defineRef   = st.defineRef;
        d.tableRows   = st.table.rows;
        d.vertical    = st.vertical || st.table.vertical;
        d.hasHeader   = st.table.hasHeader;
        return d;
    };

    for (const Step& st : file.backgroundSteps) result.backgroundSteps << toDisplay(st);
    for (const Step& st : file.cleanupSteps)    result.cleanupSteps    << toDisplay(st);
    for (const Scenario& sc : file.scenarios) {
        ParsedScenario ps;
        ps.keyword   = "Scenario";
        ps.name      = sc.name;
        ps.startLine = sc.line;
        for (const Step& st : sc.steps) ps.steps << toDisplay(st);
        result.scenarios << ps;
    }
    for (const Define& def : file.defines) {
        DisplayDefine d;
        d.name        = def.name;
        d.scalarValue = def.hasDocString ? def.docString : def.scalarValue;
        d.tableRows   = def.tableRows;
        d.isTable     = def.isTable;
        d.vertical    = def.vertical;
        result.defines << d;
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
                                              bool vertical, bool hasHeader,
                                              QVector<DisplayDefine>& defines,
                                              const QSet<QString>& resolving) const
{
    if (rows.isEmpty()) return {};

    static QRegularExpression reCellDef(R"(^=(\w+)$)");

    // A cell whose whole value is "=DefineName" is replaced with the actual
    // value: inline text for a scalar Define, or — since the Define represents
    // a whole entity/record — a nested header+data table for a table Define.
    auto cellHtml = [&](const QString& raw) -> QString {
        auto m = reCellDef.match(raw.trimmed());
        if (m.hasMatch())
            return resolveDefine(m.captured(1), defines, resolving);
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
                    html += "<th style='background:#E8E8E8; color:#1E1E1E; text-align:left; padding:3px 6px;'>"
                            + cellHtml(row[ci]) + "</th>";
                else
                    html += "<td style='padding:3px 6px;'>" + cellHtml(row[ci]) + "</td>";
            }
            html += "</tr>\n";
        }
    } else {
        for (int ri = 0; ri < rows.size(); ++ri) {
            html += "<tr>";
            const bool isHdr = (hasHeader && ri == 0);
            for (const QString& cell : rows[ri]) {
                if (isHdr)
                    html += "<th style='background:#E8E8E8; color:#1E1E1E; padding:3px 6px;'>"
                            + cellHtml(cell) + "</th>";
                else
                    html += "<td style='padding:3px 6px;'>" + cellHtml(cell) + "</td>";
            }
            html += "</tr>\n";
        }
    }
    html += "</table>\n";
    return html;
}

QString ScenarioSimulatorDialog::resolveDefine(const QString& defName,
                                                QVector<DisplayDefine>& defines,
                                                const QSet<QString>& resolving) const
{
    if (resolving.contains(defName.toLower()))
        return "<i>(circular reference to '" + defName.toHtmlEscaped() + "')</i>";
    DisplayDefine* def = findDefine(defName, defines);
    if (!def)
        return "<i>(Define '" + defName.toHtmlEscaped() + "' not found)</i>";
    if (!def->isTable)
        return "<span style='color:#CE9178;'>" + def->scalarValue.toHtmlEscaped() + "</span>";
    QSet<QString> next = resolving;
    next.insert(defName.toLower());
    return renderTable(def->tableRows, def->vertical, !def->vertical, defines, next);
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
        const QString col = kwColors.value(step.keyword.toLower(), "#333333");
        html += "<p style='margin:6px 0 2px 0;'>"
                "<b style='color:" + col + ";'>" + step.keyword.toHtmlEscaped() + "</b>"
                " " + step.text.toHtmlEscaped();
        if (!step.attrSetName.isEmpty())
            html += ": <i style='color:#0969DA;'>" + step.attrSetName.toHtmlEscaped() + "</i>";
        if (step.vertical)
            html += " <span style='color:#C586C0;'>(Vertical)</span>";
        html += "</p>\n";

        if (!step.defineRef.isEmpty()) {
            html += "<div style='margin-left:16px;'>"
                    "<span style='color:#CE9178;'>=</span>"
                    "<b>" + step.defineRef.toHtmlEscaped() + "</b><br/>\n"
                    + resolveDefine(step.defineRef, defines)
                    + "</div>\n";
        } else if (!step.tableRows.isEmpty()) {
            html += "<div style='margin-left:16px;'>"
                    + renderTable(step.tableRows, step.vertical, step.hasHeader, defines)
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
            "<hr style='border-color:#CCCCCC; margin-bottom:4px;'/>";
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
            "font-size:10pt; background:#FFFFFF; color:#1E1E1E; margin:8px;'>";

    QVector<DisplayDefine> defines = pf.defines;

    if (!pf.backgroundSteps.isEmpty())
        html += renderSection("Background", QColor("#4EC9B0"), pf.backgroundSteps, defines);

    if (target) {
        const QString scenTitle = target->keyword + ": " + target->name;
        html += renderSection(scenTitle, QColor("#569CD6"), target->steps, defines);
    } else {
        html += "<p style='color:#C0392B;'><i>No Scenario found at cursor position.</i></p>";
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
    // With an index, the file arrives merged with its siblings and resolved,
    // as the converter sees it. Without one, its own tree, with the same
    // orientation inference applied. The file is re-read from disk either way,
    // since the dialog refreshes as the file is edited and saved.
    SpectableFile model;
    if (m_index) {
        model = m_index->fileWithContext(m_filePath);
    } else {
        model = SpectableParser().parse(m_filePath);
        resolveDomainTermTypes(model);
        resolveDefineReferences(model);
        inferTableOrientation(model);
    }
    const ParsedFile pf = fromModel(model);
    m_browser->setHtml(buildHtml(pf, m_cursorLine));

    if (!m_watcher->files().contains(m_filePath) && QFile::exists(m_filePath))
        m_watcher->addPath(m_filePath);
}
