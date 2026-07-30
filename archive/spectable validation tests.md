# SpecTable v2.5 Conformance Test Suite

# All tests (valid and invalid) in a single file.

# A parser should report line numbers and error codes for invalid sections.

############################################################

# 1. IDENTIFIER TESTS

############################################################

# 1.1 Valid identifiers

DataType A
DataType _A
DataType A1
DataType A_B_C
DataType _x9_

# 1.2 Invalid identifiers (should fail)

DataType 1A          # ERROR: identifier starts with digit
DataType A-B         # ERROR: identifier contains hyphen
DataType A B         # ERROR: identifier contains space
DataType $Money      # ERROR: identifier starts with illegal character

############################################################

# 2. NAMED COMMENT TESTS

############################################################

# 2.1 Single-line Details

Details This is a single-line detail.

# 2.2 Multi-line Details (valid)

Details \
  Line one \
  Line two \
  Final line

# 2.3 Invalid multi-line Details (trailing slash on last line)

Details \
  Line one \
  Final line \    # ERROR: trailing "\" on last line

# 2.4 Description and Constraint

Description Short summary.
Constraint Must be unique.

############################################################

# 3. UNNAMED COMMENT TESTS

############################################################

# Standalone unnamed comment

# This is a comment

# Inline unnamed comment

DataType Dollar   # inline comment

# After a table row

DataType CommentTable
Examples
| Value | Notes |   # comment
| 1     | One   |

############################################################

# 4. DATATYPE TESTS

############################################################

# 4.1 Valid non-enum DataType

DataType DollarValid
Description Monetary amount.
Examples
| Value | Valid | Notes |
| 0     | Yes   | OK    |

# 4.2 Valid enum DataType

DataType AccountTypeValid
Description Allowed account categories.
| Value    | Notes |
| Checking |       |
| Savings  |       |

# 4.3 Invalid: enum with Examples

DataType AccountTypeInvalidEnum
| Value | Notes |
Examples
| A | B |      # ERROR: Examples not allowed after enum body

# 4.4 Invalid: non-enum missing Examples

DataType DollarMissingExamples
Description Missing examples.   # ERROR: non-enum DataType must have Examples

############################################################

# 5. DOMAINTERM TESTS

############################################################

# 5.1 Valid DomainTerm

DomainTerm CustomerID : AccountID

# 5.2 Invalid DomainTerm identifier

DomainTerm 1Customer : AccountID   # ERROR: invalid identifier

############################################################

# 6. ATTRIBUTES TESTS

############################################################

# 6.1 Valid Attributes

Attributes WithdrawalInputValid
| Attribute | Type   | Default | Notes | In-Out |
| Amount    | Dollar | 0       |       | In     |

# 6.2 Invalid Attributes: malformed header

Attributes WithdrawalInputInvalid
| Amount | Dollar |           # ERROR: header row malformed

############################################################

# 7. BUSINESSRULE TESTS

############################################################

# 7.1 Valid BusinessRule

Attributes OverdraftFee
| Attribute   | Type        | Default | Notes | In-Out |
| Balance     | Dollar      | 0       |       | In     |
| AccountType | AccountType | Checking|       | In     |
| Fee         | Dollar      | 0       |       | Out    |

BusinessRule OverdraftFeeValid : OverdraftFee
Description Calculates overdraft fee.
Examples
| Balance | AccountType | Fee | Notes |
| -50     | Checking    | 35  | OK    |

# 7.2 Invalid: missing Examples

BusinessRule OverdraftFeeMissingExamples : OverdraftFee
Description Missing examples.   # ERROR: BusinessRule must have Examples

# 7.3 Invalid: nested Attributes under BusinessRule

BusinessRule OverdraftFeeNestedAttributes : OverdraftFee
Attributes OverdraftFeeNested
| A | B |                      # ERROR: Attributes must be top-level, not nested

############################################################

