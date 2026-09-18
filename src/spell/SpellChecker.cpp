#include "SpellChecker.h"

#include "hunspell.hxx"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

SpellChecker* SpellChecker::instance()
{
    static SpellChecker* s = new SpellChecker();
    return s;
}

SpellChecker::SpellChecker() = default;
SpellChecker::~SpellChecker() = default;

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

QString SpellChecker::dictionaryFolder() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/dictionaries";
}

QString SpellChecker::userDictionaryPath() const
{
    return dictionaryFolder() + "/user words.txt";
}

// The dictionary is a resource, and Hunspell reads files; so it is written out
// once. Rewritten when the resource is newer than the copy, which is how an
// upgraded dictionary reaches an installation that already had one.
bool SpellChecker::writeOut(const QString& resource, const QString& target) const
{
    QFile in(resource);
    if (!in.open(QIODevice::ReadOnly)) return false;
    const QByteArray bytes = in.readAll();

    QFile out(target);
    if (out.exists() && out.size() == bytes.size()) return true;
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return out.write(bytes) == bytes.size();
}

void SpellChecker::ensureLoaded()
{
    if (m_loadTried) return;
    m_loadTried = true;

    const QString folder = dictionaryFolder();
    if (!QDir().mkpath(folder)) return;
    const QString aff = folder + "/en_US.aff";
    const QString dic = folder + "/en_US.dic";
    if (!writeOut(":/dictionaries/dictionaries/en_US.aff", aff)) return;
    if (!writeOut(":/dictionaries/dictionaries/en_US.dic", dic)) return;

    m_hunspell.reset(new Hunspell(QFile::encodeName(aff).constData(),
                                  QFile::encodeName(dic).constData()));
    loadUserWords();
}

bool SpellChecker::isEnabled() const
{
    return QSettings().value("Editors/spellCheck", true).toBool();
}

void SpellChecker::setEnabled(bool on)
{
    if (on == isEnabled()) return;
    QSettings().setValue("Editors/spellCheck", on);
    emit changed();
}

bool SpellChecker::isAvailable()
{
    ensureLoaded();
    return m_hunspell != nullptr;
}

// ---------------------------------------------------------------------------
// Asking
// ---------------------------------------------------------------------------

bool SpellChecker::isCorrect(const QString& word)
{
    if (!isAvailable()) return true;
    const QString lower = word.toLower();
    if (m_userWordsLower.contains(lower) || m_projectWordsLower.contains(lower)) return true;
    // The dictionary is UTF-8 (its .aff says SET UTF-8), so a QString goes
    // across as that.
    return m_hunspell->spell(word.toUtf8().toStdString());
}

QStringList SpellChecker::suggestions(const QString& word, int max)
{
    QStringList out;
    if (!isAvailable()) return out;
    for (const std::string& s : m_hunspell->suggest(word.toUtf8().toStdString())) {
        out << QString::fromUtf8(s.c_str());
        if (out.size() >= max) break;
    }
    return out;
}

bool SpellChecker::isCheckable(const QString& token)
{
    if (token.size() < 2) return false;
    bool anyLower = false;
    for (const QChar c : token) {
        if (c.isDigit()) return false;
        if (c.isLower()) anyLower = true;
    }
    return anyLower;   // all capitals is an abbreviation, not a word
}

QVector<SpellChecker::Part> SpellChecker::splitCamelCase(const QString& token)
{
    QVector<Part> parts;
    int start = 0;
    const int n = token.size();
    for (int i = 1; i <= n; ++i) {
        bool boundary = (i == n);
        if (!boundary) {
            const QChar prev = token[i - 1];
            const QChar cur  = token[i];
            // aB: a new word starts at B.
            if (prev.isLower() && cur.isUpper()) boundary = true;
            // ABc: the last capital of a run starts the next word (HTMLParser).
            else if (prev.isUpper() && cur.isUpper() && i + 1 < n && token[i + 1].isLower())
                boundary = true;
        }
        if (boundary) {
            Part p;
            p.offset = start;
            p.length = i - start;
            p.text   = token.mid(start, i - start);
            if (p.length > 0) parts.push_back(p);
            start = i;
        }
    }
    return parts;
}

// ---------------------------------------------------------------------------
// The user's own words
// ---------------------------------------------------------------------------

void SpellChecker::loadWordFile(const QString& path, QSet<QString>& words, QSet<QString>& lower)
{
    words.clear();
    lower.clear();
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString w = in.readLine().trimmed();
        if (w.isEmpty() || w.startsWith('#')) continue;
        words.insert(w);
        lower.insert(w.toLower());
        if (m_hunspell) m_hunspell->add(w.toUtf8().toStdString());
    }
}

