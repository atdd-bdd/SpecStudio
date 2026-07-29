* * *

📘 **SpecTable DSL — Unified Specification v2.8**
=================================================

### _(v2.7.3 + Multiples + Updated Class Model + Define Rules)_

This is now the **authoritative merged version**.

* * *

1. Identifiers
   ==============
   
    Identifier ::= [_A-Za-z][_A-Za-z0-9]*

* * *

2. Comments
   ===========

### Named comments

    Description <text>
    Constraint <text>
    Details <text>
    Details \
      line 1 \
      line 2 \
      final line

### Unnamed comments

    # comment text

* * *

3. Top‑Level Commands
   =====================
   
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

* * *

4. Built‑In DataTypes
   =====================

From v2.7.3:
    Character
    String
    Text
    Integer
    Float
    Boolean
    Date
    Time
    DateTime
    Duration
    YesNo

### ✔ YesNo normalization

Truth values: `Y y Yes YES yes T t True TRUE true`  
False values: `N n No NO no F f False FALSE false`

* * *

5. Built‑In AttributeSets
   =========================

### 5.1 EnumerationValues

    Attributes EnumerationValues
    | Attribute | Type   | Default | Notes | In-Out |
    | Value     | String |         |       | In     |
    | Notes     | Text   |         |       | In     |

### 5.2 ValidValues

    Attributes ValidValues
    | Attribute | Type    | Default | Notes | In-Out |
    | Value     | String  |         |       | In     |
    | Valid     | Boolean | false   |       | Out    |

* * *

6. Define Blocks (Updated)
   ==========================

### 6.1 Single‑line

    Define <Identifier> = <text>

### 6.2 Table (no triple quotes)

    Define Address1 =
    | Street | 123 Main |
    | City   | Cary     |
    | State  | NC       |

### 6.3 Text block

    Define LongText =
    """
    This is a long block of text.
    """

### 6.4 Multiples rules for Define blocks

(From your syntax‑addition document)

* If Multiples = empty → **exactly 1 row**
* If Multiples = N → **1..N rows**
* If Multiples = Any → **any number of rows**
* If DataType is primitive AND Multiples applies →
  * table may be **vertical** or **horizontal**

* * *

7. Value References
   ===================
   
    =Name

Allowed anywhere text is allowed.

* * *

8. Vertical Tables (v2.7.3)
   =============================
   
    Given customer : Customer Vertical
    | Name | Ken |
    | Age  | 42  |
* No headers
* Written horizontally, interpreted vertically
* Allowed in Scenario, BusinessRule, Calculation, Cleanup, Define

* * *

9. DataType Syntax
   ==================

### 9.1 Non‑enum

    DataType Amount
    Description Monetary amount
    Details Must be non-negative
    
    Examples: ValidValues
    | Value | Valid |
    | 10    | true  |
    | -5    | false |

### 9.2 Enumerated

    DataType Color
    Description Basic colors
    
    Examples: EnumerationValues
    | Value | Notes |
    | Red   | Primary |
    | Blue  | Primary |
    | Green | Secondary |

* * *

10. Attributes (Updated with Multiples)
    =======================================

### Syntax

    Attributes Customer
    | Attribute | Type    | Default | Multiples | Notes | In-Out |
    | Name      | String  |         |           |       | In     |
    | Age       | Integer |         |           |       | In     |
    | Tags      | String  |         | Any       |       | In     |

### Multiples semantics

* `""` → Single
* `N` → Limited(N)
* `Y`, `Yes`, `Any` → Any

* * *

11. Entities (Updated with Multiples)
    =====================================
    
    Entity Order
    | Attribute | Type      | Multiples | Notes |
    | Address   | Address   |           |       |
    | Items     | LineItem  | Any       |       |

* * *

12. BusinessRule
    ================
    
    BusinessRule ValidateAddress : ShippingCheck
    Examples: ShippingCheck
    | Address | Valid |
    | =Addr1  | Y     |

* * *

13. Calculation
    ===============
    
    Calculation ComputeTotal : OrderTotals
    Examples: OrderTotals
    | Items   | Total |
    | =Items1 | 45.00 |

* * *

14. Scenario
    ============

### 14.1 Normal

    Scenario Customer Info
    Given customer : Customer
    | Name | Age |
    | Ken  | 42  |

### 14.2 Vertical

    Given customer : Customer Vertical
    | Name | Ken |
    | Age  | 42  |

### 14.3 Grid (primitive DataType)

    Given keypad : Integer
    | 1 | 2 | 3 |
    | 4 | 5 | 6 |

* * *

15. Background
    ==============
    
    Background:
    Given system initialized
    And user logged in

* * *

16. Cleanup (v2.7.3)
    ====================
    
    Cleanup:
    Then no errors were logged
    And no warnings were logged



* * *

17. Import / Insert
    ===================
    
    Import "common.spec" 
    
        Any defines, attributes, entities, are made available.   This is used if the file is not in the project.
    
    
    Insert "address.spec"
    
        Insert contents of file into the specification.   If the file is a .csv file, then it is converted to a table.   The headers must match the types in the AttributeSet.  

* * *

18. Multiples — Unified Rules
    =============================

### (Merged from both documents)

### 18.1 Allowed values

| Value             | Meaning      |
| ----------------- | ------------ |
| `""`              | Single       |
| `N`               | Up to N      |
| `Y`, `Yes`, `Any` | Unlimited    |
| `0`               | Zero allowed |

### 18.2 Applies to:

* Attributes
* Entities
* Data blocks
* Define blocks
* Step tables
* Primitive DataTypes (special rules)

### 18.3 Step table rules

* Single → exactly 1 row
* Limited(N) → ≤ N rows
* Any → any number of rows
* Primitive DataType + Multiples → vertical OR horizontal allowed

### 18.4 Generator output

* Single → scalar field
* Multiple → `List<T>`

* * *

19. Generated Classes (Updated Model)
    =====================================

### 19.1 String classes (pure DTOs)

* Only string fields
* Multiples → `List<string>`
* No `.ToTyped()`

### 19.2 Typed classes

* Typed fields

* Multiples → `List<T>`

* Own both conversions:
    static <Name>Typed FromStringObject(<Name>String s)
    <Name>String ToStringObject()

* * *

20. Parser Rules (Unified)
    ==========================
* Detect Multiples column
* Normalize values
* Attach `multiplesKind` to each attribute
* Validate Define blocks vs Multiples
* Validate step tables vs Multiples
* Primitive DataTypes: allow vertical/horizontal lists

* * *

21. Semantic Analyzer Rules (Unified)
    =====================================
* Validate row counts
* Validate shapes
* Infer whether step produces:
  * `<Name>String`
  * `List<<Name>String>`

* * *

22. Unit Test Generation Rules (Unified)
    ========================================
* Tests only use **String** classes
* Glue converts using `Typed.FromStringObject`
* Multiples controls whether generator emits:
  * Single object
  * List of objects
* Primitive DataType lists: vertical or horizontal

* * *


========================================