# 8. CALCULATION TESTS

############################################################

# 8.1 Valid Calculation

Attributes NetBalance
| Attribute  | Type   | Default | Notes | In-Out |
| Balance    | Dollar | 0       |       | In     |
| Fee        | Dollar | 0       |       | In     |
| NetBalance | Dollar | 0       |       | Out    |

Calculation NetBalanceValid : NetBalance
Description Computes net balance.
Examples
| Balance | Fee | NetBalance | Notes |
| 100     | 1   | 99         | OK    |

# 8.2 Invalid: missing attribute set reference

Calculation NetBalanceMissingAttr
Description Missing attribute set.   # ERROR: must be "Calculation Name : AttributeSet"
Examples
| A | B |

############################################################

# 9. SCENARIO STEP TESTS

############################################################

# 9.1 Steps WITHOUT tables (valid)

Scenario TablelessSteps
Given account exists
When withdrawing 50 dollars
Then balance is updated

# 9.2 Steps WITH tables (valid)

Scenario StepsWithTables
Given accounts are: Account
| Type     | Balance | AccountID |
| Checking | 100     | 123-456   |
When withdrawing funds : WithdrawalInputValid
| Attribute | Value |
| Amount    | 50    |
Then resulting balance is: WithdrawalInputValid
| Attribute | Value |
| Amount    | 50    |

# 9.3 Invalid: table without step

| A | B |                      # ERROR: table must follow a step

# 9.4 Invalid: step with colon but no table

Scenario StepWithColonNoTable
Given accounts are: Account    # ERROR: colon implies table must follow

############################################################

# 10. BACKGROUND TESTS

############################################################

# 10.1 Valid Background

Background:
Description Shared setup.
Given accounts are: Account
| Type     | Balance | AccountID |
| Checking | 100     | 123-456   |
And more accounts are: Account
| Type     | Balance | AccountID |
| Savings  | 200     | 987-654   |

# 10.2 Invalid Background: When not allowed

Background:
When something happens          # ERROR: Background only allows Given/And

############################################################

# 11. SCENARIOGROUP TESTS

############################################################

# 11.1 Valid ScenarioGroup

ScenarioGroup WithdrawalsValid
Scenario WithdrawFromChecking
  Given account exists
  When withdrawing 50 dollars
  Then balance is updated

# 11.2 Invalid: nested ScenarioGroup

ScenarioGroup OuterGroup
ScenarioGroup InnerGroup       # ERROR: ScenarioGroup cannot be nested inside another

############################################################

# 12. IMPORT / INSERT TESTS

############################################################

# 12.1 Valid Import/Insert

Import "common.featurex"
Insert "../data.csv"

# 12.2 Invalid: missing quotes

Import common.featurex         # ERROR: file must be in quotes

############################################################

# 13. FULL VALID FILE SECTION

############################################################

Specification Example Spec
Description A full valid section.

DataType DollarFull
Examples
| Value | Valid | Notes |
| 0     | Yes   | OK    |

Attributes ASet
| Attribute | Type   | Default | Notes | In-Out |
| Amount    | Dollar | 0       |       | In     |

BusinessRule BR1 : ASet
Examples
| Amount | Result | Notes |
| 10     | 20     | OK    |

Scenario TestScenario
Given something happens
When processing : ASet
| Attribute | Value |
| Amount    | 10    |
Then result is : ASet
| Attribute | Value |
| Amount    | 20    |

############################################################

# 14. FULL INVALID FILE SECTION

############################################################

Specification Bad Spec
Details \ final line \         # ERROR: trailing "\" on last line

DataType 1Bad                  # ERROR: invalid identifier

BusinessRule BRMissing : MissingAttributes

# missing Examples             # ERROR: BusinessRule must have Examples

ScenarioGroup BadGroup
  Scenario BadScenario
    Given table without colon
    | A | B |                  # ERROR: table must follow step with colon