void SpellChecker::saveWordFile(const QString& path, const QSet<QString>& words,
                                const QString& heading) const
{
    if (path.isEmpty()) return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return;
    QTextStream out(&f);
    out << "# " << heading << "\n";
    QStringList list = words.values();
    list.sort(Qt::CaseInsensitive);
    for (const QString& w : list) out << w << "\n";
}

void SpellChecker::loadUserWords()
{
    loadWordFile(userDictionaryPath(), m_userWords, m_userWordsLower);
    loadWordFile(m_projectPath, m_projectWords, m_projectWordsLower);
}

void SpellChecker::saveUserWords() const
{
    saveWordFile(userDictionaryPath(), m_userWords,
                 "Words added to the dictionary from AlignThree, one per line.");
}

void SpellChecker::addToUserDictionary(const QString& word)
{
    const QString w = word.trimmed();
    if (w.isEmpty() || m_userWords.contains(w)) return;
    ensureLoaded();
    m_userWords.insert(w);
    m_userWordsLower.insert(w.toLower());
    if (m_hunspell) m_hunspell->add(w.toUtf8().toStdString());
    saveUserWords();
    emit changed();
}

void SpellChecker::removeFromUserDictionary(const QString& word)
{
    const QString w = word.trimmed();
    QString stored;
    for (const QString& u : m_userWords)
        if (u.compare(w, Qt::CaseInsensitive) == 0) { stored = u; break; }
    if (stored.isEmpty()) return;
    m_userWords.remove(stored);
    m_userWordsLower.remove(stored.toLower());
    if (m_hunspell) m_hunspell->remove(stored.toUtf8().toStdString());
    saveUserWords();
    emit changed();
}

bool SpellChecker::isUserWord(const QString& word) const
{
    return m_userWordsLower.contains(word.trimmed().toLower());
}

// ---------------------------------------------------------------------------
// The solution's shared words
// ---------------------------------------------------------------------------

void SpellChecker::setProjectDictionaryPath(const QString& path)
{
    if (path == m_projectPath) return;
    // Words from the old solution stop being right; Hunspell forgets them
    // one by one, since it has no notion of where a word came from.
    if (m_hunspell)
        for (const QString& w : m_projectWords)
            if (!m_userWordsLower.contains(w.toLower())) m_hunspell->remove(w.toUtf8().toStdString());
    m_projectPath = path;
    if (m_loadTried) loadWordFile(m_projectPath, m_projectWords, m_projectWordsLower);
    else { m_projectWords.clear(); m_projectWordsLower.clear(); }   // read with the rest, on first use
    emit changed();
}

void SpellChecker::addToProjectDictionary(const QString& word)
{
    const QString w = word.trimmed();
    if (w.isEmpty() || m_projectPath.isEmpty() || m_projectWords.contains(w)) return;
    ensureLoaded();
    m_projectWords.insert(w);
    m_projectWordsLower.insert(w.toLower());
    if (m_hunspell) m_hunspell->add(w.toUtf8().toStdString());
    saveWordFile(m_projectPath, m_projectWords,
                 "Words this solution's specifications use, one per line. Shared with everyone who opens it.");
    emit changed();
}

void SpellChecker::removeFromProjectDictionary(const QString& word)
{
    const QString w = word.trimmed();
    QString stored;
    for (const QString& u : m_projectWords)
        if (u.compare(w, Qt::CaseInsensitive) == 0) { stored = u; break; }
    if (stored.isEmpty()) return;
    m_projectWords.remove(stored);
    m_projectWordsLower.remove(stored.toLower());
    if (m_hunspell && !m_userWordsLower.contains(stored.toLower()))
        m_hunspell->remove(stored.toUtf8().toStdString());
    saveWordFile(m_projectPath, m_projectWords,
                 "Words this solution's specifications use, one per line. Shared with everyone who opens it.");
    emit changed();
}

bool SpellChecker::isProjectWord(const QString& word) const
{
    return m_projectWordsLower.contains(word.trimmed().toLower());
}

// ---------------------------------------------------------------------------
// Names the specifications declare
// ---------------------------------------------------------------------------

void SpellChecker::setKnownNames(const QSet<QString>& lowerCased)
{
    if (lowerCased == m_knownNames) return;
    m_knownNames = lowerCased;
    emit changed();
}

bool SpellChecker::isKnownName(const QString& token) const
{
    return m_knownNames.contains(token.trimmed().toLower());
}

QStringList SpellChecker::userWords() const
{
    QStringList words = m_userWords.values();
    words.sort(Qt::CaseInsensitive);
    return words;
}
