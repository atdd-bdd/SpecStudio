# SpecTable v3.3.a — Specification Document

*Adds the named comment **Uses**, and `\` to continue a named comment on the next line.*

## 1. Overview

SpecTable v3.3.a is a structured, table‑driven specification language for defining entities, collections, scenarios, and expected behaviors.

Version **3.3.a** introduces:

1. **Named Comments: Uses** A formal comment block describing dependencies, underlying primitives, or referenced business rules.
2. **Comment continuation** A line beginning with `\` continues the named comment above it, so a long `Description` need not repeat the keyword.
3. All features from v3.3:
   * Collections
   * Vertical tables
   * Reference assignment (`=`)
   * Updated grammar and semantics

## 2. Core Language Elements

### 2.1 Specification Header

```
Specification <Title>
```

Defines the name of the specification file.

### 2.2 Entities

Entities describe structured data types.

```
Entity <Name>
Uses <Comment>?
| Attribute | Type | Default | Notes |
| A1        | T1   | D1      | N1    |
| A2        | T2   | D2      | N2    |
```

#### Rules
* `<Name>` must be unique.
* `Uses` is optional.
* `Uses` may describe:
  * underlying primitive types
  * related business rules
  * referenced collections
* `Type` may be primitive, another Entity, or a Collection.
* `Default` and `Notes` are optional.

### 2.3 Collections

```
Collection <Name>
Uses <Comment>?
| DataType | Minimum | Maximum | Notes |
| <Type>   | <Min>   | <Max>   | <Notes> |
```

#### Rules
* `<Type>` must be an Entity.
* `Uses` may describe:
  * underlying primitive types of the Entity
  * rules governing collection behavior
* `<Min>` and `<Max>` define cardinality.

### 2.3.1 Spaces in a cell: `~`

A cell is trimmed, so a value whose leading or trailing space matters cannot be
written literally -- the space is gone before any type sees it. A tilde stands
for a space and is converted back when the cell is read:

```
| ~Checking  |   gives " Checking"
| !Type:Bank~ |  gives "!Type:Bank "
```

Every generator does this, so the rule holds in all nine languages. A tilde is
therefore not an ordinary character in a cell; a value that needs one must say
so some other way.

### 2.4 Vertical Tables

A vertical table is a horizontal table transposed. The attribute names run down
the first column instead of across the first row, and each further column is
another instance. Nothing else changes: the same attributes are required, the
same values are allowed, and a missing or unknown attribute is reported exactly
as it would be the other way round.

It has **no header row**. The first column already names the attributes, so
there is nothing for a header to say.

```
<Entity> Vertical
Uses <Comment>?
| A1 | V1 |
| A2 | V2 |
```

Two instances, written side by side:

```
<Entity> Vertical
| A1 | V1 | V1' |
| A2 | V2 | V2' |
```

A step may carry more than one modifier, in any order. They answer different
questions -- `Vertical` is how the table is laid out, `CompareOnly` is which of
its columns are compared, `EveryCell` is what a cell holds -- so
`: Order Vertical CompareOnly` is a transposed table that checks only the
attributes it names.

**The word is optional.** The step names its attribute set, so the attribute
names are known before the table is read, and that is enough to see which way
it runs: a table whose first row is all attribute names is horizontal; one
whose first column is all attribute names, when its first row is not, is a
transposed one and is read exactly as if `Vertical` had been written. One
attribute with one value can be written either way, and there is no ambiguity:
the first cell is the attribute's name, so `| Reference | A-4 |` and
`| Reference |` over `| A-4 |` are the same instance. A table that fits
neither reading is taken as horizontal, and reported as such. Write `Vertical`
where a reader would want the orientation stated; leave it out where the table
makes it plain.

### 2.4.1 EveryCell

A table naming an Entity is normally one row per instance, with a column per
attribute. `EveryCell` says the other thing: the table is a grid, and **each
cell holds the text form of the named type**.

```
Given the ingredients are : Ingredient EveryCell
| Sugar 200 | Butter 250 |
| Salt 5    | Yeast 7    |
```

Four Ingredients, not a table of two columns called "Sugar 200" and
"Butter 250". Each cell is read by that type's `fromText`, which is the same
text form `toString` writes: values space separated, a value containing a space
in double quotes, a nested block in single quotes.

Without `EveryCell` this reading was unavailable for an Entity -- naming one
always meant a table of attributes. A DataType already had it implicitly, and
still does; `EveryCell` states it and extends it to Entities.

A `Define` may supply a cell. `=Rent` expands before the cell is converted, so
`| =Rent | 5.00 USD |` is two instances. A **table-form** Define cannot: it is
rows rather than one value, and there is nothing to hand the constructor.

#### Rules
* The table has no header row -- every row is data.
* Each cell must hold a complete text form of the named type.
* The type may be an Entity, an Attributes block, or a DataType.

#### Rules
* Defines one instance per value column, so a table of two columns defines one
  instance and a table of four defines three.
* The first column names attributes. There is no header row.
* A missing attribute, or one the Entity does not declare, is an error on the
  same terms as in a horizontal table.
* `Uses` may describe:
  * supporting rules
  * referenced Define blocks
  * underlying primitive types

### 2.5 Reference Assignment (`=`)

```
<Entity> Vertical
| Items | =InitialItems |
```

#### Rules
* `=` means “use the previously defined value.”
* Works for Entities, Collections, and primitive values.
* Must refer to a valid Define block.

### 2.6 Define Blocks

A Define names a value so that `=Name` can stand for it elsewhere. It has
four forms.

```
Define <Name> = <Value>            # one value, to the end of the line

