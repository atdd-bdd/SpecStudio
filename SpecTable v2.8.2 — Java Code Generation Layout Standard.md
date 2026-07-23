Ken — perfect, I’ve got the exact refinement.  
You want:

### ✔ The **unit test method names** to be:

    BusinessRule_<RuleName>()
    Calculation_<CalcName>()
    DataType_<DataTypeName>()

### ✔ The **glue method names** to be:

    ExamplesBusinessRule_<RuleName>(List<...>)
    ExamplesCalculation_<CalcName>(List<...>)
    ExamplesDataType_<DataTypeName>(List<...>)

### ✔ And the unit tests call the glue methods.

This is a clean, symmetric naming scheme:

* **Unit test name** = the _thing being tested_
* **Glue method name** = the _Examples table_ that drives the test

Let’s regenerate the Java‑specific v2.8 standard with this correction.

* * *

📘 **SpecTable v2.8.2 — Java Code Generation Layout Standard (Final)**
====================================================================

### _With corrected naming rules for BusinessRules, Calculations, and DataTypes_

* * *

1. Purpose
   ==========

This standard defines:

* How `.spectable` files map to Java code
* How Java packages and folders must align
* How domain vs specification code is separated
* How glue and unit tests are generated
* How Scenarios, BusinessRules, Calculations, and DataTypes produce test methods
* The corrected naming rules you just specified

* * *

2. High‑Level Project Structure
   ===============================
   
    project/
   
        specifications/     ← human-authored .spectable files
        generated/          ← machine-generated Java code

* * *

3. Java Package Rules (Mandatory)
   =================================

Java requires folder ↔ package alignment:
    package com.example.specifications.orders;

must live in:
    com/example/specifications/orders/

SpecTable v2.8 follows this strictly.

* * *

4. Domain Model Layout (Centralized)
   ====================================

All domain classes go into:
    generated/domain/

### Java package:

    package <project>.domain;

### Contents:

    <Name>String.java
    <Name>Typed.java

Defines do **not** generate Java files.

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

### Packages:

    package <project>.specifications.orders;
    package <project>.specifications.orders.tests;

* * *

6. Glue Method Naming Rules (Corrected)
   =======================================

Glue methods must be named:
    ExamplesBusinessRule_<RuleName>(List<AttributeSetString>)
    ExamplesCalculation_<CalcName>(List<AttributeSetString>)
    ExamplesDataType_<DataTypeName>(List<AttributeSetString>)
    ExamplesScenario_<ScenarioName>(List<AttributeSetString>)   ← optional, if you want symmetry

These names correspond directly to the **Examples** tables.

* * *

7. Unit Test Naming Rules (Corrected)
   =====================================

Inside `Test_<SpecName>.java`, generate:

### ✔ For each Scenario:

    Scenario_<ScenarioName>()

### ✔ For each BusinessRule:

    BusinessRule_<RuleName>()

### ✔ For each Calculation:

    Calculation_<CalcName>()

### ✔ For each DataType:

    DataType_<DataTypeName>()

### ✔ And each test calls the corresponding glue method:

    ExamplesBusinessRule_<RuleName>(rows)
    ExamplesCalculation_<CalcName>(rows)
    ExamplesDataType_<DataTypeName>(rows)

* * *

8. Final Java Unit Test Template (Updated)
   ==========================================
   
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
            glue.ExamplesScenario_<ScenarioName>(rows);
        }
       
        // -------------------------
        // BusinessRule Tests
        // -------------------------
        @Test
        public void BusinessRule_<RuleName>() {
            <SpecName>_glue glue = new <SpecName>_glue();
            List<<AttributeSetName>String> rows = List.of(
                <generatedStringRows>
            );
            glue.ExamplesBusinessRule_<RuleName>(rows);
        }
       
        // -------------------------
        // Calculation Tests
        // -------------------------
        @Test
        public void Calculation_<CalcName>() {
            <SpecName>_glue glue = new <SpecName>_glue();
            List<<AttributeSetName>String> rows = List.of(
                <generatedStringRows>
            );
            glue.ExamplesCalculation_<CalcName>(rows);
        }
       
        // -------------------------
        // DataType Tests
        // -------------------------
        @Test
        public void DataType_<DataTypeName>() {
            <SpecName>_glue glue = new <SpecName>_glue();
            List<<AttributeSetName>String> rows = List.of(
                <generatedStringRows>
            );
            glue.ExamplesDataType_<DataTypeName>(rows);
        }
   
    }

* * *

9. Glue Template (Updated)
   ==========================
   
    package <project>.specifications.<path>;
    import java.util.List;
    import <project>.domain.*;
    public class <SpecName>_glue {
   
        public void ExamplesScenario_<ScenarioName>(List<<AttributeSetName>String> rows) {
            for (<AttributeSetName>String s : rows) {
                <AttributeSetName>Typed t = <AttributeSetName>Typed.fromStringObject(s);
                // developer logic
            }
        }
       
        public void ExamplesBusinessRule_<RuleName>(List<<AttributeSetName>String> rows) {
            for (<AttributeSetName>String s : rows) {
                <AttributeSetName>Typed t = <AttributeSetName>Typed.fromStringObject(s);
                // developer logic
            }
        }
       
        public void ExamplesCalculation_<CalcName>(List<<AttributeSetName>String> rows) {
            for (<AttributeSetName>String s : rows) {
                <AttributeSetName>Typed t = <AttributeSetName>Typed.fromStringObject(s);
                // developer logic
            }
        }
       
        public void ExamplesDataType_<DataTypeName>(List<<AttributeSetName>String> rows) {
            for (<AttributeSetName>String s : rows) {
                <AttributeSetName>Typed t = <AttributeSetName>Typed.fromStringObject(s);
                // developer logic
            }
        }
   
    }

* * *

10. Summary (One Sentence)
    ==========================

**Unit tests are named BusinessRule_<Name>, Calculation_<Name>, and DataType_<Name>; glue methods are named ExamplesBusinessRule_<Name>, ExamplesCalculation_<Name>, and ExamplesDataType_<Name>; each test calls its corresponding glue method with the Examples table.**

* * *

If you want, I can now regenerate:

### 🔹 The full Java code generator pseudocode

### 🔹 A complete example Java project

### 🔹 The C# version with the same naming rules

Just tell me.
