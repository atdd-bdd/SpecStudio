```
xample SpecTable v2.5 file

Specification Account Withdrawal Rules
Description Defines rules and scenarios for withdrawing money from accounts.
Details \
  Covers checking and savings withdrawals. \
  Includes business rules, calculations, and scenarios. 

Background:
Description Shared setup for all scenarios.
Given accounts are: Account
| Type     | Balance | AccountID |
| Checking | 100     | 123-456   |
| Savings  | 200     | 987-654   |

Entity Account
Description Represents a bank account.
| Attribute | Type        | Default  | Notes            |
| Type      | AccountType | Checking |                  |
| Balance   | Dollar      | 0        | Must be >= 0     |
| AccountID | AccountID   | (none)   | Must be provided |

DataType Dollar
Description Monetary amount in dollars.
Details  Must be non-negative for balances.
Examples
| Value | Valid | Notes              |
| 0     | Yes   | Zero allowed       |
| 0.01  | Yes   | Two decimal digits |
| -1    | No    | Negative not ok    |
| .111  | No    | More than 2 digits |

BusinessRule OverdraftFee : OverdraftFee
Description Calculates overdraft fee based on balance and account type.
Details Applies only when Balance < 0.
Examples
| Balance | AccountType | Fee | Notes                  |
| -50     | Checking    | 35  | Standard overdraft fee|
| -50     | Savings     | 25  | Lower fee for savings |
| 100     | Checking    | 0   | No fee when >= 0      |

Attributes OverdraftFee
Description Attributes for overdraft fee rule.
| Attribute   | Type        | Default | Notes | In-Out |
| Balance     | Dollar      | 0       |       | In     |
| AccountType | AccountType | Checking|       | In     |
| Fee         | Dollar      | 0       |       | Out    |


Calculation NetBalance : NetBalance
Description Computes net balance after subtracting fee.
Details \
  NetBalance = Balance - Fee \
Examples
| Balance | Fee | NetBalance | Notes          |
| 100     | 1   | 99         | Basic case     |
| -50     | 25  | -75        | Negative case  |

Attributes NetBalance
Description Attributes for net balance calculation.
| Attribute  | Type   | Default | Notes | In-Out |
| Balance    | Dollar | 0       |       | In     |
| Fee        | Dollar | 0       |       | In     |
| NetBalance | Dollar | 0       |       | Out    |



ScenarioGroup Withdrawals
Description Scenarios related to withdrawing funds.
Details \
  Covers checking and savings withdrawals. \
  Includes normal and edge cases. \
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

Attributes WithdrawalInput
Description Input for withdrawal scenarios.
| Attribute | Type   | Default | Notes             |
| Amount    | Dollar | 0       | Withdrawal amount |

Attributes BalanceCheck
Description Output for balance verification.
| Attribute | Type   | Default | Notes             |
| Balance   | Dollar | 0       | Resulting balance |

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


```

```
# Example SpecTable v2.5 file

Specification Account Withdrawal Rules
Description Defines rules and scenarios for withdrawing money from accounts.
Details \
  Covers checking and savings withdrawals. \
  Includes business rules, calculations, and scenarios. \

DataType Dollar
Description Monetary amount in dollars.
Details \
  Must be non-negative for balances. \
Examples
| Value | Valid | Notes          |
| 0     | Yes   | Zero allowed   |
| -1    | No    | Negidentifier for a customer.

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
  NetBalance = Balance - Fee \
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
  Includes normal and edge cases. \
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

```
