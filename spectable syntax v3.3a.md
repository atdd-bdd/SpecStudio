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

**Leading whitespace is ignored.** A line means the same wherever it starts, so
a specification may be indented however its author wants: steps under their
scenario, rows under their step, a continuation under the comment it carries
on. A keyword, a pipe row, a tag, a `\` continuation and a `"""` docstring
marker are each read at any column. Indentation ends nothing and continues
nothing -- what a line says is all that is read. The one place indentation is
kept is inside a `"""` docstring, where the text is the value: the opening
`"""` sets the column stripped from every line of the body, and whatever is
indented beyond it stays.

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

WhenThen <Action> : <Type>
Uses <Comment>?
<Step Table or Vertical>
```

#### Rules
* A step that names an attribute set must be followed by its table or by a
  `=Reference` to a table-form Define. A step with neither is an error, not a
  step with an empty table.
* A step with no `: <Type>` takes no table; it may carry a docstring, or
  nothing at all.
* `WhenThen` is a step like any other and stands where a `When` or a `Then`
  would. It is written when one table holds both halves of the step -- the
  input columns and the expected ones. See 4.6.
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
  every block in the file -- every scenario, every rule, every calculation.
* **A `ScenarioGroup` takes no tags.** A group organises scenarios and is
  generated into nothing, so a tag on one has no test to mark. A tag of either
  kind written above a `ScenarioGroup` line is dropped and reported:

  > `@wide $alsowide on a ScenarioGroup is ignored -- a ScenarioGroup generates
  > no test, so there is nothing for a tag to mark. Tag the scenarios in the
  > group instead.`

  Tag the scenarios in the group, or the `Specification` line if the tag is
  meant for the whole file.
* **A tag must sit immediately above its block.** A blank line between them
  clears the tag -- that is the rule that makes a tag belong to the block below
  it rather than to the next one that happens to follow. A `$tag` cleared this
  way is reported where it was written (section 4.7); an `@tag` is not.
* A tag line is not a comment line: `#` still starts a comment, and a tag
  inside comment text is text.
* **Only a blank line clears a tag; a declaration does not.** Apart from a
  `ScenarioGroup`, which drops it, a tag is held until the next block that
  takes one -- so a tag written above an `Entity`, `Attributes`, `Collection`,
  `Define` or `DomainTerm` line passes over it and lands on the `Scenario` or
  rule that follows. `$wip` above an `Attributes` block, with no blank line
  between that block and the scenario after it, tags **that scenario**.
  Writing the tag directly above the block it means, which is the rule above,
  avoids it.

## 3. Formal Grammar (v3.3.a)

```
Specification      ::= TagLine* "Specification" Identifier

TagLine            ::= (TestTag | GeneratorTag)+        # any order, any mix
TestTag            ::= "@" Identifier                   # emitted into the test
GeneratorTag       ::= "$" Identifier                   # filtering only
TaggableBlock      ::= TagLine* (Scenario | BusinessRule | Calculation | DataType)
ScenarioGroup      ::= "ScenarioGroup" Identifier        # takes no TagLine

Entity             ::= "Entity" Identifier Uses? EntityTable
EntityTable        ::= Table(AttributeRow+)

Collection         ::= "Collection" Identifier Uses? CollectionTable
CollectionTable    ::= Table(CollectionRow)

DefineBlock        ::= DefineValue | DefineText | DefineTable | DefineList
DefineValue        ::= "Define" Identifier "=" Value Comment?
DefineText         ::= "Define" Identifier "=" DocString
DefineTable        ::= "Define" Identifier Uses? Table(Row+)
DefineList         ::= "Define" Table(DefineRow+)           # header has Name and Value

Scenario           ::= (GivenStep WhenStep ThenStep)+    # or a WhenThenStep for the last two

GivenStep          ::= "Given" Identifier ":" Type Modifier* Uses? (Table | Reference)
WhenStep           ::= "When" Action ":" Type Modifier* Uses? (Table | Reference)
ThenStep           ::= "Then" Identifier "is" ":" Type Modifier* Uses? (Table | Reference)
WhenThenStep       ::= "WhenThen" Action ":" Type Modifier* Uses? (Table | Reference)
AndStep            ::= "And" <rest of the step>    # the keyword of the step above

Modifier           ::= "Vertical" | "CompareOnly" | "EveryCell"

TransposedRow      ::= "|" AttributeName ("|" Value)+ "|"

Uses               ::= "Uses" CommentText Table?
NamedComment       ::= ("Description" | "Details" | "Notes" | "Constraint" | "Uses")
                       CommentText Continuation* Table?
Continuation       ::= "\" CommentText?                # first character of the next line,
                                                       # leading whitespace aside;
                                                       # "\" alone is a paragraph break

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
* **The backslash may be indented.** Leading whitespace is ignored (section 2),
  so a continuation can sit under the keyword it carries on -- the one place
  where indenting shows a reader what the parser already knows:

  ```
  DataType AccountType
  Description Allowed account categories.
  Details
    \ These are the only valid account types in this system.
  ```

  The `Details` here is one named comment whose whole text is written on the
  next line, which is a `Details` with no text of its own followed by a
  continuation.
* **A continuation that holds only `\` is a paragraph break.** It adds no text
  and it does not break the chain, so a comment of two paragraphs is still one
  named comment:

  ```
  BusinessRule Which label wins
  Description The most specific label wins,
  \ which is taken to be the one with the longest text.
  \
  \ A tie is impossible, because two labels of the same length
  \ cannot both be most specific.
  ```

  Two of them in a row, one straight after the keyword line, and one as the
  last line of the comment are all accepted and silent. Whitespace after the
  backslash is the same as none.

  What it is **not** is a break the text carries. The pieces are still joined
  with a single space, so the comment's text is the two paragraphs run
  together, and the blank line is there for whoever reads the specification
  itself. Nothing is generated from a named comment today, so that costs
  nothing; if a rendered form of one is ever wanted, this is the line that will
  have to mean something there.

  It is still a continuation, so it must follow a named comment like any other.
  A line holding only `\` after anything else has nothing to continue, and is
  reported the same way as a continuation with words on it.
* Like the comment it continues, it does **not** end a table: a continuation
  between a step and its table leaves the table attached to that step.
* **Indentation is not a continuation marker, and never was.** Until
  2026-10-25 an indented line that was not a pipe row was discarded by the
  parser, which made indented prose look as though it carried on the comment
  above it while its text was kept nowhere. A line is now read for what it
  says, at any column, so prose that carries on a comment needs the backslash
  whether it is indented or not. What the discarding cost was not the prose: an
  indented step vanished from its scenario with nothing reported, and the
  generated test held only the steps that happened to start at column one.
* A backslash line that follows anything other than a named comment continues
  nothing. It is still treated as a comment and discarded, but it is **reported
  as a warning**, because the writer meant it to attach to something.
* **A wrapped line written without the backslash is an error.** A line whose
  first word is no keyword of the language is read as neither a keyword nor a
  continuation, and nothing can be done with it:

  ```
  ERROR:10:'longest' is not a keyword -- a misspelled one, or a wrapped line
  that needs a leading '\' to carry on the comment above it. The line is
  dropped, and nothing it was part of ends here.
  ```

  It used to be a warning, and it closed whatever block it was written inside,
  which is the part that cost: a `BusinessRule` whose `Description` was wrapped
  this way ended at the stray line, its `Examples:` table then belonged to no
  rule, the rule generated no test, and the message that said so was the third
  of three warnings. The block stays open now, so the one message is about the
  one mistake, and the file is refused until it is fixed.
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

### 4.2.1 Headings and Attribute Names

A table that names an attribute set -- a step table, or an `Examples:` table --
is matched to that set by its headings.

* A heading must name an attribute of the set. Case does not matter and a
  vertical table's first column answers for the header row, but nothing else
  about the spelling is forgiven: `Cust omer` is not `Customer`.
* A heading that names no attribute is ignored, with a warning. Its values
  reach nothing, which is worth saying out loud, because the column looks like
  data and is not.
* An attribute with no column and no default value is one the generator cannot
  fill, and the file is refused for it. An attribute with a default is filled
  from the default, which is what declaring one is for.
* `CompareOnly` names only the attributes it asserts, so an attribute with no
  column is the point there rather than a mistake (section 2.4).
* **One misspelled heading is both of the first two at once, and is reported
  once.** A column nobody recognises beside an attribute nobody filled is one
  mistake, and the message names both spellings:

  ```
  ERROR:63:Examples table for 'Order' has column 'Custmer' where attribute
  'Customer' is expected -- a heading must match the attribute name exactly, so
  the column is ignored and 'Customer' is left with no value
  ```

  That covers the attribute's own name written differently -- a space, a
  capital, a hyphen -- and a heading a typo away from it. Three cases keep the
  two separate messages instead, because there the pairing would be a guess: a
  heading that is a different word (`Remarks` is not `Notes`); one equally close
  to two attributes, which names neither (`Addend3` where `Addend1` and
  `Addend2` are both unfilled); and one that differs from an attribute only in
  its digits, since a number in a name says which one and not how it is spelled
  (`Day2` is not `Day1` misspelled). A name of three letters or fewer must
  match exactly, for the same reason: at that length one edit reaches a word
  nobody meant.

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
* **`WhenThen` acts and asserts in one step.** It works just like a `When` or a
  `Then` -- same call generated, same stub -- and is written when one table
  holds both halves. A scenario may have its `Given` and then put the table
  under the `WhenThen` because part of the table is When data and part of it is
  Then data:

  ```
  Attributes Conversion
  | Attribute  | Type    | Default | Notes            | In-Out |
  | Fahrenheit | Integer | 0       | the temperature  | In     |
  | Celsius    | Integer | 0       | what it converts | Out    |

  Scenario Convert a temperature
  Given a thermometer
  WhenThen the temperature converts : Conversion
  | Fahrenheit | Celsius |
  | 212        | 100     |
  | 32         | 0       |
  ```

  The table reaches the glue whole, in one call, rows and all:

  ```java
  glue.WhenThen_the_temperature_converts(objectList1);
  ```

  so the step method reads `Fahrenheit` from each row, does the work, and
  compares `Celsius`. The same specification written as a `When` and a `Then`
  needs two steps and two tables, naming the same rows twice. All nine
  languages generate the step, the keyword appearing in the name as that
  language writes it -- `WhenThen_the_temperature_converts`,
  `whenthen_the_temperature_converts`, `whenthenTheTemperatureConverts`,
  `WhenThenTheTemperatureConverts`.
* Because `WhenThen` is an ordinary keyword, two things follow: an `And` under
  it continues as a `WhenThen`, and a `WhenThen` in a `Cleanup` block is an
  error, exactly as a `Given` or a `When` is -- acting belongs in a Scenario.
* `And` continues the run above it: it takes the keyword of the step above, so

  ```
  Given the chart of accounts is : ChartOfAccounts
  | ... |
  And payee rules are : PayeeRules
  | ... |
  ```

  is the same specification as two `Given` steps, and the generated test is
  identical. It works after `When` and after `Then` in the same way, and in a
  `Background` or a `Cleanup` as well as a `Scenario`.
* **An `And` with no step above it in its block is read as a `Given`, and
  reported.** There is nothing to continue, so the keyword is a guess:

  ```
  'And' continues the step above it, and this is the first step of the Scenario
  -- it is read as a Given. Write the keyword you mean.
  ```

* **`But` is not a keyword.** A line beginning with it is an unrecognized
  keyword, like any other word the language does not define. `And` is the only
  continuation.
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

A tag on a `ScenarioGroup` is reported whichever kind it is, because neither
kind can mean anything there (section 2.8).

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

### Document revision 2026-10-25

* **Leading whitespace is ignored.** People indent for their own purposes, and
  a line now means the same wherever it starts; section 2 says so, and 4.1.1
  says it of the continuation marker. The parser used to discard any indented
  line that was not a pipe row, as continuation text it then kept nowhere --
  so an indented step disappeared from its scenario without a word, and the
  generated test held the steps that began at column one. Four specifications
  in the test repositories were relying on the discard for wrapped prose and
  now carry the backslash the document has always asked for.
* **`WhenThen` was in the language, in the editor and in no version of this
  document.** The parser has accepted it for as long as it has accepted
  `Given`; the highlighter colours it, autocomplete offers it, and a reader of
  this document had no way to know it existed. It is in 2.7, in the grammar and
  in 4.6 now, with what it is for: one table whose columns are part When data
  and part Then data, under one step, instead of the same rows written out
  twice. What it generates was read off a run in all nine languages rather than
  inferred -- an ordinary step call taking the whole table, with the keyword in
  the method name.
* **An `And` under a `WhenThen`, and a `WhenThen` in a `Cleanup`.** Both follow
  from the keyword being ordinary, and both are stated in 4.6: the `And`
  continues as a `WhenThen`, and the `Cleanup` is an error.

### Document revision 2026-10-24

* **What a table heading must say was in no version of this document.** The
  rule itself has not changed -- a heading names an attribute, case aside, and
  is matched no other way -- but a reader had only the diagnostics to learn it
  from. Section 4.2.1 now states it, with what happens to a heading that names
  nothing, to an attribute with no column, and to the one mistake that is both.
* **A heading a typo away from an attribute is now that one mistake too.** The
  same word written differently has been paired since 2026-10-11; a misspelling
  of it still drew a warning and an error for one cause. Both are paired now,
  and 4.2.1 says where the pairing stops.
* **A continuation holding only `\` is a paragraph break.** It always behaved
  as one -- no text added, the chain unbroken -- and that was written down
  nowhere, so every specification that uses it was relying on folklore. 4.1.1
  states it now, with what it does not do: the text is still joined with a
  single space, so the break is for a reader of the specification and not
  something the comment carries. The grammar allows a continuation with no text
  for the same reason.
* **A line that is no keyword at all is an error, and ends nothing.** It was a
  warning, and it closed the block it was written in -- so a wrapped sentence
  could take a `BusinessRule`'s `Examples:` table out of the rule and leave the
  rule generating no test. Stated in 4.1.1, where the marker that does continue
  a comment is defined.

### Document revision 2026-10-23

* **`And` was in the language and in no version of this document.** The parser
  has always taken it as the keyword of the step above, and the User Guide lists
  it among the step keywords, but section 4 named only `Given`, `When` and
  `Then`, so a reader of this document had no way to know a run of steps could
  be written the way it reads. Section 4.6 and the grammar now carry it.
* **An `And` with no step above it in its block is reported.** It used to be
  read as a `Given` in silence -- a keyword chosen by the parser, not by the
  file.
* **`But` is not a keyword and never was.** Only a comment in the parser ever
  said otherwise. Stated in 4.6, because a reader coming from Gherkin will
  reach for it, and a line starting with it ends the block it is in.

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
  parser: only a blank line clears a tag, so a tag above a declaration passes
  over it onto the next scenario or rule (section 2.8).
* **A `ScenarioGroup` now takes no tags.** Either kind above one used to pass
  over and attach to the group's first scenario -- one scenario of several,
  chosen by position, with nothing said about it. A group is generated into
  nothing, so there is no test for a tag to mark: the tags are dropped and
  reported. Tagging a group so that every scenario in it is tagged was
  considered and deliberately left for another day.

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
