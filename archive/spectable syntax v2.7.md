Ken — here is **SpecTable v2.7**, fully integrated with:

* **Define** blocks
* **`=Name` value references**
* **Nested entity representation** (single + multiple instances)
* **Examples for BusinessRules, Calculations, Scenarios, Entities**

This is the **complete, authoritative v2.7 reference**, ready for implementation.

* * *

📘 **SpecTable DSL — Complete Reference (v2.7)**
================================================

### _Includes Define, =Name references, and nested entity representation_

* * *

1. **Identifiers**
   ==================
   
    Identifier ::= [_A-Za-z][_A-Za-z0-9]*

Used for: DataType, DomainTerm, Entity, Attributes, BusinessRule, Calculation, ScenarioGroup, AttributeSet, Define names.

* * *

2. **Comments**
   ===============

2.1 Named comments
------------------

    Description <text>
    Constraint <text>
    Details <text>                     # single-line
    Details \                          # multi-line
      line 1 \
      line 2 \
      final line

2.2 Unnamed comments
--------------------

    # comment text

* * *

3. **Top‑level commands**
   =========================
   
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

4. **Define (v2.6+)**
   =====================

### 4.1 Single‑line

    Define <Identifier> = <text>

### 4.2 Multi‑line (triple‑quoted)

    Define <Identifier> =
    """
    <text block>
    """

* * *

5. **Value References (v2.6+)**
   ===============================

Use **`=Name`** anywhere text is allowed:
    =ErrorMsg
    =LongAddress
    =OrderItems

Works in:

* Tables
* Steps
* Descriptions
* Details
* Constraints

* * *

6. **Nested Entity Representation (v2.7)**
   ==========================================

**Case 1 — Single nested entity**
---------------------------------

Define the nested entity as a table:
    Define ShippingAddress =
    """
    | Street   | City   | State |
    | 123 Main | Durham | NC    |
    """

Reference it:
    | Attribute | Value            |
    | Address   | =ShippingAddress |

* * *

**Case 2 — Multiple nested entities**
-------------------------------------

Define a multi‑row table:
    Define LineItems =
    """
    | SKU     | Qty | Price |
    | ABC123  | 2   | 10.00 |
    | XYZ999  | 1   | 25.00 |
    """

Reference it:
    | Attribute | Value      |
    | Items     | =LineItems |

* * *

7. **Command Syntax**
   =====================

* * *

7.1 Specification
-----------------

    Specification <text>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)

* * *

7.2 DataType
------------

### Non‑enum

    DataType <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples
    | ... | Notes |

### Enum

    DataType <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Value | Notes |

* * *

7.3 DomainTerm
--------------

    DomainTerm <Identifier> : <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)

* * *

7.4 BusinessRule
----------------

    BusinessRule <Identifier> : <AttributeSet>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples
    | ... | Notes |

* * *

7.5 Calculation
---------------

    Calculation <Identifier> : <AttributeSet>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples
    | ... | Notes |

* * *

7.6 Entity
----------

    Entity <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Attribute | Type | Default | Notes |

Nested entities use `=Name`.

* * *

7.7 Attributes
--------------

    Attributes <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Attribute | Type | Default | Notes | In-Out? |

* * *

7.8 Scenario
------------

    Scenario <text>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    Given <statement>
    Given <desc> : <AttributeSet>
    | ... table ... |
    
    When <statement>
    Then <statement>
    And <statement>

* * *

7.9 Background
--------------

    Background:
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    Given <statement>            # table optional
    | ... table ... |
    
    And <statement>              # table optional
    | ... table ... |

* * *

7.10 ScenarioGroup
------------------

    ScenarioGroup <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    Scenario <text>
      Given ...
      When ...
      Then ...

* * *

7.11 Import / Insert
--------------------

    Import "file"
    Insert "file"

* * *

8. **Tables**
   =============
   
    | Header1 | Header2 | ... |
    | Value1  | Value2  | ... |

Rules:

* Column counts must match.
* Cells may contain `=Name`.

* * *

9. **Step Rules**
   =================
   
    Given
    When
    Then
    And
* `And` inherits previous step type.
* Table required if colon is used.

* * *

10. **Semantics Summary**
    =========================
* Named comments have no semantic effect.
* Unnamed comments allowed anywhere.
* DataType non‑enum requires Examples.
* Enum DataType must not have Examples.
* BusinessRule and Calculation require Examples.
* BusinessRule/Calculation reference AttributeSet via `Name : AttributeSet`.
* Attributes blocks are stand‑alone.
* ScenarioGroup contains only Scenarios.
* Background contains only Given/And.
* Steps may be table‑less or table‑based.
* Table must follow a step with a colon.
* `Define` introduces reusable values.
* `=Name` references defined values anywhere text is allowed.
* Nested entities must be represented using Define blocks.

* * *

📘 **Examples for v2.7**
========================

* * *

**A. BusinessRule Example (with nested entity)**
================================================

    Define Address1 =
    """
    | Street   | City   | State |
    | 123 Main | Durham | NC    |
    """
    
    Attributes ShippingCheck
    | Attribute | Type    | Default | Notes | In-Out |
    | Address   | Address | =Address1 |      | In     |
    | Valid     | Boolean | false   |      | Out    |
    
    BusinessRule ValidateAddress : ShippingCheck
    Examples
    | Address   | Valid | Notes |
    | =Address1 | true  | OK    |

* * *

**B. Calculation Example (with nested list)**
=============================================

    Define Items1 =
    """
    | SKU     | Qty | Price |
    | ABC123  | 2   | 10.00 |
    | XYZ999  | 1   | 25.00 |
    """
    
    Attributes OrderTotals
    | Attribute | Type      | Default | Notes | In-Out |
    | Items     | LineItem* | =Items1 |       | In     |
    | Total     | Dollar    | 0       |       | Out    |
    
    Calculation ComputeTotal : OrderTotals
    Examples
    | Items    | Total | Notes |
    | =Items1  | 45.00 | OK    |

* * *

**C. Entity Example (with nested entity + list)**
=================================================

    Define BillingAddress =
    """
    | Street   | City   | State |
    | 55 Oak   | Raleigh| NC    |
    """
    
    Define OrderItems =
    """
    | SKU     | Qty | Price |
    | ABC123  | 2   | 10.00 |
    | XYZ999  | 1   | 25.00 |
    """
    
    Entity Order
    | Attribute | Type      | Default        | Notes |
    | Address   | Address   | =BillingAddress|       |
    | Items     | LineItem* | =OrderItems    |       |

* * *

**D. Scenario Example (using nested values)**
=============================================

    Define Address2 =
    """
    | Street   | City   | State |
    | 77 Pine  | Cary   | NC    |
    """
    
    Define Items2 =
    """
    | SKU     | Qty | Price |
    | LMN777  | 3   | 5.00  |
    """
    
    Scenario Process order with nested data
    Given an order exists : Order
    | Attribute | Value     |
    | Address   | =Address2 |
    | Items     | =Items2   |
    
    When computing totals
    Then total should be 15.00

* * *

Ken — this is the **full SpecTable v2.7 specification**, with nested entity support integrated cleanly and examples across all major constructs.

If you want next:

* v2.7 grammar
* v2.7 test suite
* v2.7 reference card
* v2.7 parser architecture

Just tell me.
