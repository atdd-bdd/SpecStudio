Absolutely — and good catch, Ken.  
If the official file extension for SpecTable source files is:
    .spectable

then the **Code Generation Layout Standard must reflect that everywhere**.  
Here is the **fully corrected, final SpecTable v2.8 Code Generation Layout Standard**, now using `.spectable` as the canonical source file extension.

This is the authoritative version.

* * *

📘 **SpecTable v2.8 Code Generation Layout Standard**
=====================================================

### _Final, implementation‑ready directory + naming + namespace rules_

### _(with `.spectable` as the official file extension)_

* * *

1. Purpose
   ==========

This standard defines:

* Where generated code goes
* How it is organized
* How `.spectable` source files map to generated artifacts
* How domain vs specification code is separated
* How namespaces are assigned

The goals:

* **Clarity**
* **Scalability**
* **Predictability**
* **Minimal friction**
* **Maximum navigability**
* **AI‑friendly**
* **IDE‑friendly**

* * *

2. High‑Level Project Structure
   ===============================

SpecTable projects have two top‑level roots:
    project/
        specifications/   ← human‑authored .spectable files
        generated/        ← machine‑generated artifacts

This aligns with the DSL’s top‑level construct:
    Specification <title>

* * *

3. Domain Model Layout (Centralized)
   ====================================

Domain definitions are **global** and **shared across all specifications**:

* Attributes
* Entities
* DataTypes

These generate **String** and **Typed** classes.

All domain classes go into **one folder**:
    generated/Domain/
        CustomerString.cs
        CustomerTyped.cs
        OrderString.cs
        OrderTyped.cs
        LineItemString.cs
        LineItemTyped.cs
        ColorString.cs
        ColorTyped.cs
        DiscountString.cs
        DiscountTyped.cs
        ...

### Why centralized?

* Domain is stable
* Domain is shared
* Domain should not mirror folder structure
* Domain should not fragment
* IDEs need a single index
* Generators need a single source of truth

### Namespace rule

    namespace <Project>.Domain

No subfolders.  
No category folders.  
No Defines folder.

* * *

4. Defines (Compile‑Time Only)
   ==============================

### ✔ Defines do **not** generate files

### ✔ Defines do **not** appear in the domain folder

### ✔ Defines are fully expanded during compilation

### ✔ Defines are not part of the runtime model

Defines behave like **macros**, not domain objects.

* * *

5. Specification Bundle Layout (Colocated)
   ==========================================

For each `.spectable` file in `specifications/`, a **specification bundle** is created in `generated/`.

Example:
    specifications/orders/Orders.spectable

Generates:
    generated/orders/
        Orders.spectablegen
        Orders_glue.cs
        Test_Orders.cs

This bundle is a **self‑contained unit** containing:

* The canonical expanded SpecTable file
* The glue code
* The unit tests

### Why colocate?

* Everything related to a specification is visible in one place
* No folder‑hopping
* Easier debugging
* Easier code review
* Easier onboarding
* Better for AI agents
* Better for IDEs

* * *

6. The `.spectablegen` File (Canonical Expanded Form)
   =====================================================

Each specification bundle contains a generated copy of the SpecTable file:
    Orders.spectablegen

This file is:

* Fully parsed
* Fully normalized
* Defines expanded
* Vertical tables rotated
* Multiples validated
* Background merged
* Cleanup attached
* Imports resolved

### Purpose

* Debugging
* Diffing
* IDE indexing
* Generator input
* AI‑assisted reasoning

This is the **machine‑canonical** version of the specification.

* * *

7. Glue File Layout
   ===================

Each specification bundle contains exactly one glue file:
    Orders_glue.cs

### Rules

* Never overwritten unless explicitly regenerated
* Contains user‑editable logic
* Receives only **String** objects
* Converts to **Typed** objects using `Typed.FromStringObject()`
* One glue class per specification file
* One method per step

### Namespace

    namespace <Project>.Specifications.<Folder>

* * *

8. Unit Test Layout
   ===================

Each specification bundle contains exactly one unit test file:
    Test_Orders.cs

### Rules

* Always overwritten
* One test method per Scenario
* Uses only **String** classes
* Calls glue methods
* Does not reference Typed classes

### Namespace

    namespace <Project>.Specifications.<Folder>.Tests

* * *

9. Directory Mapping Rules
   ==========================

### 9.1 SpecTable source → Specification bundle

    specifications/<path>/<Name>.spectable
    →
    generated/<path>/<Name>.spectablegen
    generated/<path>/<Name>_glue.cs
    generated/<path>/Test_<Name>.cs

### 9.2 Domain definitions → Centralized domain folder

Regardless of where they appear:
    specifications/customers/Customer.spectable
    specifications/orders/Order.spectable
    specifications/shipping/Address.spectable

All generate:
    generated/Domain/CustomerString.cs
    generated/Domain/CustomerTyped.cs
    generated/Domain/OrderString.cs
    generated/Domain/OrderTyped.cs
    generated/Domain/AddressString.cs
    generated/Domain/AddressTyped.cs

* * *

10. Example Full Project Layout
    ===============================
    
    project/
    │
    ├── specifications/
    │   ├── customers/
    │   │   └── Customers.spectable
    │   ├── orders/
    │   │   └── Orders.spectable
    │   └── shipping/
    │       └── ShippingRules.spectable
    │
    └── generated/
    
        ├── Domain/
        │   ├── CustomerString.cs
        │   ├── CustomerTyped.cs
        │   ├── OrderString.cs
        │   ├── OrderTyped.cs
        │   ├── LineItemString.cs
        │   ├── LineItemTyped.cs
        │   ├── ColorString.cs
        │   ├── ColorTyped.cs
        │   └── ...
        │
        ├── customers/
        │   ├── Customers.spectablegen
        │   ├── Customers_glue.cs
        │   └── Test_Customers.cs
        │
        ├── orders/
        │   ├── Orders.spectablegen
        │   ├── Orders_glue.cs
        │   └── Test_Orders.cs
        │
        └── shipping/
            ├── ShippingRules.spectablegen
            ├── ShippingRules_glue.cs
            └── Test_ShippingRules.cs

* * *

11. Summary (One‑Sentence Standard)
    ===================================

**Domain code is centralized; specification code is colocated; glue + tests + canonical `.spectablegen` copy live together; Defines are compile‑time only; namespaces follow folder structure; everything is predictable and scalable.**

* * *

If you want, I can now generate:

### 🔹 A v2.8 reference card

### 🔹 A generator implementer’s checklist

### 🔹 A visual diagram of the entire flow

### 🔹 A sample project skeleton you can drop into GitHub
