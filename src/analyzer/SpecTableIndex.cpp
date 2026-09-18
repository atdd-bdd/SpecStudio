// The project-wide symbol table, filled from the converter's own parse trees.
//
// Until 2026-09-16 this file read every specification a second time with its
// own regular expressions -- one pattern per keyword to find what was declared
// where, and three more readers (attributeRows, defineInfo,
// collectionElementType) that reopened a file and walked it again to answer a
// question about one block. It was the last place in Analyze that read the
// language differently from the generators, and it kept its own list of
// built-in types, which fell behind theirs once.
//
// Now each file is parsed once with SpectableParser, the symbols are read off
// the SpectableFile, and every question about a block is answered from the
// same structure the generators read.

#include "SpecTableIndex.h"

#include "SpectableParser.h"

#include <QFileInfo>

// ---------------------------------------------------------------------------
// Symbols declared in one parse tree
// ---------------------------------------------------------------------------
//
// A file's tree also holds what its Imports declare, so that the generators can
// write those classes here; those carry `imported` and are left out, because a
// symbol belongs to the file that declares it -- the imported file has its own
// tree and its own entry.

static void symbolsOf(const SpectableFile& file, const QString& abs, SpecTableSymbols& out)
{
    auto at = [&](int line) { return SymbolLocation{ abs, line }; };

    if (!file.specName.isEmpty())
        out.specifications.insert(file.specName, at(file.specLine));

    for (const AttrSet& as : file.attrSets) {
        if (as.isContext || as.imported) continue;
        if (as.kind.compare("Entity", Qt::CaseInsensitive) == 0)
            out.entities.insert(as.name, at(as.line));
        else
            out.attributes.insert(as.name, at(as.line));
    }
    for (const Collection& c : file.collections)
        if (!c.isContext) out.collections.insert(c.name, at(c.line));
    for (const DomainTerm& dt : file.domainTerms)
        if (!dt.isContext) out.domainTerms.insert(dt.name, at(dt.line));
    for (const Define& d : file.defines)
        if (!d.isContext && !d.imported) out.defines.insert(d.name, at(d.line));

    for (const NamedBlock& nb : file.namedBlocks) {
        if (nb.isContext || nb.imported) continue;
        if (nb.kind.compare("DataType", Qt::CaseInsensitive) == 0)
            out.dataTypes.insert(nb.name, at(nb.line));
        else if (nb.kind.compare("BusinessRule", Qt::CaseInsensitive) == 0)
            out.businessRules.insert(nb.name, at(nb.line));
        else if (nb.kind.compare("Calculation", Qt::CaseInsensitive) == 0)
            out.calculations.insert(nb.name, at(nb.line));
    }

    for (const Scenario& s : file.scenarios)
        out.scenarios.insert(s.name.trimmed(), at(s.line));
    for (const ScenarioGroup& g : file.scenarioGroups)
        out.scenarioGroups.insert(g.name, at(g.line));
}

static void mergeSymbols(SpecTableSymbols& into, const SpecTableSymbols& from)
{
    for (auto it = from.entities.cbegin();       it != from.entities.cend();       ++it) into.entities.insert(it.key(), it.value());
    for (auto it = from.domainTerms.cbegin();    it != from.domainTerms.cend();    ++it) into.domainTerms.insert(it.key(), it.value());
    for (auto it = from.dataTypes.cbegin();      it != from.dataTypes.cend();      ++it) into.dataTypes.insert(it.key(), it.value());
    for (auto it = from.attributes.cbegin();     it != from.attributes.cend();     ++it) into.attributes.insert(it.key(), it.value());
    for (auto it = from.collections.cbegin();    it != from.collections.cend();    ++it) into.collections.insert(it.key(), it.value());
    for (auto it = from.businessRules.cbegin();  it != from.businessRules.cend();  ++it) into.businessRules.insert(it.key(), it.value());
    for (auto it = from.calculations.cbegin();   it != from.calculations.cend();   ++it) into.calculations.insert(it.key(), it.value());
    for (auto it = from.scenarios.cbegin();      it != from.scenarios.cend();      ++it) into.scenarios.insert(it.key(), it.value());
    for (auto it = from.scenarioGroups.cbegin(); it != from.scenarioGroups.cend(); ++it) into.scenarioGroups.insert(it.key(), it.value());
    for (auto it = from.specifications.cbegin(); it != from.specifications.cend(); ++it) into.specifications.insert(it.key(), it.value());
    for (auto it = from.defines.cbegin();        it != from.defines.cend();        ++it) into.defines.insert(it.key(), it.value());
}

// ---------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------

