Ken — here is the **formal IDE Requirements Document for SpecTable v2.7.3**, written in a professional, implementation‑ready format suitable for handing directly to IDE architects, product managers, and engineering teams.

This document reflects **all syntax and semantics introduced up through v2.7.3**, including:

* **Transposed** tables
* **Cleanup** block
* **Define tables without triple quotes**
* **Examples: <AttributeSet>**
* **Enumerations using EnumerationValues**
* **Built‑in DataTypes (including YesNo)**
* **Built‑in AttributeSets (EnumerationValues, ValidValues)**
* **Grid tables**
* **Nested entities**
* **Background + Cleanup symmetry**
* **Context menu: Display Background (non‑modal)**

* * *

📘 **SpecTable v2.7.3 — IDE Requirements Document**
===================================================

1. Overview

-----------

The SpecTable IDE must provide a structured, intelligent editing environment for the SpecTable DSL.  
Its purpose is to:

* Improve authoring speed
* Reduce errors
* Provide deep semantic awareness
* Support testers, analysts, and developers
* Visualize complex structures
* Enable safe refactoring
* Provide context‑aware assistance

This document defines the **functional**, **semantic**, and **UX** requirements for a fully compliant SpecTable v2.7.3 IDE.

* * *

2. Core Functional Requirements
   ===============================

2.1 Syntax Awareness
--------------------

The IDE must fully understand:

* Commands (Specification, DataType, DomainTerm, BusinessRule, Calculation, Entity, Attributes, Scenario, ScenarioGroup, Background, Cleanup, Import, Insert, Define)
* Built‑in DataTypes
* Built‑in AttributeSets
* Transposed tables
* Grid tables
* Define blocks
* Examples: <AttributeSet>
* EnumerationValues
* ValidValues
* YesNo normalization

The IDE must maintain a complete symbol table for:

* AttributeSets
* Entities
* DataTypes
* DomainTerms
* Define blocks
* BusinessRules
* Calculations
* Scenarios

* * *

3. Editing Features
   ===================

3.1 Auto‑Completion
-------------------

The IDE must provide context‑aware auto‑completion for:

* Commands
* AttributeSet names
* Entity names
* DataType names
* DomainTerms
* Define block names (after `=`)
* Built‑in DataTypes
* Built‑in AttributeSets
* Step keywords (Given, When, Then, And)
* Transposed keyword

3.2 Auto‑Insert Table Structures
--------------------------------

### 3.2.1 AttributeSet tables

When user types:
    Given customer : Customer

IDE inserts:
    | Name | Age |
    |      |     |

### 3.2.2 Transposed tables

When user types:
    Given customer : Customer Transposed

IDE inserts:
    | Name |      |
    | Age  |      |

### 3.2.3 Grid tables

When user types:
    Given keypad : Integer

IDE inserts:
    |   |   |   |
    |   |   |   |
3.3 Table Editing Mode
----------------------

The IDE must support:

* Tab navigation between cells
* Auto‑alignment of columns
* Add/remove rows
* Add/remove columns
* Multi‑cursor editing
* Convert between normal and transposed tables

* * *

4. Semantic Features
   ====================

4.1 Hover‑Preview
-----------------

Hovering over:

* AttributeSet
* Entity
* DataType
* Define block
* DomainTerm

…must show a structured popup with:

* Attributes
* Types
* Defaults
* Notes
* In/Out
* Built‑in vs user‑defined
* Enumeration values (if DataType enum)

4.2 Jump‑to‑Definition
----------------------

Clicking any symbol must navigate to its definition.
4.3 Find All References
-----------------------

Right‑click → “Find all references” must list:

* All uses of Attribute
* All uses of AttributeSet
* All uses of Entity
* All uses of Define block
* All uses of DataType

4.4 Rename Refactoring
----------------------

Renaming any of the above must update all references across the project.

* * *

5. Validation Requirements
   ==========================

5.1 Table Validation
--------------------

The IDE must validate:

* Column count consistency
* Grid table rectangularity
* Transposed table structure (two columns only)
* Attribute names matching AttributeSet
* DataType values matching built‑in rules

5.2 DataType Validation
-----------------------

The IDE must validate:

