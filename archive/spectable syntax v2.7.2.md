Ken — excellent. I’ll now **regenerate the full SpecTable v2.7.2 reference**, incorporating _all_ of your latest changes:

* **Define tables**: no triple quotes; table recognized by leading `|`
* **Enumerated DataTypes**: use `Examples: EnumerationValues`
* **Examples syntax**: always `Examples: <AttributeSet>`
* **Built‑in AttributeSets**: `EnumerationValues`, `ValidValues`
* **Built‑in DataTypes**: Character, String, Text, Integer, Float, Boolean, Date, Time, DateTime, Duration, YesNo
* **Vertical tables**: indicated by `Vertical` after the AttributeSet name
* **IDE features updated**
* **Grammar updated**
* **Test suite updated**

Here is the complete, authoritative SpecTable v2.7.2 specification.

* * *

📘 **SpecTable DSL — Complete Reference (v2.7.2)**
==================================================

### _With Built‑Ins, Vertical Tables, Define Tables, EnumerationValues, ValidValues, YesNo, and Updated Examples Syntax_

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
    Import
    Insert
    Define

* * *

4. Built‑In DataTypes (Core Set)
   ================================

These require **no DataType declaration** and have **native string‑to‑type conversion** in C++, Python, C#, Java.
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

### ✔ YesNo

A human‑friendly Boolean type accepting:

Truth:
    Y, y, Yes, YES, yes, T, t, True, TRUE, true

False:
    N, n, No, NO, no, F, f, False, FALSE, false

Normalizes to Boolean.

* * *

5. Built‑In AttributeSets
   =========================

5.1 EnumerationValues
---------------------

Used for enumerated DataTypes.
    Attributes EnumerationValues
    | Attribute | Type   | Default | Notes | In-Out |
    | Value     | String |         |       | In     |
    | Notes     | Text   |         |       | In     |
5.2 ValidValues
---------------

Used for validation rules.
    Attributes ValidValues
    | Attribute | Type    | Default | Notes | In-Out |
    | Value     | String  |         |       | In     |
    | Valid     | Boolean | false   |       | Out    |

* * *

6. Define (Reusable Values)
   ===========================

6.1 Single‑line
---------------

    Define <Identifier> = <text>

6.2 Table (no triple quotes)
----------------------------

    Define Address1 =
    | Street | 123 Main |
    | City   | Cary     |
    | State  | NC       |

6.3 Text block (triple quotes still allowed)
--------------------------------------------

    Define LongText =
    """
    This is a long block of text.
    """

* * *

7. Value References
   ===================

Use `=Name` anywhere text is allowed.
    =Address1
    =Items1
    =LongText

* * *

8. Vertical Tables (New in v2.7.2)
   ====================================

### ✔ Syntax:

Add **Vertical** after the AttributeSet name:
    Given customer info : Customer Vertical
    | Name | Ken |
    | Age  | 42  |

### ✔ No Attribute/Value headers

### ✔ Table is written horizontally but interpreted vertically

### ✔ IDE may visually rotate the table

### ✔ Works in:

* Define
* Scenario
* BusinessRule
* Calculation

* * *

9. DataType Syntax
   ==================

9.1 Non‑enum DataType
---------------------

    DataType Amount
    Description Monetary amount
    Details Must be non-negative
    Examples: ValidValues
    | Value | Valid |
    | 10    | true  |
    | -5    | false |

9.2 Enumerated DataType (updated)
---------------------------------

    DataType Color
    Description Basic colors
    
    Examples: EnumerationValues
    | Value | Notes     |
    | Red   | Primary   |
    | Blue  | Primary   |
    | Green | Secondary |

* * *

10. Attributes
    ==============
    
    Attributes Customer
    | Attribute | Type   | Default | Notes | In-Out |
    | Name      | String |         |       | In     |
    | Age       | Integer|         |       | In     |

* * *

11. Entities
    ============
    
    Entity Order
    | Attribute | Type      | Default | Notes |
    | Address   | Address   |         |       |
    | Items     | LineItem* |         |       |

* * *

12. BusinessRule
    ================
    
    BusinessRule ValidateAddress : ShippingCheck
    Examples: ShippingCheck
    | Address   | Valid |
    | =Addr1    | Y     |

* * *

13. Calculation
    ===============
    
    Calculation ComputeTotal : OrderTotals
    Examples: OrderTotals
    | Items    | Total |
    | =Items1  | 45.00 |

* * *

14. Scenario
    ============

14.1 Normal table
-----------------

    Scenario Customer Info
    Given customer : Customer
    | Name | Ken |
    | Age  | 42  |

14.2 Vertical table
---------------------

    Scenario Customer Info
    Given customer : Customer Vertical
    | Name | Ken |
    | Age  | 42  |

14.3 Grid table (DataType only)
-------------------------------

    Given keypad : Integer
    | 1 | 2 | 3 |
    | 4 | 5 | 6 |

* * *

15. Background
    ==============
    
    Background:
    Given defaults : Customer Vertical
    | Name | Default |

* * *

16. Import / Insert
    ===================
    
    Import "common.spec"
    Insert "address.spec"

* * *

17. Grammar (Updated for v2.7.2)
    ================================

### Step header

    StepHeader ::= StepKeyword Text ":" AttributeSetName [ "Vertical" ]

### Define block

    DefineBlock ::= "Define" Identifier "=" ( Table | TextBlock )

### Examples

    ExamplesBlock ::= "Examples:" AttributeSetName Table

### Enumerations

    DataTypeEnum ::= "DataType" Identifier
                     [ Description ]
                     [ Details ]
                     "Examples:" "EnumerationValues"
                     Table

* * *

18. Test Suite (Built‑Ins + Vertical + Define Tables)
    =======================================================

✔ YesNo
-------

    Scenario Test YesNo
    Given flags : YesNo
    | Y | N | yes | no | t | f |

✔ Vertical
------------

    Scenario Test Vertical
    Given customer : Customer Vertical
    | Name | Ken |
    | Age  | 42  |

✔ Define table
--------------

    Define Address1 =
    | Street | 123 Main |
    | City   | Cary     |
    | State  | NC       |

✔ Enumeration
-------------

    DataType Color
    Examples: EnumerationValues
    | Value | Notes |
    | Red   |       |
    | Blue  |       |

✔ Examples: <AttributeSet>
--------------------------

    BusinessRule Validate : Customer
    Examples: Customer
    | Name | Age |
    | Ken  | 42  |

* * *

19. IDE Feature Updates (v2.7.2)
    ================================

### ✔ Auto‑insert Vertical when user types it

### ✔ Auto‑rotate table visually

### ✔ Auto‑insert table headers for AttributeSets

### ✔ Auto‑insert grid tables for DataTypes

### ✔ Hover preview for AttributeSets

### ✔ Rename refactoring for Attributes, AttributeSets, Entities, Define blocks

### ✔ Validate YesNo values

### ✔ Validate enumerations via EnumerationValues

### ✔ Validate Examples: <AttributeSet> syntax

### ✔ Generate Define block when referenced but missing

* * *

Ken — this is the **complete, regenerated SpecTable v2.7.2 specification**, fully incorporating every change you requested.

If you want next:

* A **v2.7.2 reference card** (1 page)
* A **v2.7.2 PEG grammar**
* A **v2.7.2 conformance test suite**
* A **v2.7.2 example project**

Just tell me.
