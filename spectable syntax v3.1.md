Absolutely, Ken — here is your **fully updated, complete SpecTable Syntax Document (v2.1)** with explicit support for:

* Nested Entities

* Nested Define blocks

* Deeply nested Scenario assignments

* Updated built‑in types (**Scientific**, **Decimal**)

* Updated example semantics (**IsValid**)

* Clean, consistent formatting

This is a **single, cohesive, ready‑to‑publish** syntax document.
**SpecTable Syntax — Version 2.1**
==================================

A SpecTable document defines domain models, reusable attribute groups, data types, business rules, and executable scenarios using a compact, table‑driven syntax. It supports arbitrarily nested Entities and structured example data through Define blocks.
**1. Top‑Level Constructs**
===========================

A SpecTable file may contain any number of:

* **Entity**

* **AttributeSet**

* **DataType**

* **Define**

* **BusinessRule**

* **Scenario**

Each construct begins with a header keyword and a name.

Example:

Code
    Entity Customer
    AttributeSet Address
    DataType PostalCode
    Define AShippingAddress
    BusinessRule CustomerMustHaveValidAddress
    Scenario ValidCustomerAddress
**2. Entities**
===============

2.1 Syntax
----------

Code
    Entity <Name>
    | Name | Type | Default | Notes |
    | ...  | ...  | ...     | ...   |
2.2 Semantics
-------------

* Defines a domain object with named attributes.

* Attribute types may be:
  
  * Built‑in types
  
  * DataTypes
  
  * AttributeSets
  
  * Other Entities (supports nesting)

* Default values are optional.

* Notes are optional.

2.3 Example
-----------

Code
    Entity Address
    | Name   | Type   | Default | Notes |
    | Street | String |         |       |
    | City   | String |         |       |
    | State  | String |         |       |
    | ZIP    | String |         |       |
**3. AttributeSets**
====================

3.1 Syntax
----------

Code
    AttributeSet <Name>
    | Name | Type | Default | Notes |
    | ...  | ...  | ...     | ...   |
3.2 Semantics
-------------

* Defines reusable attribute groups.

* May be referenced by Entities or other AttributeSets.

**4. DataTypes**
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

### Example Table

* Must contain columns **Value** and **IsValid**.

* **IsValid** is a Boolean expressed as **Yes** or **No**.

* Indicates whether the example satisfies the DataType’s rules.

### Rules

Supported rule forms include:

* `MustMatch <Regex>`

* `MustBeOneOf <List>`

* `MustBeAtLeast <Number>`

* `MustBeAtMost <Number>`

* `MustBeBetween <Min> and <Max>`

* `MustHaveLength <N>`

* `MustHaveLengthBetween <Min> and <Max>`

* `Format: <NaturalLanguageDescription>`

* `MustBeValid <OtherDataType>`

4.3 Example
-----------

Code
    DataType PostalCode
    Examples:
    | Value | IsValid |
    | 27701 | Yes     |
    | 27A01 | No      |
    Rules:
        MustMatch ^\d{5}$
**5. Define Blocks**
====================

5.1 Syntax
----------

Code
    Define <Name> =
    | Column1 | Column2 | ... |
    | Value1  | Value2  | ... |
5.2 Semantics
-------------

* A Define block represents **an instance of an Entity**.

* The table columns must match the Entity’s attributes.

* Define blocks may contain attributes whose types are Entities.

* Nested Entities may be assigned using `=OtherDefineName`.

* Nesting depth is unlimited.

5.3 Example
-----------

Code
    Define AShippingAddress =
    | Street       | City      | State | ZIP   |
    | 2 Apple Lane | Somewhere | NC    | 27706 |
**6. BusinessRules**
====================

6.1 Syntax
----------

Code
    BusinessRule <Name>
    Given:
        <Condition>
    When:
        <Trigger>
    Then:
        <Outcome>
6.2 Semantics
-------------

* Defines a rule that applies to Entities or attributes.

* Conditions may reference nested attributes.

* May use DataTypes and built‑in types.

**7. Scenarios**
================

7.1 Syntax
----------

Code
    Scenario <Name>
    Given <InstanceName> : <EntityName>
    | Attribute1 | Attribute2 | ... |
    | Value1     | Value2     | ... |
    When:
        <Action>
    Then:
        <ExpectedOutcome>
7.2 Semantics
-------------

* Scenarios instantiate Entities using tables.

* Entity attributes may be assigned using:
  
  * Literal values
  
  * Define blocks (`=Name`)
  
  * Nested Define blocks

* Supports arbitrarily deep nested structures.

7.3 Example (Nested Entities)
-----------------------------

Code
    Scenario A ShoppingCart with Addresses
    Given cart : ShoppingCart
    | ShippingAddress   | BillingAddress   |
    | =AShippingAddress | =ABillingAddress |
    When:
        cart is validated
    Then:
        Result = Success
**8. Built‑In Types**
=====================

8.1 String
----------

Text value.
8.2 Integer
-----------

Whole number.
8.3 Decimal
-----------

* Exact base‑10 number

* No exponent

* Maps to BigDecimal / decimal / Decimal

8.4 Scientific
--------------

* Approximate number

* Supports exponent notation

* Maps to double / float

8.5 Boolean
-----------

Values: **Yes**, **No**
8.6 Date, Time, DateTime
------------------------

Standard ISO‑like formats.
**9. Expressions**
==================

Supported forms:

* Equality: `A = B`

* Inequality: `A != B`

* Comparison: `<`, `<=`, `>`, `>=`

* Boolean logic: `and`, `or`, `not`

* Membership: `A in [x, y, z]`

* Pattern matching: `A matches <Regex>`

* Validity: `A IsValid`

**10. Comments**
================

Code
    # This is a comment
**11. Formatting Rules**
========================

* Tables use `|` separators.

* Names are case‑sensitive.

* Indentation is optional but recommended.

* Nested tables are allowed only through Define blocks.

**12. Complete Syntax Summary**
===============================

Code
    Entity <Name>
    AttributeSet <Name>
    DataType <Name>
    Define <Name>
    BusinessRule <Name>
    Scenario <Name>

    Examples:
    Rules:
    Given:
    When:
    Then:

    IsValid
    Decimal
    Scientific
    Boolean

Ken, this is now a **complete, polished, production‑ready syntax document** that fully supports nested Entities, nested Define blocks, and your updated type system.

If you want, I can also generate:

* A formal EBNF grammar

* A parser‑friendly version

* A validator specification

* A code‑generation mapping for Java/C#/Python/C++

Just tell me what direction you want next.
