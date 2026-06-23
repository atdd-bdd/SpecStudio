Ken — here is the **updated, comprehensive IDE Feature Set for SpecTable v2.7.3**, fully aligned with:

* **Transposed tables**
* **Cleanup block**
* **Define tables without triple quotes**
* **Examples: <AttributeSet>**
* **Enumerations using EnumerationValues**
* **Built‑in DataTypes including YesNo**
* **Built‑in AttributeSets (EnumerationValues, ValidValues)**
* **Grid tables**
* **Nested entities**
* **YesNo normalization**
* **Background + Cleanup symmetry**

This list is written _directly for IDE creators_ — precise, actionable, and structured.

And yes: it includes your requested **“Display Background”** context‑menu item in a **non‑modal dialog**.

* * *

⭐ **SpecTable v2.7.3 — IDE Feature Set**
========================================

### _A complete, implementation‑ready list for IDE authors_

* * *

1. **Structural Awareness & Navigation**
   ========================================

### ✔ 1.1 Hover‑Preview for AttributeSets, Entities, DataTypes

Hovering over:
    : Customer
    : OrderTotals
    : Color

Shows a popup with:

* Attributes
* Types
* Defaults
* Notes
* In/Out
* Whether Transposed is used
* Whether it’s a built‑in AttributeSet

### ✔ 1.2 Jump‑to‑Definition

Clicking any of these jumps to its definition:

* AttributeSet
* Entity
* DataType
* DomainTerm
* BusinessRule
* Calculation
* Define block

### ✔ 1.3 Find All References

Right‑click → “Find all uses of…” for:

* Attribute
* AttributeSet
* Entity
* Define block
* DataType
* BusinessRule
* Calculation

### ✔ 1.4 Rename Symbol (Refactor)

Renaming:

* Attribute
* AttributeSet
* Entity
* Define block
* DataType

…updates all references across the project.

* * *

2. **Smart Editing & Auto‑Generation**
   ======================================

### ✔ 2.1 Auto‑Insert Table Headers for AttributeSets

Typing:
    Given customer : Customer

IDE inserts:
    | Name | Age |
    |      |     |

### ✔ 2.2 Auto‑Insert Transposed Tables

Typing:
    Given customer : Customer Transposed

IDE inserts:
    | Name |      |
    | Age  |      |

### ✔ 2.3 Auto‑Insert Grid Tables for DataTypes

Typing:
    Given keypad : Integer

IDE inserts:
    |   |   |   |
    |   |   |   |

### ✔ 2.4 Auto‑Complete Attribute Names

Inside a table:

Typing `| Na` → suggests `Name`.

### ✔ 2.5 Auto‑Complete Define References

Typing `=` suggests all Define blocks.

### ✔ 2.6 Auto‑Generate Define Block

If user types:
    | Address | =ShippingAddress |

…but no Define exists, IDE offers:

> “Create Define block for ShippingAddress”

### ✔ 2.7 Auto‑Generate AttributeSet from Table

If a step has a table but no AttributeSet, IDE can generate:
    Attributes <Name>
    | Attribute | Type | Default | Notes | In-Out |

* * *

3. **Validation & Error Checking**
   ==================================

### ✔ 3.1 Validate Table Column Count

* Header count must match row count
* Grid tables must be rectangular

### ✔ 3.2 Validate Attribute Names

* Must exist in AttributeSet
* Suggest corrections

### ✔ 3.3 Validate DataType Values

For built‑ins:

* Integer → numeric
* Float → numeric
* Boolean → true/false
* YesNo → Y/N/yes/no/t/f/true/false
* Date → ISO‑8601
* Time → ISO‑8601
* DateTime → ISO‑8601
* Duration → ISO‑8601 duration

### ✔ 3.4 Validate Transposed Tables

* Must match AttributeSet
* Must have exactly two columns
* First column must match attribute names

### ✔ 3.5 Validate Enumerations

* Must use `Examples: EnumerationValues`
* Must have Value column

### ✔ 3.6 Validate Examples Blocks

* Must use `Examples: <AttributeSet>`
* Table must match AttributeSet

### ✔ 3.7 Validate Cleanup Block

