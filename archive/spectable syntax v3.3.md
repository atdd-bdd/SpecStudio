Here it is, Ken — a **clean, formal, fully updated SpecTable v3.3 syntax** that incorporates:

* **Collections** as first‑class types

* **Vertical** tables for single‑record expressions

* **Reference assignment using** `=`

* Your full example rewritten in polished v3.3 form

* Updated grammar sections

* Updated semantic rules

This is a **complete, ready‑to‑publish** v3.3 syntax update.
⭐ **SpecTable Syntax v3.3 — Collection & Vertical Enhancements**
================================================================

📘 **1. New Type: Collection**
------------------------------

A **Collection** is a named list type that contains zero or more instances of a DataType.

Code
    Collection <Name>
    | DataType | Minimum | Maximum | Notes |
    | <Type>   | <Min>   | <Max>   | <Notes> |

### Semantics

* `<Type>` must be an Entity.

* `<Min>` and `<Max>` define cardinality.

* Collections may be used as attribute types inside Entities.

* Collections may be instantiated in Step Tables or Define blocks.

📘 **2. Vertical Tables**
-------------------------

A **Vertical** table expresses a single Entity instance using a vertical attribute layout.

Code
    <Entity> Vertical
    | Attribute | Value |
    | A1        | V1    |
    | A2        | V2    |

### Semantics

* Equivalent to a one‑row horizontal table.

* Used when adding or modifying a single item.

* Works naturally with Collections.

📘 **3. Reference Assignment Using** `=`
----------------------------------------

References to Define blocks or named collections use:

Code
    | Attribute | Value |
    | Items     | =InitialItems |

### Semantics

* `=` means “use the previously defined value.”

* Works for Entities, Collections, and primitive values.

📘 **4. Entities**
------------------

Code
    Entity <Name>
    | Attribute | Type | Default | Notes |
    | A1        | T1   | D1      | N1    |
    | A2        | T2   | D2      | N2    |

Types may include:

* Primitive types

* Other Entities

* Collections

📘 **5. Scenarios**
-------------------

Code
    Given <Name> : <Type>
    <Step Table>

    When <Action> : <Type>
    <Step Table or Vertical>

    Then <Name> is : <Type>
    <Step Table or Reference>
⭐ **SpecTable v3.3 — Full Example Using Collections + Vertical +** `=`
======================================================================

This is your example rewritten in polished v3.3 syntax.
**Entities**
------------

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
**Simple Collection Manipulation**
----------------------------------

Code
    Given Items : ItemCollection
    | Name     | Quantity |
    | Widget   | 1        |

    When item added : Item Vertical
    | Attribute | Value   |
    | Name      | Widget2 |
    | Quantity  | 3       |

    Then Items are : ItemCollection
    | Name     | Quantity |
    | Widget   | 1        |
    | Widget2  | 3        |
**Define Blocks**
-----------------

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
**ShoppingCart Scenario**
-------------------------

Code
    Given cart is : ShoppingCart
    | Attribute | Value         |
    | Orderer   | Bill          |
    | Items     | =InitialItems |

    When item added : Item Vertical
    | Attribute | Value   |
    | Name      | Widget4 |
    | Quantity  | 2       |

    Then cart is : ShoppingCart
    | Attribute | Value        |
    | Orderer   | Bill         |
    | Items     | =FinalItems  |
⭐ **Why v3.3 Is a Big Improvement**
===================================

### ✔ Collections are explicit

No more hidden Multiples semantics.

### ✔ Vertical tables are intuitive

Non‑programmers understand “vertical form.”

### ✔ Reference assignment with `=` is natural

Matches Define syntax and Check syntax.

### ✔ Step tables remain simple

Collections behave exactly how users expect.

### ✔ Code generation becomes trivial

Collections → listsVertical → single objectReferences → reuse existing objects
⭐ If you want, I can produce next:
==================================

* The **formal grammar** for v3.3

* The **semantic analyzer rules**

* The **code generator rules** for all languages

* A **v3.3 specification document** ready for publication

* A **migration guide** from v3.2 → v3.3

Just tell me which piece you want next.
