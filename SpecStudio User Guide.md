# SpecStudio User Guide

SpecStudio is an IDE for writing, managing, and converting specification files.
It supports `.spectable` files — a structured DSL for defining data types, business rules, calculations, and test scenarios — and can generate C# unit test scaffolding from them automatically.

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

| Area | Description |
|------|-------------|
| **Solution Explorer** (left) | Tree of projects and files. Double-click to open a file. |
| **Symbol Tree** (left, tabbed) | Hierarchy of all symbols across the project after Analyze. |
| **Editor** (center) | Tabbed editor for open files. |
| **Attribute Inspector** (right) | Shows attribute details when the cursor is on a symbol. |
| **Output Panel** (bottom) | Build, Analysis, Find Results, Diff, and Git tabs. |
| **Status Bar** | Line/column, file type, git branch. |

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

### Data Types

Built-in types need no declaration: `String`, `Integer`, `Float`, `Boolean`, `YesNo`,
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

### Domain Terms

```
DomainTerm WithdrawalAmount : Dollar
Description The amount of money a customer requests to withdraw.
Constraint Must be greater than zero.
```

### Entities and Attributes

An **Entity** or **Attributes** block defines a data class used in scenarios:

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

### Defines

Constants and reusable tables:

```
Define StandardFee = 35

Define DefaultAccount =
| Type      | Checking |
| Balance   | 100      |
| AccountID | 123-456  |
```

Reference a Define inside a step table with `=DefineName`.

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

| Format | When to use |
|--------|-------------|
| Normal `\| Col1 \| Col2 \|` (header row + data rows) | Multiple instances |
| Transposed `\| Attribute \| Value \|` (header + key/value rows) | Single instance, many fields |
| `Account Transposed` (explicit, no header row) | Single instance without header |
| `=DefineName` | Substitute a pre-defined table |

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

