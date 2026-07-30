# SpecStudio™

An IDE for writing specifications as tables, and turning them into runnable
tests in nine languages.

You write a `.spectable` file describing what the software should do — the
domain objects, the business rules, the scenarios, each illustrated by a table
of examples. SpecStudio generates the test code and the glue that connects
those tests to your production classes, for Java, C#, Python, Go, Rust, Swift,
JavaScript, TypeScript and C++.

The specification is the source of truth. The generated tests are disposable.

---

## Why

A specification that cannot be executed drifts. It is written before the code,
consulted during, and quietly falsified afterwards — and nothing tells you when
that happened. The usual answer is to write the examples twice: once in prose
for people, once in test code for the machine. Two artefacts, one intent, and
no mechanism keeping them honest with each other.

Executable specification collapses that into one artefact. The examples the
team agreed on *are* the test suite, so a specification that has gone stale
fails the build rather than sitting there misleading the next reader.

SpecStudio takes a particular position on how to do that:

**Tables, not prose.** Business rules are usually easier to agree on as a table
of cases than as sentences. A column heading with six rows underneath it makes
the boundary cases visible and the missing case conspicuous — which is exactly
the conversation worth having before any code is written.

**A domain model in the specification.** The spec declares its own types:
`Entity`, `Collection`, `DataType`, `Attributes`. That declaration is what makes
everything else possible — type checking, generated conversion, cross-file
analysis, and a spec that reads in the domain's own vocabulary.

**Generation, not binding.** The types you declared become real classes in your
language, and each step becomes a real method. Wiring is done by the generator,
at build time, where mistakes are compile errors rather than surprises at 3am.

## Why it is useful

- **One specification, nine languages.** The same `.spectable` drives a Java
  team and a Go team. Polyglot systems get one agreed description of behaviour
  instead of one per stack.
- **Errors surface before you run anything.** Analyze reports unknown types,
  duplicate scenario names, `=Value` references with no `Define`, table rows
  whose column count does not match the header — across every file in the
  solution.
- **You cannot silently skip a step.** Every generated glue stub ends in a
  failure, so an unimplemented step is red, never a false green.
- **Your code is never overwritten.** Glue methods are created once and only
  ever appended to. Production classes are written only when the type does not
  already exist anywhere in the folder.
- **The examples stay readable.** Simulate Scenario expands a scenario the way
  it will actually run, with `Background` folded in and `=Define` references
  resolved. Run Examples type-checks a table before you generate anything.

---

## Where it comes from

SpecStudio is a recombination of three ideas that already worked, none of them
new, and each borrowed for a different reason.

### FIT — the table as the unit of agreement

Ward Cunningham's **Framework for Integrated Test** (2002) established that a
table of examples is a better medium for agreeing on a business rule than a
paragraph describing it. Columns are inputs and expected outputs; each row is a
case; a fixture binds the table to the code beneath.

SpecTable takes the table wholesale. A `BusinessRule` or `Calculation` with an
`Examples:` block is FIT's ColumnFixture in a different notation: named columns,
one row per case, expectations sitting beside their inputs.

### Gherkin — the scenario as narrative

Cucumber's **Gherkin** (2008) contributed `Given` / `When` / `Then`: a shape for
describing behaviour over time that non-programmers can read and, more
importantly, argue with. Setup, action, consequence, in that order.

SpecTable keeps the keywords and the discipline. `Scenario`, `Background`,
`Given`/`When`/`Then`/`And` mean what they mean in Gherkin, and a scenario reads
top to bottom as a narrative.

### Domain-Driven Design — the vocabulary

Eric Evans' **Domain-Driven Design** (2003) supplied the model. `Entity`,
`DataType` (a Value Object), `Collection` and `DomainTerm` are DDD's terms, used
deliberately: the specification is where the ubiquitous language is written
down, and the generated production stubs carry those names into the code.

This is also why glue contains no business logic. Glue drives production
objects and compares results; the calculation belongs on the object that owns
the data. Glue that computes its own answers is a test agreeing with itself.

---

## How it differs from FIT

| | FIT | SpecTable |
|---|---|---|
| Format | HTML or wiki pages | plain text — diffs, merges, reviews like code |
| Binding | fixtures written by hand, matched by reflection at run time | test and glue generated at build time |
| Types | none; cells are strings a fixture interprets | declared in the spec, with generated conversion |
| Languages | a fixture per language, written per language | nine targets from one source |
| Narrative | tables only | tables **and** Given/When/Then scenarios |

FIT's tables describe rules well but have no way to say what a *thing* is. There
is no equivalent of `Entity Address` — no declared type reused across tables,
and so nothing to check consistency against. SpecTable adds that declaration,
which is what lets it generate typed classes rather than reflecting over strings.

