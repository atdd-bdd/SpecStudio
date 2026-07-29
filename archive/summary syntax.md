Ken — here it is.  
A **compact, reload‑friendly, no‑nonsense SpecTable v2.5 reference card** you can paste in anytime after a restart.  
It’s tight, complete, and contains _only_ the essential syntax rules — no fluff, no examples, no commentary.

* * *

📘 **SpecTable DSL — Compact Reference Card (v2.5)**
====================================================

### _Authoritative minimal syntax_

* * *

**1. Identifiers**
------------------

    Identifier ::= [_A-Za-z][_A-Za-z0-9]*

Used for: DataType, DomainTerm, Entity, Attributes, BusinessRule, Calculation, ScenarioGroup, AttributeSet.

* * *

**2. Comments**
---------------

### **Named comments** (optional anywhere under a top‑level command)

    Description <text>
    Constraint <text>
    Details <text>                     # single-line
    Details \                          # multi-line
      line 1 \
      line 2 \
      final line

### **Unnamed comments**

    # comment text

May appear anywhere.

* * *

**3. Top‑level commands**
-------------------------

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

* * *

**4. Command Syntax**
---------------------

### **Specification**

    Specification <text>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)

* * *

### **DataType**

**Non‑enum:**
    DataType <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples
    | ... | Notes |

**Enum:**
    DataType <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Value | Notes |

* * *

### **DomainTerm**

    DomainTerm <Identifier> : <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)

* * *

### **BusinessRule**

    BusinessRule <Identifier> : <AttributeSet>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples
    | ... | Notes |

* * *

### **Calculation**

    Calculation <Identifier> : <AttributeSet>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    Examples
    | ... | Notes |

* * *

### **Entity**

    Entity <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Attribute | Type | Default | Notes |

* * *

### **Attributes**

    Attributes <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    | Attribute | Type | Default | Notes | In-Out? |

* * *

### **Scenario**

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

### **Background**

    Background:
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    Given <statement>            # table optional
    | ... table ... |
    
    And <statement>              # table optional
    | ... table ... |

Only `Given` and `And` allowed.

* * *

### **ScenarioGroup**

    ScenarioGroup <Identifier>
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    Scenario <text>
      Given ...
      When ...
      Then ...
    
    Scenario <text>
      ...

Contains only Scenarios.

* * *

### **Import / Insert**

    Import "file"
    Description <summary>        (optional)
    Details <text or multi-line> (optional)
    
    Insert "file"
    Description <summary>        (optional)
    Details <text or multi-line> (optional)

File path must be quoted.

* * *

**5. Tables**
-------------

    | Header1 | Header2 | ... |
    | Value1  | Value2  | ... |

Rules:

* Column counts must match.
* Used in DataType, BusinessRule, Calculation, Entity, Attributes, and Scenario steps.

* * *

**6. Step Rules**
-----------------

    Given
    When
    Then
    And

* `And` inherits previous step type.
* Table required if colon is used.

* * *

**7. Semantics Summary**
------------------------

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

* * *

Ken — this is the **reload‑fast**, **copy‑paste‑ready**, **minimal but complete** SpecTable v2.5 reference card.

If you want, I can also generate:

* a **one‑page grammar card**,
* a **one‑page example file**,
* or a **cheat sheet for writing new specs**.

Just tell me what you want next.
