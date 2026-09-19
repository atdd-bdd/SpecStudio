#include "editors/CursorModel.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    if (argc < 2) { out << "usage: cursor_cli FILE [LINE ...]\n"; return 2; }

    QFile f(QString::fromLocal8Bit(argv[1]));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { out << "cannot open\n"; return 2; }
    const QString text = QTextStream(&f).readAll();
    const QStringList lines = text.split('\n');

    CursorModel cm;
    cm.update(text, QString::fromLocal8Bit(argv[1]), 1);

    QList<int> wanted;
    for (int i = 2; i < argc; ++i) wanted << QString::fromLocal8Bit(argv[i]).toInt();
    if (wanted.isEmpty()) for (int l = 1; l <= lines.size(); ++l) wanted << l;

    for (int line : wanted) {
        QStringList what;
        if (cm.isBlockStart(line)) what << "block-start";
        if (const Step* st = cm.stepAt(line))
            what << QString("step[%1|%2|%3%4%5]").arg(st->text, st->attrSetName,
                        st->vertical ? "V" : "", st->compareOnly ? "C" : "", st->everyCell ? "E" : "");
        else if (const Step* st = cm.stepOwning(line))
            what << QString("in-step[%1]").arg(st->text);
        if (const AttrSet* as = cm.attrSetAt(line)) what << QString("attrset[%1]").arg(as->name);
        else if (const AttrSet* as = cm.attrSetOwning(line)) what << QString("in-attrset[%1]").arg(as->name);
        if (cm.collectionAt(line)) what << "collection";
        const NamedBlock* nb = nullptr;
        if (cm.isExamplesLine(line, &nb)) what << QString("examples[%1]").arg(nb->examples.attrSetName);
        if (const NamedBlock* b = cm.namedBlockOwning(line)) what << QString("in-block[%1 %2]").arg(b->kind, b->name);
        if (const NamedComment* c = cm.namedCommentAt(line)) what << QString("comment[%1]").arg(c->keyword);
        if (cm.isImportLine(line)) what << "import";
        if (cm.isInsertLine(line)) what << "insert";
        if (cm.isTableRow(line))   what << "row";
        out << line << ": " << what.join(' ') << "    | "
            << (line - 1 < lines.size() ? lines[line - 1].trimmed().left(50) : QString()) << "\n";
    }
    out.flush();
    return 0;
}
