#pragma once

// How a generated test or glue file says which specification it came from.
//
// Nine languages, nine spellings of one name:
//
//   Record_Filter_Example_Test.java   Record_Filter_Example_glue.java
//   Record_Filter_Example_Tests.cs    Test_RecordFilterExample.py
//   record_filter_example_glue.py     record_filter_example_test.go
//   test_record_filter_example.rs     test_record_filter_example.test.ts
//   RecordFilterExampleTests.swift    RecordFilterExampleGlue.swift
//
// Reducing a name to its letters and digits in lower case makes all of them one
// key, so no caller has to mirror four naming functions per language. The file
// names are still listed one language at a time rather than as "*_glue.*", so a
// copied specification or an editor backup is never taken for generated output.
//
// In a header because it decides what gets deleted: it is exercised on its own
// rather than only read.

#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QVector>

// A name reduced to what every language's spelling of it has in common.
inline QString generatedKey(const QString& text)
{
    QString s = text;
    s.remove(QRegularExpression("[^A-Za-z0-9]"));
    return s.toLower();
}

// The key of the specification a generated file belongs to, or empty if the
// file is not one a generator writes.
inline QString specKeyOfGeneratedFile(const QString& fileName)
{
    // TypeScript's compiled output carries a second suffix: bowling_glue.d.ts
    // and test_bowling.test.js.map sit beside the sources in dist/.
    QString name = fileName;
    if (name.endsWith(".map"))  name.chop(4);
    if (name.endsWith(".d.ts")) name.replace(name.size() - 5, 5, ".ts");

    static const QVector<QRegularExpression> patterns = {
        // tests
        QRegularExpression(R"(^(.+)_Test\.java$)"),
        QRegularExpression(R"(^(.+)_Tests\.cs$)"),
        QRegularExpression(R"(^Test_(.+)\.py$)"),
        QRegularExpression(R"(^(.+)_test\.go$)"),
        QRegularExpression(R"(^test_(.+)\.rs$)"),
        QRegularExpression(R"(^test_(.+)\.test\.(?:js|ts)$)"),
        QRegularExpression(R"(^(.+)Tests\.swift$)"),
        QRegularExpression(R"(^test_(.+)\.cpp$)"),
        // glue
        QRegularExpression(R"(^(.+)_glue\.(?:java|cs|py|go|rs|js|ts|h|cpp)$)"),
        QRegularExpression(R"(^(.+)Glue\.swift$)"),
    };

    for (const QRegularExpression& re : patterns) {
        const QRegularExpressionMatch m = re.match(name);
        if (m.hasMatch()) return generatedKey(m.captured(1));
    }
    return {};
}