void SpecTableIndex::rebuildProject(const QStringList& specTableFiles,
                                    const QStringList& externalFiles)
{
    m_fileSymbols.clear();
    m_project = {};
    m_externalFilePaths.clear();
    m_parsed.clear();

    SpectableParser parser;

    // Each file once, then whatever they Import that was not already listed:
    // a specification imported from outside the project still declares names
    // this project uses, and its symbols were always visible project-wide.
    QStringList toParse;
    for (const QString& f : externalFiles) {
        const QString abs = QFileInfo(f).absoluteFilePath();
        m_externalFilePaths.insert(abs);
        toParse << abs;
    }
    for (const QString& f : specTableFiles)
        toParse << QFileInfo(f).absoluteFilePath();

    for (int i = 0; i < toParse.size(); ++i) {
        const QString abs = toParse[i];
        if (m_parsed.contains(abs) || !QFileInfo::exists(abs)) continue;
        m_parsed.insert(abs, parser.parse(abs));
        for (const QString& imp : m_parsed.value(abs).imports)
            if (!m_parsed.contains(imp) && !toParse.contains(imp)) toParse << imp;
    }

    for (auto it = m_parsed.cbegin(); it != m_parsed.cend(); ++it) {
        SpecTableSymbols& sym = m_fileSymbols[it.key()];
        symbolsOf(it.value(), it.key(), sym);
        mergeSymbols(m_project, sym);
    }
}

SpecTableSymbols SpecTableIndex::buildFor(const QString& filePath) const
{
    // The file's own declarations and, as the old reader also gave, those of
    // every file it Imports -- the tree already holds them.
    const QString abs = QFileInfo(filePath).absoluteFilePath();
    const SpectableFile file = m_parsed.contains(abs) ? m_parsed.value(abs)
                                                      : SpectableParser().parse(abs);
    SpecTableSymbols result;
    symbolsOf(file, abs, result);
    for (const QString& imp : file.imports) {
        if (m_parsed.contains(imp)) {
            SpecTableSymbols s;
            symbolsOf(m_parsed.value(imp), imp, s);
            mergeSymbols(result, s);
        } else if (QFileInfo::exists(imp)) {
            SpecTableSymbols s;
            symbolsOf(SpectableParser().parse(imp), imp, s);
            mergeSymbols(result, s);
        }
    }
    return result;
}

SpecTableSymbols SpecTableIndex::symbolsForFile(const QString& filePath) const
{
    return m_fileSymbols.value(QFileInfo(filePath).absoluteFilePath());
}

const SpectableFile* SpecTableIndex::parsedFile(const QString& filePath) const
{
    const auto it = m_parsed.constFind(QFileInfo(filePath).absoluteFilePath());
    return it == m_parsed.cend() ? nullptr : &it.value();
}

QSet<QString> SpecTableIndex::declaredNames() const
{
    QSet<QString> names;
    for (const auto* m : { &m_project.entities, &m_project.domainTerms, &m_project.dataTypes,
                            &m_project.attributes, &m_project.collections, &m_project.businessRules,
                            &m_project.calculations, &m_project.defines })
        for (auto it = m->cbegin(); it != m->cend(); ++it)
            names.insert(it.key().toLower());
    for (auto it = m_parsed.cbegin(); it != m_parsed.cend(); ++it)
        for (const AttrSet& as : it.value().attrSets)
            for (const Field& f : as.fields)
                if (!f.name.isEmpty()) names.insert(f.name.toLower());
    return names;
}

SpectableFile SpecTableIndex::fileWithContext(const QString& filePath) const
{
    const QString abs = QFileInfo(filePath).absoluteFilePath();
    SpectableFile file = m_parsed.contains(abs) ? m_parsed.value(abs)
                                                : SpectableParser().parse(filePath);
    for (auto it = m_parsed.cbegin(); it != m_parsed.cend(); ++it)
        if (it.key() != abs) mergeContext(file, it.value());
    // The same resolution the converter does, in the same order.
    resolveDomainTermTypes(file);
    resolveDefineReferences(file);
    inferTableOrientation(file);
    return file;
}

bool SpecTableIndex::isExternalFile(const QString& absFilePath) const
{
    return m_externalFilePaths.contains(QFileInfo(absFilePath).absoluteFilePath());
}

QStringList SpecTableIndex::importsFor(const QString& filePath) const
{
    const SpectableFile* f = parsedFile(filePath);
    return f ? f->imports : QStringList();
}

QStringList SpecTableIndex::insertsFor(const QString& filePath) const
{
    const SpectableFile* f = parsedFile(filePath);
    return f ? f->inserts : QStringList();
}

// ---------------------------------------------------------------------------
// Questions about one block, answered from its tree
// ---------------------------------------------------------------------------

// The tree that declares the symbol, and the declaration in it. A name
// declared in two files resolves to whichever the project map recorded, which
// is the same choice the old reader made.
template <typename T, typename Pred>
static const T* findIn(const QMap<QString, SpectableFile>& parsed,
                       const SymbolLocation& where, const QVector<T>& (*vec)(const SpectableFile&),
                       Pred pred)
{
    const auto it = parsed.constFind(where.filePath);
    if (it == parsed.cend()) return nullptr;
    for (const T& item : vec(it.value()))
        if (pred(item)) return &item;
    return nullptr;
}

