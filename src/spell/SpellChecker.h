#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

class Hunspell;

// Spelling, for the prose a specification is mostly made of.
//
// One checker serves every editor. Hunspell answers "is this a word" and
// "what might it have been" from the en_US dictionary compiled into the
// binary; the words the user adds live in a file of their own, so they
// survive an upgrade and can be read, edited or carried to another machine.
//
// A specification's own vocabulary is camel-cased -- TotalScore, FrameValues
// -- so a token is split into its words before any is looked up, and a token
// written entirely in capitals (TBR, DNC, JSON) is taken as an abbreviation
// and never queried.
class SpellChecker : public QObject
{
    Q_OBJECT
public:
    static SpellChecker* instance();

    // Whether checking is on at all. Kept in the settings; off, nothing is
    // marked and no dictionary is loaded.
    bool isEnabled() const;
    void setEnabled(bool on);

    // True once a dictionary is loaded. Loading happens on first use and is
    // skipped silently if the dictionary cannot be written out, in which case
    // nothing is ever marked misspelled.
    bool isAvailable();

    // Whether a single word -- already split from any camel-case -- is spelled
    // correctly. Case matters to Hunspell the way it does in English: "paris"
    // is wrong, "Paris" is not, and "PARIS" is fine either way.
    bool isCorrect(const QString& word);

    // Suggestions for a misspelled word, best first, at most `max`.
    QStringList suggestions(const QString& word, int max = 8);

    // One word of a token, with where it sits inside the token.
    struct Part {
        int     offset = 0;
        int     length = 0;
        QString text;
    };
    // "TotalScore" -> Total, Score. "HTMLParser" -> HTML, Parser. "score" ->
    // score. An apostrophe stays inside its word ("don't").
    static QVector<Part> splitCamelCase(const QString& token);

    // Whether a token is worth looking up at all: two letters or more, not all
    // capitals, no digits.
    static bool isCheckable(const QString& token);

    // The user's own words, kept in userDictionaryPath(), one per line.
    void        addToUserDictionary(const QString& word);
    void        removeFromUserDictionary(const QString& word);
    bool        isUserWord(const QString& word) const;
    QStringList userWords() const;
    QString     userDictionaryPath() const;

    // A solution's shared words: "dictionary.txt" beside the .sspec, committed
    // with the specifications, so a team shares its vocabulary. Set when a
    // solution opens (empty when none is), read then, written on every change.
    void        setProjectDictionaryPath(const QString& path);
    QString     projectDictionaryPath() const { return m_projectPath; }
    void        addToProjectDictionary(const QString& word);
    void        removeFromProjectDictionary(const QString& word);
    bool        isProjectWord(const QString& word) const;

    // Names the specifications declare -- every Entity, Attributes, DataType,
    // DomainTerm, Define, Collection, BusinessRule, Calculation and attribute
    // name in the project. A declared name is spelled the way it is declared,
    // so a token that is one is never looked up. Lower-cased; set by whoever
    // rebuilds the index.
    void        setKnownNames(const QSet<QString>& lowerCased);
    bool        isKnownName(const QString& token) const;

signals:
    // A word was added or removed, or checking was switched on or off, so
    // every open editor should look again.
    void changed();

private:
    SpellChecker();
    ~SpellChecker() override;

    void    ensureLoaded();
    QString dictionaryFolder() const;
    bool    writeOut(const QString& resource, const QString& target) const;
    void    loadUserWords();
    void    saveUserWords() const;

    std::unique_ptr<Hunspell> m_hunspell;
    bool                      m_loadTried = false;
    QSet<QString>             m_userWords;      // as written
    QSet<QString>             m_userWordsLower; // for the case-insensitive question

    QString                   m_projectPath;
    QSet<QString>             m_projectWords;
    QSet<QString>             m_projectWordsLower;
    QSet<QString>             m_knownNames;

    void loadWordFile(const QString& path, QSet<QString>& words, QSet<QString>& lower);
    void saveWordFile(const QString& path, const QSet<QString>& words, const QString& heading) const;
};
