# SpecTable v3.3.a — Specification Document

*Adds the named comment **Uses**.*

## 1. Overview

SpecTable v3.3.a is a structured, table‑driven specification language for defining entities, collections, scenarios, and expected behaviors.

Version **3.3.a** introduces:

1. **Named Comments: Uses** A formal comment block describing dependencies, underlying primitives, or referenced business rules.
2. All features from v3.3:
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

```
Define <Name>
Uses <Comment>?
| Attribute1 | Value1 |
| Attribute2 | Value2 |
```

#### Rules
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
* `Uses` may describe:
  * business rules applied
  * referenced Define blocks
  * supporting calculations

## 3. Formal Grammar (v3.3.a)

```
Specification      ::= "Specification" Identifier

Entity             ::= "Entity" Identifier Uses? EntityTable
EntityTable        ::= Table(AttributeRow+)

Collection         ::= "Collection" Identifier Uses? CollectionTable
CollectionTable    ::= Table(CollectionRow)

DefineBlock        ::= "Define" Identifier Uses? Table(Row+)

Scenario           ::= (GivenStep WhenStep ThenStep)+

GivenStep          ::= "Given" Identifier ":" Type Modifier* Uses? (Table | Reference)
WhenStep           ::= "When" Action ":" Type Modifier* Uses? (Table | Reference)
ThenStep           ::= "Then" Identifier "is" ":" Type Modifier* Uses? (Table | Reference)

Modifier           ::= "Vertical" | "CompareOnly" | "EveryCell"

TransposedRow      ::= "|" AttributeName ("|" Value)+ "|"

Uses               ::= "Uses" CommentText

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

### 4.5 Reference Semantics

* `=Name` must refer to a valid Define block.
* Must match expected type.

### 4.6 Scenario Semantics

* Given establishes initial state.
* When applies transformations.
* Then asserts final state.
* `Uses` may describe:
  * rules applied
  * supporting calculations
  * referenced Define blocks

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