* Only Then/And allowed
* AttributeSets must match
* Transposed allowed

* * *

4. **Visualization & UX Enhancements**
   ======================================

### ✔ 4.1 AttributeSet Inspector Panel

Sidebar showing:

* Attributes
* Types
* Defaults
* Notes
* In/Out
* Built‑in vs user‑defined

### ✔ 4.2 Entity Structure Tree

Visual tree of nested entities.

### ✔ 4.3 Define Block Preview

Hovering over `=Address1` shows the table.

### ✔ 4.4 Scenario Flow Visualization

Shows:

* Background
* Scenario steps
* Cleanup

### ✔ 4.5 **Context Menu: “Display Background”**

Right‑click inside a Scenario →

> **Display Background**

Opens a **non‑modal dialog** showing:

* Background steps
* Tables
* Transposed tables
* Define references resolved

This stays open while editing.

### ✔ 4.6 **Context Menu: “Display Cleanup”**

Same as Background, but for Cleanup block.

* * *

5. **Code Generation Support (Optional)**
   =========================================

### ✔ 5.1 Generate Models (C++/Python/C#/Java)

From:

* Entities
* AttributeSets
* DataTypes

### ✔ 5.2 Generate Test Stubs

From:

* Scenarios
* BusinessRules
* Calculations

### ✔ 5.3 Generate Serialization/Deserialization

For built‑ins and user DataTypes.

* * *

6. **Project‑Level Features**
   =============================

### ✔ 6.1 Cross‑File Symbol Indexing

* AttributeSets
* Entities
* Define blocks
* DataTypes

### ✔ 6.2 Import / Insert Resolution

Show where imported files are located.

### ✔ 6.3 Broken Reference Detection

* Missing Define blocks
* Missing AttributeSets
* Missing Entities

* * *

7. **Editing Quality‑of‑Life Features**
   =======================================

### ✔ 7.1 Table Editing Mode

* Tab moves between cells
* Auto‑align columns
* Add/remove rows with shortcuts

### ✔ 7.2 Multi‑Cursor Editing for Tables

Edit entire columns at once.

### ✔ 7.3 Convert Between Table Types

* Normal → Transposed
* Transposed → Normal
* AttributeSet → Grid (if DataType)

### ✔ 7.4 Snippets

Typing `brule` → inserts BusinessRule skeleton.

* * *

8. **SpecTable‑Specific Intelligence**
   ======================================

### ✔ 8.1 Suggest AttributeSet Names

Based on step text.

### ✔ 8.2 Suggest Define Block Names

Based on attribute names.

### ✔ 8.3 Suggest DataTypes for Attributes

Based on attribute name patterns.

### ✔ 8.4 Suggest Transposed when appropriate

If user types a vertical table manually.

* * *

9. **Testing & Simulation**
   ===========================

### ✔ 9.1 Example Runner

Validates:

* Input types
* Output types
* Missing values

### ✔ 9.2 Scenario Simulator

Executes:

* Background
* Scenario
* Cleanup

### ✔ 9.3 Coverage Report

Shows which:

* AttributeSets
* Entities
* BusinessRules
* Calculations

…are exercised by scenarios.

* * *

10. **AI‑Assisted Features (Optional)**
    =======================================

### ✔ 10.1 Generate AttributeSets from English

### ✔ 10.2 Generate Scenarios from English

### ✔ 10.3 Suggest Transposed vs Normal

### ✔ 10.4 Suggest DataTypes for new attributes

* * *

⭐ Summary — IDE Feature Set for SpecTable v2.7.3
================================================

This is the final, complete list:

* Structural navigation
* Refactoring
* Smart editing
* Validation
* Visualization
* Code generation
* Project‑level indexing
* Table editing enhancements
* SpecTable‑specific intelligence
* Testing & simulation
* AI assistance
* **Context menu: “Display Background” (non‑modal)**
* **Context menu: “Display Cleanup” (non‑modal)**

* * *

Ken — if you want, I can now:

* Produce a **formal IDE Requirements Document**
* Produce a **developer‑friendly checklist**
* Produce **mock UI screenshots**
* Produce a **SpecTable Language Server Protocol (LSP) spec**

Just tell me.
