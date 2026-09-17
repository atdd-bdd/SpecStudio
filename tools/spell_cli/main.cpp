#include "spell/SpellChecker.h"

#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("SpecStudio");
    app.setOrganizationName("SpecStudio");
    QTextStream out(stdout);

    SpellChecker* checker = SpellChecker::instance();
    if (!checker->isAvailable()) {
        out << "no dictionary\n";
        return 2;
    }
    out << "user dictionary: " << checker->userDictionaryPath() << "\n";

    int wrong = 0;
    for (int i = 1; i < argc; ++i) {
        const QString token = QString::fromLocal8Bit(argv[i]);
        // --add WORD / --remove WORD change the user dictionary, as the
        // editor's context menu does, and the words after them see the change.
        if (token == "--add" && i + 1 < argc) {
            checker->addToUserDictionary(QString::fromLocal8Bit(argv[++i]));
            out << "added " << QString::fromLocal8Bit(argv[i]) << "\n";
            continue;
        }
        if (token == "--remove" && i + 1 < argc) {
            checker->removeFromUserDictionary(QString::fromLocal8Bit(argv[++i]));
            out << "removed " << QString::fromLocal8Bit(argv[i]) << "\n";
            continue;
        }
        if (!SpellChecker::isCheckable(token)) {
            out << token << ": not checked\n";
            continue;
        }
        for (const SpellChecker::Part& p : SpellChecker::splitCamelCase(token)) {
            if (!SpellChecker::isCheckable(p.text)) { out << "  " << p.text << ": not checked\n"; continue; }
            if (checker->isCorrect(p.text)) {
                out << "  " << p.text << ": ok\n";
            } else {
                ++wrong;
                out << "  " << p.text << ": WRONG -> " << checker->suggestions(p.text).join(", ") << "\n";
            }
        }
    }
    out.flush();
    return wrong;
}
