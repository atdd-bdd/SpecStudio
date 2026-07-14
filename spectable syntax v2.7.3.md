Ken — here it is.  
The **full, authoritative SpecTable v2.7.3** specification, regenerated cleanly with _every_ change you requested, including:

* **Cleanup** block (post‑scenario, Then/And only)
* **Vertical** keyword after AttributeSet name
* **Define tables without triple quotes**
* **Enumerations using `Examples: EnumerationValues`**
* **Examples always using `Examples: <AttributeSet>`**
* **Built‑in AttributeSets**: `EnumerationValues`, `ValidValues`
* **Built‑in DataTypes**: Character, String, Text, Integer, Float, Boolean, Date, Time, DateTime, Duration, YesNo
* **Language binding tables**
* **Test suite**
* **IDE feature list updated**
* **Grammar updated**

This is the version you can hand to implementers, IDE authors, and parser writers.

* * *

📘 **SpecTable DSL — Complete Reference (v2.7.3)**
==================================================

### _With Cleanup, Vertical Tables, YesNo, Define Tables, EnumerationValues, ValidValues, and Updated Examples Syntax_

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

8. Vertical Tables (New in v2.7.3)
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
* Cleanup

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
    Given system initialized
    And user logged in

* * *

16. Cleanup (New in v2.7.3)
    ===========================

A post‑scenario block that mirrors Background.

### ✔ Only Then and And allowed

### ✔ Runs after every Scenario

### ✔ Optional

### ✔ One per file

    Cleanup:
    Then no errors were logged
    And no warnings were logged

### With AttributeSet:

    Cleanup:
    Then metrics valid : Metrics
    | CPU | < 80 |
    | MEM | < 70 |

### With Vertical:

    Cleanup:
    Then audit entry exists : Audit Vertical
    | User  | Ken |
    | Event | Login |

* * *

17. Import / Insert
    ===================
    
    Import "common.spec"
    Insert "address.spec"

* * *

18. Grammar (Updated for v2.7.3)
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

### Cleanup

    CleanupBlock ::= "Cleanup:" [ Description ] [ Details ] CleanupStep+
    CleanupStep ::= "Then" Text [ ":" AttributeSetName [ "Vertical" ] ]
                  | "And"  Text [ ":" AttributeSetName [ "Vertical" ] ]

* * *

19. Language Binding Table (Built‑Ins)
    ======================================

| SpecTable Type | C++                  | Python              | C#                 | Java               |
| -------------- | -------------------- | ------------------- | ------------------ | ------------------ |
| Character      | `char`               | `str` (len=1)       | `char`             | `char`             |
| String         | `std::string`        | `str`               | `string`           | `String`           |
| Text           | `std::string`        | `str`               | `string`           | `String`           |
| Integer        | `int` / `long`       | `int`               | `int` / `long`     | `int` / `long`     |
| Float          | `float` / `double`   | `float`             | `float` / `double` | `float` / `double` |
| Boolean        | `bool`               | `bool`              | `bool`             | `boolean`          |
| YesNo          | `bool`               | `bool`              | `bool`             | `boolean`          |
| Date           | `<chrono>`           | `datetime.date`     | `DateOnly`         | `LocalDate`        |
| Time           | `<chrono>`           | `datetime.time`     | `TimeOnly`         | `LocalTime`        |
| DateTime       | `<chrono>`           | `datetime.datetime` | `DateTime`         | `LocalDateTime`    |
| Duration       | `<chrono::duration>` | `timedelta`         | `TimeSpan`         | `Duration`         |

* * *

20. Test Suite (v2.7.3)
    =======================

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

✔ Cleanup
---------

    Cleanup:
    Then no errors occurred

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

21. IDE Feature List (Updated for v2.7.3)
    =========================================

### ✔ Cleanup block support

### ✔ Vertical table visualization

### ✔ Auto‑insert table headers for AttributeSets

### ✔ Auto‑insert grid tables for DataTypes

### ✔ Hover preview for AttributeSets

### ✔ Rename refactoring

### ✔ Validate YesNo values

### ✔ Validate enumerations via EnumerationValues

### ✔ Validate Examples: <AttributeSet> syntax

### ✔ Generate Define block when referenced but missing

### ✔ Visual toggle for Vertical

### ✔ Scenario flow visualization including Cleanup

* * *

Ken — this is the **complete, regenerated SpecTable v2.7.3** specification.

If you want next:

* A **v2.7.3 reference card**
* A **v2.7.3 PEG grammar**
* A **v2.7.3 conformance test suite**
* A **v2.7.3 example project**

Just tell me.
