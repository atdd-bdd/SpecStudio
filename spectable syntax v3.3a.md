Ken — perfect addition. A **named comment block** called **Uses** fits naturally into SpecTable’s structure and solves two real needs:

* For **DataTypes**: show the underlying primitive (Integer, Text, Decimal, etc.)

* For **BusinessRules / Calculations / Scenarios**: show dependencies (“Uses: Rule A, Rule B”)

* For **Entities**: show related rules or supporting structures

Below is **SpecTable v3.3.a**, fully revised, with **Uses** added as a first‑class named comment element.

I’ve updated:

* Core language elements

* Grammar

* Semantics

* Examples

* Version notes

Everything is integrated cleanly and consistently.
📘 SpecTable v3.3.a — Specification Document
============================================

### _(Adds Named Comments:_ _**Uses**__)_

**1. Overview**
---------------

SpecTable v3.3.a is a structured, table‑driven specification language for defining entities, collections, scenarios, and expected behaviors.

Version **3.3.a** introduces:

1. **Named Comments: Uses** A formal comment block describing dependencies, underlying primitives, or referenced business rules.

2. All features from v3.3:
   
   * Collections
   
   * Vertical tables
   
   * Reference assignment (`=`)
   
   * Updated grammar and semantics

**2. Core Language Elements**
=============================

**2.1 Specification Header**
----------------------------

Code
    Specification <Title>

Defines the name of the specification file.
**2.2 Entities**
----------------

Entities describe structured data types.

Code
    Entity <Name>
    Uses <Comment>?
    | Attribute | Type | Default | Notes |
    | A1        | T1   | D1      | N1    |
    | A2        | T2   | D2      | N2    |

### **Rules**

* `<Name>` must be unique.

* `Uses` is optional.

* `Uses` may describe:
  
  * underlying primitive types
  
  * related business rules
  
  * referenced collections

* `Type` may be primitive, another Entity, or a Collection.

* `Default` and `Notes` are optional.

**2.3 Collections**
-------------------

Code
    Collection <Name>
    Uses <Comment>?
    | DataType | Minimum | Maximum | Notes |
    | <Type>   | <Min>   | <Max>   | <Notes> |

### **Rules**

* `<Type>` must be an Entity.

* `Uses` may describe:
  
  * underlying primitive types of the Entity
  
  * rules governing collection behavior

* `<Min>` and `<Max>` define cardinality.

**2.4 Vertical Tables**
-----------------------

Code
    <Entity> Vertical
    Uses <Comment>?
    | Attribute | Value |
    | A1        | V1    |
    | A2        | V2    |

### **Rules**

* Defines a single instance of an Entity.

* `Uses` may describe:
  
  * supporting rules
  
  * referenced Define blocks
  
  * underlying primitive types

**2.5 Reference Assignment (**`=`**)**
--------------------------------------

Code
    | Attribute | Value |
    | Items     | =InitialItems |

### **Rules**

* `=` means “use the previously defined value.”

* Works for Entities, Collections, and primitive values.

* Must refer to a valid Define block.

**2.6 Define Blocks**
---------------------

Code
    Define <Name>
    Uses <Comment>?
    | Attribute1 | Value1 |
    | Attribute2 | Value2 |

### **Rules**

* `Uses` may describe:
  
  * the purpose of the block
  
  * related rules
  
  * underlying primitive types

**2.7 Scenario Structure**
--------------------------

Code
    Given <Name> : <Type>
    Uses <Comment>?
    <Step Table or Vertical>

    When <Action> : <Type>
    Uses <Comment>?
    <Step Table or Vertical>

    Then <Name> is : <Type>
    Uses <Comment>?
    <Step Table or Reference>

### **Rules**

* `Uses` may describe:
  
  * business rules applied
  
  * referenced Define blocks
  
  * supporting calculations

**3. Formal Grammar (v3.3.a)**
==============================

Code
    Specification      ::= "Specification" Identifier

    Entity             ::= "Entity" Identifier Uses? EntityTable
    EntityTable        ::= Table(AttributeRow+)

    Collection         ::= "Collection" Identifier Uses? CollectionTable
    CollectionTable    ::= Table(CollectionRow)

    DefineBlock        ::= "Define" Identifier Uses? Table(Row+)

    Scenario           ::= (GivenStep WhenStep ThenStep)+

    GivenStep          ::= "Given" Identifier ":" Type Uses? (Table | Vertical)
    WhenStep           ::= "When" Action ":" Type Uses? (Table | Vertical)
    ThenStep           ::= "Then" Identifier "is" ":" Type Uses? (Table | Reference)

    Vertical           ::= Identifier "Vertical" Uses? Table(AttributeValueRow+)

    Uses               ::= "Uses" CommentText

    Reference          ::= "=" Identifier
