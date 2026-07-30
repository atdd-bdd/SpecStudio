# SpecTable Code Generation Guide

This guide explains how to generate unit test scaffolding from `.spectable` files using **SpecTableConverter**, and how to integrate the output into a Java, C#, or Rust project.

---

## Table of Contents

1. [Overview](#1-overview)
2. [The .specconfig File](#2-the-specconfig-file)
3. [Running the Converter](#3-running-the-converter)
4. [Java](#4-java)
5. [C#](#5-c)
6. [Rust](#6-rust)
7. [Tag Filtering with $tags](#7-tag-filtering-with-tags)
8. [Extra Imports / Use Statements](#8-extra-imports--use-statements)

---

## 1. Overview

The converter reads a `.spectable` file and produces:

| Output file | Purpose |
|---|---|
| `common/{Name}String.*` | Data class — all fields as strings, constructed from test data |
| `common/{Name}Typed.*` | Data class — fields parsed to their declared types |
| Test file | One test function per Scenario / BusinessRule / Calculation / DataType block |
| Glue file | Stub functions for you to implement — connects tests to your production code |

The test file is regenerated every build. The glue file is **never overwritten** once it exists; subsequent builds only append stubs for new steps.

---

## 2. The .specconfig File

Each project has a `.specconfig` file (JSON) that controls the converter. Edit it in SpecStudio's `.specconfig` editor or by hand:

```json
{
  "version": 1,
  "outputDirectory": "generated",
  "language": "Java",
  "framework": "JUnit",
  "namespace": "com.example",
  "overwriteGlue": false,
  "converterPath": "path/to/SpecTableConverter.exe",
  "imports": [
    "import com.example.domain.Money;"
  ],
  "tagFilter": "smoke AND NOT wip"
}
```

| Field | Description |
|---|---|
| `outputDirectory` | Where generated files are written (relative to the `.specconfig` file) |
| `language` | `CSharp`, `Java`, or `Rust` |
| `framework` | C#: `MSTest` / `NUnit` / `xUnit` — Java: `JUnit` / `TestNG` — Rust: `builtin` |
| `namespace` | Package/namespace prefix (Java and C# only) |
| `overwriteGlue` | If `true`, the glue file is rewritten from scratch each build |
| `converterPath` | Path to `SpecTableConverter.exe` |
| `imports` | Extra import/use statements added verbatim to every generated file |
| `tagFilter` | Boolean `$tag` expression — only matching blocks are generated (see §7) |

---

## 3. Running the Converter

From SpecStudio, press **F6** (Build) or use **Build > Build Current File**.

From the command line:

```
SpecTableConverter.exe  <input.spectable>  <output-dir>  [options]
```

Key options:

| Option | Description |
|---|---|
| `-l` / `--language` | `CSharp` (default), `Java`, or `Rust` |
| `-f` / `--framework` | `MSTest`, `NUnit`, `xUnit`, `JUnit`, `TestNG`, `builtin` |
| `-n` / `--namespace` | Namespace / package prefix |
| `--overwrite-glue` | Overwrite glue file even if it exists |
| `--import <stmt>` | Extra import/use statement (repeatable) |
| `--context <file>` | Additional `.spectable` whose Attributes/Defines are globally visible but generate no code |
| `--source-root <dir>` | Java only — project source root, used to derive the package from the subfolder path |
| `--tag-filter <expr>` | Boolean `$tag` expression controlling which blocks are generated |

---

## 4. Java

### Generated file layout

```
output-dir/
  common/
    AdderString.java         ← String data class
    AdderTyped.java          ← Typed data class
  Test_Calculator.java       ← @Test methods (regenerated every build)
  Calculator_glue.java       ← Step implementation stubs (never overwritten)
  Calculator.spectable       ← Copy of the source file
```

### Package structure

The converter derives the Java package from the `--namespace` prefix combined with the subfolder path relative to `--source-root`. For example:

- Namespace: `com.example`
- Source root: `src/main/java`
- Spec file: `src/main/java/specs/calculator/Calculator.spectable`
- Domain package: `com.example.common`
- Spec package: `com.example.specs.calculator`
- Test package: `com.example.specs.calculator.tests`

### Frameworks

| Value | Annotation | Assert |
|---|---|---|
| `JUnit` (default) | `@Test` from `org.junit.jupiter.api` | `Assertions.fail()` |
| `TestNG` | `@Test` from `org.testng.annotations` | `Assert.fail()` |

### Build tool — Maven example

Add the output directory to your test sources and include the framework dependency:

```xml
<dependency>
  <groupId>org.junit.jupiter</groupId>
  <artifactId>junit-jupiter</artifactId>
  <version>5.10.0</version>
  <scope>test</scope>
</dependency>
```

Run with: `mvn test`

### Step method naming

| Scenario step | Generated glue method |
|---|---|
| `Given list of numbers : LabelValue` | `given_list_of_numbers(List<LabelValueString> values)` |
| `When filtered by ID with value : ID` | `when_filtered_by_id_with_value(List<List<String>> values)` |
| `Then sum is : Integer` | `then_sum_is(List<List<String>> values)` |

For named blocks: `ExamplesCalculation_Add_two_numbers(List<AdderString> values)`

---

## 5. C#

### Generated file layout

```
output-dir/
  common/
    AdderString.cs           ← String data class
    AdderTyped.cs            ← Typed data class
  Test_Calculator.cs         ← [TestMethod] / [Test] / [Fact] methods
  Calculator_glue.cs         ← Step stubs
  Calculator.spectable
```

### Namespaces

With namespace prefix `MyApp`:

- Domain: `MyApp.Common`
- Spec: `MyApp.Specifications.Calculator`

### Frameworks

| Value | Attribute | Assert |
|---|---|---|
| `MSTest` (default) | `[TestMethod]` | `Assert.Fail()` |
| `NUnit` | `[Test]` | `Assert.Fail()` |
| `xUnit` | `[Fact]` | `throw new XunitException()` |

### Running tests

```
dotnet test
```

### Step method naming

Same pattern as Java, but methods take `List<AdderString>` (C# `List<T>`):

```csharp
public void given_numbers(List<AdderString> values) { ... }
public void examples_calculation_add_two_numbers(List<AdderString> values) { ... }
```

---

## 6. Rust

### Generated file layout

```
output-dir/
  common/
    adder_string.rs          ← String struct
    adder_typed.rs           ← Typed struct
    mod.rs                   ← Re-exports all types
  test_calculator.rs         ← #[test] functions (regenerated every build)
  calculator_glue.rs         ← Stub impl (never overwritten)
  Calculator.spectable
```

### Wiring into your crate

Add the generated modules to your `src/lib.rs` (or `src/main.rs`):

```rust
pub mod common;
pub mod calculator_glue;

#[cfg(test)]
mod test_calculator;
```

Run tests with: `cargo test`

### Naming conventions

All Rust identifiers use **snake_case**; struct types use **PascalCase**.

| Spec name | Rust type | Rust module |
|---|---|---|
| `Adder` | `AdderString` / `AdderTyped` | `common::adder_string` / `common::adder_typed` |
| `Calculator` | `CalculatorGlue` | `calculator_glue` |

### Type mapping

| Spec type | Rust type | Parse expression |
|---|---|---|
| `Integer` / `Int` | `i32` | `.parse::<i32>().unwrap_or_default()` |
| `Float` / `Decimal` | `f64` | `.parse::<f64>().unwrap_or_default()` |
| `Boolean` / `YesNo` | `bool` | `matches!(... , "true" \| "t" \| "yes" \| "y" \| "1")` |
| `String` / `Text` / `Char` | `String` | `.clone()` |
| `Date` / `Time` / `DateTime` / `Duration` | `String` | `.clone()` |
| User DataType (e.g. `ID`) | `ID` | `ID::from(s.field.clone())` |

### User-defined DataTypes

For fields typed with a user DataType (e.g. `ID`), the Typed struct uses the type name directly and converts via `From<String>`:

```rust
// generated in common/filtervalue_typed.rs
pub value: ID,
// ...
value: ID::from(s.value.clone()),
```

You must define the type in your crate. The simplest option is a type alias:

```rust
// in your lib.rs or a domain module
pub type ID = String;
```

Or a newtype with a `From<String>` implementation:

```rust
pub struct ID(pub String);
impl From<String> for ID {
    fn from(s: String) -> Self { ID(s) }
}
```

### String struct API

```rust
// Construct from a slice of &str (same order as Attributes table columns)
let row = AdderString::from_vec(&["2", "3", "5"]);

// Fields are pub String
println!("{}", row.number1);

// Display impl shows all fields
println!("{}", row);  // "number1=2, number2=3, result=5"
```

### Typed struct API

```rust
// Convert String struct to typed values
let typed = AdderTyped::from_str_struct(&row);
assert_eq!(typed.number1, 2_i32);
```

### Glue method signatures

| Step | Glue signature |
|---|---|
| Bare step (no table) | `fn given_user_is_logged_in(&mut self)` |
| Step with AttrSet table | `fn given_numbers(&mut self, values: &[AdderString])` |
| Step with DataType/primitive table | `fn when_filtered(&mut self, values: &[Vec<String>])` |
| BusinessRule / Calculation / DataType | `fn examples_calculation_add_two_numbers(&mut self, values: &[AdderString])` |

### Sample glue implementation

```rust
// calculator_glue.rs — fill in the bodies
impl CalculatorGlue {
    pub fn examples_calculation_add_two_numbers(&mut self, values: &[AdderString]) {
        for row in values {
            let t = AdderTyped::from_str_struct(row);
            assert_eq!(t.number1 + t.number2, t.result,
                "{}+{} should equal {}", t.number1, t.number2, t.result);
        }
    }
}
```

### Multi-spec projects

If multiple `.spectable` files share Attributes/Defines, pass the shared files as context:

```
SpecTableConverter.exe Main.spectable out/ --language Rust \
  --context Shared.spectable
```

Context files contribute their types to the generated code but do not themselves produce test or glue files.

---

## 7. Tag Filtering with $tags

Prefix any Scenario, BusinessRule, Calculation, or DataType block with `$TagName` lines to mark it for generator filtering:

```
$smoke
Scenario Add positive numbers
...

$wip
Scenario Draft feature
...
```

Set `tagFilter` in `.specconfig` (or `--tag-filter` on the CLI) to a boolean expression:

| Expression | Generates |
|---|---|
| `smoke` | Only blocks tagged `$smoke` |
| `smoke AND NOT wip` | Blocks tagged `$smoke` but not `$wip` |
| `(smoke OR regression) AND NOT wip` | Parentheses for grouping |
| *(empty)* | All blocks (no filtering) |

`$tags` are consumed by the generator and do **not** appear in the generated test code. They are separate from `@tags`, which are passed through as test annotations (Java/C# only).

---

## 8. Extra Imports / Use Statements

Use the `imports` field in `.specconfig` (or `--import` on the CLI) to inject additional statements into every generated file. The statement is written verbatim — include the full syntax for the target language.

**Java:**
```json
"imports": ["import com.example.domain.Money;"]
```

**C#:**
```json
"imports": ["using Example.Domain;"]
```

**Rust:**
```json
"imports": ["use crate::domain::Money;"]
```

Multiple statements are supported — add one entry per import.
