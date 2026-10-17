# Configuring AlignThree™ Code Generation

*(formerly named SpecStudio)*

How to set up a `.specconfig`, how to keep specifications in a different
repository from the code generated out of them, and how the JSON support works.

This is the developer-facing companion to `User Guide.md`, which covers the same
ground from the IDE's side. Everything here is a plain JSON file and a
command-line tool, so it works the same in CI as it does in the IDE.

**Contents**

1. [What a .specconfig is](#what-a-specconfig-is)
2. [Every field](#every-field)
3. [Where the output goes](#where-the-output-goes)
4. [Specifications in a separate repository](#specifications-in-a-separate-repository)
5. [Types from another project](#types-from-another-project)
6. [Running the converter directly](#running-the-converter-directly)
7. [JSON](#json)
8. [Checklist](#checklist)

---

## What a `.specconfig` is

A `.specconfig` is a JSON file describing how to turn `.spectable` files into
test code. Generating for Java and Python from the same specifications means two
config files side by side — nothing in the format is single-target.

**Which config applies to a given specification:** AlignThree walks up from the
`.spectable` file's own folder to the project root and uses the *nearest*
`.specconfig` it finds. So a config at the project root covers everything, and a
config in a subfolder overrides it for that subfolder.

A minimal one:

```json
{
    "version": 1,
    "language": "Java",
    "framework": "JUnit",
    "outputDirectory": "generated"
}
```

A real one, from `AlignThreeVariousTests/TestProject/A Java Config.specconfig`:

```json
{
    "createProductionClasses": true,
    "framework": "JUnit",
    "imports": [
        "import java.util.*;"
    ],
    "language": "Java",
    "namespace": "spectable",
    "outputDirectory": "../Java/SpecTableVariousTests/src/test/java/spectable",
    "overwriteGlue": false,
    "productionClassesDir": "../Java/SpecTableVariousTests/src/main/java/production",
    "productionClassesPackage": "production",
    "version": 1
}
```

---

## Every field

| Field | Default | Meaning |
|---|---|---|
| `version` | `1` | Format version. |
| `language` | `"CSharp"` | `CSharp`, `Java`, `Rust`, `Python`, `Cpp`, `JavaScript`, `TypeScript`, `Go`, `Swift`. |
| `framework` | `"MSTest"` | Valid values depend on the language — `MSTest`/`NUnit`/`xUnit` for C#, `JUnit`/`TestNG` for Java. |
| `namespace` | *(empty)* | Namespace or package prefix for generated code. Empty means none. |
| `outputDirectory` | `"generated"` | Where tests, glue and `common` go. Relative to **this file**. |
| `overwriteGlue` | `false` | Regenerate glue even when it already exists. See the warning below. |
| `copySpectable` | `false` | Copy the `.spectable` source beside the generated tests. Off by default — see below. |
| `converterPath` | *(empty)* | Path to `SpecTableConverter`. Empty means auto-detect — leave it empty. |
| `imports` | `[]` | Extra `import`/`using` lines injected into every generated file. |
| `tagFilter` | *(empty)* | Boolean `$tag` expression; only matching blocks are generated. `smoke AND NOT wip`. |
| `createProductionClasses` | `false` | Write production stubs for Entities, Collections and DataTypes that do not exist yet. |
| `productionClassesDir` | *(empty)* | Where those stubs go. Relative to this file. |
| `productionClassesPackage` | *(empty)* | Package/namespace for them. |
| `failEveryTest` | `true` | End every generated glue stub with a failure, so an unimplemented step cannot report success. |
| `flattenSingleRowToList` | `true` | A DataType or built-in table of one row or one column reaches the glue as a list rather than a grid. |
| `flattenSingleCellToScalar` | `false` | A DataType or built-in table of one cell reaches the glue as a single value rather than a list of one. |
| `stepNameIncludesAttrSet` | `false` | Append the step's AttributeSet or Entity to its glue method name. New Project writes `true`. |
| `stepNameAttrSetOnCollision` | `false` | Where that appending applies: `true` only where two steps would otherwise share one method. New Project writes `true`. |
| `externalSpectables` | `[]` | Specifications from elsewhere whose types are visible here — see below. |

### The shape a DataType table arrives in

A step naming a DataType or a built-in hands the glue the declared type rather
than a grid of text. A built-in arrives converted — an `Integer` column is an
`int` — because the converter checks those cells as it generates and a cell
that would not convert is a build error. A DataType you declared arrives as
`<Name>String`, the cell's text with the type in its name, and the glue reads
the DataType from it: the rule is your project's, a specification may state a
value it refuses on purpose, and a test body must not throw.

These two options say what shape they arrive in, either way.

```
Given rolls are : Pins
| 5 | 5 | 4 | 10 |
```

| Options | The glue receives |
|---|---|
| both off | a grid — `List<List<PinsString>>` |
| `flattenSingleRowToList` (the default) | a list — `List<PinsString>` |
| both on, and one cell | one value — `PinsString` |

Row flattening is on by default because the row count is a property of the
table's shape, and every occurrence of a step agrees on it. Cell flattening is
off because cell count is **cardinality**, which legitimately differs between
scenarios of the same step: the Bowling example has `Given rolls are` with 16,
17, 18 and 1 cells, and a one-cell table there is a list of one, not a
different kind of thing.

One step is one glue method, so every occurrence of a step must agree on the
shape of its table. A disagreement is an error, naming both lines, and turning
cell flattening on can create one where none existed — which is the point: you
are told, rather than given a method whose signature depends on which scenario
was generated last.

### Naming a glue method after the table it takes

Two steps can read alike and take different tables:

```
Given the value is : Money
Given the value is : Count
```

Both want the method `Given_the_value_is`, and only one can have it. The build
says so, and `stepNameIncludesAttrSet` is the fix: the table's name joins the
method's, so the two become `Given_the_value_is_Money` and
`Given_the_value_is_Count`.

`stepNameAttrSetOnCollision` says where that happens.

| Settings | What the glue methods are called |
|---|---|
| `stepNameIncludesAttrSet: false` | `Given_the_value_is` for both, and the build warns about the clash |
| `stepNameIncludesAttrSet: true` | every method whose step takes a table is qualified, collision or not |
| both `true` | only the methods that would collide are qualified; the rest read as their steps do |

New projects get both, which is the combination worth having: a name is
qualified exactly where ambiguity would otherwise force it, and
`When_the_total_is_computed` is not renamed to
`When_the_total_is_computed_Count` for nothing.

**Changing either setting renames glue methods, and nothing renames them back.**
A build appends the stubs the new names need and leaves your old methods where
they are, under their old names, holding their implementations, called by
nothing. The editor asks before it lets you change either one. Do it when the
glue is empty, or be ready to move code across.

### Why `copySpectable` defaults to off

A copy of the specification beside the generated code is useful when the output
tree has to stand on its own — someone reading the tests can see what they came
from. It costs something, though, and the cost is not obvious: the copy is a
second declaration of every symbol in that file. If `outputDirectory` is a
folder inside the project, as it is by default, Analyze then sees the project
twice and reports every Entity, Attributes block, DataType and Collection in it
as a duplicate. One real project turned 12 files into 24 and 0 diagnostics into
496.

So it is off unless you ask for it. Turn it on where the output is published
somewhere separate from the specifications, and prefer an `outputDirectory`
that is not inside the project if you do.

This default changed on 2026-10-11. A `.specconfig` written before then that
does not mention `copySpectable` will stop copying; add `"copySpectable": true`
to keep the old behaviour. Every config saved from now on states it explicitly.

**`overwriteGlue` deserves care.** Glue files are yours: `appendMissingStubs`
adds a stub for each new step and never removes or rewrites what is there.
Setting `overwriteGlue` to `true` discards your implementations. Leave it `false`
unless you mean it.

**Leave `converterPath` empty.** A hardcoded path breaks on every other machine.
Empty means AlignThree looks, in order:

1. `SpecTableConverter` beside `AlignThree` itself — how a release is laid out;
2. `../../converter/Debug/`, then `../../converter/Release/` — a Visual Studio
   build tree, which nests by configuration;
3. `../converter/` — Ninja and Makefile builds, which do not nest.

---

## Where the output goes

`outputDirectory` and `productionClassesDir` both resolve **relative to the
`.specconfig` file**, not to the current working directory and not to the
solution root. Absolute paths are taken as given.

```
outputDirectory: "generated"                  -> <folder holding the config>/generated
outputDirectory: "../Java/App/src/test/java"  -> up one from the config, then down
outputDirectory: "C:/code/App/tests"          -> exactly that
```

Prefer relative. An absolute path is a per-machine setting committed to a shared
repository, and it will be wrong for everyone else.

`productionClassesDir` used to be passed through to the converter exactly as
written, so a relative value resolved against whatever the working directory
happened to be — meaning only absolute paths really worked, and a hand-written
relative one quietly scattered files somewhere unexpected. Both are now resolved
against the config, consistently.

What lands in `outputDirectory`:

| | |
|---|---|
| `<Spec>_Test.*` | Generated. Overwritten every time. |
| `<Spec>_glue.*` | **Yours.** Created once; new steps appended. |
| `common/` | Generated: the `*String` and `*Typed` classes, `Json`, table helpers. |
| `<Spec>.spectable` | A copy of the source, only when `copySpectable` is `true`. |

`productionClassesDir` holds hand-written code. The generator writes a stub only
when it cannot find a declaration of that type anywhere under the folder, so
grouping several classes into one file is fine.

---

## Specifications in a separate repository

Specifications and the code generated from them do not have to live in the same
repository, and there are good reasons to separate them: the specification is
the thing the whole team agrees on, while the generated tests belong beside the
production code they exercise, in that language's build.

Point `outputDirectory` and `productionClassesDir` across the boundary. Both
repositories are checked out side by side, so the relative path is stable for
everyone:

```
repos/
  MySpecs/                       <- the specification repository
    Cart/
      Shopping Cart.spectable
      Java.specconfig            <- outputDirectory: "../../../MyApp/src/test/java/spectable"
  MyApp/                         <- the application repository
    src/main/java/production/
    src/test/java/spectable/
```

This project's own test trees are set up the other way round — the
specifications sit inside the repository that holds the generated code, one
folder per language:

```
AlignThreeVariousTests/
  TestProject/
    include.spectable
    A Java Config.specconfig     <- outputDirectory: "../Java/SpecTableVariousTests/src/test/java/spectable"
  Java/SpecTableVariousTests/
  Python/VariousTests/
  ...
```

Either shape works. What matters is that the path is **relative**, so the same
config resolves on every machine.

### What this buys you, and what it costs

Generating into another repository is a plain file write. AlignThree does not
know or care that a repository boundary was crossed, which has consequences
worth knowing before you rely on it:

- **No coupling between the two.** Nothing keeps the specification repository and
  the generated code in step. A spec change that is committed while the
  regenerated tests are not leaves the two repositories disagreeing, and no tool
  will tell you. Regenerate and commit both together, or have CI regenerate.
- **Git operations stay in the specification's project.** Save, Share Changes and
  the rest act on the repository holding the `.spectable` file. The generated
  code is committed by whatever normally commits that repository.
- **Analyze reads specifications, not generated code.** It will not notice that
  the code on the other side is stale.
- **Renaming a step or symbol does reach across.** Both rename commands read
  every `.specconfig` in the project and search each `outputDirectory` they name,
  so glue in another repository is found and updated. They deliberately do not
  rebuild afterwards — they tell you which projects need it, because a build can
  take a while and the timing is yours to choose.
- **A deleted specification leaves its output behind.** Its glue and copied
  `.spectable` stay where they are and have to be removed by hand.

---

## Types from another project

`externalSpectables` makes the types declared in another specification visible
here without generating any code for them. Use it when several projects share a
domain model.

```json
{
    "language": "Java",
    "framework": "JUnit",
    "outputDirectory": "../Java/App/src/test/java/spectable",
    "externalSpectables": [
        {
            "file": "../SharedModel/Domain.spectable",
            "productionDir": "../../SharedModel/Java/src/main/java/domain",
            "codeImports": [ "import domain.*;" ]
        }
    ]
}
```

| Key | Meaning |
|---|---|
| `file` | The other `.spectable`. Relative to this config, or absolute. |
| `productionDir` | Where the production code for types declared in that file lives. |
| `codeImports` | `import`/`using` lines needed to reach those types. Wildcards are fine. |

Each entry becomes `--context <file>` on the converter command line, plus one
`--import` per `codeImports` entry. `--context` means *read this for symbols,
generate nothing for it* — so the shared model is declared once and compiled
once.

This is not the same as the specification's own `Import` keyword, which brings in
another file's `Attributes` and `Define` blocks at the language level. Nor is it
`Insert`, which splices text. `externalSpectables` is a build-time visibility
setting.

---

## Running the converter directly

The IDE only assembles a command line. The same generation runs in CI without it:

```bash
SpecTableConverter --language Java --framework JUnit \
                   --namespace spectable \
                   --prod-dir ../Java/App/src/main/java/production \
                   --prod-package production \
                   --context ../SharedModel/Domain.spectable \
                   --import "import domain.*;" \
                   "Shopping Cart.spectable" \
                   ../Java/App/src/test/java/spectable
```

The last two arguments are input and output. Useful flags:

| Flag | Config field |
|---|---|
| `--language`, `--framework`, `--namespace` | `language`, `framework`, `namespace` |
| `--prod-dir`, `--prod-package` | `productionClassesDir`, `productionClassesPackage` |
| `--context <file>` | `externalSpectables[].file` |
| `--import <statement>` | `imports`, `externalSpectables[].codeImports` |
| `--tag-filter <expr>` | `tagFilter` |
| `--overwrite-glue` | `overwriteGlue` |
| `--no-copy-spectable` | `copySpectable: false` |
| `--no-fail-every-test` | `failEveryTest: false` |
| `--step-name-attrset` | `stepNameIncludesAttrSet` |
| `--step-name-attrset-on-collision` | `stepNameAttrSetOnCollision` |

`--fail-every-test` is accepted and does nothing; failing stubs are the default.

Exit code 0 means generated. Diagnostics go to standard output as
`ERROR:<line>:<message>`, `WARNING:<line>:<message>` and `INFO:<line>:<message>`,
which is what the IDE's Analyze tab parses.

---

## JSON

There are **two** JSON facilities, and they are easy to confuse because both
end up in generated projects.

### `common/Json` — generated, used by the generated code

A dependency-free JSON reader and writer emitted into `common/` alongside the
`*String` and `*Typed` classes. The `*Typed` classes use it; you rarely call it
directly. It exists so that a generated project needs no JSON library at all —
no Jackson, no `serde`, no `Newtonsoft`.

```java
String  text = Json.write(value);              // any value -> JSON text
Object  any  = Json.parse(text);               // JSON text -> Map/List/String/Number
Map<String, Object> obj = Json.parseObject(text);
List<Object>        arr = Json.parseArray(text);

// Typed accessors, which raise a clear error rather than returning null
String     name  = Json.getString (obj, "name");
int        count = Json.getInt    (obj, "count");
BigDecimal price = Json.getDecimal(obj, "price");
boolean    ok    = Json.getBoolean(obj, "ok");
Map<String, Object> nested = Json.getObject(obj, "address");
List<Object>        items  = Json.getArray (obj, "items");
```

`getDecimal` returning `BigDecimal` is deliberate: money read out of JSON as a
`double` is a rounding bug waiting to happen.

### `SimpleJson` — production code, driven by a specification

The other facility is an ordinary production class, written by you, that a
specification exercises like anything else. `json.spectable` in
`AlignThreeExampleTests` is the worked example. It states the conversion as four
scenarios — object out, object in, array out, array in:

```
Specification Json

Scenario Convert to Json
Given one object is : SimpleClass
| anInt | aString |
| 1     | B       |
Then Json should be
"""
{anInt:"1",aString:"B"}
"""

Scenario: Convert from Json
Given Json is
"""
{anInt:  "1"   ,   aString:"B"  }
"""
Then the converted object is : SimpleClass
| anInt | aString |
| 1     | B       |

Attributes SimpleClass
Description example class
| Name    | Default | Datatype |
| anInt   | 0       | Integer  |
| aString | Q       | String   |
```

Note the expected JSON is given as a docstring — a block of text — and the
incoming JSON is deliberately written with untidy whitespace, because tolerating
that is part of what is being specified.

The glue is where the interesting constraint shows up:

```java
// SimpleJson takes plain name/value maps: it lives in production and so
// cannot see the generated test classes. These two moves are the whole of
// the mapping — the conversion itself belongs to SimpleJson.
private static LinkedHashMap<String, String> fieldsOf(SimpleClassString value) {
    LinkedHashMap<String, String> fields = new LinkedHashMap<>();
    fields.put("anInt", value.anInt);
    fields.put("aString", value.aString);
    return fields;
}

private static SimpleClassString objectOf(Map<String, String> fields) {
    return new SimpleClassString(fields.get("anInt"), fields.get("aString"));
}

public void Given_one_object_is(List<SimpleClassString> values) {
    simpleClassValues = values;
    actualJson = SimpleJson.toObject(fieldsOf(values.get(0)));
}

public void Then_Json_should_be(String value) {
    // Text to text, with the whitespace between tokens removed from both
    // sides. Whitespace inside a quoted value is kept.
    assertEquals(SimpleJson.withoutWhitespace(value),
                 SimpleJson.withoutWhitespace(actualJson));
}
```

Two rules are visible there, and both are general:

- **Production code cannot see generated test classes.** In Maven, `main` cannot
  depend on test sources; the same separation is applied in every language, so
  `common` never depends on `production`. A production class therefore takes
  plain types — `Map<String, String>` here — and the glue does the small mapping.
- **The glue does no computing.** `fieldsOf` and `objectOf` move values; every
  decision about what JSON looks like is inside `SimpleJson`. If a step's glue
  starts calculating, the logic is in the wrong place.

The expected and actual JSON are compared as **text**, normalised only for
whitespace between tokens. Parsing both sides and comparing the results would
pass even if the writer emitted something no other parser would accept.

### The methods, by language

| | Java | C# | Python | Go |
|---|---|---|---|---|
| object out | `toObject` | `ToObject` | `to_object` | `JSONToObject` |
| array out | `toArray` | `ToArray` | `to_array` | `JSONToArray` |
| object in | `parseObject` | `ParseObject` | `parse_object` | `JSONParseObject` |
| array in | `parseArray` | `ParseArray` | `parse_array` | `JSONParseArray` |
| normalise for comparison | `withoutWhitespace` | `WithoutWhitespace` | `without_whitespace` | `JSONWithoutWhitespace` |

Each follows its own language's conventions rather than a single imposed
spelling. Go has no map with predictable ordering, so it uses an ordered
`[]Field` of `{Name, Value}` where the others use a `LinkedHashMap` or `dict`;
field order in the output is part of what the specification pins down, so it
cannot be left to a hash table.

C++, Rust, Swift, JavaScript and TypeScript follow the same shape under their own
naming (`simple_json.h`, `simple_json.rs`, and so on).

---

## Checklist

Setting up a new project:

1. Put a `.specconfig` at the project root; add more in subfolders only if they
   genuinely differ.
2. Set `language` and `framework`. They must agree — `frameworksFor` in
   `SpecConfig` is the authority on which pairs are valid.
3. Set `outputDirectory` **relative** to the config.
4. If the specification declares Entities, Collections or DataTypes, set
   `createProductionClasses`, `productionClassesDir` and
   `productionClassesPackage`.
5. Leave `converterPath` empty and `overwriteGlue` false.
6. Generate, then compile and run the tests. A fresh scaffold is **all red** by
   design: every glue stub ends in a failure until you implement it.

Things that go wrong:

| Symptom | Cause |
|---|---|
| Files appear somewhere unexpected | A relative path was read against a different base than you assumed. It is relative to the **config file**. |
| Glue implementations disappeared | `overwriteGlue: true`. |
| Works for you, nobody else | An absolute path in a committed config. |
| A type is "not declared" but plainly is | It is declared in another file that is not in this project and not in `externalSpectables`. |
| Generated code does not compile — unknown type | A production class exists but no `imports` entry reaches it. |
| A fresh scaffold passes green | `failEveryTest` was turned off. |
| Glue methods no step calls | Left from a step that was reworded or a specification that was deleted. `appendMissingStubs` never removes. |

---

## Related documents

- `User Guide.md` — using the IDE
- `spectable syntax v3.3a.md` — the language
- `Building Distributions.md` — packaging and signing AlignThree itself

Two repositories hold working configurations for all nine languages, and they
have different jobs:

- **`AlignThreeExampleTests`** — examples meant to be read. Start with
  `Calculator.spectable`, and see that repository's `README.md`.
- **`AlignThreeVariousTests`** — where new AlignThree features get exercised,
  including the awkward cases. Look here for a configuration that does something
  unusual; do not read it as a style guide.
