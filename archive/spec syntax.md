### SpecTable DSL v2.5 — Complete Syntax (clean, with corrected multi‑line comments)

* * *

1. Core structure

-----------------

**Top‑level commands:**

* `Specification`
* `DataType`
* `DomainTerm`
* `BusinessRule`
* `Calculation`
* `Entity`
* `Attributes`
* `Scenario`
* `ScenarioGroup`
* `Background`
* `Import`
* `Insert`

**Comments:**

* **Named comments** (optional under any top‑level command, no semantic effect):
  * `Description <text>`
  * `Details \` (multi‑line; every line except the last ends with `\`)
  * `Constraint <text>`
* **Unnamed comments**:
  * Any text starting with `#`
  * May be on their own line or appended to any line

* * *

2. Command syntax

-----------------

### Specification

    Specification <text>
    Description <summary>        (optional)
    Details \                     (optional)
      First line of explanation \
      Second line of explanation \
      Final line of explanation

### DataType

**Non‑enum:**
    DataType <Name>
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2
    Examples                      (required)
    | ... | Notes |

**Enum:**
    DataType <Name>
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2
    | Value | Notes |

### DomainTerm

    DomainTerm <Name> : <DataTypeName>
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2

### BusinessRule

    BusinessRule <Name> : <AttributeSet>
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2
    Examples                      (required)
    | ... | Notes |

### Calculation

    Calculation <Name> : <AttributeSet>
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2
    Examples                      (required)
    | ... | Notes |

### Entity

    Entity <Name>
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2
    | Attribute | Type | Default | Notes |

### Attributes

    Attributes <Name>
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2
    | Attribute | Type | Default | Notes | In-Out? |

### Scenario

    Scenario <text>
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2
    Given <desc> : <AttributeSet>
    | ... table ... |
    And <desc> : <AttributeSet>
    | ... table ... |
    When <desc> : <AttributeSet>
    | ... table ... |
    Then <desc> : <AttributeSet>
    | ... table ... |

### Background

    Background:
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2
    Given <desc> : <AttributeSet>
    | ... table ... |
    And <desc> : <AttributeSet>
    | ... table ... |

### ScenarioGroup

    ScenarioGroup <name>
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2
    Scenario <text>
      Given ...
      When ...
      Then ...
    Scenario <text>
      ...

### Directives

    Import "file"
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2
    
    Insert "file"
    Description <summary>        (optional)
    Details \                     (optional)
      Explanation line 1 \
      Explanation line 2

* * *

3. Compact EBNF‑style grammar

-----------------------------

    File          ::= { TopLevelCommand | CommentLine } ;
    
    TopLevelCommand ::=
          Specification
        | DataType
        | DomainTerm
        | BusinessRule
        | Calculation
        | Entity
        | AttributesBlock
        | Scenario
        | ScenarioGroup
        | Background
        | ImportDirective
        | InsertDirective ;
    
    CommentLine   ::= "#" { ANY_CHAR } ;
    
    NamedComment  ::= DescriptionBlock | DetailsBlock | ConstraintBlock ;
    
    DescriptionBlock ::= "Description" WS TextLine ;
    DetailsBlock      ::= "Details" WS "\" NL { IndentedTextLine "\" NL } IndentedTextLine NL ;
    ConstraintBlock   ::= "Constraint" WS TextLine ;
    
    Specification ::= "Specification" WS TextLine NL
                      { NamedComment NL } ;
    
    DataType ::= "DataType" WS Identifier NL
                { NamedComment NL }
                ( EnumBody | DataTypeBody ) ;
    
    EnumBody ::= TableHeaderRow NL { TableDataRow NL } ;
    DataTypeBody ::= "Examples" NL
                     TableHeaderRow NL { TableDataRow NL } ;
    
    DomainTerm ::= "DomainTerm" WS Identifier WS ":" WS Identifier NL
                  { NamedComment NL } ;
    
    BusinessRule ::= "BusinessRule" WS Identifier WS ":" WS Identifier NL
                    { NamedComment NL }
                    "Examples" NL
                    TableHeaderRow NL { TableDataRow NL } ;
    
    Calculation ::= "Calculation" WS Identifier WS ":" WS Identifier NL
                   { NamedComment NL }
                   "Examples" NL
                   TableHeaderRow NL { TableDataRow NL } ;
    
    Entity ::= "Entity" WS Identifier NL
              { NamedComment NL }
              TableHeaderRow NL { TableDataRow NL } ;
    
    AttributesBlock ::= "Attributes" WS Identifier NL
                       { NamedComment NL }
                       TableHeaderRow NL { TableDataRow NL } ;
    
    Scenario ::= "Scenario" WS TextLine NL
                { NamedComment NL }
                { StepBlock } ;
    
    StepBlock ::= StepCommand WS StepDesc WS ":" WS Identifier NL
                 TableHeaderRow NL { TableDataRow NL } ;
    
    StepCommand ::= "Given" | "When" | "Then" | "And" ;
    StepDesc    ::= TextLine ;
    
    Background ::= "Background:" NL
                  { NamedComment NL }
                  { BackgroundStep } ;
    
    BackgroundStep ::= ("Given" | "And") WS StepDesc WS ":" WS Identifier NL
                       TableHeaderRow NL { TableDataRow NL } ;
    
    ScenarioGroup ::= "ScenarioGroup" WS Identifier NL
                     { NamedComment NL }
                     { Scenario } ;
    
    ImportDirective ::= "Import" WS StringLiteral NL
                       { NamedComment NL } ;
    
    InsertDirective ::= "Insert" WS StringLiteral NL
                       { NamedComment NL } ;
    
    TableHeaderRow ::= "|" WS HeaderCell { WS "|" WS HeaderCell } WS "|" ;
    TableDataRow   ::= "|" WS DataCell   { WS "|" WS DataCell   } WS "|" ;
    
    HeaderCell ::= TEXT ;
    DataCell   ::= TEXT ;
    
    Identifier  ::= TEXT ;
    StringLiteral ::= '"' { ANY_CHAR_EXCEPT_QUOTE } '"' ;
    
    TextLine    ::= { ANY_CHAR_EXCEPT_NL } ;
    WS          ::= { " " | "\t" } ;
    NL          ::= "\r\n" | "\n" ;
    IndentedTextLine ::= WS { ANY_CHAR_EXCEPT_NL } ;

