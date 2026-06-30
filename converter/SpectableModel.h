#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// One field row from an Attributes or Entity table
struct Field {
    QString name;
    QString type;
    QString defaultValue;
    QString notes;
    QString inOut;
};

// An Attributes or Entity declaration (defines a data class)
struct AttrSet {
    QString        name;
    QString        kind;   // "Attributes" or "Entity"
    QVector<Field> fields;
    int            line      = 0;
    bool           isContext = false;  // from a context file — symbols only, no class generation
};

// A Define block (constant value or table)
struct Define {
    QString              name;
    QString              scalarValue;  // for "Define Name = scalar"
    QVector<QStringList> tableRows;    // for table-form Define (raw cell lists)
    bool                 isTable    = false;
    bool                 transposed = false;  // vertical key/value Define table
    int                  line       = 0;
    bool                 isContext  = false;  // from a context file
};

// A table attached to a step
struct StepTable {
    QVector<QStringList> rows;        // rows[0] is header when hasHeader=true
    bool                 hasHeader   = false;
    bool                 transposed  = false;  // explicit Transposed or | Attribute | Value | format
};

// One Given/When/Then step inside a Scenario or Background
struct Step {
    QString   keyword;      // Given / When / Then  (And/But normalized to previous)
    QString   text;         // step description before ':'
    QString   attrSetName;  // attribute set name after ':' (empty if none)
    bool      transposed = false;
    StepTable table;
    bool      hasTable   = false;
    QString   defineRef;    // "=DefineName" in place of a table
    int       line = 0;
};

// One Scenario block
struct Scenario {
    QString       name;
    QStringList   tags;
    QVector<Step> steps;
    int           line = 0;
};

// A parse-time diagnostic
struct ParseMessage {
    int     line    = 0;
    QString text;
    bool    warning = false;
};

// Examples table attached to a BusinessRule / Calculation / DataType block
struct ExamplesBlock {
    QString              attrSetName;   // optional — from "Examples: AttrSetName"
    QStringList          header;        // first pipe row (column headers)
    QVector<QStringList> rows;          // data rows (header excluded)
    int                  line = 0;
};

// A named spec block (BusinessRule, Calculation, or DataType)
struct NamedBlock {
    QString      kind;          // "BusinessRule", "Calculation", or "DataType"
    QString      name;
    QStringList  tags;
    ExamplesBlock examples;
    bool         hasExamples = false;
    int          line        = 0;
};

// Top-level result of parsing one .spectable file
struct SpectableFile {
    QString                specName;
    QString                filePath;
    QVector<AttrSet>       attrSets;
    QVector<Define>        defines;
    QStringList            dataTypeNames;  // user-declared DataType names
    QVector<NamedBlock>    namedBlocks;    // BusinessRule / Calculation / DataType with Examples
    QVector<Step>          backgroundSteps;
    QVector<Step>          cleanupSteps;
    QVector<Scenario>      scenarios;
    QVector<ParseMessage>  messages;
};