| Color | Elements |
|-------|----------|
| Blue | `Specification`, `Entity`, `Attributes`, `BusinessRule`, `Calculation`, `Scenario`, etc. |
| Green italic | `Description`, `Details`, `Constraint`, `Notes` |
| Purple | `Transposed` |
| Orange | `=DefineName` references |
| Grey | Attribute set references in steps |
| Teal | Built-in DataType names |

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **Tab** in a pipe table | Move to next cell; add row at end of table |
| **Ctrl+S** | Save (also strips trailing `\` from last continuation line) |
| **F12** | Go to Definition of symbol under cursor |
| **Shift+F12** | Find All References |
| **F2** | Rename Symbol (project-wide) |
| **Ctrl+Alt+F** | Format/align all pipe tables |
| **Ctrl+Shift+T** | Open table in Grid Dialog |
| **Ctrl+G** | Go to line |
| **Ctrl+F** / **Ctrl+H** | Find / Replace |
| **F7** | Analyze project |

### Context Menu (Right-click)

- **Go to Definition** — jump to where the symbol is declared
- **Find References** — list all uses in the project
- **Rename Symbol** — project-wide rename with auto-commit
- **Edit Comment…** — opens a dialog to edit multi-line Description/Details/Constraint text as a single string, then reflows it at the current editor width
- **Extract as AttributeSet…** — wrap selected table columns into a new `Attributes` block
- **Extract as Define…** — insert a `Define Name` above the current table

### Hover Tooltip

Hover over a symbol name to see its declaration summary in a tooltip.

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
- **Output directory** — where generated `.cs` files go. Relative paths are resolved from the `.specconfig` file's location. Examples: `generated`, `../tests/generated`, `C:/absolute/path`.
- **Browse…** — pick a folder.

**Target Language**
- **Language** — `CSharp` (default), `Java`, `Python` (for future converter versions).
- **Test framework** — for C#: `MSTest` (default), `NUnit`, `xUnit`. For Java: `JUnit`, `TestNG`.
- **Namespace prefix** — C# namespace prefix for generated classes. Default: `gherkinexecutor`.

**Glue File**
- **Regenerate glue stubs even if the file already exists** — by default the glue file is never overwritten so your hand-written test logic is preserved. Enable this only when you want to reset the stubs.

**Converter**
- **SpecTableConverter path** — leave blank to auto-detect. SpecStudio searches next to its own executable, then in the Visual Studio dev-build location.

### Config File Format (JSON)

The `.specconfig` file is plain JSON — you can edit it directly in any text editor:

```json
{
    "version": 1,
    "outputDirectory": "generated",
    "language": "CSharp",
    "framework": "MSTest",
    "namespace": "gherkinexecutor",
    "overwriteGlue": false,
    "converterPath": ""
}
```

### Which Config is Used?

When you build a `.spectable` file, SpecStudio searches for a `.specconfig` file starting in the same folder as the `.spectable` file and walking up to the project root. The first one found is used. If none is found, default values apply and the generated files go into a `generated/` subfolder next to the `.spectable` file.

---

## 7. Generating Unit Tests (Build)

### Build Current File

1. Open a `.spectable` file in the editor.
2. **Build > Build Current File** (or the toolbar button).

The Output panel shows progress and any errors or warnings from the converter.

### Build Project

**Build > Build Project** runs the converter on every `.spectable` file in the solution, using the project's `.specconfig` if one exists.

### What the Converter Produces

The converter (`SpecTableConverter.exe`) reads the `.spectable` file and writes to the configured output directory:

| Generated file | Overwritten? | Purpose |
|---------------|--------------|---------|
| `<AttrSet>String.cs` | Yes | External-facing data class with all-string fields |
| `<AttrSet>Typed.cs` | Yes | Internal typed class with correct C# types |
| `<SpecName>_Tests.cs` | Yes | Unit test class, one `[TestMethod]` per Scenario |
| `<SpecName>_glue.cs` | **No** (unless overwriteGlue=true) | Stub methods — you fill these in |

---

## 8. Understanding the Generated Files

### `WithdrawalInputString.cs` — String data class

```csharp
namespace gherkinexecutor.Account_Withdrawal_Rules
{
    public class WithdrawalInputString
    {
        public string Amount;

        public WithdrawalInputString(string amount)
        {
            this.Amount = amount;
        }

        public WithdrawalInputTyped ToWithdrawalInputTyped()
        {
            return new WithdrawalInputTyped(new Dollar(this.Amount));
        }

        public override string ToString()
        {
            return $"Amount={Amount}";
        }
    }
}
```

### `WithdrawalInputTyped.cs` — Typed data class

```csharp
namespace gherkinexecutor.Account_Withdrawal_Rules
{
    public class WithdrawalInputTyped
    {
        public Dollar Amount;

        public WithdrawalInputTyped(Dollar amount)
        {
            this.Amount = amount;
        }
    }
}
```

### `Account_Withdrawal_Rules_Tests.cs` — Test class

```csharp
[TestClass]
public class Account_Withdrawal_Rules
{
    [TestMethod]
    public void Test_Scenario_Withdraw_exact_balance_from_checking()
    {
        Account_Withdrawal_Rules_glue glue = new Account_Withdrawal_Rules_glue();

        // Background step
        List<List<string>> accounts = new List<List<string>>{ ... };
        glue.Given_accounts_exist_in_the_system(accounts);

        // Scenario steps
        List<WithdrawalInputString> input = new List<WithdrawalInputString>
        {
            new WithdrawalInputString("100")
        };
        glue.When_withdrawal_is_submitted(input);

        List<BalanceCheckString> result = new List<BalanceCheckString>
        {
            new BalanceCheckString("0", "true")
        };
        glue.Then_balance_is_zero(result);
    }
}
```

### Type Mapping

| SpecTable type | C# typed field | Conversion in ToTyped() |
|---------------|----------------|------------------------|
| `String`, `Text`, `Character` | `string` | `this.Field` |
| `Integer` | `int` | `int.Parse(this.Field)` |
| `Float` | `double` | `double.Parse(this.Field)` |
| `Boolean`, `YesNo` | `bool` | `bool.Parse(this.Field)` |
| `Date`, `Time`, `DateTime` | `DateTime` | `DateTime.Parse(this.Field)` |
| `Duration` | `TimeSpan` | `TimeSpan.Parse(this.Field)` |
| Custom (e.g. `Dollar`, `AccountType`) | `TypeName` | `new TypeName(this.Field)` |

---

## 9. Writing the Glue Code

The `*_glue.cs` file is generated once and then **never overwritten**. You implement the test logic here.

### Structure of a Glue Method

```csharp
public void When_withdrawal_is_submitted(List<WithdrawalInputString> values)
{
    Console.WriteLine("---  " + "When_withdrawal_is_submitted");
    foreach (WithdrawalInputString value in values)
    {
        Console.WriteLine(value);
        WithdrawalInputTyped input = value.ToWithdrawalInputTyped();
        // Call your production code:
        result = accountService.Withdraw(input.Amount);
    }
}
```

### Tips

- Call `.ToXxxTyped()` on each string object to get strongly-typed values for your production code.
- Use `AreEqual(expected, actual, message)` (from `using static Microsoft.VisualStudio.TestTools.UnitTesting.Assert`) for assertions.
- Store state between steps in fields on the glue class (e.g. `AccountService accountService = new AccountService();`).
- Each `[TestMethod]` creates a fresh glue object, so there is no shared state between tests.
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

---

## 11. Git Integration

Each project has its own git repository. SpecStudio provides:

| Action | Menu |
|--------|------|
| Commit + Push | **Git > Commit and Push** — prompts for a change reason |
| Auto-commit on save | Happens automatically after every **Ctrl+S** |
| Fetch | **Git > Fetch** |
| Pull (with conflict resolution) | **Git > Pull** |
| Diff current file | **Git > Diff Current File** (Ctrl+D) — shown in Output panel |

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
