// Analyze: what is wrong with a specification, reported while it is open.
//
// Everything here reads the index -- the project-wide symbol table -- because
// these four checks are about names declared in more than one file. Every
// check about the contents of one file reads the converter's own parse tree
// instead, and lives in SpecTableModelChecks.cpp. Until 2026-09-16 this file
// also held ten checks that reopened the file and matched line patterns, a
// second reading of the language that disagreed with the generators three
// times in one month.

#include "SpecTableAnalyzer.h"

#include <QDir>
#include <QFileInfo>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

Diagnostic SpecTableAnalyzer::makeDiag(const QString& filePath, int line,
                                        const QString& msg, Diagnostic::Severity sev)
{
    Diagnostic d;
    d.filePath = filePath;
    d.line     = line;
    d.column   = 1;
    d.message  = msg;
    d.severity = sev;
    return d;
}

// ---------------------------------------------------------------------------
// SpecTableAnalyzer
// ---------------------------------------------------------------------------

SpecTableAnalyzer::SpecTableAnalyzer(SpecTableIndex* index)
    : m_index(index)
{}

QList<Diagnostic> SpecTableAnalyzer::analyzeFile(const QString& filePath) const
{
    QList<Diagnostic> diags;
    if (!m_index) return diags;

    // All files in the same project share full visibility.
    // Import is only needed for files outside the project.
    const SpecTableSymbols& visible = m_index->projectSymbols();

    // The file's own contents, read from the parse tree.
    runModelChecks                  (filePath, diags);

    // Names declared in more than one file, read from the index.
    checkDomainTermDuplicates       (filePath, diags);
    checkDomainTermVsDataTypeNames  (filePath, visible, diags);
    checkCollectionElementTypes     (filePath, visible, diags);
    checkDuplicateDeclarations      (filePath, diags);

    return diags;
}

// ---------------------------------------------------------------------------
// Check — DomainTerm name declared in more than one file
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDomainTermDuplicates(const QString& filePath,
                                                   QList<Diagnostic>& out) const
{
    const auto dupes = m_index->duplicateDomainTerms();
    for (auto it = dupes.cbegin(); it != dupes.cend(); ++it) {
        for (const SymbolLocation& loc : it.value()) {
            if (loc.filePath != filePath) continue;

            QStringList others;
            for (const SymbolLocation& other : it.value()) {
                if (other.filePath == filePath) continue;
                others << QFileInfo(other.filePath).fileName()
                          + ":" + QString::number(other.line);
            }
            out.append(makeDiag(filePath, loc.line,
                QString("DomainTerm '%1' already declared in %2")
                    .arg(it.key(), others.join(", ")),
                Diagnostic::Severity::Warning));
        }
    }
}

// ---------------------------------------------------------------------------
// Check — DomainTerm name must not collide with a built-in DataType name
// ---------------------------------------------------------------------------



// ---------------------------------------------------------------------------
// Check — DomainTerm name must not collide with a built-in DataType name
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDomainTermVsDataTypeNames(
    const QString& filePath,
    const SpecTableSymbols& visible,
    QList<Diagnostic>& out) const
{
    for (auto it = visible.domainTerms.cbegin(); it != visible.domainTerms.cend(); ++it) {
        if (it.value().filePath != filePath) continue;  // report once, at the DomainTerm's own file

        for (const QString& builtin : k_builtinDataTypes) {
            if (it.key().compare(builtin, Qt::CaseInsensitive) == 0) {
                out.append(makeDiag(filePath, it.value().line,
                    QString("DomainTerm '%1' has the same name as the built-in DataType '%2' — "
                            "DomainTerm and DataType names must be distinct")
                        .arg(it.key(), builtin),
                    Diagnostic::Severity::Warning));
                break;
            }
        }

        // A DomainTerm sharing a name with a declared DataType is reported by
        // checkNameDeclaredAsTwoKinds, as an error at both declarations; only
        // the built-ins, which have no declaration to point at, are this
        // check's to report.
    }
}

// ---------------------------------------------------------------------------
// Check — a Collection's element type must be an Entity
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkCollectionElementTypes(const QString& filePath,
                                                     const SpecTableSymbols& visible,
                                                     QList<Diagnostic>& out) const
{
    if (!m_index) return;

    for (auto it = visible.collections.cbegin(); it != visible.collections.cend(); ++it) {
        if (it.value().filePath != filePath) continue;   // report at its own declaration

        const QString element = m_index->collectionElementType(it.key());
        if (element.isEmpty()) continue;                 // no DataType column to read

        if (visible.entities.contains(element)) continue;            // the good case
        if (k_builtinDataTypes.contains(element)) continue;          // a list of a built-in

        if (visible.attributes.contains(element)) {
            out.append(makeDiag(filePath, it.value().line,
                QStringLiteral("Collection '%1' holds '%2', which is declared with Attributes. "
                               "A Collection's type must be an Entity -- a production class is "
                               "written for an Entity and not for an Attributes block, so this "
                               "generates code referring to a type nothing writes.")
                    .arg(it.key(), element)));
            continue;
        }

        if (!visible.hasDataType(element) && !visible.hasAttributeSet(element)
            && !visible.domainTerms.contains(element))
            out.append(makeDiag(filePath, it.value().line,
                QStringLiteral("Collection '%1' holds unknown type '%2'")
                    .arg(it.key(), element)));
    }
}

// ---------------------------------------------------------------------------
// Check — a name declared in more than one file
// ---------------------------------------------------------------------------



// ---------------------------------------------------------------------------
// Check — a name declared in more than one file
// ---------------------------------------------------------------------------

void SpecTableAnalyzer::checkDuplicateDeclarations(const QString& filePath,
                                                    QList<Diagnostic>& out) const
{
    if (!m_index) return;

    struct KindName { SpecTableIndex::SymbolKind kind; const char* label; };
    static const KindName kinds[] = {
        { SpecTableIndex::SymbolKind::Entity,     "Entity"     },
        { SpecTableIndex::SymbolKind::Attributes, "Attributes" },
        { SpecTableIndex::SymbolKind::DataType,   "DataType"   },
        { SpecTableIndex::SymbolKind::Collection, "Collection" },
    };

    for (const KindName& k : kinds) {
        const auto dupes = m_index->duplicatesOfKind(k.kind);
        for (auto it = dupes.cbegin(); it != dupes.cend(); ++it) {
            for (const SymbolLocation& loc : it.value()) {
                if (loc.filePath != filePath) continue;

                QStringList others;
                for (const SymbolLocation& other : it.value()) {
                    if (other.filePath == filePath && other.line == loc.line) continue;
                    // A bare file name is no help when the other declaration is in
                    // a file of the same name in another folder -- the message then
                    // appears to point at the line you are already looking at. Show
                    // the containing folder too when the names match.
                    const QFileInfo of(other.filePath);
                    const QString shown =
                        (of.fileName() == QFileInfo(filePath).fileName())
                            ? of.dir().dirName() + "/" + of.fileName()
                            : of.fileName();
                    others << shown + ":" + QString::number(other.line);
                }
                if (others.isEmpty()) continue;

                out.append(makeDiag(filePath, loc.line,
                    QStringLiteral("%1 '%2' is also declared in %3 -- only the first declaration "
                                   "is generated, so the others are never tested")
                        .arg(QString::fromLatin1(k.label), it.key(), others.join(", ")),
                    Diagnostic::Severity::Warning));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Check — an Examples: table against the AttributeSet it names
// ---------------------------------------------------------------------------