**4. Semantics (v3.3.a)**
=========================

**4.1 Uses Semantics**
----------------------

* `Uses` is a **named comment**, not executable logic.

* It may appear on:
  
  * Entities
  
  * Collections
  
  * Define blocks
  
  * Vertical tables
  
  * Scenario steps

* It must contain human‑readable text.

* It may describe:
  
  * underlying primitive types
  
  * related business rules
  
  * referenced calculations
  
  * dependencies
  
  * purpose or intent

**4.2 Entity Semantics**
------------------------

* Attributes must match declared types.

* `Uses` may clarify primitive types or rule dependencies.

**4.3 Collection Semantics**
----------------------------

* Must contain between `Minimum` and `Maximum` items.

* `Uses` may describe collection rules or primitive types.

**4.4 Vertical Semantics**
--------------------------

* Produces exactly one instance.

* `Uses` may describe supporting rules or references.

**4.5 Reference Semantics**
---------------------------

* `=Name` must refer to a valid Define block.

* Must match expected type.

**4.6 Scenario Semantics**
--------------------------

* Given establishes initial state.

* When applies transformations.

* Then asserts final state.

* `Uses` may describe:
  
  * rules applied
  
  * supporting calculations
  
  * referenced Define blocks

**5. Full Example (v3.3.a)**
============================

**5.1 Entities**
----------------

Code
    Entity Item
    Uses Underlying primitive types: Name → LimitedText, Quantity → Integer
    | Attribute | Type        | Default | Notes |
    | Name      | LimitedText |         |       |
    | Quantity  | Integer     | 1       |       |

    Collection ItemCollection
    Uses Items are of type Item; underlying primitives: LimitedText, Integer
    | DataType | Minimum | Maximum | Notes |
    | Item     | 0       | 100     |       |

    Entity ShoppingCart
    Uses Uses ItemCollection and SimpleText; supports business rule AddItem
    | Attribute | Type           | Default | Notes |
    | Items     | ItemCollection |         |       |
    | Orderer   | SimpleText     |         |       |
**5.2 Simple Collection Manipulation**
--------------------------------------

Code
    Given Items : ItemCollection
    Uses Initial item list
    | Name     | Quantity |
    | Widget   | 1        |

    When item added : Item Vertical
    Uses Business rule: AddItem
    | Attribute | Value   |
    | Name      | Widget2 |
    | Quantity  | 3        |

    Then Items are : ItemCollection
    Uses Final expected list
    | Name     | Quantity |
    | Widget   | 1        |
    | Widget2  | 3        |
**5.3 Define Blocks**
---------------------

Code
    Define InitialItems
    Uses Starting inventory
    | Name     | Quantity |
    | Widget   | 1        |
    | Widget2  | 3        |

    Define FinalItems
    Uses Expected inventory after AddItem
    | Name     | Quantity |
    | Widget   | 1        |
    | Widget2  | 3        |
    | Widget4  | 2        |
**5.4 ShoppingCart Scenario**
-----------------------------

Code
    Given cart is : ShoppingCart
    Uses Initial cart setup
    | Attribute | Value         |
    | Orderer   | Bill          |
    | Items     | =InitialItems |

    When item added : Item Vertical
    Uses Business rule: AddItem
    | Attribute | Value   |
    | Name      | Widget4 |
    | Quantity  | 2        |

    Then cart is : ShoppingCart
    Uses Final cart state
    | Attribute | Value        |
    | Orderer   | Bill         |
    | Items     | =FinalItems  |
**6. Deprecated Features**
==========================

### **6.1 Multiples Column**

Removed in v3.3.Collections replace all multiplicity semantics.
**7. Version Notes**
====================

### **v3.3.a Enhancements**

* Added **Uses** named comment block

* Updated grammar

* Updated semantics

* Updated examples

* Clarified primitive type documentation

* Improved scenario readability

Ken — this is a **complete, publication‑ready v3.3.a spec** with **Uses** fully integrated.

If you want, I can also produce:

* **v3.3.a Quick Reference Guide**

* **v3.3.a Migration Guide (v3.3 → v3.3.a)**

* **v3.3.a Example Library**

* **v3.3.a Parser Implementation Plan**

Just tell me which one you want next.
