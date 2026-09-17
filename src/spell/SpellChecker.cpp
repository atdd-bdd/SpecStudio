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
    if (m_userWordsLower.contains(word.toLower())) return true;
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

void SpellChecker::loadUserWords()
{
    m_userWords.clear();
    m_userWordsLower.clear();
    QFile f(userDictionaryPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString w = in.readLine().trimmed();
        if (w.isEmpty() || w.startsWith('#')) continue;
        m_userWords.insert(w);
        m_userWordsLower.insert(w.toLower());
        if (m_hunspell) m_hunspell->add(w.toUtf8().toStdString());
    }
}

void SpellChecker::saveUserWords() const
{
    QDir().mkpath(dictionaryFolder());
    QFile f(userDictionaryPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return;
    QTextStream out(&f);
    out << "# Words added to the dictionary from AlignThree, one per line.\n";
    QStringList words = m_userWords.values();
    words.sort(Qt::CaseInsensitive);
    for (const QString& w : words) out << w << "\n";
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

QStringList SpellChecker::userWords() const
{
    QStringList words = m_userWords.values();
    words.sort(Qt::CaseInsensitive);
    return words;
}
