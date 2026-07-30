Ken, here is a **complete, publication‑ready SpecTable v3.3 Specification Document**.It incorporates:

* **Collections**

* **Vertical tables**

* **Reference assignment using** `=`

* Updated grammar

* Updated semantics

* Updated examples

* Your ShoppingCart + ItemCollection example

* Clean structure suitable for your official documentation

Everything is written as a formal specification, not a tutorial.
📘 **SpecTable v3.3 — Specification Document**
==============================================

**1. Overview**
---------------

SpecTable v3.3 is a structured, table‑driven specification language for defining entities, collections, scenarios, and expected behaviors.Version 3.3 introduces three major enhancements:

1. **Collection Types** — first‑class list types with cardinality rules

2. **Vertical Tables** — single‑record entity expressions

3. **Reference Assignment (**`=`**)** — reuse of named Define blocks or collections

These features simplify modeling, improve readability, and eliminate the need for the deprecated _Multiples_ concept.
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
    | Attribute | Type | Default | Notes |
    | A1        | T1   | D1      | N1    |
    | A2        | T2   | D2      | N2    |

### **Rules**

* `<Name>` must be unique.

* `Type` may be:
  
  * Primitive type
  
  * Another Entity
  
  * A Collection

* `Default` is optional.

* `Notes` is optional.

**2.3 Collections**
-------------------

Collections define list‑typed structures with cardinality constraints.

Code
    Collection <Name>
    | DataType | Minimum | Maximum | Notes |
    | <Type>   | <Min>   | <Max>   | <Notes> |

### **Rules**

* `<Type>` must be an Entity.

* `<Min>` and `<Max>` define allowed number of items.

* Collections may be used as attribute types inside Entities.

* Collections may be instantiated in Step Tables or Define blocks.

**2.4 Vertical Tables**
-----------------------

Vertical tables define a **single instance** of an Entity using a vertical attribute layout.

Code
    <Entity> Vertical
    | Attribute | Value |
    | A1        | V1    |
    | A2        | V2    |

### **Rules**

* Equivalent to a one‑row horizontal table.

* Used for actions that add or modify a single item.

* Works naturally with Collections.

**2.5 Reference Assignment (**`=`**)**
--------------------------------------

References allow reuse of previously defined data.

Code
    | Attribute | Value |
    | Items     | =InitialItems |

### **Rules**

* `=` means “use the previously defined value.”

* Works for Entities, Collections, and primitive values.

* References must point to a valid Define block.

**2.6 Define Blocks**
---------------------

Define blocks create reusable named data sets.

Code
    Define <Name>
    | Attribute1 | Value1 |
    | Attribute2 | Value2 |

Define blocks may represent:

* Entity instances

* Collections

* Primitive values

**2.7 Scenario Structure**
--------------------------

A scenario describes a sequence of Given → When → Then steps.

Code
    Given <Name> : <Type>
    <Step Table or Vertical>

    When <Action> : <Type>
    <Step Table or Vertical>

    Then <Name> is : <Type>
    <Step Table or Reference>
**3. Formal Grammar (v3.3)**
============================

This grammar is intentionally high‑level and implementation‑neutral.

Code
    Specification      ::= "Specification" Identifier

    Entity             ::= "Entity" Identifier EntityTable
    EntityTable        ::= Table(AttributeRow+)

    Collection         ::= "Collection" Identifier CollectionTable
    CollectionTable    ::= Table(CollectionRow)

    AttributeRow       ::= Identifier Type Default? Notes?
    CollectionRow      ::= Type Integer Integer Notes?

    DefineBlock        ::= "Define" Identifier Table(Row+)

    Scenario           ::= (GivenStep WhenStep ThenStep)+

    GivenStep          ::= "Given" Identifier ":" Type (Table | Vertical)
    WhenStep           ::= "When" Action ":" Type (Table | Vertical)
    ThenStep           ::= "Then" Identifier "is" ":" Type (Table | Reference)

    Vertical           ::= Identifier "Vertical" Table(AttributeValueRow+)

    Reference          ::= "=" Identifier
**4. Semantics (v3.3)**
=======================

### **4.1 Entity Semantics**

* Each attribute must match its declared type.

* Collection attributes must receive a Collection instance.

### **4.2 Collection Semantics**

* A Collection instance must contain between `Minimum` and `Maximum` items.

* Each item must be of the declared DataType.

### **4.3 Vertical Semantics**

* A Vertical table produces exactly one instance of the Entity.

* All required attributes must be present.

### **4.4 Reference Semantics**

* `=Name` must refer to a valid Define block.

* The referenced block must match the expected type.

### **4.5 Scenario Semantics**

* Given establishes initial state.

* When applies transformations.

* Then asserts final state.

**5. Full Example (v3.3)**
==========================

This example demonstrates Collections, Vertical tables, and reference assignment.
**5.1 Entities**
----------------

Code
    Entity Item
    | Attribute | Type        | Default | Notes |
    | Name      | LimitedText |         |       |
    | Quantity  | Integer     | 1       |       |

    Collection ItemCollection
    | DataType | Minimum | Maximum | Notes |
    | Item     | 0       | 100     |       |

    Entity ShoppingCart
    | Attribute | Type           | Default | Notes |
    | Items     | ItemCollection |         |       |
    | Orderer   | SimpleText     |         |       |
**5.2 Simple Collection Manipulation**
--------------------------------------

Code
    Given Items : ItemCollection
    | Name     | Quantity |
    | Widget   | 1        |

    When item added : Item Vertical
    | Attribute | Value   |
    | Name      | Widget2 |
    | Quantity  | 3        |

    Then Items are : ItemCollection
    | Name     | Quantity |
    | Widget   | 1        |
    | Widget2  | 3        |
**5.3 Define Blocks**
---------------------

Code
    Define InitialItems
    | Name     | Quantity |
    | Widget   | 1        |
    | Widget2  | 3        |

    Define FinalItems
    | Name     | Quantity |
    | Widget   | 1        |
    | Widget2  | 3        |
    | Widget4  | 2        |
**5.4 ShoppingCart Scenario**
-----------------------------

Code
    Given cart is : ShoppingCart
    | Attribute | Value         |
    | Orderer   | Bill          |
    | Items     | =InitialItems |

    When item added : Item Vertical
    | Attribute | Value   |
    | Name      | Widget4 |
    | Quantity  | 2        |

    Then cart is : ShoppingCart
    | Attribute | Value        |
    | Orderer   | Bill         |
    | Items     | =FinalItems  |
**6. Deprecated Features**
==========================

### **6.1 Multiples Column**

The Multiples column is removed in v3.3.Collections replace all multiplicity semantics.
**7. Version Notes**
====================

### **v3.3 Enhancements**

* Added **Collection** type

* Added **Vertical** tables

* Added **Reference assignment (**`=`**)**

* Removed **Multiples**

* Updated grammar and semantics

Ken, this is a **complete, formal v3.3 specification document** ready for inclusion in your official SpecStudio documentation.

If you want, I can also produce:

* A **v3.3 Quick Reference Guide**

* A **v3.3 Migration Guide (v3.2 → v3.3)**

* A **v3.3 Example Library**

* A **v3.3 Parser Implementation Plan**

Just tell me which one you want next.