Define <Name> =                    # one multi-line value
"""
<Text>
"""

Define <Name>                      # a table, used in place of a step table
Uses <Comment>?
| Attribute1 | Value1 |
| Attribute2 | Value2 |

Define                             # several one-line Defines at once
| Name | Value | Notes                    |
| TBR  | -1    | Roll has not occurred    |
| TBS  | -1    | Score not yet computable |
```

#### Rules
* A `#` comment may follow the value of the one-line form; it is not part of
  the value. `Define TBR = -1  # not yet rolled` defines `-1`.
* A bare `Define` with no name is followed by a table whose header has a
  `Name` and a `Value` column; each row is exactly a one-line Define. Other
  columns, such as `Notes`, are documentation.
* `Uses` may describe:
  * the purpose of the block
  * related rules
  * underlying primitive types

### 2.7 Scenario Structure

```
Given <Name> : <Type>
Uses <Comment>?
<Step Table or Vertical>

When <Action> : <Type>
Uses <Comment>?
<Step Table or Vertical>

Then <Name> is : <Type>
Uses <Comment>?
<Step Table or Reference>
```

#### Rules
* A step that names an attribute set must be followed by its table or by a
  `=Reference` to a table-form Define. A step with neither is an error, not a
  step with an empty table.
* A step with no `: <Type>` takes no table; it may carry a docstring, or
  nothing at all.
* `Uses` may describe:
  * business rules applied
  * referenced Define blocks
  * supporting calculations

### 2.8 Tags

A tag labels the block written under it. There are two kinds, told apart by
their first character, and they do different things.

```
@smoke @checkout $ready
Scenario Add items
Given item collection is : OrderItemCollection
| Name | Price | Quantity | ItemTotal |
```

* **`@Name`** reaches the generated test, as whatever the target framework uses
  to categorise one -- a JUnit `@Tag("smoke")`, for instance.
* **`$Name`** is read by the generator and never emitted. It exists only so a
  filter can decide whether to generate the block at all.

#### Rules

* A tag name is letters, digits and underscores. There is no quoted form and no
  hyphen.
* Any number of tags of either kind may stand on one line, and tag lines may be
  stacked. Both kinds may share a line.
* Tags apply to the **next block**: a `Scenario`, a `BusinessRule`, a
  `Calculation`, a `DataType`, or the `Specification` line, where they apply to
  every block in the file.
* **A tag must sit immediately above its block.** A blank line between them
  clears the tag -- that is the rule that makes a tag belong to the block below
  it rather than to the next one that happens to follow. A `$tag` cleared this
  way is reported where it was written (section 4.7); an `@tag` is not.
* A tag line is not a comment line: `#` still starts a comment, and a tag
  inside comment text is text.
* **Only a blank line clears a tag; a declaration does not.** A tag is held
  until the next block that takes one, so a tag written above an `Entity`,
  `Attributes`, `Collection`, `Define`, `DomainTerm` or `ScenarioGroup` line
  passes over it and lands on the `Scenario` or rule that follows. Two
  consequences worth knowing:
  * A tag of either kind above a `ScenarioGroup` applies to the **first
    scenario in the group**, not the group.
  * `$wip` above an `Attributes` block, with no blank line between that block
    and the scenario after it, tags **that scenario**.

  Writing the tag directly above the block it means, which is the rule above,
  avoids both.

