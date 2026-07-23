Absolutely — let’s generate a **complete, clean, fully updated syntax document** for your DSL, incorporating your latest decisions:

* **IsValid** as the example-table boolean column

* **Scientific** as the floating‑point numeric type

* **Decimal** as the exact numeric type

* All semantics aligned with your current SpecTable direction

Below is the **full, standalone syntax document**, written as a cohesive specification.No templates, no file creation — just the full syntax, ready for you to drop into your repo.
**SpecTable Syntax — Full Updated Specification (v3.0)**
========================================================

This document defines the complete syntax for the SpecTable DSL, including Entities, AttributeSets, DataTypes, BusinessRules, Scenarios, and built‑in types. It incorporates updated semantics for **IsValid** and **Scientific**.

1. **Top‑Level Structure**

--------------------------

A SpecTable document consists of zero or more of the following top‑level constructs:

* **Entity**

* **AttributeSet**

* **DataType**

* **BusinessRule**

* **Scenario**

Each construct begins with a header keyword and a name.

Example:

Code
    Entity Customer
    AttributeSet Address
    DataType PostalCode
    BusinessRule CustomerMustHaveValidAddress
    Scenario CustomerAddressValidation

2. **Entities**
   ===============

2.1 Syntax
----------

Code
    Entity <Name>
    Attributes:
        <AttributeName>: <Type>
        <AttributeName>: <Type>
        ...
2.2 Semantics
-------------

* Defines a domain object with named attributes.

* Attributes reference:
  
  * Built‑in types (String, Integer, Decimal, Scientific, Boolean, Date, Time, DateTime)
  
  * DataTypes
  
  * AttributeSets

2.3 Example
-----------

Code
    Entity Customer
    Attributes:
        Id: Integer
        Name: String
        Address: Address

3. **AttributeSets**
   ====================

3.1 Syntax
----------

Code
    AttributeSet <Name>
    Attributes:
        <AttributeName>: <Type>
        ...
3.2 Semantics
-------------

* Defines reusable attribute groups.

* Can be referenced by Entities or other AttributeSets.

3.3 Example
-----------

Code
    AttributeSet Address
    Attributes:
        Street: String
        City: String
        PostalCode: PostalCode

4. **DataTypes**
   ================

4.1 Syntax
----------

Code
    DataType <Name>
    Examples:
        | Value | IsValid |
        | ...   | Yes/No  |
    Rules:
        <RuleExpression>
        <RuleExpression>
4.2 Semantics
-------------

### **IsValid Column (Updated)**

* The example table MUST use **IsValid** as the boolean column header.

* Values are **Yes** or **No**.

* Indicates whether the example satisfies the DataType’s rules.

### Rules

Rules describe constraints on the value.Supported rule forms include:

* `MustMatch <Regex>`

* `MustBeOneOf <List>`

* `MustBeAtLeast <Number>`

* `MustBeAtMost <Number>`

* `MustBeBetween <Min> and <Max>`

* `MustHaveLength <N>`

* `MustHaveLengthBetween <Min> and <Max>`

* `MustBeValid <OtherDataType>`

4.3 Example
-----------

Code
    DataType PostalCode
    Examples:
        | Value   | IsValid |
        | 12345   | Yes     |
        | 12-345  | No      |
    Rules:
        MustMatch ^\d{5}$

5. **BusinessRules**
   ====================

5.1 Syntax
----------

Code
    BusinessRule <Name>
    Given:
        <EntityOrAttribute> <Condition>
    When:
        <TriggerCondition>
    Then:
        <Outcome>
5.2 Semantics
-------------

* Defines a rule that applies to entities or attributes.

* Conditions may reference:
  
  * Entity attributes
  
  * DataTypes
  
  * Built‑in types

5.3 Example
-----------

Code
    BusinessRule CustomerMustHaveValidAddress
    Given:
        Customer.Address.PostalCode IsValid
    When:
        Customer is created
    Then:
        Reject creation

6. **Scenarios**
   ================

6.1 Syntax
----------

Code
    Scenario <Name>
    Given:
        <InitialState>
    When:
        <Action>
    Then:
        <ExpectedOutcome>
6.2 Semantics
-------------

* Defines an executable example of system behavior.

* Uses concrete values for Entities and DataTypes.

6.3 Example
-----------

Code
    Scenario ValidCustomerAddress
    Given:
        Customer:
            Id = 1
            Name = "Alice"
            Address:
                Street = "10 Main St"
                City = "Durham"
                PostalCode = "27701"
    When:
        Customer is validated
    Then:
        Result = Success

7. **Built‑In Types**
   =====================

7.1 Overview
------------

SpecTable provides built‑in primitive types:

* **String**

* **Integer**

* **Decimal**

* **Scientific** (Updated)

* **Boolean**

* **Date**

* **Time**

* **DateTime**

7.2 **Decimal**
---------------

### Semantics

* Exact base‑10 numeric type.

* No exponent.

* Ideal for money, business rules, and precise quantities.

* Maps to:
  
  * Java: `BigDecimal`
  
  * C#: `decimal`
  
  * Python: `Decimal`
  
  * C++: arbitrary precision libraries

### Examples

Code
    12.34
    0.99
    100.00
7.3 **Scientific** (Updated)
----------------------------

### Semantics

* Approximate numeric type.

* Supports fractional digits.

* Supports exponent notation (e.g., `1.23e6`).

* Suitable for measurements, physics, engineering, large/small magnitudes.

* Maps to:
  
  * Java: `double`
  
  * C#: `double`
  
  * Python: `float`
  
  * C++: `double`

### Examples

Code
    1.23
    4.5e6
    -3.2e-4
    0.000045

### Rationale

* “Scientific” is intuitive for non‑programmers.

* Conveys exponent capability without exposing floating‑point implementation details.

* Distinguishes clearly from **Decimal**.

7.4 **Boolean**
---------------

Values:

* `Yes`

* `No`

Used in:

* BusinessRules

* Scenarios

* DataType example tables (via **IsValid**)
8. **Expressions**
   ==================

Supported expression forms include:

* Equality: `A = B`

* Inequality: `A != B`

* Comparison: `<`, `<=`, `>`, `>=`

* Boolean logic: `and`, `or`, `not`

* Membership: `A in [x, y, z]`

* Pattern matching: `A matches <Regex>`

* Validity: `A IsValid`
9. **Comments**
   ===============

Single‑line comments:

Code
    # This is a comment

10. **Whitespace and Formatting**
    =================================
* Indentation is significant for readability but not syntactically required.

* Tables must use `|` separators.

* Names are case‑sensitive.

**Complete Syntax Summary**
===========================

Code
    Entity <Name>
    AttributeSet <Name>
    DataType <Name>
    BusinessRule <Name>
    Scenario <Name>
    Attributes:
    Examples:
    Rules:
    Given:
    When:
    Then:
    IsValid
    Decimal
    Scientific

If you'd like, I can also generate:

* A **formal grammar** (EBNF)

* A **parser‑friendly version**

* A **LLM guidance appendix**

* A **reference implementation mapping** for Java/C#/Python/C++

Just tell me what direction you want next.