* * *

4. Full example file (v2.5)  update this with the other feature file    

---------------------------

    # Example SpecTable v2.5 file
    
    Specification Account Withdrawal Rules
    Description Defines rules and scenarios for withdrawing money from accounts.
    Details \
      Covers checking and savings withdrawals. \
      Includes business rules, calculations, and scenarios.
    
    DataType Dollar
    Description Monetary amount in dollars.
    Details \
      Must be non-negative for balances.
    Examples
    | Value | Valid | Notes          |
    | 0     | Yes   | Zero allowed   |
    | -1    | No    | Negative not ok|
    
    DataType AccountType
    Description Allowed account categories.
    Details \
      These values represent the only valid account types.
    | Value    | Notes |
    | Checking |       |
    | Savings  |       |
    
    DomainTerm CustomerID : AccountID
    Description Business identifier for a customer.
    
    Entity Account
    Description Represents a bank account.
    | Attribute | Type        | Default  | Notes             |
    | Type      | AccountType | Checking |                   |
    | Balance   | Dollar      | 0        | Must be >= 0     |
    | AccountID | AccountID   | (none)   | Must be provided |
    
    Attributes OverdraftFee
    Description Attributes for overdraft fee rule.
    | Attribute   | Type        | Default | Notes | In-Out |
    | Balance     | Dollar      | 0       |       | In     |
    | AccountType | AccountType | Checking|       | In     |
    | Fee         | Dollar      | 0       |       | Out    |
    
    BusinessRule OverdraftFee : OverdraftFee
    Description Calculates overdraft fee based on balance and account type.
    Details \
      Applies only when Balance < 0. \
      Savings accounts have lower fees.
    Examples
    | Balance | AccountType | Fee | Notes                  |
    | -50     | Checking    | 35  | Standard overdraft fee|
    | -50     | Savings     | 25  | Lower fee for savings |
    | 100     | Checking    | 0   | No fee when >= 0      |
    
    Attributes NetBalance
    Description Attributes for net balance calculation.
    | Attribute  | Type   | Default | Notes | In-Out |
    | Balance    | Dollar | 0       |       | In     |
    | Fee        | Dollar | 0       |       | In     |
    | NetBalance | Dollar | 0       |       | Out    |
    
    Calculation NetBalance : NetBalance
    Description Computes net balance after subtracting fee.
    Details \
      NetBalance = Balance - Fee
    Examples
    | Balance | Fee | NetBalance | Notes          |
    | 100     | 1   | 99         | Basic case     |
    | -50     | 25  | -75        | Negative case  |
    
    Attributes WithdrawalInput
    Description Input for withdrawal scenarios.
    | Attribute | Type   | Default | Notes             |
    | Amount    | Dollar | 0       | Withdrawal amount |
    
    Attributes BalanceCheck
    Description Output for balance verification.
    | Attribute | Type   | Default | Notes             |
    | Balance   | Dollar | 0       | Resulting balance |
    
    Background:
    Description Shared setup for all scenarios.
    Details \
      Ensures accounts exist before each scenario.
    Given accounts are: Account
    | Type     | Balance | AccountID |
    | Checking | 100     | 123-456   |
    And more accounts are: Account
    | Type     | Balance | AccountID |
    | Savings  | 200     | 987-654   |
    
    ScenarioGroup Withdrawals
    Description Scenarios related to withdrawing funds.
    Details \
      Covers checking and savings withdrawals. \
      Includes normal and edge cases.
    Scenario Withdraw from checking
    Description Normal checking withdrawal.
    Given accounts are: Account
    | Type     | Balance | AccountID |
    | Checking | 100     | 123-456   |
    When withdrawing funds : WithdrawalInput
    | Attribute | Value |
    | Amount    | 50    |
    Then resulting balance is: BalanceCheck
    | Attribute | Value |
    | Balance   | 50    |
    
    Scenario Withdraw from savings
    Description Normal savings withdrawal.
    Given accounts are: Account
    | Type     | Balance | AccountID |
    | Savings  | 200     | 987-654   |
    When withdrawing funds : WithdrawalInput
    | Attribute | Value |
    | Amount    | 50    |
    Then resulting balance is: BalanceCheck
    | Attribute | Value |
    | Balance   | 150   |
    
    Import "common-types.featurex"
    Description Imports shared datatype definitions.
    
    Insert "../data/accounts.csv"
    Description Inserts raw account data for documentation.

If you want to tighten identifiers or add explicit numeric/string types in the grammar next, we can do that as a focused pass.
