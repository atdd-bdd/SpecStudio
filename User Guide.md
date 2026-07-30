# SpecStudio™ User Guide

SpecStudio is an IDE for writing specifications as tables and turning them into
runnable tests. You write a `.spectable` file describing what the software
should do; SpecStudio generates the test code and the glue that connects those
tests to your production classes, in any of nine languages.

The specification is the source of truth. The generated tests are disposable —
regenerate them whenever the specification changes.

**Contents**

1. [Getting started](#getting-started)
2. [The workspace](#the-workspace)
3. [Writing a specification](#writing-a-specification)
4. [Working with tables](#working-with-tables)
5. [Seeing what you wrote](#seeing-what-you-wrote)
6. [Navigating and renaming](#navigating-and-renaming)
7. [Configuration](#configuration)
8. [Generating code](#generating-code)
9. [What gets generated](#what-gets-generated)
10. [Analyze](#analyze)
11. [Sharing work](#sharing-work)
12. [Settings](#settings)
13. [Keyboard reference](#keyboard-reference)

---

## Getting started

### Create a solution

**File → New Solution...**

A *solution* is a container for related projects. It is stored as a `.sspec`
JSON file in the solution folder.

You will be asked how the team will share the work: a **shared file system** or
**GitHub**. That choice changes which Git commands appear later, so answer it
for how your team actually works. You get the same question if you begin with
**New Project...** and create the solution on the fly — it is not skippable.

### Create a project

**File → New Project...**

A *project* is one folder of specifications with its own Git repository —
`git init` runs when it is created. A solution can hold several.

Every new project starts with a `Java.specconfig` describing how to generate
code: Java, JUnit, tests into `src/test/java/spectable`, production stubs into
`src/main/java/production`. If you work in another language, edit that file or
add a second one beside it; see [Configuration](#configuration).

### Add a specification

**File → New File...** (`Ctrl+N`), or right-click a project in the Solution
Explorer → **New File...**

Give it a `.spectable` extension to get the specification editor.

### Open existing work

- **File → Open Solution/Project...**
- **File → Recent Solutions**
- **File → Clone an Existing Solution...** — clone a team repository and open it

---

## The workspace

The layout follows Visual Studio. Every panel is on the **View** menu, so
anything you close can be brought back.

| Panel | What it shows |
|---|---|
| **Solution Explorer** | Projects and their files |
| **Symbol Tree** | Entities, Collections, DataTypes and Attributes in the current file |
| **Attribute Inspector** | Fields of the attribute set under the cursor |
| **Output** | Build output, analysis results, and search results, in tabs |
| Editor tabs | Open files |

**Solution Explorer ordering** is deliberate: `.spectable` files first, then
`.md`, then everything else, with `.specconfig` files last. What you edit most
sits at the top.

By default the Explorer hides generated output — anything inside a config's
`outputDirectory`. **View → Show All Files** reveals it.

Other view controls:

- **View → Refresh** (`F5`) — re-scan the solution after changing files outside SpecStudio
- **View → Split Editor Right** (`Ctrl+\`) — two files side by side; `Ctrl+Shift+\` closes it

---

## Writing a specification

A `.spectable` file is plain text. Blocks start at column one with a keyword;
tables are pipe-delimited rows; `#` starts a comment.

### Specification

The file header:

```
Specification Shopping Cart
```

### Attributes — a named set of fields

```
Attributes Adder
| Name    | Default | Datatype |
| number1 | 0       | Integer  |
| number2 | 0       | Integer  |
| result  | 0       | Integer  |
```

### Entity — a domain object

```
Entity CatalogItem
| Attribute | Type       | Default | Notes |
| Name      | SimpleText | NoName  |       |
| Price     | Dollar     | 1       |       |
```

### Collection — many of something

```
Collection Catalog
| DataType    | Minimum | Maximum  | Notes          |
| CatalogItem | 0       | 10000000 | Each is unique |
```

### DataType — a value with its own validity rules

```
DataType StandardID
Description  Used to identify accounts and other items
Details Must be three digits, dash, three digits
Examples: ValidValues
| Value   | IsValid | Notes |
| 123-456 | Yes     |       |
| 123-45  | No      |       |
```

`ValidValues` and `EnumerationValues` are built-in attribute sets for exactly
this purpose.

Built-in types: `Character`, `String`, `Text`, `Integer`, `Float`,
`Scientific`, `Decimal`, `Boolean`, `Date`, `Time`, `DateTime`, `Duration`,
`YesNo`. Anything else must be a `DataType`, `Entity` or `Collection` you
declared. (`Float` and `Decimal` are accepted but not yet syntax-coloured —
cosmetic only.)

### Define — a named example row

```
Define ABillingAddress =
| Street       | City      | State | ZIP   |
| 1 Apple Lane | Somewhere | NC    | 27705 |
```

Refer to it later with `=ABillingAddress`. Defines keep long tables out of
scenarios and give recurring examples a name worth reading.

### Scenario — behaviour, as Given / When / Then

```
Scenario Add items
Given item collection is : OrderItemCollection
| Name | Price | Quantity | ItemTotal |
When item added : OrderItem Vertical
| Name     | Widget |
| Quantity | 2      |
Then item collection is : OrderItemCollection
| Name   | Price  | Quantity | ItemTotal |
| Widget | $10.00 | 2        | $20.00    |
```

Step keywords: `Given`, `When`, `Then`, `And`, `WhenThen`.

The text after `:` names the attribute set describing the table's shape.
`Vertical` transposes a table — fields down the left, one value column — which
reads better when a table has one row and many columns. `CompareOnly` limits
equality to the columns actually shown.

### Docstrings — a block of text as a step's argument

Some steps take a paragraph rather than a table. Put `"""` on its own line
directly under the step, and again to close:

```
Scenario Define with a String
Given this string
"""
This is
a multiline
string
"""
Then should be equal to string
=EQUAL_STRING
```

The step's glue method receives the whole block as one string, so this is how
you hand a step an email body, a JSON payload, a rendered report.

Three rules the parser enforces:

- The step must be **bare** — no `: AttributeSet` and no table under it. A step
  cannot take both a table and a docstring.
- `"""` must be alone on its line. The opening `"""` sets the indentation:
  that much leading whitespace is stripped from every line inside, so you can
  indent the block to match the surrounding text without it appearing in the
  value.
- Only inside `Scenario`, `Background` or `Cleanup`.

A `Define` can hold one too, which is how the same text is reused as an
expectation:

```
Define EQUAL_STRING =
"""
This is
a multiline
string
"""
```

**Edit String...** (`Ctrl+Shift+Q`) opens the block in a proper text box rather
than making you edit it inside the specification.

### BusinessRule and Calculation — a rule stated by example

```
BusinessRule Total Cart Price
Description How to Apply Discount and Shipping
Examples: CartInput
| TotalItems | Shipping | Discount | Total Price | Notes                              |
| $110       | $5       | $11      | $104        | Discount applied before shipping   |
| $80        | $5       | $4       | $81         |                                    |
```

Each `Examples:` row becomes a test case. `Calculation` behaves the same way
and reads better for pure arithmetic. A step can invoke one with
`applying <RuleName>`.

### Background and Cleanup

`Background` runs before every scenario in the file; `Cleanup` runs after.

### Other blocks

| Keyword | Purpose |
|---|---|
| `ScenarioGroup` | Group related scenarios |
| `DomainTerm` | Define vocabulary |
| `Import` | Make another `.spectable`'s `Attributes` and `Define` blocks visible |
| `Insert` | Splice another file's contents in — see below |

`Description`, `Details`, `Constraint` and `Uses` are named comments — prose
that travels with the block and is carried into the generated code.

### Insert — pull a file's contents in

`Insert` splices another file's contents into the specification at the point it
appears, before anything is generated. The file name may be written
`Insert "name"`, `Insert 'name'` or `Insert <name>` — all three currently resolve
relative to the folder holding the `.spectable` file.

`Import` is the other one, and they are not alike: `Import` makes another file's
`Attributes` and `Define` blocks *visible by reference*. `Insert` copies text in.

Where it goes changes what it means. There are three positions.

**A CSV as a step's table.** Put `Insert` where the table would go, under a step
that names an `Attributes` set:

```
Scenario An include of CSV file
Given a table : CSVContents
Insert "TestFolder/TableExample.csv"
```

The CSV becomes the step's table. Quoted fields survive — `a,"b,c",d` is three
values, not four. The column names are checked against the `Attributes` set, so
mistakes are caught rather than silently generating the wrong test:

```
ERROR:3:Table is missing column 'B' and 'B' has no default value
WARNING:3:Table has column 'Z' which doesn't match any field on 'CSVContents' — it will be ignored
```

**Any file inside a docstring.** Between `"""` lines, `Insert` is replaced by the
file's contents:

```
Given a string include
"""
Insert "string.txt"
"""
```

A `.csv` or `.tsv` here is still converted to a table; anything else is inserted
as literal text. Leading whitespace up to the column of the opening `"""` is
stripped, as it is for text typed inline.

**A whole `.spectable` at the top level.** The inserted file's declarations —
`Entity`, `Attributes`, `Scenario`, `BusinessRule` and the rest — are spliced in
and parsed as though typed in place:

```
Specification Host
Insert "shared_entities.spectable"
```

Two things to know about this form:

- **The inserted file's `Specification` line is dropped only if the host file has
  one of its own.** If the host has no `Specification`, the inserted one is used,
  and it names the generated test class — `Insert "part.spectable"` into a file
  with no `Specification` produces `Inserted_Part_Test`, not something named after
  the host. Give the host its own `Specification` and this stops being a surprise.
- **The same file is spliced only once.** A second `Insert` of it is skipped,
  which is also what stops two files that insert each other from looping.

**`Insert` in a position that is not one of those three does nothing, and says
nothing.** It is treated as a comment keyword, so there is no warning and no
error. The one to watch for is a CSV at the top level:

```
Specification Host
Insert "data.csv"        <- silently ignored; a CSV needs a step or a docstring
```

A file named by an `Insert` that *is* in a working position but cannot be opened
does report `WARNING: Cannot insert file:`, and Analyze lists it.

---

## Tags

Tags label a block so it can be selected later — to run only the smoke tests, or
to keep unfinished work out of the build. There are two kinds, and the
difference matters.

**`@Tag` reaches the generated tests.** It becomes whatever the target
framework uses to categorise a test — a JUnit 5 `@Tag("smoke")`, for instance.
Use it when your test runner will do the selecting.

**`$Tag` never leaves SpecStudio.** It exists only so the generator can decide
whether to emit the block at all. Nothing about it appears in the output.

```
@smoke @checkout
$wip
Scenario Add items
Given item collection is : OrderItemCollection
| Name | Price | Quantity | ItemTotal |
```

Several tags go on one line or on separate lines, and they apply to whatever
block comes next — `Scenario`, `BusinessRule`, `Calculation`, `DataType`, or the
`Specification` line itself, where they apply to everything in the file.

> **Tags must sit immediately above their block.** A blank line between the tag
> and the block discards it, silently. This is deliberate — it is what stops a
> stray tag at the top of a file from attaching itself to the first scenario
> that happens to follow — but it does mean a tag separated by whitespace does
> nothing at all.

### Filtering what gets generated

Set `tagFilter` in the `.specconfig` to a boolean expression over the `$` tags.
Only matching blocks are generated; an empty filter generates everything.

```
smoke                          only blocks tagged $smoke
NOT wip                        everything except unfinished work
smoke AND NOT wip
(smoke OR regression) AND NOT draft
```

`AND`, `OR`, `NOT` and the tag names are all case-insensitive, and parentheses
group as you would expect.

Filtering happens at generation time, so a filtered-out scenario produces no
test and no glue stub. That is the point — `$wip` on a half-written scenario
keeps it in the specification, where the conversation about it continues,
without breaking the build.

---

## Working with tables

Most editing effort in a specification is table editing, so the editor is
table-aware. Right-click inside a table:

| Command | Effect |
|---|---|
| **Insert Row Below** / **Delete Row** | Row editing |
| **Insert Table Header** | Add a header row |
| **Transpose Table** | Swap rows and columns |
| **Import CSV...** | Replace the table from a CSV file |
| **Extract as AttributeSet...** | Turn the table's columns into a named `Attributes` block |
| **Extract as Define...** | Turn the selected row into a named `Define` |
| **Create Attributes 'X'** | Declare an attribute set you referenced but never wrote |

And from the **Edit** menu:

- **Format Table** (`Ctrl+Alt+F`) — align the pipes
- **Edit Table...** (`Ctrl+Shift+T`) — edit in a spreadsheet-style grid, with
  row and column context menus
- **Edit String...** (`Ctrl+Shift+Q`) — edit a long cell value in a proper
  text box instead of squinting at a table row

---

## Seeing what you wrote

These are the features to reach for when a specification does not behave the
way you expected. All are on the right-click menu in a `.spectable` file, and
each stays open and refreshes as you type.

**Simulate Scenario...** — put the cursor in a scenario and see it expanded as
it will actually run: `Background` steps included, `=Define` references
resolved, `Vertical` tables turned the right way round. This is the fastest way
to catch a scenario that reads correctly but does not say what you meant.

**Run Examples...** — put the cursor in a `BusinessRule`, `Calculation` or any
`Examples:` block and check every row against the declared types *before*
generating anything. Cells are flagged as valid, wrong type, missing, or
"does not compare".

**Display Background... / Display Cleanup...** — show what runs around every
scenario in the file.

---

## Navigating and renaming

Right-click anywhere on a line — on a symbol, on the step text, inside a table:

- **Go to Definition**
- **Find All References**
- **Show Attributes: X** / **Show Define: X** — peek without leaving your place
- **Find Step Usages** — every scenario using this step
- **Rename Symbol: X...**
- **Rename Step: X...**

The symbol entries appear only when the word under the cursor is one SpecStudio
knows; the step and table entries appear whenever the line qualifies, wherever
on it you clicked.

**Rename Symbol** renames an entity, collection or attribute set across every
`.spectable` file in the solution, commits the change, and updates your glue
files in all nine languages — replacing `XString`, `XTyped` and the bare name.

**Rename Step** — from the context menu or **Edit → Rename Step...** (`F2`) —
asks once for the new text, pre-filled with the current step. It replaces the
text across the solution, commits, and renames the corresponding glue methods.
A glue method is a mangled identifier derived from the step — `When_item_added`
in Java, `when_item_added` in Python, `WhenItemAdded` in Go, `whenItemAdded` in
Swift and JavaScript — so replacing the step text alone would never reach it.
SpecStudio derives the name in every shape and renames whole-word matches, then
reports how many it changed. `F2` uses the step the caret is on; select text
first if you want to rename only part of one.

Neither command rebuilds anything — a build can take a while, and when to spend
that time is your call. Both tell you which projects need one, because the
generated `_Test` files keep referring to the old names until they are
regenerated. Your glue and production code is never overwritten by that build.

**Edit → Find All Usages...** (`Shift+F12`) searches the whole solution;
results land in the Output panel, and double-clicking one jumps to it.

---

## Configuration

A `.specconfig` file at the project root tells SpecStudio how to generate code.
A project may hold several — one per language — and you pick the active one
from **Build → Configuration**. Opening a `.specconfig` gives you a form, not
raw JSON.

| Setting | Meaning |
|---|---|
| `language` | `Java`, `CSharp`, `Python`, `Go`, `Rust`, `Swift`, `JavaScript`, `TypeScript`, `Cpp` |
| `framework` | Test framework — see below |
| `outputDirectory` | Where generated tests go, relative to the config file |
| `namespacePrefix` | Namespace or package for generated code |
| `imports` | Extra import/using lines added to every generated file |
| `copySpectable` | Copy the source `.spectable` beside the generated code |
| `overwriteGlue` | Regenerate glue stubs even when they exist — **off by default, and normally leave it off**, since glue is where your code lives |
| `tagFilter` | Boolean `$tag` expression; only matching blocks are generated |
| `converterPath` | Empty means auto-detect next to SpecStudio |
| `createProductionClasses` | Write production stubs for types that have none |
| `productionClassesDir` / `productionClassesPackage` | Where those stubs go |
| `failEveryTest` | End every generated glue stub with a failure, so a step that was never implemented cannot pass silently. **On by default** — turning it off means an unimplemented step reports success |
| `externalSpectables` | `.spectable` files from other projects whose types are visible here, each with its own production folder and imports |

Frameworks by language:

| Language | Frameworks |
|---|---|
| Java | JUnit, TestNG |
| C# | MSTest, NUnit, xUnit |
| Python | pytest, unittest |
| JavaScript / TypeScript | Jest, Vitest |
| Go | testing |
| Rust | builtin |
| C++ | GoogleTest |
| Swift | XCTest |

---

## Generating code

| Command | Shortcut | Scope |
|---|---|---|
| **Build → Current File** | `F6` | The open `.spectable` |
| **Build → Project** | `Shift+F6` | Every specification in the project |
| **Build → Solution** | `Ctrl+F6` | Everything |

Output appears in the Output panel. Errors are clickable — double-click to land
on the offending line.

**Project and solution builds delete the generated `*String` and `*Typed`
classes first**, then regenerate them. This is what removes files left behind by
a renamed or deleted type; without it, stale classes accumulate and break the
build with errors about types that no longer exist. Single-file builds do not
clear, so use a project build after renaming or deleting anything.

Your glue and production code is never touched by this.

---

## What gets generated

```
<outputDirectory>/
  common/          generated value classes — XString, XTyped, and an index
  <Spec>_Test      the tests
  <Spec>_glue      the glue — where you write code
production/        stubs for your production classes
```

**`common/` is fully generated.** Never edit it; every build rewrites it.

**Glue is yours.** SpecStudio creates each glue method once, as a stub, and
thereafter only *appends* methods that do not exist yet. It never rewrites or
removes what you wrote. The cost of that safety: glue for a step you deleted
stays behind until you remove it.

Each stub ends with a failure — `fail()`, `Assert.Fail`, `raise
NotImplementedError`, `t.Fatal`, `panic!`, `XCTFail`, `throw new Error` or
`ADD_FAILURE()`, depending on the language — so a step you have not implemented
yet cannot report success. Write your code above it and delete the failure line
when the step is done. Clearing `failEveryTest` in the configuration omits
those lines, at the cost of a scaffold that passes green before anything is
implemented.

**Production files are yours too, and are never overwritten.** Before writing a
stub, SpecStudio searches the production folder for a class, struct, enum,
interface, record, protocol or type of that name — anywhere in the folder, not
just in the file it would have created. If it finds one, it writes nothing and
logs an INFO line saying where the type already lives. So consolidating several
types into one file is fine, and so is renaming the file. Turn
`createProductionClasses` off and no production files are written at all.

**Keep computation out of glue.** Glue should drive production objects and
compare results. If a total needs calculating, the production class calculates
it. Glue that does arithmetic is testing itself.

---

## Analyze

**Analyze → Solution** (`Shift+F7`), or **Analyze → Project** for one project.

Analysis reads every specification together and reports what no single file can
show. Among the checks:

- **Unknown references** — an `AttributeSet`, `Entity`, `DataType`,
  `BusinessRule` or `Calculation` that is used but never declared, and
  `=Value` references with no matching `Define`
- **Duplicates** — repeated `Scenario`, `Step` or `Data` names, and a
  `DomainTerm` colliding with a built-in or declared `DataType`
- **Table shape** — a row whose column count does not match its header
- **Missing pieces** — a `DataType` with no data table or `Examples:` section;
  a step with a data table but no attribute set naming its columns
- **Misuse** — an unrecognized keyword or step modifier, or a `Cleanup` block
  containing anything but `Then` and `And`
- **Files** — an `Import` or `Insert` pointing at a file that is not there

Results fill the Analysis tab; double-click one to jump to it.

Analyze saves modified editors first and restores your cursor position and
scroll position afterwards, so it is safe to run mid-edit.

---

## Sharing work

The Git menu reflects the sharing mode chosen when the solution was created.

**Shared file system** — a single **Share with Git...** item.

**GitHub mode:**

| Command | Effect |
|---|---|
| **Commit and Push...** | Asks for a reason for the change, then commits and pushes |
| **Fetch** | Fetch without merging |
| **Get Others' Changes** | Pull teammates' work; conflicts open a resolution dialog |
| **Diff Current File** (`Ctrl+D`) | Diff against the last commit |
| **Repository Settings...** | Remote URL, branch, credentials |

**Every save also commits.** `File → Save` writes the file and then commits that
project with the message `Auto-save`. Your specification history is complete
without any effort, and the reason you give at push time is what your teammates
actually read.

Credentials are stored per project. SpecStudio supplies them to Git through a
helper program rather than embedding them in the remote URL, so tokens do not
end up in `.git/config` or in the output panel.

---

## Settings

**File → Settings...**

| Tab | Contents |
|---|---|
| **General** | Automatically reload files changed outside the editor |
| **Editors** | External program per file extension — blank means the built-in editor |
| **FeatureX** | Implicit `Data` import across folders; unique Scenario names; unique Step names; step suggestion scope |
| **Appearance** | Dark theme |
| **Fonts** | Editor font |

The **Editors** tab is how `.csv`, `.xlsx` and `.md` files open in a real
spreadsheet or Markdown editor. Leave an extension blank and SpecStudio edits it
as text.

---

## Keyboard reference

**File**

| Action | Key |
|---|---|
| New File | `Ctrl+N` |
| Open File | `Ctrl+O` |
| Save | `Ctrl+S` |
| Save All | `Ctrl+Shift+S` |
| Print | `Ctrl+P` |

**Edit**

| Action | Key |
|---|---|
| Find | `Ctrl+F` |
| Replace | `Ctrl+H` |
| Go to Line | `Ctrl+G` |
| Find All Usages | `Shift+F12` |
| Rename Step | `F2` |
| Format Table | `Ctrl+Alt+F` |
| Edit Table | `Ctrl+Shift+T` |
| Edit String | `Ctrl+Shift+Q` |

**View, Build, Analyze, Git**

| Action | Key |
|---|---|
| Refresh | `F5` |
| Split Editor Right | `Ctrl+\` |
| Close Split | `Ctrl+Shift+\` |
| Build Current File | `F6` |
| Build Project | `Shift+F6` |
| Build Solution | `Ctrl+F6` |
| Analyze Solution | `Shift+F7` |
| Diff Current File | `Ctrl+D` |

**Mouse, in a `.spectable` editor**

| Action | Gesture |
|---|---|
| Select a word | Double-click it |
| **Select a whole block** | **Double-click the block's keyword, or triple-click any line in it** |
| Context menu for what is under the pointer | Right-click |

Selecting a whole block is meant for cut and copy: the selection runs from the
header line through the block's last non-blank line and includes the line break,
so `Ctrl+X` lifts the block out cleanly and `Ctrl+V` puts it back as whole lines.
Blank lines between blocks are left where they are, as separators.

A block ends at the next block keyword, not by indentation — specifications are
normally written flush left, so the steps and tables under a `Scenario` share its
column. Text inside a `"""` docstring is part of the block that owns it even when
it contains something that looks like a keyword, which is what makes
`Insert "file.txt"` inside a docstring safe to select across.

The double-click is bound to the **keyword only** — the `Scenario` in
`Scenario Add two numbers`, not the whole header line. Double-clicking the
block's *name* still selects a word, as it does everywhere else.

---

## A first pass, end to end

1. **File → New Solution...**, choose a sharing mode.
2. **File → New Project...** — you get `Java.specconfig`. Edit it if you work
   in another language.
3. **File → New File...**, name it `Calculator.spectable`.
4. Write a `Calculation` with an `Examples:` table and the `Attributes` block
   describing its columns.
5. Right-click in the examples table → **Run Examples...** and confirm every
   row is valid.
6. **Build → Project** (`Shift+F6`).
7. Open the generated `_glue` file and implement the stubs against your
   production classes.
8. Run the tests the way you normally would — from your IDE, or with your
   language's runner (`mvn test`, `dotnet test`, `pytest`, `go test`,
   `cargo test`, `npm test`, `swift test`, `ctest`).
9. Change the specification, build again, re-run. Only the glue for genuinely
   new steps needs writing.

---

## Related documents

- `spectable syntax v3.3a.md` — the language reference
- `Configuration Guide.md` — every `.specconfig` field, keeping specifications in
  a different repository from the generated code, and the JSON support
- `Git Setup.md` — the one-time GitHub sign-in setup, for whoever installs it
- `Building Distributions.md` — packaging and signing SpecStudio itself
- `Remaining Work.txt` — the project backlog
- `archive/` — superseded syntax revisions, design notes and earlier backlogs,
  kept for history. Nothing there is current.
