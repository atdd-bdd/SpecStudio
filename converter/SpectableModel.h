#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// A named comment -- Description, Details, Notes, Constraint or Uses -- and
// the table that may follow it. The table is part of the comment: a
// BusinessRule is often clearer as a small grid of conditions and outcomes
// than as a sentence, and this is where such a grid goes. Nothing reads it,
// nothing is generated from it, nothing asserts it; it is kept so that the
// editor and the tools can show it, and so that the parser does not call it
// an unexpected table.
struct NamedComment {
    QString              keyword;   // as written: Description, Details, ...
    QString              text;      // the rest of the line
    QVector<QStringList> rows;      // the table under it, if any
    int                  line = 0;
};

// One field row from an Attributes or Entity table
struct Field {
    QString name;
    QString type;
    QString defaultValue;
    QString notes;
    QString inOut;
    int     line = 0;   // the table row it was read from, so a diagnostic about
                        // one field can point at that row rather than the block
};

// An Attributes or Entity declaration (defines a data class)
struct AttrSet {
    QString        name;
    QString        kind;   // "Attributes" or "Entity"
    QVector<Field> fields;
    QString        uses;   // Uses named comment -- documentation only, never executable
    QVector<NamedComment> comments;   // every named comment in the block, tables included
    int            line      = 0;
    bool           isContext = false;  // from a context file — symbols only, no class generation
    bool           imported  = false;  // merged from a file this one Imports: generated here,
                                       // but declared there, which is where a symbol table
                                       // and a diagnostic should place it
};

// A Define block (constant value, table, or docstring)
struct Define {
    QString              name;
    QString              scalarValue;  // for "Define Name = scalar"
    QVector<QStringList> tableRows;    // for table-form Define (raw cell lists)
    bool                 isTable    = false;
    bool                 vertical   = false;  // vertical key/value Define table
    QString              docString;           // for docstring-form Define
    bool                 hasDocString = false;
    int                  docStringIndent = 0; // column of the opening """, for dedenting content lines
    QString              uses;         // Uses named comment
    int                  line       = 0;
    bool                 isContext  = false;  // from a context file
    bool                 imported   = false;  // merged from an Imported file; see AttrSet
};

// A table attached to a step
struct StepTable {
    QVector<QStringList> rows;        // rows[0] is header when hasHeader=true
    QVector<int>         rowLines;    // the line each row was read from; empty when the
                                      // rows came from an Inserted file rather than this one
    bool                 hasHeader   = false;
    bool                 vertical    = false;  // explicit Vertical or | Attribute | Value | format
};

// One Given/When/Then step inside a Scenario or Background
struct Step {
    QString   keyword;      // Given / When / Then  (And/But normalized to previous)
    QString   text;         // step description before ':'
    QString   attrSetName;  // attribute set name after ':' (empty if none)
    bool      vertical     = false;
    bool      compareOnly  = false;  // CompareOnly modifier — unlisted fields filled with DNCString
    // EveryCell modifier: the table is a grid and each cell holds the text form
    // of the named type, rather than the table being one row per instance with a
    // column per attribute. Without it, naming an Entity means the second
    // reading, which is the only one that used to exist.
    bool      everyCell    = false;
    StepTable table;
    bool      hasTable   = false;
    QString   defineRef;    // "=DefineName" in place of a table
    int       defineRefLine = 0;   // the line it was written on, for a diagnostic about it
    QString   docString;    // content between opening and closing """
    bool      hasDocString = false;
    // A pipe table followed this step, but the step named no attribute set, so
    // nothing reads the table and no argument reaches the glue. Recorded rather
    // than merely warned about, so Analyze can ask the model instead of
    // re-reading the file to work out where a table began.
    bool      tableWithoutAttrSet = false;
    int       orphanTableLine     = 0;
    int       docStringIndent = 0; // column of the opening """, for dedenting content lines
    QString   uses;         // Uses named comment
    QVector<NamedComment> comments;   // named comments on the step, tables included
    int       line = 0;
};

// One Scenario block
struct Scenario {
    QString       name;
    QStringList   tags;           // @Tags — passed through as test annotations
    QStringList   generatorTags;  // $Tags — consumed by generator for filtering only
    QString       uses;           // Uses named comment
    QVector<NamedComment> comments;   // named comments between the steps, tables included
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
    QVector<int>         rowLines;      // the line each data row was read from
    int                  headerLine = 0;
    int                  line = 0;      // the Examples: line itself
};

// A named spec block (BusinessRule, Calculation, or DataType)
struct NamedBlock {
    QString      kind;          // "BusinessRule", "Calculation", or "DataType"
    QString      name;
    QStringList  tags;           // @Tags — passed through as test annotations
    QStringList  generatorTags;  // $Tags — consumed by generator for filtering only
    QString      description;    // the Description named comment, if it has one
    QVector<NamedComment> comments;   // every named comment in the block, tables included
    ExamplesBlock examples;
    bool         hasExamples = false;
    bool         isContext   = false;  // from a context file — used for isEnumType lookup only
    bool         imported    = false;  // merged from an Imported file; see AttrSet
    QString      uses;           // Uses named comment
    int          line        = 0;
};

// A Collection declaration: named list type containing instances of an Entity/Attributes type
struct Collection {
    QString name;
    QString elementType;   // the Entity/Attributes type it contains (from DataType column)
    QString minimum;
    QString maximum;
    QString notes;
    QString uses;          // Uses named comment
    int     line      = 0;
    bool    isContext = false;
};

// A DomainTerm: a word this domain uses, and the type it stands for.
//
// "DomainTerm Roll : Pins" says that a Roll is a Pins. It generates nothing of
// its own, so a field declaring the type Roll would name a class nothing writes
// -- which is why the type is resolved to its underlying one before any
// generator sees it. See resolveDomainTermTypes.
struct DomainTerm {
    QString name;
    QString type;       // the type it stands for
    int     line      = 0;
    bool    isContext = false;
};

// A ScenarioGroup heading. Nothing is generated for one; it is a name the
// editor can find and the index can list.
struct ScenarioGroup {
    QString name;
    int     line = 0;
};

// Top-level result of parsing one .spectable file
struct SpectableFile {
    QString                specName;
    int                    specLine = 0;   // the Specification line
    QString                filePath;
    QStringList            imports;        // absolute paths named by Import, in order
    QStringList            inserts;        // absolute paths named by Insert, in order
    QVector<ScenarioGroup> scenarioGroups;
    QVector<NamedComment>  comments;       // named comments belonging to no block
    QStringList            tags;           // @Tags before Specification line — applied to all blocks
    QStringList            generatorTags;  // $Tags before Specification line — applied to all blocks
    QVector<AttrSet>       attrSets;
    QVector<Collection>    collections;
    QVector<DomainTerm>    domainTerms;
    QVector<Define>        defines;
    QStringList            dataTypeNames;  // user-declared DataType names
    QVector<NamedBlock>    namedBlocks;    // BusinessRule / Calculation / DataType with Examples
    QVector<Step>          backgroundSteps;
    QVector<Step>          cleanupSteps;
    QVector<Scenario>      scenarios;
    QVector<ParseMessage>  messages;
};
