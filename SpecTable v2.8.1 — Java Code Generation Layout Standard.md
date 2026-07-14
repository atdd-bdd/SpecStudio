Ken — here is the **fully regenerated, authoritative, Java‑specific SpecTable v2.8 Code Generation Layout Standard**, now incorporating:

* `.spectable` as the source file extension
* Centralized domain folder
* Colocated specification bundles
* Glue + ALL unit tests in the same file
* Scenarios + BusinessRules + Calculations + DataTypes all in **Test_<SpecName>.java**
* Java package rules (folder ↔ package alignment)
* Updated naming rules for test methods
* Defines expanded at compile time

This is the clean, final version.

* * *

📘 **SpecTable v2.8 — Java Code Generation Layout Standard**
============================================================

### _Final, implementation‑ready directory + naming + namespace rules for Java_

* * *

1. Purpose
   ==========

This standard defines:

* How `.spectable` files map to Java code
* Where generated Java files are placed
* How Java packages must be structured
* How domain vs specification code is separated
* How unit tests are generated
* How glue methods are named
* How Calculations, BusinessRules, DataTypes, and Scenarios produce test methods

The goals:

* **Clarity**
* **Scalability**
* **Predictability**
* **Minimal friction**
* **Maximum navigability**
* **Java‑compliant package structure**

* * *

2. High‑Level Project Structure
   ===============================
   
    project/
   
        specifications/     ← human-authored .spectable files
        generated/          ← machine-generated Java code

* * *

3. Java Package Rules (Mandatory)
   =================================

Java requires:
    package com.example.specifications.orders;

to live in:
    com/example/specifications/orders/

Therefore:

### ✔ Folder names must match package names

### ✔ Package names must be lowercase

### ✔ Folder names must be lowercase

SpecTable v2.8 follows this strictly.

* * *

4. Domain Model Layout (Centralized)
   ====================================

Domain definitions:

* Attributes
* Entities
* DataTypes

generate **String** and **Typed** classes.

All domain classes go into:
    generated/domain/

### Java package:

    package <project>.domain;

### Contents:

    CustomerString.java
    CustomerTyped.java
    OrderString.java
    OrderTyped.java
    LineItemString.java
    LineItemTyped.java
    ColorString.java
    ColorTyped.java
    ...

### Defines

* Defines do **not** generate Java files
* Defines are fully expanded during compilation
* Defines never appear in the domain folder

* * *

5. Specification Bundle Layout (Colocated)
   ==========================================

For each `.spectable` file:
    specifications/orders/Orders.spectable

Generate:
    generated/specifications/orders/
        Orders.spectablegen
        Orders_glue.java
        Test_Orders.java

This bundle is a **self‑contained unit** containing:

* The canonical expanded SpecTable file
* The glue code
* All unit tests for the specification

### Java package:

    package <project>.specifications.orders;

Tests:
    package <project>.specifications.orders.tests;

* * *

6. The `.spectablegen` File
   ===========================

Generated canonical form:
    Orders.spectablegen

Contains:

* Expanded Defines
* Normalized tables
* Rotated Vertical tables
* Merged Background
* Attached Cleanup
* Expanded Multiples
* Resolved imports

Used by:

* IDEs
* Generators
* Debugging
* AI agents

* * *

7. Java Glue File Layout
   ========================
   
    Orders_glue.java

### Java package:

    package <project>.specifications.orders;

### Rules

* One glue class per specification
* One glue method per Scenario, BusinessRule, Calculation, DataType
* Glue receives **String** objects
* Glue converts to **Typed** objects using `Typed.fromStringObject()`
* Glue is user‑editable
* Glue is not overwritten unless explicitly regenerated

### Glue method naming:

    Scenario_<Name>()
    BusinessRule_<Name>()
    Calculation_<Name>()
    DataType_<Name>()

Each accepts:
    List<AttributeSetString>

* * *

8. Java Unit Test Layout
   ========================

### **All tests for a specification live in the same file**

    Test_Orders.java

### Java package:

    package <project>.specifications.orders.tests;

### Contains test methods for:

