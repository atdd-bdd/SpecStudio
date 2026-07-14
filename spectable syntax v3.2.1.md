Ken, here is **SpecTable DSL v3.2.1**, fully updated from your v3.2 document **with Multiples removed from the normative specification and moved to a final “Future Enhancement” section**.

Everything else remains exactly as in v3.2, except Multiples semantics are no longer active anywhere in the main body.

This is now the authoritative v3.2.1.
📘 **SpecTable DSL — Unified Specification v3.2.1**
===================================================

### _(v3.2 with Multiples moved to Future Enhancements)_

This version preserves all syntax and semantics from v3.2 **except Multiples**, which is now explicitly marked as **reserved** and **not yet implemented**.
**1. Identifiers**
==================

Code
    Identifier ::= [_A-Za-z][_A-Za-z0-9]*
**2. Comments**
===============

### Named comments

Code
    Description <text>
    Constraint <text>
    Details <text>
    Details \
      line 1 \
      line 2 \
      final line

### Unnamed comments

Code
    # comment text
**3. Top‑Level Commands**
=========================

Code
    Specification
    DataType
    DomainTerm
    BusinessRule
    Calculation
    Entity
    Attributes
    Scenario
    ScenarioGroup
    Background
    Cleanup
    Import
    Insert
    Define
**4. Built‑In DataTypes**
=========================

### Primitive Types

Code
    Character
    String
    Text
    Integer
    Decimal        # exact base‑10 number
    Scientific     # floating‑point number with exponent
    Boolean        # true/false
    YesNo          # human-friendly boolean
    Date
    Time
    DateTime
    Duration

### ✔ YesNo normalization

Truth:

Code
    Y y Yes YES yes T t True TRUE true

False:

Code
    N n No NO no F f False FALSE false

### ✔ Decimal

* Exact base‑10

* No exponent

* Maps to BigDecimal / decimal / Decimal

### ✔ Scientific

* Approximate

* Supports exponent notation

* Maps to double / float

**5. Built‑In AttributeSets**
=============================

### 5.1 EnumerationValues

Code
    Attributes EnumerationValues
    | Attribute | Type   | Default | Notes | In-Out |
    | Value     | String |         |       | In     |
    | Notes     | Text   |         |       | In     |

### 5.2 ValidValues (Updated for IsValid)

Code
    Attributes ValidValues
    | Attribute | Type    | Default | Notes | In-Out |
    | Value     | String  |         |       | In     |
    | IsValid   | Boolean | false   |       | Out    |
**6. Define Blocks**
====================

### 6.1 Single‑line

Code
    Define <Identifier> = <text>

### 6.2 Table

Code
    Define Address1 =
    | Street | 123 Main |
    | City   | Cary     |
    | State  | NC       |

### 6.3 Text block

Code
    Define LongText =
    """
    This is a long block of text.
    """

### 6.4 Nested Entities

Define blocks may contain nested Entity attributes using:

Code
    | Address | =OtherDefine |

Nesting depth is unlimited.
**7. Value References**
=======================

Code
    =Name

Allowed anywhere text is allowed.
**8. Vertical Tables**
========================

Code
    Given customer : Customer Vertical
    | Name | Ken |
    | Age  | 42  |

* No headers

* Horizontal writing, vertical interpretation

* Allowed in Scenario, BusinessRule, Calculation, Cleanup, Define

**9. DataType Syntax (Updated with IsValid)**
=============================================

### 9.1 Non‑enum

Code
    DataType Amount
    Description Monetary amount
    Details Must be non-negative

    Examples: ValidValues
    | Value | IsValid |
    | 10    | true    |
    | -5    | false   |

    Rules:
        MustBeAtLeast 0

### 9.2 Enumerated

Code
    DataType Color
    Description Basic colors

    Examples: EnumerationValues
    | Value | Notes     |
    | Red   | Primary   |
    | Blue  | Primary   |
    | Green | Secondary |
**10. Attributes**
==================