## 3. Formal Grammar (v3.3.a)

```
Specification      ::= TagLine* "Specification" Identifier

TagLine            ::= (TestTag | GeneratorTag)+        # any order, any mix
TestTag            ::= "@" Identifier                   # emitted into the test
GeneratorTag       ::= "$" Identifier                   # filtering only
TaggableBlock      ::= TagLine* (Scenario | BusinessRule | Calculation | DataType)

Entity             ::= "Entity" Identifier Uses? EntityTable
EntityTable        ::= Table(AttributeRow+)

Collection         ::= "Collection" Identifier Uses? CollectionTable
CollectionTable    ::= Table(CollectionRow)

DefineBlock        ::= DefineValue | DefineText | DefineTable | DefineList
DefineValue        ::= "Define" Identifier "=" Value Comment?
DefineText         ::= "Define" Identifier "=" DocString
DefineTable        ::= "Define" Identifier Uses? Table(Row+)
DefineList         ::= "Define" Table(DefineRow+)           # header has Name and Value

Scenario           ::= (GivenStep WhenStep ThenStep)+

GivenStep          ::= "Given" Identifier ":" Type Modifier* Uses? (Table | Reference)
WhenStep           ::= "When" Action ":" Type Modifier* Uses? (Table | Reference)
ThenStep           ::= "Then" Identifier "is" ":" Type Modifier* Uses? (Table | Reference)

Modifier           ::= "Vertical" | "CompareOnly" | "EveryCell"

TransposedRow      ::= "|" AttributeName ("|" Value)+ "|"

Uses               ::= "Uses" CommentText Table?
NamedComment       ::= ("Description" | "Details" | "Notes" | "Constraint" | "Uses")
                       CommentText Continuation* Table?
Continuation       ::= "\" CommentText                 # first character of the next line

Reference          ::= "=" Identifier
```

## 4. Semantics (v3.3.a)

### 4.1 Uses Semantics

* `Uses` is a **named comment**, not executable logic.
* It may appear on:
  * Entities and Attributes
  * Collections
  * Define blocks
  * BusinessRule, Calculation and DataType blocks
  * Scenarios
  * Scenario steps, including vertical ones
* It follows the line it describes, and may be written over several lines --
  each further `Uses` is appended to the one before.
* It does not end a table: a `Uses` between a step and its table leaves the
  table attached to that step.
* It must contain human‑readable text.

### 4.1.1 Continuing a named comment with `\`

* A line whose first character is a backslash **continues the named comment on
  the line above it**, so a long `Description` can be written over several lines
  without repeating the keyword.

  ```
  DataType Amount
  Description An amount of money,
  \ written the way a person writes it,
  \ and refused when it is not.
  ```

  The text is joined with a single space, so every reader of the comment sees
  one line: the `Description` above is `An amount of money, written the way a
  person writes it, and refused when it is not.`

* It must be the **very next line**. A blank line or anything else in between
  breaks the chain.
* A continuation may follow another continuation, so a comment can run to any
  length.
* Like the comment it continues, it does **not** end a table: a continuation
  between a step and its table leaves the table attached to that step.
* A backslash line that follows anything other than a named comment continues
  nothing. It is still treated as a comment and discarded, but it is **reported
  as a warning**, because the writer meant it to attach to something.
* It is documentation only. Nothing is generated from it today, though the
  whole of a `Description` may later be used.

### 4.1.2 A table under a named comment

* Any named comment -- `Description`, `Details`, `Notes`, `Constraint` or
  `Uses` -- may be followed directly by a table. The table is part of the
  comment: a business rule is often clearer as a small grid of conditions and
  outcomes than as a sentence, and this is where such a grid goes.

  ```
  BusinessRule Discount
  Description Ten percent over a hundred
  Details The bands are
  | From | To  | Rate |
  | 0    | 99  | 0%   |
  | 100  |     | 10%  |
  Examples: DiscountInput
  | Amount | Rate |
  | 150    | 10   |
  ```

* It is documentation. Nothing reads it, nothing is generated from it and
  nothing asserts it -- which is exactly how it differs from an `Examples:`
  table.
* It must follow the comment directly; a blank line or any other line in
  between ends the comment.
* A block that is waiting for its own table takes the table instead: an
  `Attributes` header, a step naming an attribute set, an `Examples:` line.
  A named comment between such a block and its table leaves the table where
  it was going.
