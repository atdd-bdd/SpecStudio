# SpecTable DSL — Complete Reference (v2.7.2)

A unified, business-friendly, automation-ready language for describing
specifications, datatypes, domain terms, business rules, calculations,
entities, scenarios, and scenario groups.

---

## 1. Identifiers

    Identifier ::= [_A-Za-z][_A-Za-z0-9]*

---

## 2. Comments

### Named comments

    Description <text>
    Constraint <text>
    Details <text>
    Details \
      line 1 \
      line 2 \
      final line

Named comments may appear under any top-level command. They do not affect semantics.

### Unnamed comments

    # comment text

May appear on their own line or appended to the end of any line.

---

## 3. Top-Level Commands

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

---

## 4. Built-In DataTypes

These require no `DataType` declaration and have native string-to-type conversion.

    Character   String   Text
    Integer     Float    Boolean
    Date        Time     DateTime   Duration
    YesNo

### YesNo

A human-friendly Boolean type accepting:

| Truth values          | False values           |
|-----------------------|------------------------|
| Y y Yes YES yes       | N n No NO no           |
| T t True TRUE true    | F f False FALSE false  |

Normalizes to Boolean.

---

## 5. Built-In AttributeSets

### EnumerationValues — used for enumerated DataTypes

    | Attribute | Type   | Default | Notes | In-Out |
    | Value     | String |         |       | In     |
    | Notes     | Text   |         |       | In     |

### ValidValues — used for validation rules

    | Attribute | Type    | Default | Notes | In-Out |
    | Value     | String  |         |       | In     |
    | Valid     | Boolean | false   |       | Out    |

---

## 6. Define (Reusable Values)

### Single-line

    Define <Identifier> = <text>

### Table form (leading `|` rows, no triple quotes needed)

    Define Address1 =
    | Street | 123 Main |
    | City   | Cary     |
    | State  | NC       |

### Text block

    Define LongText =
    """
    This is a long block of text.
    """

---

## 7. Value References

Use `=Name` anywhere text is allowed to reference a `Define` value.

    =Address1
    =Items1
    =LongText

---

## 8. Transposed Tables

Add `Transposed` after the AttributeSet name in a step header.
The table is written as key-value rows (no Attribute/Value header row).

    Given customer info : Customer Transposed
    | Name | Ken |
    | Age  | 42  |

Works in: Define, Scenario, BusinessRule, Calculation, Background.

---

## 9. DataType

### Enumerated form

    DataType Color
    Description Basic colors
    Examples: EnumerationValues
    | Value | Notes     |
    | Red   | Primary   |
    | Blue  | Primary   |
    | Green | Secondary |

### Non-enumerated form

    DataType Amount
    Description Monetary amount
    Details Must be non-negative
    Examples: ValidValues
    | Value | Valid |
    | 10    | true  |
    | -5    | false |

---

## 10. DomainTerm

    DomainTerm <Name> : <DataTypeName>
    Description <summary>

Example:

    DomainTerm CustomerID : AccountID
    Description Business identifier for a customer.

---

## 11. Attributes

    Attributes <Name>
    Description <summary>
    | Attribute | Type    | Default | Notes | In-Out |
    | Name      | String  |         |       | In     |
    | Age       | Integer |         |       | In     |

---

## 12. Entity

    Entity <Name>
    Description <summary>
    | Attribute | Type      | Default | Notes |
    | Address   | Address   |         |       |
    | Items     | LineItem* |         |       |

---

## 13. BusinessRule

    BusinessRule <Name>
    Description <summary>
    Details <multi-line using \>
    Constraint <text>
    Examples: <AttributeSet>
    | col1 | col2 | ... |
    | ...  | ...  | ... |
    Attributes <Name>
    | Attribute | Type | Default | Notes | In-Out |

Example:

    BusinessRule OverdraftFee
    Description Calculates overdraft fee when balance falls below zero.
    Examples: OverdraftFeeData
    | Balance | AccountType | Fee |
    | -50     | Checking    | 35  |
    Attributes OverdraftFeeData
    | Attribute   | Type    | Default | Notes | In-Out |
    | Balance     | Dollar  | 0       |       | In     |
    | AccountType | String  |         |       | In     |
    | Fee         | Dollar  | 0       |       | Out    |

---

## 14. Calculation

    Calculation <Name>
    Description <summary>
    Examples: <AttributeSet>
    | col1 | col2 | ... |
    Attributes <Name>
    | Attribute | Type | Default | Notes | In-Out |

---

## 15. Scenario

### Normal table

    Scenario <text>
    Description <summary>
    Given <desc> : <AttributeSet>
    | col1 | col2 |
    | val1 | val2 |
    When <desc> : <AttributeSet>
    | ...  |
    Then <desc> : <AttributeSet>
    | ...  |

### Transposed table

    Scenario Customer Info
    Given customer : Customer Transposed
    | Name | Ken |
    | Age  | 42  |

### Grid table (DataType only)

    Given keypad : Integer
    | 1 | 2 | 3 |
    | 4 | 5 | 6 |

---

## 16. ScenarioGroup

    ScenarioGroup <name>
    Description <summary>
    Details \
      <multi-line> \
    Scenario <text>
      Given ...
      When ...
      Then ...
    Scenario <text>
      ...

---

## 17. Background

    Background:
    Description <summary>
    Given <desc> : <AttributeSet> [Transposed]
    | ...  |
    And <desc> : <AttributeSet>
    | ...  |

---

## 18. Import / Insert

    Import "common.spectable"
    Description Imports shared type/rule definitions.

    Insert "data/accounts.csv"
    Description Inserts reference data for documentation.

`Import` follows the file and resolves its symbols.
`Insert` records the path for documentation; it is not parsed.

---

## 19. Grammar Summary

    StepHeader    ::= StepKeyword Text ":" AttributeSetName [ "Transposed" ]
    ExamplesBlock ::= "Examples:" AttributeSetName Table
    DefineBlock   ::= "Define" Identifier "=" ( Table | TextBlock | Text )
    DataTypeEnum  ::= "DataType" Identifier [ Description ] [ Details ]
                      "Examples:" "EnumerationValues" Table
    DataTypeValid ::= "DataType" Identifier [ Description ] [ Details ]
                      "Examples:" "ValidValues" Table

---

## 20. Semantics

- `Description`, `Details`, `Constraint` are named comments with no runtime semantics.
- Unnamed `#` comments may appear anywhere on any line.
- `Examples:` requires a colon and an AttributeSet name.
- `Transposed` may follow any AttributeSet name in a step header.
- `=Name` is a value reference that must resolve to a `Define` declaration.
- `EnumerationValues` and `ValidValues` are built-in AttributeSets — no declaration needed.
- Built-in DataTypes (`Character` … `YesNo`) need no `DataType` declaration.
- `Background` applies its steps to all Scenarios in the file.
- `ScenarioGroup` ends at the next `ScenarioGroup` or EOF.
- `Scenario` ends at the next top-level command.
