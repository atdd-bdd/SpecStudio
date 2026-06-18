# SpecTable DSL Syntax Guide (v2.2)

A unified, business‑friendly, automation‑ready language for describing specifications, datatypes, domain terms, business rules, calculations, entities, scenarios, and scenario groups.

============================================================
0. SPECIFICATION
============================================================

Top-level command:
Specification <text>

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Notes:

- Specification appears once at the top of the file.
- Description and Details are optional and treated as comments.

Example:
Specification Account Withdrawal Rules
Description Defines the rules and scenarios for withdrawing money.
Details \
  Covers checking and savings withdrawals. \

============================================================

1. DATATYPES
   ============================================================

Top-level command:
DataType <Name>

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Examples (required unless enumeration):
Examples
| ... columns including optional Notes ... |

Enumeration form (no Examples):
| Value | Notes |

Example:
DataType AccountID
Description Must be three digits, a dash, then three digits.
Details \
  Regex: ^\d{3}-\d{3}$ \
Examples
| Value   | Valid | Notes |
| 123-456 | Yes   | Correct format |

============================================================
2. DOMAIN TERMS
============================================================

Top-level command:
DomainTerm <Name> : <DataTypeName>

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Example:
DomainTerm CustomerID : AccountID
Description Business identifier for a customer.

============================================================
3. BUSINESS RULES
============================================================

Top-level command:
BusinessRule <Name>

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Examples (required):
Examples
| ... | Notes |

Attributes (required):
Attributes <Name>
| Attribute | Type | Default | Notes | In-Out |

Example:
BusinessRule OverdraftFee
Description Calculates overdraft fee.
Details \
  Applies only when Balance < 0. \
Examples
| Balance | AccountType | Fee | Notes |
| -50     | Checking    | 35  | Standard fee |
Attributes OverdraftFee
| Attribute | Type | Default | Notes | In-Out |
| Balance   | Dollar | 0 | | In |
| Fee       | Dollar | 0 | | Out |

============================================================
4. CALCULATIONS
============================================================

Top-level command:
Calculation <Name>

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Examples (required):
Examples
| ... | Notes |

Attributes (required):
Attributes <Name>
| Attribute | Type | Default | Notes | In-Out |

Example:
Calculation NetBalance
Description Computes net balance.
Details \
  NetBalance = Balance - Fee \
Examples
| Balance | Fee | NetBalance | Notes |
| 100     | 1   | 99         | Basic case |
Attributes NetBalance
| Attribute  | Type   | Default | Notes | In-Out |
| Balance    | Dollar | 0       |       | In     |
| Fee        | Dollar | 0       |       | In     |
| NetBalance | Dollar | 0       |       | Out    |

============================================================
5. ENTITIES
============================================================

Top-level command:
Entity <Name>

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Example:
Entity Account
Description Represents a bank account.
| Attribute | Type        | Default  | Notes |
| Type      | AccountType | Checking |       |
| Balance   | Dollar      | 0        |       |
| AccountID | AccountID   | (none)   | Must be provided |

============================================================
6. ATTRIBUTES
============================================================

Top-level command:
Attributes <Name>

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Example:
Attributes WithdrawalInput
| Attribute | Type | Default | Notes |
| Amount    | Dollar | 0 | Withdrawal amount |

============================================================
7. SCENARIOS
============================================================

Top-level command:
Scenario <text>

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Allowed children:
Given / When / Then / And

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
8. COMMENTS
============================================================

Named comments:
Description <text>
Details \
  <multi-line text> \
Constraint <text>

Notes:

- These are comments only.
- They may appear under any top-level command.
- They do not affect semantics.

Example:
Constraint AccountID must be unique.



Unnamed comment begins with a #.   It may appear on its own line or be appended to any line 

============================================================
9. BACKGROUND
============================================================

Top-level command:
Background:

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Contains:
Given / And steps only.

Example:
Background:
Description Shared setup for all scenarios.
Given accounts are: Account
| Type     | Balance | AccountID |
| Checking | 100     | 123-456   |

============================================================
10. SCENARIO GROUPS
============================================================

Top-level command:
ScenarioGroup <name>

Optional named comments:
Description <summary>
Details \
  <multi-line explanation> \

Contains:
Scenario blocks only.

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
11. DIRECTIVES (IMPORT / INSERT)
============================================================

Top-level commands:
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
12. SEMANTICS
============================================================

Description
Defines the meaning of all constructs.

Details \

- Description, Details, Constraint are comments. \
- Examples required for BusinessRule, Calculation, DataType (non-enum). \
- Scenario ends at next top-level command. \
- ScenarioGroup ends at next ScenarioGroup or EOF. \
- Background applies to all Scenarios. \

============================================================
13. TAXONOMY
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
  (Comments: Description, Details, Constraint) \

Example:
| Concept       | Purpose |
|---------------|---------|
| DataType      | Defines structure |
| Scenario      | Behavioral flow |
| ScenarioGroup | Organizes scenarios |
