Ken — this is a _great_ question, and honestly one of the hardest problems in any table‑driven DSL:

> **How do you represent an attribute whose value is itself an Entity (with multiple attributes)?**
> 
> And even harder:
> 
> **How do you represent _multiple_ instances of that nested entity?**

SpecTable v2.6 is table‑centric, which is great for flat structures, but nested entities require a clean, readable, and parser‑friendly extension.

Let me walk you through the **four viable patterns**, then I’ll give you the **recommended official v2.7 syntax** that fits perfectly with Define and =Name.

* * *

⭐ The Two Cases You Identified
==============================

**Case 1 — A single nested entity**
-----------------------------------

Example:  
An `Order` has an `Address` attribute, and `Address` has:

* Street
* City
* State

**Case 2 — Multiple nested entities**
-------------------------------------

Example:  
An `Order` has multiple `LineItem` entries, each with:

* SKU
* Quantity
* Price

These two cases need different solutions.

* * *

⭐ The Four Realistic Representation Strategies
==============================================

1️⃣ **Inline expansion (not recommended)**
------------------------------------------

    | Attribute | Value                |
    | Address   | Street=..., City=... |

Problems:

* Ugly
* Hard to parse
* Hard to read
* Breaks when values get long

We reject this.

* * *

2️⃣ **Nested table inside a cell (looks nice, impossible to parse)**
--------------------------------------------------------------------

    | Attribute | Value |
    | Address   | | Street | City | State |
                 | 123 A   | ...  | ...   |

Looks cute, but:

* Ambiguous
* Breaks table alignment
* Parser nightmare

We reject this too.

* * *

3️⃣ **Define + =Name (single nested entity)**
---------------------------------------------

This is where v2.6 shines.

### **Define the nested entity as a block:**

    Define Address1 =
    """
    | Street     | City      | State |
    | 123 Main   | Durham    | NC    |
    """

### **Reference it in the parent table:**

    | Attribute | Value     |
    | Address   | =Address1 |

### ✔ Pros

* Clean
* Reusable
* Parser‑friendly
* Human‑readable
* Works for any size entity

### ✔ This solves **Case 1** perfectly.

* * *

4️⃣ **Define a list of nested entities (multiple instances)**
-------------------------------------------------------------

For multiple nested entities, we extend the same idea:

### **Define a multi‑row table block:**

    Define LineItems =
    """
    | SKU     | Qty | Price |
    | ABC123  | 2   | 10.00 |
    | XYZ999  | 1   | 25.00 |
    """

### **Reference it in the parent entity:**

    | Attribute | Value       |
    | Items     | =LineItems  |

### ✔ Pros

* Clean
* Reusable
* No special syntax
* Works for any number of nested rows

### ✔ This solves **Case 2** perfectly.

* * *

⭐ The Official Recommendation for SpecTable v2.7
================================================

Add this rule:

> **Any attribute whose value is an Entity or list of Entities must reference a Define block using `=Name`.**
> 
> The Define block contains a table representing the nested entity or list of entities.

This gives you:

### **Single nested entity**

    Define ShippingAddress =
    """
    | Street   | City   | State |
    | 123 Main | Durham | NC    |
    """
    
    Entity Order
    | Attribute | Type    | Default | Notes |
    | Address   | Address | =ShippingAddress |

### **Multiple nested entities**

    Define OrderItems =
    """
    | SKU     | Qty | Price |
    | ABC123  | 2   | 10.00 |
    | XYZ999  | 1   | 25.00 |
    """
    
    Entity Order
    | Attribute | Type      | Default | Notes |
    | Items     | LineItem* | =OrderItems |

(_You can optionally use `LineItem*` to indicate multiplicity, but that’s your call._)

* * *

⭐ Why this is the best solution
===============================

### ✔ Uses existing v2.6 features

No new syntax needed.

### ✔ Clean and readable

Tables stay narrow and readable.

### ✔ Works for both single and multiple nested entities

One mechanism covers both.

### ✔ Parser‑friendly

* `Define` blocks are already top‑level
* `=Name` is already a reference
* No nested table parsing required

### ✔ Reusable

Same Address or LineItems block can be reused across scenarios.

### ✔ Extensible

Works for deeply nested structures too.

* * *

⭐ If you want, I can now:
=========================

* Integrate this into **SpecTable v2.7**
* Update the reference card
* Update the grammar
* Update the test suite
* Provide examples for BusinessRules, Calculations, Scenarios, Entities

Just tell me.