* Every Scenario
* Every BusinessRule
* Every Calculation
* Every DataType with Examples

### Test class naming:

    Test_<SpecName>

### Test method naming:

    Scenario_<ScenarioName>()
    ExamplesBusinessRule_<RuleName>()
    ExamplesCalculation_<CalcName>()
    ExamplesDataType_<DataTypeName>()

### Rule:

> **If someone wants tests in a different file, they move the constructs into a different `.spectable` file.**

The Specification file is the unit of test grouping.

* * *

9. Final Java Unit Test Template
   ================================
   
    package <project>.specifications.<path>.tests;
    import org.junit.Test;
    import java.util.List;
    import <project>.specifications.<path>.<SpecName>_glue;
    import <project>.domain.*;
    public class Test_<SpecName> {
   
        // -------------------------
        // Scenario Tests
        // -------------------------
        @Test
        public void Scenario_<ScenarioName>() {
            <SpecName>_glue glue = new <SpecName>_glue();
            List<<AttributeSetName>String> rows = List.of(
                <generatedStringRows>
            );
            glue.Scenario_<ScenarioName>(rows);
        }
       
        // -------------------------
        // BusinessRule Tests
        // -------------------------
        @Test
        public void ExamplesBusinessRule_<RuleName>() {
            <SpecName>_glue glue = new <SpecName>_glue();
            List<<AttributeSetName>String> rows = List.of(
                <generatedStringRows>
            );
            glue.BusinessRule_<RuleName>(rows);
        }
       
        // -------------------------
        // Calculation Tests
        // -------------------------
        @Test
        public void ExamplesCalculation_<CalcName>() {
            <SpecName>_glue glue = new <SpecName>_glue();
            List<<AttributeSetName>String> rows = List.of(
                <generatedStringRows>
            );
            glue.Calculation_<CalcName>(rows);
        }
       
        // -------------------------
        // DataType Tests
        // -------------------------
        @Test
        public void ExamplesDataType_<DataTypeName>() {
            <SpecName>_glue glue = new <SpecName>_glue();
            List<<AttributeSetName>String> rows = List.of(
                <generatedStringRows>
            );
            glue.DataType_<DataTypeName>(rows);
        }
   
    }

* * *

10. Directory Mapping Rules
    ===========================

### Source → Generated

    specifications/<path>/<Name>.spectable
    →
    generated/specifications/<path>/<Name>.spectablegen
    generated/specifications/<path>/<Name>_glue.java
    generated/specifications/<path>/Test_<Name>.java

### Domain definitions → Centralized domain folder

    generated/domain/<Name>String.java
    generated/domain/<Name>Typed.java

* * *

11. Example Full Java Project Layout
    ====================================
    
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
    
        ├── domain/
        │   ├── CustomerString.java
        │   ├── CustomerTyped.java
        │   ├── OrderString.java
        │   ├── OrderTyped.java
        │   ├── LineItemString.java
        │   ├── LineItemTyped.java
        │   ├── ColorString.java
        │   ├── ColorTyped.java
        │   └── ...
        │
        ├── specifications/
            ├── customers/
            │   ├── Customers.spectablegen
            │   ├── Customers_glue.java
            │   └── Test_Customers.java
            │
            ├── orders/
            │   ├── Orders.spectablegen
            │   ├── Orders_glue.java
            │   └── Test_Orders.java
            │
            └── shipping/
                ├── ShippingRules.spectablegen
                ├── ShippingRules_glue.java
                └── Test_ShippingRules.java

* * *

12. Summary (One Sentence)
    ==========================

**Domain code is centralized; specification bundles are colocated; glue + all tests + canonical `.spectablegen` live together; Defines are compile‑time only; Java packages match folder structure; Scenarios, BusinessRules, Calculations, and DataTypes all generate test methods inside the same `Test_<SpecName>.java` file.**

* * *

Ken — if you want, I can now generate:

### 🔹 A full example Java project

### 🔹 Glue generator pseudocode

### 🔹 Test generator pseudocode

### 🔹 Full Java code generator implementation

Just tell me.