* Integer → numeric
* Float → numeric
* Boolean → true/false
* YesNo → Y/N/yes/no/t/f/true/false
* Date → ISO‑8601
* Time → ISO‑8601
* DateTime → ISO‑8601
* Duration → ISO‑8601 duration

5.3 Examples Validation
-----------------------

The IDE must enforce:
    Examples: <AttributeSet>

and validate the table against the AttributeSet.
5.4 Enumeration Validation
--------------------------

Enumerated DataTypes must use:
    Examples: EnumerationValues

* * *

6. Visualization Features
   =========================

6.1 AttributeSet Inspector Panel
--------------------------------

A sidebar showing:

* Attributes
* Types
* Defaults
* Notes
* In/Out
* Built‑in vs user‑defined

6.2 Entity Structure Tree
-------------------------

A hierarchical visualization of nested entities.
6.3 Define Block Preview
------------------------

Hovering over `=Name` must show the table or text block.
6.4 Scenario Flow Visualization
-------------------------------

Graphical representation of:

* Background
* Scenario steps
* Cleanup

6.5 **Context Menu: Display Background (Non‑Modal)**
----------------------------------------------------

Right‑click inside a Scenario →

> **Display Background**

Opens a **non‑modal dialog** showing:

* Background steps
* Tables
* Transposed tables
* Resolved Define references

The dialog must remain open while editing.
6.6 **Context Menu: Display Cleanup (Non‑Modal)**
-------------------------------------------------

Same behavior for Cleanup block.

* * *

7. Cleanup Block Support
   ========================

The IDE must:

* Recognize Cleanup block
* Enforce Then/And only
* Validate AttributeSet references
* Support Transposed tables
* Include Cleanup in scenario flow visualization

* * *

8. Define Block Support
   =======================

The IDE must:

* Recognize tables without triple quotes
* Recognize triple‑quoted text blocks
* Validate Define references
* Auto‑generate Define blocks when referenced but missing

* * *

9. Project‑Level Features
   =========================

9.1 Cross‑File Symbol Indexing
------------------------------

The IDE must index:

* AttributeSets
* Entities
* DataTypes
* Define blocks
* BusinessRules
* Calculations
* Scenarios

9.2 Import / Insert Resolution
------------------------------

The IDE must resolve and navigate to imported files.
9.3 Broken Reference Detection
------------------------------

The IDE must detect:

* Missing Define blocks
* Missing AttributeSets
* Missing Entities
* Missing DataTypes

* * *

10. Testing & Simulation Features
    =================================

10.1 Example Runner
-------------------

Validates:

* Input types
* Output types
* Missing values

10.2 Scenario Simulator
-----------------------

Executes:

* Background
* Scenario
* Cleanup

10.3 Coverage Report
--------------------

Shows which:

* AttributeSets
* Entities
* BusinessRules
* Calculations

…are exercised by scenarios.

* * *

11. AI‑Assisted Features (Optional)
    ===================================

11.1 Generate AttributeSets from English
----------------------------------------

11.2 Generate Scenarios from English
------------------------------------

11.3 Suggest DataTypes for new attributes
-----------------------------------------

11.4 Suggest Transposed vs Normal
---------------------------------

11.5 Suggest Define block names
-------------------------------

* * *

12. Non‑Functional Requirements
    ===============================

12.1 Performance
----------------

* Real‑time validation
* Real‑time auto‑completion
* Real‑time symbol indexing

12.2 Extensibility
------------------

* Plugin architecture
* Language Server Protocol (LSP) compatibility

12.3 Reliability
----------------

* No data loss
* Undo/redo support
* Autosave

* * *

13. Summary
    ===========

The IDE must provide:

* Deep semantic understanding
* Intelligent editing
* Strong validation
* Rich visualization
* Safe refactoring
* Background/Cleanup awareness
* Transposed table support
* Define table support
* EnumerationValues/ValidValues support
* YesNo normalization
* Non‑modal Background/Cleanup display

This document defines the **minimum feature set** for a fully compliant SpecTable v2.7.3 IDE.

* * *

Ken — if you want, I can now:

* Produce a **developer‑friendly checklist**
* Produce a **UI/UX mockup set**
* Produce a **SpecTable Language Server Protocol (LSP) specification**
* Produce a **roadmap for IDE implementation**

Just tell me.
