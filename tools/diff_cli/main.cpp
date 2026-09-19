#include "ui/DiffView.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QTextStream out(stdout);
    if (argc < 3) { out << "usage: diff_cli OLD NEW\n"; return 2; }

    auto read = [](const char* path) {
        QFile f(QString::fromLocal8Bit(path));
        return f.open(QIODevice::ReadOnly | QIODevice::Text) ? QTextStream(&f).readAll() : QString();
    };
    const auto rows = DiffView::compare(read(argv[1]), read(argv[2]));
    for (const DiffView::Row& r : rows) {
        switch (r.kind) {
        case DiffView::Row::Same:    out << "= " << r.left << "\n"; break;
        case DiffView::Row::Removed: out << "- " << r.left << "\n"; break;
        case DiffView::Row::Added:   out << "+ " << r.right << "\n"; break;
        case DiffView::Row::Changed: out << "~ " << r.left << "  ==>  " << r.right << "\n"; break;
        }
    }
    out.flush();
    return 0;
}
