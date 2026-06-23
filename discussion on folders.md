Ken — this is a _great_ architectural question, and it’s one that every DSL with multi‑file semantics eventually has to answer. You’re basically asking:

> **How should SpecTable resolve global definitions (DataTypes, Attributes, Entities, Defines) across many files in a large project?**

There are really **three viable models**, and each one has deep consequences for usability, tooling, and maintainability. Let me walk you through them — and then I’ll tell you which one is the right choice for SpecTable.

* * *

⭐ The Three Models
==================

**Model 1 — Everything is global (automatic import of all files)**
------------------------------------------------------------------

This is your first inclination:

* Every `.spectable` file automatically sees:
  * All DataTypes
  * All Attributes
  * All Entities
  * All Defines
  * All DomainTerms
  * All BusinessRules
  * All Calculations

### ✔ Advantages

* **Zero friction** — everything is available everywhere.
* **Perfect for large teams** — no one has to think about imports.
* **IDE simplicity** — global index, global autocomplete.
* **Generators love it** — no dependency graph needed.
* **SpecTable files stay clean** — no boilerplate imports.

### ✘ Disadvantages

* Name collisions become global collisions.
* Harder to isolate subsystems.
* Harder to reuse a subset of files in another project.

### When this model works best

* Enterprise projects with hundreds of scenarios.
* Teams that want a “single shared domain model.”
* Projects where domain terms are stable and universal.

* * *

**Model 2 — Only the “common” file is automatically imported**
--------------------------------------------------------------

This is your second idea:

* One file (e.g., `Common.spec`) is implicitly imported everywhere.
* Everything else must be explicitly imported.

### ✔ Advantages

* Gives you a **controlled global namespace**.
* Allows modularity:
  * `Payments.spec` can import `Money.spec`
  * `Shipping.spec` can import `Address.spec`
* Easier to reuse subsets of the project.

### ✘ Disadvantages

* Every file must declare imports.
* More boilerplate.
* More cognitive load.
* IDE must track per‑file dependency graphs.
* Generators must resolve imports recursively.

### When this model works best

* Projects with multiple bounded contexts.
* Teams that want domain isolation.
* DSLs that behave like programming languages.

* * *

**Model 3 — Each file is its own package**
------------------------------------------

This is the strictest model:

* Every file is a module.
* Nothing is visible unless imported.
* No automatic imports at all.

### ✔ Advantages

* Maximum modularity.
* Perfect for library ecosystems.
* No global namespace collisions.

### ✘ Disadvantages

* **Way too heavy** for SpecTable’s purpose.
* Turns a business‑friendly DSL into a programming language.
* Requires package management, versioning, dependency resolution.
* Slows down authoring dramatically.

### When this model works best

* Never, for SpecTable.
* This is for languages like Python, Rust, Java.

* * *

⭐ Which model fits SpecTable’s philosophy?
==========================================

SpecTable is:

* A **business‑friendly DSL**
* Designed for **clarity**, **readability**, **low friction**
* Intended for **large projects** with many scenarios
* Intended to be **AI‑digestible**
* Intended to be **tool‑friendly**

Given that, the right choice is:

* * *

🎯 **Recommended Model: Model 1 (Everything is global)**
========================================================

This is the model that:

* Fits the mental model of business analysts
* Fits the mental model of testers
* Fits the mental model of AI agents
* Fits the mental model of code generators
* Minimizes friction
* Maximizes productivity
* Keeps files clean
* Keeps the DSL simple
* Keeps the IDE simple
* Avoids dependency hell
* Avoids import boilerplate
* Avoids package management

And importantly:

### ✔ It matches how Gherkin, FitNesse, DMN, and most DSLs behave

They all assume a **global domain model**.

* * *

⭐ But what about name collisions?
=================================

Easy solution:

### ✔ Namespaces by directory

If two teams define `Customer`, you can do:
    billing/Customer.spec
    shipping/Customer.spec

And the generator can namespace them:
    Billing.Customer
    Shipping.Customer

This gives you modularity **without** forcing imports.

* * *

⭐ What about people who _don’t_ want global definitions?
========================================================

You already solved it:

> **If one didn’t want the common one, they could use a different name.**

Exactly.

If someone wants isolation:

* Put their definitions in a different directory
* Or use a prefix (`PaymentsCustomer`, `ShippingCustomer`)
* Or use a different project root

SpecTable doesn’t need to enforce modularity — the filesystem already gives it.

* * *

⭐ Final Recommendation
======================

### ✔ **SpecTable should use a global domain model.**

### ✔ **All files automatically see all definitions.**

### ✔ **No imports required.**

### ✔ **No packages.**

### ✔ **No dependency graphs.**

### ✔ **Directory structure provides optional namespacing.**

This is the model that keeps SpecTable:

* Simple
* Scalable
* Predictable
* Business‑friendly
* AI‑friendly
* IDE‑friendly
* Generator‑friendly

And it avoids turning SpecTable into a programming language.

* * *

If you want, I can now produce:
===============================