* It may describe:
  * underlying primitive types
  * related business rules
  * referenced calculations
  * dependencies
  * purpose or intent

### 4.2 Entity Semantics

* Attributes must match declared types.
* `Uses` may clarify primitive types or rule dependencies.

### 4.3 Collection Semantics

* Must contain between `Minimum` and `Maximum` items.
* `Uses` may describe collection rules or primitive types.

### 4.4 Vertical Semantics

* A vertical table is a horizontal table transposed, and means the same thing.
* Produces one instance per value column: two columns is one instance, four is
  three.
* Has no header row -- the first column names the attributes.
* A missing or unknown attribute is reported as it would be in a horizontal
  table.
* The word `Vertical` is optional (section 2.4): a table whose first column
  is all attribute names, when its first row is not, is read as transposed
  without it. One attribute with one value is the same instance either way.

### 4.5 Reference Semantics

* `=Name` must refer to a valid Define block.
* Must match expected type.
* A one-line or multi-line Define may stand in a cell of a step table, a cell
  of an `Examples` table, or the `Default` column of an Attributes or Entity
  table. The cell is read as though the value had been written there:
  `| Roll1 | Pins | =TBR |` gives Roll1 the default `-1`.
* A table-form Define stands only in place of a whole step table.

### 4.6 Scenario Semantics

* Given establishes initial state.
* When applies transformations.
* Then asserts final state.
* `Uses` may describe:
  * rules applied
  * supporting calculations
  * referenced Define blocks

### 4.7 Tag Semantics

* An **`@tag`** is carried through to the generated test and means whatever the
  target framework makes of it. It never affects what is generated.
* A **`$tag`** is consumed by the generator. It appears nowhere in the output --
  not in the test, not in the glue, not in a comment.
* The tags that apply to a block are the block's own plus those written above
  the `Specification` line, which belong to every block in the file.

#### Filtering

* The configuration carries one boolean expression, `tagFilter`. A block is
  generated when the expression is true of its tags; an empty expression
  generates everything.

  ```
  smoke                            only blocks tagged $smoke
  NOT wip                          everything except unfinished work
  smoke AND NOT wip
  (smoke OR regression) AND NOT draft
  ```

* `AND`, `OR`, `NOT`, the parentheses and the tag names are all
  **case-insensitive**: `READY or WIP` means the same as `ready OR wip`.
* A tag name in the expression may be written **with or without its `$`** --
  `NOT $wip` and `NOT wip` are the same filter. The `@` kind cannot be filtered
  on at all; that is what a test runner's own selection is for.
* A name that no block carries is simply false. An unknown tag is not an error,
  so a filter can be written before the tag it names exists.
* Filtering happens at generation time, so a filtered-out block produces no
  test. **Its glue stub is still written**, deliberately: implementing a
  filtered block is then write the code, fill in the glue, take the tag off.
  Were stubs filtered too, the tag would have to come off before there was any
  glue to fill in, and the build would break in between.

#### A tag that marks nothing

A blank line clears a pending tag, so a `$tag` separated from its block marks
nothing and the block is generated as though it were untagged. That is silent
in both directions -- a block believed skipped is tested, and after an edit
that inserts a line, a block believed tested is skipped, which looks exactly
like a passing test. So the parser reports it where the tag was written:

```
$Orphan

Scenario Separated By A Blank Line
```

> `$Orphan marks nothing -- a blank line ends a tag, so what follows is
> generated as though it were untagged. Put the tag immediately above the block
> it belongs to.`

It is a warning, not an error: generation continues. The same is said for a
`$tag` at the end of a file with no block after it. An `@tag` cleared the same
way is not reported -- it costs a category on a test, not a test.

## 5. Full Example (v3.3.a)

### 5.1 Entities

```
Entity Item
Uses Underlying primitive types: Name → LimitedText, Quantity → Integer
| Attribute | Type        | Default | Notes |
| Name      | LimitedText |         |       |
| Quantity  | Integer     | 1       |       |

Collection ItemCollection
Uses Items are of type Item; underlying primitives: LimitedText, Integer
| DataType | Minimum | Maximum | Notes |
| Item     | 0       | 100     |       |

Entity ShoppingCart
Uses Uses ItemCollection and SimpleText; supports business rule AddItem
| Attribute | Type           | Default | Notes |
| Items     | ItemCollection |         |       |
| Orderer   | SimpleText     |         |       |
```