The pragmatic difference is the medium. FIT's HTML tables meant the
specification lived apart from the code, often in a wiki, with its own history.
A `.spectable` file sits in the repository beside everything else.

## How it differs from Gherkin

**Gherkin is untyped.** Every value is a string until a step definition parses
it. `| 42 | 2026-07-29 | $10.00 |` are three strings, and each step definition
re-does that parsing by hand. SpecTable declares the types once, in the spec,
and generates the conversion — the `XString` class holds what was written, the
`XTyped` class holds what it means.

**Gherkin binds by regex; SpecTable binds by name.** A Cucumber step definition
matches a sentence by pattern. Reword the sentence and the binding breaks —
sometimes loudly, sometimes by silently matching a different definition.
SpecTable derives a method name from the step, so the connection is an
identifier the compiler checks, and renaming a step renames its glue method.

**Tables are primary, not incidental.** Gherkin has data tables and
`Scenario Outline`, but they are attachments to a sentence. In SpecTable a step
names the attribute set describing its table — `When item added : OrderItem` —
so the table's shape is declared and checked, not inferred at run time.

**There is a model, not just steps.** A `.feature` file has no vocabulary
beyond its own sentences; nothing relates the `ShoppingCart` in one file to the
`ShoppingCart` in another. SpecTable's declarations are project-wide, which is
what makes Go to Definition, Find All References, rename and cross-file analysis
possible at all.

**What Gherkin does better:** it reads like English, and that matters. Anyone
can read a `.feature` file aloud in a meeting. SpecTable's tables ask a little
more of the reader in exchange for being checkable — a real trade, and the wrong
one if your audience will not read a table.

SpecStudio also edits `.feature` and `.featurex` files, so the two can coexist.

---

## The format in brief

```
Specification Shopping Cart

Entity CatalogItem
| Attribute | Type       | Default | Notes |
| Name      | SimpleText | NoName  |       |
| Price     | Dollar     | 1       |       |

Collection Catalog
| DataType    | Minimum | Maximum  | Notes          |
| CatalogItem | 0       | 10000000 | Each is unique |

Scenario Add items
Given item collection is : OrderItemCollection
| Name | Price | Quantity | ItemTotal |
When item added : OrderItem Vertical
| Name     | Widget |
| Quantity | 2      |
Then item collection is : OrderItemCollection
| Name   | Price  | Quantity | ItemTotal |
| Widget | $10.00 | 2        | $20.00    |

BusinessRule Total Cart Price
Description How to Apply Discount and Shipping
Examples: CartInput
| TotalItems | Shipping | Discount | Total Price |
| $110       | $5       | $11      | $104        |
| $80        | $5       | $4       | $81         |
```

Full syntax reference: [spectable syntax v3.3a.md](spectable%20syntax%20v3.3a.md).
Earlier revisions are in [archive/](archive).

---

## What gets generated

```
<outputDirectory>/
  common/          generated value classes — XString, XTyped, and an index
  <Spec>_Test      the tests
  <Spec>_glue      the glue — where you write code
production/        stubs for your production classes
```

`common/` is rewritten on every build; never edit it. Glue and production code
are yours and are never overwritten.

## Building

**Requirements:** Qt 6.10, CMake 3.16+, and MSVC 2022 on Windows (Clang or GCC
elsewhere).

```powershell
$cmake  = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
$qt6Dir = "C:\Qt\6.10.0\msvc2022_64\lib\cmake\Qt6"

& $cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DQt6_DIR=$qt6Dir"
& $cmake --build build --config Debug
```

Three executables are produced and must stay together — SpecStudio finds the
other two beside itself:

```
build\src\Debug\SpecStudio.exe
build\src\Debug\SpecStudioAskPass.exe
build\converter\Debug\SpecTableConverter.exe
```

To run without Qt on the `PATH`, deploy with `windeployqt`, or see
[Building Distributions.md](Building%20Distributions.md) for packaging and
signing on Windows, Linux and macOS.

## Layout

```
src/
  app/            MainWindow, AppController, AppSettings, ThemeManager
  model/          Solution, Project, ProjectFile, FileType, SpecConfig
  ui/             SolutionExplorer, EditorTabWidget, OutputPanel, dialogs/
  editors/        SpecTableEditor, FeatureEditor, SpecConfigEditor, syntax/
  git/            GitClient, GitStatusCache
  analyzer/       SpecTableIndex, SpecTableAnalyzer, ProjectIndex
  build/          BuildController, BuildOutputParser
converter/        SpectableParser and one generator per target language
```

## Documentation

- [User Guide.md](User%20Guide.md) — using the IDE
- [spectable syntax v3.3a.md](spectable%20syntax%20v3.3a.md) — the language
- [Building Distributions.md](Building%20Distributions.md) — packaging and signing