### 🔹 SpecTable v2.8 “Global Domain Model” rules

### 🔹 SpecTable v2.8 “Namespacing by directory” rules

### 🔹 SpecTable v2.8 “Import semantics (optional)”

### 🔹 SpecTable v2.8 “Project structure best practices”

Just tell me which direction you want to go.



Ken — this is one of those deceptively simple questions that actually determines how **clean**, **scalable**, and **maintainable** SpecTable projects will be over the next decade. You’re thinking about it at exactly the right time.

Let’s break it down the way an architect would.

* * *

🎯 The Core Issue
=================

You have:

* **SpecTable source files** (organized in folders however the user wants)
* **Generated code** (String classes, Typed classes, Glue, Unit tests)

And the question is:

> Should the generated code mirror the folder structure of the SpecTable files,  
> or should it be flattened into a few canonical folders?

This is really about **domain stability** vs **scenario volatility**.

* * *

⭐ The Right Answer (and why):
=============================

**Generated domain code (Attributes, Entities, DataTypes, DomainTerms, Defines) should go into a single canonical folder.**
---------------------------------------------------------------------------------------------------------------------------

**Generated scenario code (Glue + Unit Tests) should mirror the folder structure of the SpecTable files.**
----------------------------------------------------------------------------------------------------------

This hybrid model is the sweet spot.

Let me explain why.

* * *

🧠 Why Domain Code Should NOT Mirror Folder Structure
=====================================================

Domain definitions are:

* **Global**
* **Stable**
* **Shared across all scenarios**
* **Part of the project’s canonical domain model**

If you mirror folder structure, you get:
    domain/customers/CustomerTyped.cs
    domain/orders/OrderTyped.cs
    domain/shipping/AddressTyped.cs

This creates **artificial fragmentation** of the domain model.

It also creates:

* Namespace sprawl
* Harder-to-find domain objects
* Confusion about where a DataType “belongs”
* Circular dependencies if folders import each other
* IDE autocomplete that jumps between folders

And worst of all:

### ❌ The domain model becomes tied to the file layout

which is the opposite of what you want.

Domain concepts should be **logical**, not **physical**.

* * *

⭐ Why Scenario Code SHOULD Mirror Folder Structure
==================================================

Scenarios are:

* **Volatile**
* **Organized by feature area**
* **Often grouped by team**
* **Frequently added/removed**
* **Not part of the domain model**
* **Not reused across features**

If your project has:
    features/customers/*.spec
    features/orders/*.spec
    features/shipping/*.spec

Then the generated glue + tests should follow:
    generated/Glue/customers/*.cs
    generated/Glue/orders/*.cs
    generated/Glue/shipping/*.cs

    generated/UnitTests/customers/*.cs
    generated/UnitTests/orders/*.cs
    generated/UnitTests/shipping/*.cs

This gives:

### ✔ Clean organization

### ✔ Easy navigation

### ✔ Team‑friendly structure

### ✔ No namespace collisions

### ✔ No domain pollution

### ✔ Perfect alignment with how people think about features

* * *

⭐ The Final Recommended Layout
==============================

**1. Domain (flattened)**
-------------------------

All domain definitions go here:
    generated/Domain/
        DataTypes/
        Attributes/
        Entities/
        Defines/

This is your **canonical domain model**.

* * *

**2. Glue (mirrors folder structure)**
--------------------------------------

    generated/Glue/
        customers/
        orders/
        shipping/

Each SpecTable file produces a glue file in the same relative folder.

* * *

**3. Unit Tests (mirrors folder structure)**
--------------------------------------------

    generated/UnitTests/
        customers/
        orders/
        shipping/

Each scenario produces a test file in the same relative folder.

* * *

⭐ Why This Hybrid Model Is the Best
===================================

### ✔ Domain is stable → keep it centralized

### ✔ Scenarios are volatile → keep them organized by feature

### ✔ Avoids namespace chaos

### ✔ Avoids domain fragmentation

### ✔ Keeps generated code predictable

### ✔ Keeps IDE navigation clean

### ✔ Works beautifully with large projects

### ✔ Works beautifully with AI agents

### ✔ Works beautifully with code generators

This is the same model used by:

* Cucumber
* FitNesse
* DMN
* Protobuf
* GraphQL codegen
* Swagger/OpenAPI codegen
* Thrift
* gRPC

They all centralize the domain model and mirror the feature structure for tests.

* * *

🎁 Summary (the short version)
==============================

**Put all generated domain code in one place.**  
**Put all generated scenario code in folders that mirror the SpecTable file structure.**

This gives you:

* Clean domain
* Clean features
* Clean glue
* Clean tests
* Maximum scalability
* Minimum friction

* * *

If you want, I can now generate:

### 🔹 The official “SpecTable v2.8 Project Layout Standard”

### 🔹 The official “SpecTable v2.8 Code Generation Directory Rules”

### 🔹 A diagram showing the full flow from `.spectable` → generated code

### 🔹 A sample folder tree with namespaces included

Just tell me.
