Ken — here it is.  
The **full, integrated, authoritative SpecTable DSL v2.6 Reference**, including the new **Define** mechanism and the **`=Name` value‑reference syntax**.

This is the version you can reload after a restart.  
It’s compact, complete, and ready for implementation.

* * *

📘 **SpecTable DSL — Complete Reference (v2.6)**
================================================

### _Authoritative syntax including Define + =Name value references_

* * *

1. **Identifiers**
   ==================
   
    Identifier ::= [_A-Za-z][_A-Za-z0-9]*

Used for: DataType, DomainTerm, Entity, Attributes, BusinessRule, Calculation, ScenarioGroup, AttributeSet, Define names.

* * *

2. **Comments**
   ===============

2.1 Named comments (optional under any top‑level command)
---------------------------------------------------------

    Description <text>
    Constraint <text>
    Details <text>                     # single-line
    Details \                          # multi-line
      line 1 \
      line 2 \
      final line

Rules:

* Multi‑line Details use `\` on every line **except the last**.
* Named comments have **no semantic effect**.

2.2 Unnamed comments
--------------------

    # comment text

May appear anywhere.

* * *

3. **Top‑level commands**
   =========================

Exactly these:
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

4. **Define (new in v2.6)**
   ===========================

### Named reusable values for tables, steps, descriptions, etc.

4.1 Single‑line definition
--------------------------

    Define <Identifier> = <text>

4.2 Multi‑line definition (triple‑quoted)
-----------------------------------------

    Define <Identifier> =
    """
    <text block>
    """

Rules:

* Triple‑quoted block ends at the next `"""`.
* No escaping required.
* May appear anywhere at top level.

* * *

5. **Referencing defined values**
   =================================

### **Use `=Name` anywhere text is allowed**

    =ErrorMsg
    =LongDescription
    =SQLBlock

### Works in:

* Tables
* Steps
* Descriptions
* Details
* Constraint
* Any text field

### Grammar:

    ValueReference ::= "=" Identifier

* * *

6. **Command Syntax**
   =====================

* * *

6.1 Specification
-----------------

    Specification <text>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)

* * *

6.2 DataType
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

Rules:

* Non‑enum requires `Examples`.
* Enum must NOT have `Examples`.

* * *

6.3 DomainTerm
--------------

    DomainTerm <Identifier> : <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)

* * *

6.4 BusinessRule
----------------

    BusinessRule <Identifier> : <AttributeSet>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples
    | ... | Notes |

* * *

6.5 Calculation
---------------

    Calculation <Identifier> : <AttributeSet>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples
    | ... | Notes |

* * *

6.6 Entity
----------

    Entity <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Attribute | Type | Default | Notes |

* * *

6.7 Attributes
--------------

    Attributes <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Attribute | Type | Default | Notes | In-Out? |

* * *

6.8 Scenario
------------

    Scenario <text>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    # Step WITHOUT table
    Given <statement>
    
    # Step WITH table
    Given <desc> : <AttributeSet>
    | ... table ... |
    
    And <statement>
    When <statement>
    Then <statement>

Rules:

* Steps may be table‑less or table‑based.
* If a colon is used, a table must follow.

* * *

6.9 Background
--------------

    Background:
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    Given <statement>            # table optional
    | ... table ... |
    
    And <statement>              # table optional
    | ... table ... |

Rules:

* Only `Given` and `And` allowed.

* * *

6.10 ScenarioGroup
------------------

    ScenarioGroup <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    Scenario <text>
      Given ...
      When ...
      Then ...
    
    Scenario <text>
      ...

Rules:

* Contains only Scenarios.
* Ends at next ScenarioGroup or EOF.

* * *

6.11 Import / Insert
--------------------

    Import "file"
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    Insert "file"
    Description <summary>        (optional)
    Details <text or multi-line> (optional)

Rules:

* File path must be quoted.

* * *

7. **Tables**
   =============
   
    | Header1 | Header2 | ... |
    | Value1  | Value2  | ... |

Rules:

* Column counts must match.
* Used in DataType, BusinessRule, Calculation, Entity, Attributes, and Scenario steps.
* Cells may contain `=Name` references.

* * *

8. **Step Rules**
   =================
   
    Given
    When
    Then
    And
* `And` inherits previous step type.
* Table required if colon is used.

* * *

9. **Semantics Summary**
   ========================
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

* * *

Ken — this is the **complete, authoritative SpecTable v2.6 reference** with the new Define/value‑reference system fully integrated.

If you want next:

* Updated grammar
* Updated test suite
* Updated example file
* A v2.6 parser roadmap

Just tell me.
