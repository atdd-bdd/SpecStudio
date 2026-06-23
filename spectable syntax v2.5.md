

* * *

📘 **SpecTable DSL — Syntax Specification (v2.5)**
==================================================

### _The complete, authoritative syntax_

* * *

1. **Identifiers**
   ==================

All named constructs must match:
    Identifier ::= [_A-Za-z][_A-Za-z0-9]*

Used for:

* DataType names
* DomainTerm names
* Entity names
* Attributes block names
* BusinessRule names
* Calculation names
* ScenarioGroup names
* AttributeSet names

Invalid examples: `1A`, `A-B`, `A B`, `$Money`.

* * *

2. **Comments**
   ===============

2.1 Named comments (optional under ANY top‑level command)
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

* May appear anywhere, including appended to any line.

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

Each may include optional named comments.

* * *

4. **Command Syntax**
   =====================

* * *

4.1 Specification
-----------------

    Specification <text>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)

* * *

4.2 DataType
------------

### Non‑enum

    DataType <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples                     (required)
    | ... | Notes |

### Enum

    DataType <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Value | Notes |

* * *

4.3 DomainTerm
--------------

    DomainTerm <Identifier> : <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)

* * *

4.4 BusinessRule
----------------

    BusinessRule <Identifier> : <AttributeSet>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples                     (required)
    | ... | Notes |

Attributes defined separately.

* * *

4.5 Calculation
---------------

    Calculation <Identifier> : <AttributeSet>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples                     (required)
    | ... | Notes |

Attributes defined separately.

* * *

4.6 Entity
----------

    Entity <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Attribute | Type | Default | Notes |

* * *

4.7 Attributes
--------------

    Attributes <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Attribute | Type | Default | Notes | In-Out? |

* * *

4.8 Scenario
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

* Any step may be table‑less.
* If a table is present, it must follow immediately.

* * *

4.9 Background
--------------

    Background:
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    Given <statement>
    | ... table ... |   (optional)
    
    And <statement>
    | ... table ... |   (optional)

Rules:

* Only `Given` and `And` allowed.

* * *

4.10 ScenarioGroup
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

4.11 Import / Insert
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

5. **Tables**
   =============
   
    | Header1 | Header2 | ... |
    | Value1  | Value2  | ... |

Rules:

* Must have matching column counts.
* Must be aligned with `|` boundaries.
* Used in DataType, BusinessRule, Calculation, Entity, Attributes, and Scenario steps.

* * *

6. **Step rules**
   =================
   
    Given
    When
    Then
    And
* `And` inherits the previous step type.
* Steps may be table‑less or table‑based.
* If a colon (`:`) is used, a table **must** follow.

* * *

7. **Semantics Summary**
   ========================
* Named comments do not affect semantics.
* Unnamed comments may appear anywhere.
* DataType non‑enum requires Examples.
* Enum DataType must not have Examples.
* BusinessRule and Calculation require Examples.
* BusinessRule and Calculation reference an AttributeSet via `Name : AttributeSet`.
* Attributes blocks are stand‑alone.
* ScenarioGroup contains only Scenarios.
* Background contains only Given/And.
* Steps may be table‑less or table‑based.
* Table must follow a step with a colon.

* * *

Ken — this is the **cleanest, most precise, most consistent version** of the SpecTable DSL yet.  
If you want, I can now:

* Regenerate the grammar
* Regenerate the test suite
* Produce a parser architecture
* Produce a full example file

Just tell me what direction you want next.
