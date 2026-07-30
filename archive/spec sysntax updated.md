```
# SpecTable DSL Syntax Guide (v2.4)

A unified, business‑friendly, automation‑ready language for describing specifications, datatypes, domain terms, business rules, calculations, entities, scenarios, and scenario groups.

============================================================
0. COMMENTS (NAMED AND UNNAMED)
============================================================

Description
SpecTable supports two kinds of comments: named and unnamed.

Details \
  Named comments: \
    - Description <text> \
    - Details <multi-line text using \> \
    - Constraint <text> \
  These may appear under ANY top-level command. \
  They do not affect semantics. \
  \
  Unnamed comments: \
    - Begin with "#" \
    - May appear on their own line \
    - Or appended to the end of any line \
  Unnamed comments have no semantic meaning. \

Example:
# Unnamed comment
DataType AccountID   # Inline unnamed comment
Description Identifier for an account.
Details \
  Must match regex. \
Constraint Must be unique.

============================================================
1. TOP-LEVEL COMMANDS
============================================================

The following are the ONLY top-level commands:

- Specification
- DataType
- DomainTerm
- BusinessRule
- Calculation
- Entity
- Attributes
- Scenario
- ScenarioGroup
- Background
- Import
- Insert

Each may contain optional named comments (Description, Details, Constraint).

============================================================
2. SPECIFICATION
============================================================

Syntax:
Specification <text>
Description <summary>        (optional)
Details \                     (optional)
  <multi-line explanation> \

Example:
Specification Account Withdrawal Rules
Description Defines the rules and scenarios for withdrawing money.
Details \
  Covers checking and savings withdrawals. \

============================================================
3. DATATYPES
============================================================

Syntax:
DataType <Name>
Description <summary>        (optional)
Details \                     (optional)
  <multi-line explanation> \
Examples                      (required unless enumeration)
| ... | Notes |

Enumeration form:
| Value | Notes |

Example:
DataType AccountType
Description Allowed account categories.
Details \
  These values represent the only valid account types. \
| Value    | Notes |
| Checking |       |
| Savings  |       |

============================================================
4. DOMAIN TERMS
============================================================

Syntax:
DomainTerm <Name> : <DataTypeName>
Description <summary>        (optional)
Details \                     (optional)
  <multi-line explanation> \

Example:
DomainTerm CustomerID : AccountID
Description Business identifier for a customer.

============================================================
5. BUSINESS RULES
============================================================

Syntax:
BusinessRule <Name> : <AttributeSet>
Description <summary>        (optional)
Details \                     (optional)
  <multi-line explanation> \
Examples                      (required)
| ... | Notes |

Example:
BusinessRule OverdraftFee : OverdraftFee
Description Calculates overdraft fee.
Details \
  Applies only when Balance < 0. \
Examples
| Balance | AccountType | Fee | Notes |
| -50     | Checking    | 35  | Standard fee |
| -50     | Savings     | 25  | Lower fee |

============================================================
6. CALCULATIONS
============================================================

Syntax:
Calculation <Name> : <AttributeSet>
Description <summary>        (optional)
Details \                     (optional)
  <multi-line explanation> \
Examples                      (required)
| ... | Notes |

Example:
Calculation NetBalance : NetBalance
Description Computes net balance.
Details \
  NetBalance = Balance - Fee \
Examples
| Balance | Fee | NetBalance | Notes |
| 100     | 1   | 99         | Basic case |

============================================================
7. ENTITIES
============================================================

Syntax:
Entity <Name>
Description <summary>        (optional)
Details \                     (optional)
  <multi-line explanation> \
| Attribute | Type | Default | Notes |

Example:
Entity Account
Description Represents a bank account.
| Attribute | Type        | Default  | Notes |
| Type      | AccountType | Checking |       |
| Balance   | Dollar      | 0        |       |
| AccountID | AccountID   | (none)   | Must be provided |

============================================================
8. ATTRIBUTES
============================================================

Syntax:
Attributes <Name>
Description <summary>        (optional)
Details \                     (optional)
  <multi-line explanation> \
| Attribute | Type | Default | Notes | In-Out? |

Notes:
- Attributes blocks are stand-alone.
- Used by BusinessRule, Calculation, Scenario step tables.

Example:
Attributes OverdraftFee
| Attribute   | Type        | Default | Notes | In-Out |
| Balance     | Dollar      | 0       |       | In     |
| AccountType | AccountType | Checking|       | In     |
| Fee         | Dollar      | 0       |       | Out    |

============================================================
9. SCENARIOS
============================================================

Syntax:
Scenario <text>
Description <summary>        (optional)
Details \                     (optional)
  <multi-line explanation> \
Given <desc> : <AttributeSet>
And <desc> : <AttributeSet>
When <desc> : <AttributeSet>
Then <desc> : <AttributeSet>

Example:
Scenario Withdraw from checking
Description Normal withdrawal scenario.
Given accounts are: Account
| Type     | Balance | AccountID |
| Checking | 100     | 123-456   |
When withdrawing funds : WithdrawalInput
| Attribute | Value |
| Amount    | 50    |
Then resulting balance is: BalanceCheck
| Attribute | Value |
| Balance   | 50    |

============================================================
10. BACKGROUND
============================================================

Syntax:
Background:
Description <summary>        (optional)
Details \                     (optional)
  <multi-line explanation> \
Given ...
And ...

Example:
Background:
Description Shared setup for all scenarios.
Given accounts are: Account
| Type     | Balance | AccountID |
| Checking | 100     | 123-456   |

============================================================
11. SCENARIO GROUPS
============================================================

Syntax:
ScenarioGroup <name>
Description <summary>        (optional)
Details \                     (optional)
  <multi-line explanation> \
Scenario <text>
  Given ...
  When ...
  Then ...

Example:
ScenarioGroup Withdrawals
Description Scenarios related to withdrawals.
Details \
  Covers checking and savings. \
Scenario Withdraw from checking
  Given ...
  When ...
  Then ...

============================================================
12. DIRECTIVES (IMPORT / INSERT)
============================================================

Syntax:
Import "file"
Insert "file"

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Example:
Import "common-types.featurex"
Insert "../data/accounts.csv"

============================================================
13. SEMANTICS
============================================================

Description
Defines the meaning of all constructs.

Details \
  - Description, Details, Constraint are comments. \
  - Unnamed comments (#) may appear anywhere. \
  - Examples required for BusinessRule, Calculation, DataType (non-enum). \
  - Scenario ends at next top-level command. \
  - ScenarioGroup ends at next ScenarioGroup or EOF. \
  - Background applies to all Scenarios. \
  - BusinessRule <Name> : <AttributeSet> binds rule to its Attributes block. \
  - Calculation <Name> : <AttributeSet> binds calculation to its Attributes block. \

============================================================
14. TAXONOMY
============================================================

Description
Summary of all top-level commands.

Details \
  Specification \
  DataType \
  DomainTerm \
  BusinessRule \
  Calculation \
  Entity \
  Attributes \
  Scenario \
  ScenarioGroup \
  Background \
  Import \
  Insert \
  (Comments: Description, Details, Constraint, #) \

Example:
| Concept       | Purpose |
|---------------|---------|
| Scenario      | Behavioral flow |
| ScenarioGroup | Organizes scenarios |
| DataType      | Defines structure |
| BusinessRule  | Defines logic |
| Calculation   | Defines formulas |

```