QVector<QStringList> SpecTableIndex::attributeRows(const QString& name) const
{
    SymbolLocation where = m_project.attributes.value(name);
    if (where.filePath.isEmpty()) where = m_project.entities.value(name);
    if (where.filePath.isEmpty()) return {};

    const AttrSet* as = findIn<AttrSet>(m_parsed, where,
        [](const SpectableFile& f) -> const QVector<AttrSet>& { return f.attrSets; },
        [&](const AttrSet& a) {
            return !a.imported && !a.isContext && a.name.compare(name, Qt::CaseInsensitive) == 0;
        });
    if (!as) return {};

    // The table as the parser read it: one row per field, in the columns the
    // language defines. An In-Out column appears only when a field uses it.
    bool anyInOut = false;
    for (const Field& f : as->fields) if (!f.inOut.isEmpty()) anyInOut = true;

    QVector<QStringList> rows;
    QStringList header{ "Name", "DataType", "Default", "Notes" };
    if (anyInOut) header << "In-Out";
    rows << header;
    for (const Field& f : as->fields) {
        QStringList row{ f.name, f.type, f.defaultValue, f.notes };
        if (anyInOut) row << f.inOut;
        rows << row;
    }
    return rows;
}

QString SpecTableIndex::collectionElementType(const QString& name) const
{
    const SymbolLocation where = m_project.collections.value(name);
    if (where.filePath.isEmpty()) return {};

    const Collection* c = findIn<Collection>(m_parsed, where,
        [](const SpectableFile& f) -> const QVector<Collection>& { return f.collections; },
        [&](const Collection& col) {
            return !col.isContext && col.name.compare(name, Qt::CaseInsensitive) == 0;
        });
    return c ? c->elementType : QString();
}

QPair<QString, QVector<QStringList>> SpecTableIndex::defineInfo(const QString& name) const
{
    const SymbolLocation where = m_project.defines.value(name);
    if (where.filePath.isEmpty()) return {};

    const Define* d = findIn<Define>(m_parsed, where,
        [](const SpectableFile& f) -> const QVector<Define>& { return f.defines; },
        [&](const Define& def) {
            return !def.imported && !def.isContext && def.name.compare(name, Qt::CaseInsensitive) == 0;
        });
    if (!d) return {};
    if (d->isTable)       return { {}, d->tableRows };
    if (d->hasDocString)  return { d->docString, {} };
    return { d->scalarValue, {} };
}

QMap<QString, QString> SpecTableIndex::domainTermTypes() const
{
    QMap<QString, QString> result;
    for (auto it = m_parsed.cbegin(); it != m_parsed.cend(); ++it)
        for (const DomainTerm& dt : it.value().domainTerms)
            if (!dt.isContext && !dt.type.isEmpty()) result.insert(dt.name, dt.type);
    return result;
}

// ---------------------------------------------------------------------------
// Names declared more than once
// ---------------------------------------------------------------------------

QMap<QString, QVector<SymbolLocation>> SpecTableIndex::duplicatesOfKind(SymbolKind kind) const
{
    QMap<QString, QVector<SymbolLocation>> all;
    for (auto fit = m_fileSymbols.cbegin(); fit != m_fileSymbols.cend(); ++fit) {
        const SpecTableSymbols& s = fit.value();
        const QMap<QString, SymbolLocation>* from = nullptr;
        switch (kind) {
        case SymbolKind::Entity:     from = &s.entities;    break;
        case SymbolKind::Attributes: from = &s.attributes;  break;
        case SymbolKind::DataType:   from = &s.dataTypes;   break;
        case SymbolKind::Collection: from = &s.collections; break;
        }
        for (auto it = from->cbegin(); it != from->cend(); ++it)
            all[it.key()].append(it.value());
    }

    QMap<QString, QVector<SymbolLocation>> dupes;
    for (auto it = all.cbegin(); it != all.cend(); ++it)
        if (it.value().size() > 1)
            dupes.insert(it.key(), it.value());
    return dupes;
}

QMap<QString, QVector<SymbolLocation>> SpecTableIndex::duplicateDomainTerms() const
{
    QMap<QString, QVector<SymbolLocation>> all;
    for (auto fit = m_fileSymbols.cbegin(); fit != m_fileSymbols.cend(); ++fit)
        for (auto it = fit.value().domainTerms.cbegin(); it != fit.value().domainTerms.cend(); ++it)
            all[it.key()].append(it.value());

    QMap<QString, QVector<SymbolLocation>> dupes;
    for (auto it = all.cbegin(); it != all.cend(); ++it)
        if (it.value().size() > 1)
            dupes.insert(it.key(), it.value());
    return dupes;
}