### 5.2 Simple Collection Manipulation

```
Given Items : ItemCollection
Uses Initial item list
| Name     | Quantity |
| Widget   | 1        |

When item added : Item Vertical
Uses Business rule: AddItem
| Name     | Widget2 |
| Quantity | 3       |

Then Items are : ItemCollection
Uses Final expected list
| Name     | Quantity |
| Widget   | 1        |
| Widget2  | 3        |
```

### 5.3 Define Blocks

```
Define InitialItems
Uses Starting inventory
| Name     | Quantity |
| Widget   | 1        |
| Widget2  | 3        |

Define FinalItems
Uses Expected inventory after AddItem
| Name     | Quantity |
| Widget   | 1        |
| Widget2  | 3        |
| Widget4  | 2        |
```

### 5.4 ShoppingCart Scenario

```
Given cart is : ShoppingCart Vertical
Uses Initial cart setup
| Orderer | Bill          |
| Items   | =InitialItems |

When item added : Item Vertical
Uses Business rule: AddItem
| Name     | Widget4 |
| Quantity | 2       |

Then cart is : ShoppingCart Vertical
Uses Final cart state
| Orderer | Bill        |
| Items   | =FinalItems |
```

## 6. Deprecated Features

### 6.1 Multiples Column

Removed in v3.3. Collections replace all multiplicity semantics.

## 7. Version Notes

### Document revision 2026-10-17

* Tags were in the language, in all nine generators and in the IDE, and in no
  version of this document: sections 2.8, 4.7 and the grammar now carry them --
  the two kinds, what each one does, that a blank line clears a pending tag and
  that a cleared `$tag` is reported, the filter expression's case-insensitivity
  and optional `$`, and that a filtered-out block still gets its glue stub.
* Writing it found that a line mixing the two kinds kept only the sigil it
  started with: `@smoke $ready` dropped `$ready` silently, so a filter on
  `ready` skipped a scenario that carried it. Both kinds are read from a tag
  line now. No specification in the test repositories mixed them, so nothing
  already written changes.
* Also stated, having been found by trying it rather than by reading the
  parser: only a blank line clears a tag, so a tag above a declaration or a
  `ScenarioGroup` passes over it onto the next scenario or rule (section 2.8).

### v3.3.a Enhancements
* Added **Uses** named comment block
* Updated grammar
* Updated semantics
* Updated examples
* Clarified primitive type documentation
* Improved scenario readability

### Document revision 2026-09-14

* A step may carry more than one modifier, in any order. `Vertical` and
  `CompareOnly` were mutually exclusive until 2026-09-15, for no better reason
  than a regular expression that allowed one.
* Added `EveryCell`, so that a table of Entities written one per cell can be
  said. Only a DataType could be read that way before, and only implicitly.
* Stated that a vertical table is a horizontal table transposed, that it has no
  header row, and that it may carry more than one instance. The examples had
  shown a literal `| Attribute | Value |` header, which the parser reads as data
  and reports as an attribute that does not exist.
* Two steps in section 5.4 were written in vertical shape without saying
  `Vertical`; they now say it.
* `Uses` is now read by the parser and kept with the element it documents.
* Editorial only otherwise: the document was published with the conversation
  that produced it still wrapped around it, and with its code samples and
  headings malformed. No rule of the language was changed.

### Document revision 2026-10-11

* `~` in a cell stands for a space (section 2.3.1). The convention has been in
  all nine generators and in the parser since long before this; it was in no
  document, so the only way to learn it was to read the generators. Found by
  someone writing a real specification, who invented a quoting convention to
  work around a problem that did not exist, and got the quotes in the value.

### Document revision 2026-09-19

* `Vertical` is optional. The step names its attribute set, so the parser can
  see which way a table runs; the word is kept for the reader (sections 2.4
  and 4.4).
* `Define` has four forms, each written out in section 2.6 and in the grammar:
  one value to the end of the line (a `#` comment allowed after it), a
  docstring, a table, and a bare `Define` over a `Name`/`Value` table that
  declares several one-line Defines at once.
* A `=Name` may stand in a cell of a step table, an `Examples` table or a
  `Default` column, not only in place of a whole table (section 4.5).
* Any named comment may be followed by a table, which is part of the comment
  and is documentation only (section 4.1.1).
* A step that names an attribute set with no table after it is an error
  (section 2.7).
* A DomainTerm may name a type it stands for; a built-in type name used as a
  DomainTerm is reported, not obeyed.
