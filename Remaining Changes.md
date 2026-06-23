# SpecStudio — Remaining Proposed Changes

Features proposed in the requirements documents that have not yet been implemented.
Items are grouped by category and ordered roughly by implementation complexity.

---

## Editing & Auto-Generation

### Auto-Insert Grid Table for DataType Steps
When a step references a built-in DataType instead of an AttributeSet:
```
Given keypad : Integer
```
The IDE should auto-insert a blank grid table:
```
|   |   |   |
|   |   |   |
```
Currently only AttributeSet-referenced steps get auto-inserted headers.
*(IDE Requirements 3.2.3, IDE ADDITIONS 2.2)*

### Auto-Generate Define Block When Missing
When the user types `=Name` in a table cell and no `Define Name` exists,
offer a prompt (similar to the unknown AttributeSet prompt):
> "Create Define block for 'Name'?"

Selecting Yes should insert a `Define Name =` (or table skeleton) at the
end of the file and navigate to it.
*(IDE ADDITIONS 2.5, IDE Requirements 8)*

---

## Validation

### Validate That DataType Uses EnumerationValues or ValidValues
The analyzer currently warns if a `DataType` block has no `Examples:` section,
but does not check that the Examples AttributeSet is `EnumerationValues` or
`ValidValues`. Add a diagnostic:
> "DataType 'X' Examples: must use EnumerationValues or ValidValues"
*(IDE Requirements 5.4)*

### Validate BusinessRule/Calculation Examples — In/Out Coverage
The `checkExamples` check verifies the AttributeSet exists but does not verify
that the Examples table contains at least one `In` column and one `Out` column.
Add a warning if the referenced AttributeSet has no `In` or no `Out` attributes
in its definition.
*(IDE Requirements 5.3, IDE ADDITIONS 3.5)*

### Validate Nested Entity References
When a table cell contains `=Name`, the Define block it references should be
validated against the Entity definition for that column:
- The Define block must exist (already checked)
- If the attribute's type is an Entity name, the Define's table columns must
  match the Entity's declared attributes
*(IDE ADDITIONS 3.4)*

---

## Intelligence & Suggestions

### Suggest DataTypes for Attributes by Name Pattern
When the user is editing inside an Attributes/Entity table and types an
attribute name, suggest a DataType based on common naming conventions:

| Attribute name pattern | Suggested type |
|------------------------|----------------|
| *Date*, *DateOf*, *Birthday*, *On* | Date |
| *Time*, *At* | Time |
| *Amount*, *Price*, *Cost*, *Rate*, *Salary*, *Balance* | Float |
| *Count*, *Number*, *Quantity*, *Qty*, *Age*, *Year*, *Month*, *Day* | Integer |
| *Is*, *Has*, *Can*, *Should*, *Flag*, *Active*, *Enabled* | Boolean |
| *Description*, *Details*, *Notes*, *Comment*, *Text* | Text |
| *Email*, *Phone*, *Name*, *Address*, *Code*, *Id*, *Key*, *Url* | String |

*(IDE ADDITIONS 8.3, IDE Requirements 11.3)*

### Suggest AttributeSet Names from Step Text
When the user types a new step (e.g., `Given customer information`) and there
is no `: AttrSetName` suffix, suggest existing AttributeSet names that
semantically relate to the step text (e.g., suggest `Customer` or
`CustomerInformation`).
*(IDE ADDITIONS 8.1)*

### Suggest Define Block Names from Attribute Context
When the user types `=` in a table cell, autocomplete currently lists all
Defines. Extend this so that when the column header is a known attribute
name (e.g., `Address`), the suggestions are weighted/ordered to prefer
Defines whose names contain that attribute name first.
*(IDE ADDITIONS 8.2)*

### Suggest Transposed Layout When Appropriate
If the user manually writes a table with many rows and only 2 columns where
the first column looks like attribute names, offer a hint:
> "This table could be written as a Transposed table."
*(IDE ADDITIONS 8.4, IDE Requirements 11.4)*

---

## Code Generation

### Generate Serialization / Deserialization
Extend the converter to generate serializer/deserializer code for declared
Entities and AttributeSets. For C#: generate JSON helper classes or
`System.Text.Json` converters. For Java: generate Jackson annotated classes.
*(IDE ADDITIONS 5.3)*

---

## Table Editing

### Multi-Cursor Column Editing
Allow the user to select an entire table column and edit all cells in that
column simultaneously using multi-cursor. This requires building a column-
selection mode on top of QTextEdit's existing multi-cursor support.
*(IDE ADDITIONS 7.2, IDE Requirements 3.3)*

### Convert Grid Table ↔ AttributeSet Table
Context-menu option to convert a raw grid table (no declared AttributeSet)
into a proper `Attributes Name` block with headers, and vice versa.
Transpose already exists; this is the Grid↔Named conversion.
*(IDE ADDITIONS 7.3)*

---

## AI-Assisted Features (Optional)

### Generate AttributeSets from English
User types a plain-English description:
> "Customer has name, email, phone, and address."

The IDE (via Claude API) generates:
```
Attributes Customer
| Attribute | Type   | Default | Notes | In-Out |
| Name      | String |         |       | In     |
| Email     | String |         |       | In     |
| Phone     | String |         |       | In     |
| Address   | String |         |       | In     |
```
*(IDE ADDITIONS 10.1, IDE Requirements 11.1)*

### Generate Scenarios from English
User types:
> "When the user withdraws more than the balance, show an error."

The IDE (via Claude API) generates a Scenario skeleton with appropriate
Given/When/Then steps.
*(IDE ADDITIONS 10.2, IDE Requirements 11.2)*

---

## Performance & Polish (Phase 8)

### Autosave
Periodically save open editors to a temporary location so that work is
not lost if the application exits unexpectedly. Offer to restore unsaved
changes on next open.
*(IDE Requirements 12.3)*

### Incremental Parsing / Performance Tuning
The `SpecTableIndex::rebuildProject` currently re-parses all files on every
Analyze. For large projects, parse only files that have changed since the
last rebuild (use file modification timestamps or a hash cache).
*(Roadmap Phase 8)*

### LSP Compatibility Layer
Expose the symbol index, diagnostics, and completion engine through a
Language Server Protocol interface so that other editors (VS Code, Neovim)
can use the SpecTable intelligence without embedding SpecStudio.
*(IDE Requirements 12.2)*
