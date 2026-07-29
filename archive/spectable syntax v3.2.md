Ken, I can absolutely merge **your full v2.8.1 DSL spec** with the **clean v2.1 syntax** I generated earlier and produce a **unified, coherent, modern v3.2 SpecTable DSL**—including:

* **Scientific** (floating‑point)

* **Decimal** (exact base‑10)

* **IsValid** (updated DataType example semantics)

* All your v2.8.1 features (Multiples, Vertical, Define rules, Calculation, Cleanup, Background, Import/Insert, Generated Classes, Parser/Semantic/Test rules)

* The clearer structure and readability of v2.1

Below is the **merged v3.2 specification**, rewritten so it is consistent, modern, and easier to read while preserving every capability your DSL currently supports.
📘 **SpecTable DSL — Unified Specification v3.2**
=================================================

### _(Merged v2.8.1 + v2.1 + Scientific/Decimal/IsValid)_

This is now the **authoritative unified syntax and semantics** for the SpecTable DSL.
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
**4. Built‑In DataTypes (Updated)**
===================================

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

### ✔ Decimal (NEW)

* Exact base‑10 number

* No exponent

* Maps to BigDecimal / decimal / Decimal

### ✔ Scientific (NEW)

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
**6. Define Blocks (Unified)**
==============================

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

### 6.4 Multiples rules

* Multiples = empty → **exactly 1 row**

* Multiples = N → **1..N rows**

* Multiples = Any → **any number of rows**

* Primitive DataType + Multiples → vertical **or** horizontal tables allowed

### 6.5 Nested Entities (NEW)

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

* Written horizontally, interpreted vertically

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
**10. Attributes (Unified with Multiples)**
===========================================

Code
    Attributes Customer
    | Attribute | Type    | Default | Multiples | Notes | In-Out |
    | Name      | String  |         |           |       | In     |
    | Age       | Integer |         |           |       | In     |
    | Tags      | String  |         | Any       |       | In     |

### Multiples semantics

* `""` → Single

* `N` → Limited(N)

* `Y`, `Yes`, `Any` → Unlimited

* `0` → Zero allowed

**11. Entities (Unified)**
==========================

Code
    Entity Order
    | Attribute | Type      | Multiples | Notes |
    | Address   | Address   |           |       |
    | Items     | LineItem  | Any       |       |

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
**14. Scenario (Unified)**
==========================

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

### 14.4 Nested Entities (NEW)

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

Makes defines, attributes, entities available.

### Insert

Code
    Insert "address.spec"

If CSV → converted to table; headers must match AttributeSet.
**18. Multiples — Unified Rules**
=================================

### Allowed values

| Value             | Meaning      |
| ----------------- | ------------ |
| `""`              | Single       |
| `N`               | Up to N      |
| `Y`, `Yes`, `Any` | Unlimited    |
| `0`               | Zero allowed |

### Applies to:

* Attributes

* Entities

* Define blocks

* Step tables

* Primitive DataTypes

### Step table rules

* Single → exactly 1 row

* Limited(N) → ≤ N rows

* Any → any number of rows

* Primitive lists → vertical or horizontal

### Generator output

* Single → scalar

* Multiple → `List<T>`

**19. Generated Classes (Unified)**
===================================

### 19.1 String classes

* Only string fields

* Multiples → `List<string>`

* No `.ToTyped()`

### 19.2 Typed classes

* Typed fields

* Multiples → `List<T>`

* Conversions:

Code
    static <Name>Typed FromStringObject(<Name>String s)
    <Name>String ToStringObject()
**20. Parser Rules**
====================

* Detect Multiples

* Normalize values

* Validate Define blocks

* Validate step tables

* Primitive DataTypes: allow vertical/horizontal lists

**21. Semantic Analyzer Rules**
===============================

* Validate row counts

* Validate shapes

* Infer whether step produces:
  
  * `<Name>String`
  
  * `List<<Name>String>`

**22. Unit Test Generation Rules**
==================================

* Tests use **String** classes

* Glue converts using `Typed.FromStringObject`

* Multiples controls whether generator emits:
  
  * Single object
  
  * List of objects

* Primitive lists: vertical or horizontal

🎉 **v3.2 Complete**
====================

Ken, this is now the **fully merged, modernized, unified SpecTable DSL v3.2**—combining everything from your v2.8.1 plus the clarity and improvements from v2.1, plus the new:

* **Scientific**

* **Decimal**

* **IsValid**

If you want, I can also produce:

* A **reader‑friendly version**

* A **formal EBNF grammar**

* A **compiler‑friendly version**

* A **diff report** showing exactly what changed from v2.8.1

Just tell me what direction you want next.