Code
    Attributes Customer
    | Attribute | Type    | Default | Notes | In-Out |
    | Name      | String  |         |       | In     |
    | Age       | Integer |         |       | In     |
    | Tags      | String  |         |       | In     |

> **Note:** The `Multiples` column is reserved for future use and MUST be ignored in v3.2.1.

**11. Entities**
================

Code
    Entity Order
    | Attribute | Type      | Notes |
    | Address   | Address   |       |
    | Items     | LineItem  |       |

Entities may contain nested Entities at any depth.
**12. BusinessRule**
====================

Code
    BusinessRule ValidateAddress : ShippingCheck
    Examples: ShippingCheck
    | Address | IsValid |
    | =Addr1  | Y       |
**13. Calculation**
===================

Code
    Calculation ComputeTotal : OrderTotals
    Examples: OrderTotals
    | Items   | Total |
    | =Items1 | 45.00 |
**14. Scenario**
================

### 14.1 Normal

Code
    Scenario Customer Info
    Given customer : Customer
    | Name | Age |
    | Ken  | 42  |

### 14.2 Vertical

Code
    Given customer : Customer Vertical
    | Name | Ken |
    | Age  | 42  |

### 14.3 Grid (primitive)

Code
    Given keypad : Integer
    | 1 | 2 | 3 |
    | 4 | 5 | 6 |

### 14.4 Nested Entities

Code
    Scenario ShoppingCart with Addresses
    Given cart : ShoppingCart
    | ShippingAddress   | BillingAddress   |
    | =AShippingAddress | =ABillingAddress |
**15. Background**
==================

Code
    Background:
    Given system initialized
    And user logged in
**16. Cleanup**
===============

Code
    Cleanup:
    Then no errors were logged
    And no warnings were logged
**17. Import / Insert**
=======================

### Import

Code
    Import "common.spec"

### Insert

Code
    Insert "address.spec"

CSV → converted to table; headers must match AttributeSet.
**18. Parser Rules**
====================

* Validate Define blocks

* Validate step tables

* Primitive DataTypes: vertical/horizontal lists allowed

* Multiples column MUST be ignored in v3.2.1

**19. Semantic Analyzer Rules**
===============================

* Validate row counts

* Validate shapes

* Infer whether step produces:
  
  * `<Name>String`
  
  * `List<<Name>String>` (only when explicitly using primitive grids)

Multiples MUST NOT affect semantic analysis in v3.2.1.
**20. Unit Test Generation Rules**
==================================

* Tests use **String** classes

* Glue converts using `Typed.FromStringObject`

* Multiples MUST be ignored

* Primitive grids allowed

**21. Generated Classes**
=========================

### 21.1 String classes

* Only string fields

* No Multiples support in v3.2.1

### 21.2 Typed classes

* Typed fields

* No Multiples support in v3.2.1

**22. Future Enhancement: Multiples**
=====================================

### _(Reserved for v3.3 or later)_

The `Multiples` column appears in Attributes, Entities, and some tables as a placeholder for future list/collection semantics.

In v3.2.1:

* Multiples MAY appear but MUST be ignored by parsers, generators, and analyzers.

* All attributes and entities are treated as single-valued.

* Step tables are validated only for shape and content, not row counts.

Planned semantics (not yet active):

| Value             | Meaning      |
| ----------------- | ------------ |
| `""`              | Exactly 1    |
| `0`               | Zero allowed |
| `N`               | 0..N         |
| `Any`, `Yes`, `Y` | Unlimited    |

Generator (future):

* Single → scalar

* Multiple → list type (List<T>, Vec<T>, Array<T>, etc.)

These rules are **informational only** in v3.2.1.
🎉 **v3.2.1 Complete**
======================

Ken, this is now the **correct v3.2.1**:

* Multiples removed from normative semantics

* Multiples moved to a final “Future Enhancement” section

* All other v3.2 features preserved exactly

If you want, I can generate:

* A diff between v3.2 and v3.2.1

* A reader‑friendly version

* A formal EBNF grammar

* A code generator requirements doc

Just tell me what direction you want next.
