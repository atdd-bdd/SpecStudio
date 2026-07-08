# SpecStudio User Guide

SpecStudio is an IDE for writing, managing, and converting specification files.
It supports `.spectable` files — a structured DSL for defining data types, business rules, calculations, and test scenarios — and can generate unit test scaffolding in C#, Java, or Rust from them automatically.

---

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [The IDE Layout](#2-the-ide-layout)
3. [Solutions and Projects](#3-solutions-and-projects)
4. [Writing .spectable Files](#4-writing-spectable-files)
5. [The SpecTable Editor](#5-the-spectable-editor)
6. [Configuring a Project (.specconfig)](#6-configuring-a-project-specconfig)
7. [Generating Unit Tests (Build)](#7-generating-unit-tests-build)
8. [Understanding the Generated Files](#8-understanding-the-generated-files)
9. [Writing the Glue Code](#9-writing-the-glue-code)
10. [Analysis and Diagnostics](#10-analysis-and-diagnostics)
11. [Git Integration](#11-git-integration)
12. [Settings](#12-settings)

---

## 1. Getting Started

### Opening a Solution

1. Launch `SpecStudio.exe`.
2. **File > Open Solution** — choose an `.sspec` file.
3. **File > New Solution** — create a new solution and project from scratch.

Recent solutions appear under **File > Recent Solutions**.

### Creating a New Project

1. **File > New Project** — enter a name and location.
2. The project gets its own folder and a local git repository.

---

## 2. The IDE Layout

| Area                            | Description                                                |
| ------------------------------- | ---------------------------------------------------------- |
| **Solution Explorer** (left)    | Tree of projects and files. Double-click to open a file.   |
| **Symbol Tree** (left, tabbed)  | Hierarchy of all symbols across the project after Analyze. |
| **Editor** (center)             | Tabbed editor for open files.                              |
| **Attribute Inspector** (right) | Shows attribute details when the cursor is on a symbol.    |
| **Output Panel** (bottom)       | Build, Analysis, Find Results, Diff, and Git tabs.         |
| **Status Bar**                  | Line/column, file type, git branch.                        |

---

## 3. Solutions and Projects

- A **Solution** is a container for related projects. Stored as `<name>.sspec` (JSON).
- A **Project** is a folder of specification files with its own git repository.
- Adding a project: right-click the solution node in Solution Explorer > **Add New Project**.
- Files are scanned automatically from the project folder; no manual "add file" step is needed.

---

## 4. Writing .spectable Files

`.spectable` files use a structured DSL. The full grammar is summarized below.

### File Header

```
Specification Account Withdrawal Rules
Description Defines rules and scenarios for withdrawing money from accounts.
```

### Tags

Two kinds of tags can appear before any block:

**`@Tag`** — passed through to generated test annotations (e.g. `[TestCategory]` in C#, `@Tag` in JUnit):

```
@Smoke
Scenario Withdraw exact balance from checking
```

**`$Tag`** — generator-only tags used for filtering which tests are generated; never emitted into test code:

```
$WIP
Scenario Work in progress scenario
```

Tags placed **before the `Specification` line** apply to every Scenario, BusinessRule, Calculation, and DataType in the file. Individual blocks can add their own tags; the file-level tags and block-level tags are merged when evaluating filters.

```
$Skip
Specification Token Examples

$NotSkip
Scenario This scenario has both $Skip and $NotSkip tags
```

### Data Types

Built-in types need no declaration: `String`, `Integer`, `Float`, `Double`, `Boolean`, `YesNo`,
`Date`, `Time`, `DateTime`, `Duration`, `Character`, `Text`.

Custom types are declared with `DataType`:

```
DataType Dollar
Description Monetary amount in US dollars.
Examples: ValidValues
| Value | Valid |
| 0     | true  |
| 0.01  | true  |
```

**Enum DataTypes** — if the Examples AttributeSet is named `EnumerationValues`, the type is treated as a Java enum and generates `TypeName.valueOf(string)` instead of `new TypeName(string)`:

```
DataType Priority
Description Task priority level.
Examples: EnumerationValues
| Name   |
| LOW    |
| MEDIUM |
| HIGH   |
```

### Domain Terms

```
DomainTerm WithdrawalAmount : Dollar
Description The amount of money a customer requests to withdraw.
Constraint Must be greater than zero.
```

### Entities and Attributes

An **Entity** or **Attributes** block defines a data class used in scenarios.

Recognized column headers: `Name` (or `Attribute`), `Type` (or `DataType`), `Default`, `Note` / `Notes`, `In-Out` (or `In/Out`), `Multiples`. Any other header produces a warning and is ignored.

```
Entity Account
Description Represents a bank account.
| Attribute | Type        | Default  | Notes            |
| Type      | AccountType | Checking |                  |
| Balance   | Dollar      | 0        | Must be >= 0     |
| AccountID | AccountID   | (none)   |                  |

Attributes WithdrawalInput
| Attribute | Type   | Default | Notes             | In-Out |
| Amount    | Dollar | 0       | Withdrawal amount | In     |
```

**Default values** — when a step table omits a column, the missing fields are filled in from the `Default` column of the Attributes definition. This applies to normal tables, transposed tables, define-referenced tables, and CSV-inserted tables.

### Defines

Constants and reusable tables:

```
Define StandardFee = 35

Define DefaultAccount =
| Type      | Checking |
| Balance   | 100      |
| AccountID | 123-456  |
```

**Docstring defines** — a define can hold multi-line text:

```
Define WelcomeMessage =
"""
Hello, welcome to the system.
Please log in to continue.
"""
```

Reference a Define inside a step table with `=DefineName`. In the editor, hovering over `=DefineName` shows the define's value in a tooltip, and typing `=` triggers autocomplete listing all defined names.

### Business Rules and Calculations

```
BusinessRule OverdraftFee
Description Calculates the overdraft fee when a balance falls below zero.
Examples: OverdraftFeeData
| Balance | AccountType | Fee          |
| -50     | Checking    | =StandardFee |

Attributes OverdraftFeeData
| Attribute   | Type   | Default | In-Out |
| Balance     | Dollar | 0       | In     |
| Fee         | Dollar | 0       | Out    |
```

### Background

Steps shared by every scenario in the file:

```
Background:
Given accounts exist in the system: Account
| Type     | Balance | AccountID |
| Checking | 100     | 123-456   |
| Savings  | 200     | 987-654   |
```

### Scenarios

```
Scenario Withdraw exact balance from checking
Description Withdrawing the full checking balance leaves a zero balance.
Given checking account: Account Transposed
| Type      | Checking |
| Balance   | 100      |
| AccountID | 123-456  |
When withdrawal is submitted: WithdrawalInput
| Attribute | Value |
| Amount    | 100   |
Then balance is zero: BalanceCheck
| Attribute | Value |
| Balance   | 0     |
| Valid      | true  |
```

**Step table formats:**

| Format                                                          | When to use                    |
| --------------------------------------------------------------- | ------------------------------ |
| Normal `\| Col1 \| Col2 \|` (header row + data rows)            | Multiple instances             |
| Transposed `\| Attribute \| Value \|` (header + key/value rows) | Single instance, many fields   |
| `Account Transposed` (explicit, no header row)                  | Single instance without header |
| `=DefineName`                                                   | Substitute a pre-defined table |
| `Insert "file.csv"`                                             | Read rows from a CSV/TSV file  |

### Docstrings

A step can carry a multi-line text block using `"""`:

```
Given a message is sent
"""
Hello, this is the message body.
It can span multiple lines.
"""
Then the response should equal
"""
Acknowledged.
"""
```

The text is passed to the glue method as a `String` parameter. Leading whitespace common to all non-blank lines is stripped (based on the indentation of the closing `"""`). If a line does not align with the expected indentation, a warning is issued during build.

**`Insert` inside a docstring** — embed the contents of a file:

```
Given a document is loaded
"""
Insert "templates/welcome.txt"
"""
```

When the inserted file is a CSV or TSV, it is converted to pipe-table format before being embedded. The delimiters `"..."`, `'...'`, and `<...>` are all accepted for the filename. The path is relative to the project root.

### `Insert` in Step Table Position

A CSV or TSV file can also be used directly as a step table:

```
Given a table : CSVContents
Insert "data/TableExample.csv"
```

The file's header row is mapped to the Attributes fields by name (case-insensitive). Quoted fields containing commas are handled correctly. A warning is issued for any CSV header that does not match an Attributes field. Missing fields are filled from the Attributes `Default` column.

### Comments

Lines beginning with `#` are comments and are ignored by the parser and analyzer. In pipe tables, rows whose first non-whitespace character is `#` are skipped.

### Multi-line Text Fields

`Description`, `Details`, `Constraint`, and `Notes` support continuation with `\`:

```
Details \
  First paragraph. \
  Second paragraph.
```

The last line must **not** end with `\`. SpecStudio's Save command enforces this automatically.

---

## 5. The SpecTable Editor

### Syntax Highlighting

| Color        | Elements                                                                                 |
| ------------ | ---------------------------------------------------------------------------------------- |
| Blue         | `Specification`, `Entity`, `Attributes`, `BusinessRule`, `Calculation`, `Scenario`, etc. |
| Green italic | `Description`, `Details`, `Constraint`, `Notes`                                          |
| Purple       | `Transposed`                                                                             |
| Orange       | `=DefineName` references                                                                 |
| Grey         | Attribute set references in steps                                                        |
| Teal         | Built-in DataType names                                                                  |

### Keyboard Shortcuts

| Key                     | Action                                                      |
| ----------------------- | ----------------------------------------------------------- |
| **Tab** in a pipe table | Move to next cell; add row at end of table                  |
| **Ctrl+S**              | Save (also strips trailing `\` from last continuation line) |
| **Ctrl+/**              | Toggle `# ` comment prefix on current line or selection     |
| **F12**                 | Go to Definition of symbol under cursor                     |
| **Shift+F12**           | Find All References                                         |
| **F2**                  | Rename Symbol (project-wide)                                |
| **Ctrl+Alt+F**          | Format/align all pipe tables                                |
| **Ctrl+Shift+T**        | Open table in Grid Dialog                                   |
| **Ctrl+G**              | Go to line                                                  |
| **Ctrl+F** / **Ctrl+H** | Find / Replace                                              |
| **F7**                  | Analyze project                                             |

### Autocomplete

- **After `:` on a Given/When/Then/Examples line** — shows all `Attributes` and `Entity` names in the project.
- **In the Type column of an Attributes table** — shows built-in types and all declared `DataType` names.
- **After typing `=`** — shows all `Define` names as `=DefineName` completions.

### Hover Tooltip

Hover over a symbol name to see its declaration summary. Hovering over `=DefineName` shows the define's value — inline for scalar defines, as an HTML table for table defines.

### Context Menu (Right-click)

- **Go to Definition** — jump to where the symbol is declared
- **Find References** — list all uses in the project
- **Rename Symbol** — project-wide rename with auto-commit
- **Edit Comment…** — opens a dialog to edit multi-line Description/Details/Constraint text
- **Extract as AttributeSet…** — wrap selected table columns into a new `Attributes` block
- **Extract as Define…** — insert a `Define Name` above the current table

### Attribute Inspector

The **Attribute Inspector** panel (right side) shows the field list for whatever symbol the cursor is on. Open it with **View > Attribute Inspector**.

---

## 6. Configuring a Project (.specconfig)

A `.specconfig` file controls how `SpecTableConverter` generates unit tests for a project.

### Creating a Config File

1. In Solution Explorer, right-click the project folder and choose **New File**.
2. Name it e.g. `MyProject.specconfig`.
3. The file opens in the SpecConfig Editor automatically.

Or create it manually in the project folder — any file ending in `.specconfig` is recognised.

### The SpecConfig Editor

Opening a `.specconfig` file shows a form with these sections:

**Output**

- **Output directory** — where generated files go. Relative paths are resolved from the `.specconfig` file's location. Examples: `generated`, `../tests/generated`, `C:/absolute/path`.
- **Browse…** — pick a folder.

**Target Language**

- **Language** — `CSharp` (default), `Java`, `Rust`.
- **Test framework** — for C#: `MSTest` (default), `NUnit`, `xUnit`. For Java: `JUnit` (default), `TestNG`.
- **Namespace / package prefix** — C# namespace or Java package prefix for generated classes. Leave blank to use no prefix.

**Tag Filter**

- **Tag filter expression** — a boolean expression of `$Tag` names controlling which Scenarios/BusinessRules/Calculations/DataTypes are generated. Examples:
  - `$Smoke` — only generate blocks tagged `$Smoke`
  - `NOT $Skip` — generate everything except blocks tagged `$Skip`
  - `$Smoke AND NOT $WIP` — smoke tests that are not works-in-progress
  - Empty — generate all blocks (default)

  Operators `AND`, `OR`, `NOT` are case-insensitive. Parentheses are supported. The `$` prefix is optional in the expression (both `$Skip` and `Skip` work).

  The test file is always written even if all blocks are filtered out.

**Glue File**

- **Regenerate glue stubs even if the file already exists** — by default the glue file is never overwritten so your hand-written test logic is preserved. Enable this only when you want to reset the stubs.

**Converter**

- **SpecTableConverter path** — leave blank to auto-detect. SpecStudio searches next to its own executable, then in the Visual Studio dev-build location.

### Config File Format (JSON)

```json
{
    "version": 1,
    "outputDirectory": "generated",
    "language": "Java",
    "framework": "JUnit",
    "namespace": "com.example",
    "overwriteGlue": false,
    "tagFilter": "NOT $Skip",
    "converterPath": ""
}
```

### Which Config is Used?

When you build a `.spectable` file, SpecStudio searches for a `.specconfig` file starting in the same folder as the `.spectable` file and walking up to the project root. The first one found is used. If none is found, default values apply.

---

## 7. Generating Unit Tests (Build)

### Build Current File

1. Open a `.spectable` file in the editor.
2. **Build > Build Current File** (or the toolbar button).

The Output panel shows progress and any warnings or errors from the converter.

### Build Project

**Build > Build Project** runs the converter on every `.spectable` file in the solution, using the project's `.specconfig` if one exists.

### What the Converter Produces (Java)

| Generated file             | Overwritten?                        | Purpose                                           |
| -------------------------- | ----------------------------------- | ------------------------------------------------- |
| `<AttrSet>String.java`     | Yes                                 | Data class with all-String fields                 |
| `<AttrSet>Typed.java`      | Yes                                 | Data class with correctly-typed fields            |
| `<SpecName>_Test.java`     | Yes                                 | Unit test class, one `@Test` per Scenario         |
| `<SpecName>_glue.java`     | **No** (unless overwriteGlue=true)  | Stub methods — you fill these in                  |

### What the Converter Produces (C#)

| Generated file          | Overwritten?                        | Purpose                                           |
| ----------------------- | ----------------------------------- | ------------------------------------------------- |
| `<AttrSet>String.cs`    | Yes                                 | Data class with all-string fields                 |
| `<AttrSet>Typed.cs`     | Yes                                 | Data class with correctly-typed fields            |
| `<SpecName>_Tests.cs`   | Yes                                 | Unit test class, one `[TestMethod]` per Scenario  |
| `<SpecName>_glue.cs`    | **No** (unless overwriteGlue=true)  | Stub methods — you fill these in                  |

### Warnings and Errors

The converter emits messages in the Output panel:

- **WARNING** — a recoverable issue (unknown type, unrecognised column header, missing CSV field, text block misalignment). Generation continues.
- **ERROR** — a fatal issue. The test file is not written.

---

## 8. Understanding the Generated Files

### Java Example

Given:

```
Attributes WithdrawalInput
| Attribute | Type   | Default |
| Amount    | Dollar | 0       |
| Account   | String |         |
```

**`WithdrawalInputString.java`**

```java
public class WithdrawalInputString {
    public String amount;
    public String account;

    public WithdrawalInputString(String amount, String account) {
        this.amount = amount;
        this.account = account;
    }

    public WithdrawalInputTyped toWithdrawalInputTyped() {
        return new WithdrawalInputTyped(
            new Dollar(this.amount),
            this.account
        );
    }

    @Override public String toString() { return "Amount=" + amount + ", Account=" + account; }

    @Override public boolean equals(Object o) { ... }
    @Override public int hashCode() { ... }
}
```

**`Test_Account_Withdrawal.java`**

```java
@Test
public void Scenario_Withdraw_exact_balance() {
    Account_Withdrawal_glue glue = new Account_Withdrawal_glue();

    List<WithdrawalInputString> objectList1 = new ArrayList<>();
    objectList1.add(new WithdrawalInputString("100", "123-456"));
    glue.Given_checking_account(objectList1);

    List<WithdrawalInputString> objectList2 = new ArrayList<>();
    objectList2.add(new WithdrawalInputString("0", "123-456"));
    glue.Then_balance_is_zero(objectList2);
}
```

### Java Type Mapping

| Spec type                             | String field | Typed field   | Conversion in `toTyped()`                      |
| ------------------------------------- | ------------ | ------------- | ---------------------------------------------- |
| `String`, `Text`, `Character`         | `String`     | `String`      | `this.field`                                   |
| `Integer`, `Int`, `Long`              | `String`     | `int`         | `Integer.parseInt(this.field)`                 |
| `Float`, `Decimal`, `Double`          | `String`     | `double`      | `Double.parseDouble(this.field)`               |
| `Boolean`, `YesNo`, `Bool`            | `String`     | `boolean`     | `this.field.equalsIgnoreCase("true") \|\| ...` |
| `Date`                                | `String`     | `LocalDate`   | `LocalDate.parse(this.field)`                  |
| `Time`                                | `String`     | `LocalTime`   | `LocalTime.parse(this.field)`                  |
| `DateTime`                            | `String`     | `LocalDateTime` | `LocalDateTime.parse(this.field)`            |
| `Duration`                            | `String`     | `Duration`    | `Duration.parse(this.field)`                   |
| Enum DataType (EnumerationValues)     | `String`     | `TypeName`    | `TypeName.valueOf(this.field)`                 |
| Other custom type (e.g. `Dollar`)     | `String`     | `TypeName`    | `new TypeName(this.field)`                     |
| Unknown (not a declared DataType)     | `String`     | `String`      | `this.field` *(warning issued)*                |

### C# Type Mapping

| Spec type                             | C# typed field | Conversion in `ToTyped()`    |
| ------------------------------------- | -------------- | ---------------------------- |
| `String`, `Text`, `Character`         | `string`       | `this.Field`                 |
| `Integer`                             | `int`          | `int.Parse(this.Field)`      |
| `Float`, `Double`                     | `double`       | `double.Parse(this.Field)`   |
| `Boolean`, `YesNo`                    | `bool`         | `bool.Parse(this.Field)`     |
| `Date`, `Time`, `DateTime`            | `DateTime`     | `DateTime.Parse(this.Field)` |
| `Duration`                            | `TimeSpan`     | `TimeSpan.Parse(this.Field)` |
| Custom (e.g. `Dollar`, `AccountType`) | `TypeName`     | `new TypeName(this.Field)`   |

### Docstring Steps

A step with a `"""` docstring generates a call passing the text as a `String` parameter:

```java
// Java
glue.Given_a_message_is_sent("Hello, this is the message body.\nIt can span multiple lines.");
```

```csharp
// C#
glue.Given_a_message_is_sent("Hello, this is the message body.\nIt can span multiple lines.");
```

### Typed Grid Steps (DataType columns)

A step referencing a `DataType` name (e.g. `:Integer`) generates a `List<List<BoxedType>>`:

```
Given a table of integers : Integer
| 1 | 2 |
| 3 | 4 |
```

```java
List<List<Integer>> objectList1 = new ArrayList<>();
objectList1.add(List.of(1, 2));
objectList1.add(List.of(3, 4));
glue.Given_a_table_of_integers(objectList1);
```

---

## 9. Writing the Glue Code

The `*_glue` file is generated once and then **never overwritten**. You implement the test logic here.

### Java Glue Structure

```java
public void When_withdrawal_is_submitted(List<WithdrawalInputString> values) {
    for (WithdrawalInputString value : values) {
        System.out.println(value);
        WithdrawalInputTyped input = value.toWithdrawalInputTyped();
        // Call your production code:
        result = accountService.withdraw(input.amount);
    }
}
```

### C# Glue Structure

```csharp
public void When_withdrawal_is_submitted(List<WithdrawalInputString> values)
{
    foreach (WithdrawalInputString value in values)
    {
        Console.WriteLine(value);
        WithdrawalInputTyped input = value.ToWithdrawalInputTyped();
        result = accountService.Withdraw(input.Amount);
    }
}
```

### Tips

- Construct a `*Typed` object from a `*String` object using the `*Typed` constructor: `new WithdrawalInputTyped(value)` (Java) or `new WithdrawalInputTyped(value)` (C#). The conversion logic lives in `*Typed`, keeping `*String` as a plain data holder.
- Store state between steps in fields on the glue class.
- Each test method creates a fresh glue object, so there is no shared state between tests.
- Use `assertIterableEquals` (Java) or `CollectionAssert.AreEqual` (C#) to compare lists of data objects — the generated `equals()`/`hashCode()` overrides make this work correctly.
- `DNCString = "?DNC?"` is a sentinel you can use for "Do Not Check" fields.

---

## 10. Analysis and Diagnostics

Press **F7** or **Build > Analyze** to run the SpecTable analyzer across all project files.

Diagnostics appear in the **Analysis** tab of the Output panel. Double-click any diagnostic to jump to the file and line.

Error squiggles appear in the editor after analysis. The **Symbol Tree** panel (View > Symbol Tree) populates with all entities, attributes, business rules, etc.

### What the Analyzer Checks

- Missing `Description` on `BusinessRule`, `Calculation`, `DataType`
- `Examples:` sections referencing an undeclared `Attributes` or `Entity`
- `=DefineName` references where the Define is not declared
- Table column count inconsistencies
- Step attribute set references where the set is not declared
- Import file existence (for external-file imports)

### Build Warnings

The converter (run during Build) issues warnings for:

- Unrecognized column header in an `Attributes` or `Entity` table
- Unknown field type that is not a declared DataType
- CSV header column that does not match any Attributes field
- Text block (`"""`) lines that do not align with the closing `"""`

---

## 11. Git Integration

Each project has its own git repository. SpecStudio provides:

| Action                          | Menu                                                         |
| ------------------------------- | ------------------------------------------------------------ |
| Commit + Push                   | **Git > Commit and Push** — prompts for a change reason      |
| Auto-commit on save             | Happens automatically after every **Ctrl+S**                 |
| Fetch                           | **Git > Fetch**                                              |
| Pull (with conflict resolution) | **Git > Pull**                                               |
| Diff current file               | **Git > Diff Current File** (Ctrl+D) — shown in Output panel |

---

## 12. Settings

Open **Edit > Settings** (or **Tools > Settings**).

### General

- Default project location — base folder used when creating new projects.

### Appearance

- Font family and size for the editor, output panel, and UI.
- Dark theme toggle.

### Editors

- Associate file extensions with external programs (e.g. open `.xlsx` in Excel).
- "Use OS Defaults" fills in the system-registered applications.

### FeatureX

- Implicit folder import, unique scenario/step name enforcement, step suggestion scope.

---

*SpecStudio is built with Qt 6 / C++17. Source: https://github.com/atdd-bdd/SpecStudio*
