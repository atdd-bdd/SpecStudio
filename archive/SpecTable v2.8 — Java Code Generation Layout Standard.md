Ken — here it is.  
A **Java‑specific** version of the SpecTable v2.8 Code Generation Layout Standard _plus_ a **Java code generator template** for String/Typed classes that follows Java conventions, package rules, naming rules, and the folder structure we finalized.

This is the version you can hand directly to Java implementers.

* * *

📘 **SpecTable v2.8 — Java Code Generation Layout Standard**
============================================================

This standard defines:

* Java package naming
* Java folder layout
* Java class naming
* Java file placement
* Java‑specific rules for domain vs specification bundles

It is fully aligned with:

* Java compiler rules
* JVM classloader rules
* Maven/Gradle conventions
* IDE expectations (IntelliJ, Eclipse, VS Code)

* * *

1. Java Project Structure
   =========================

A Java project using SpecTable v2.8 should follow:
    project/
        specifications/        ← human-authored .spectable files
        generated/             ← machine-generated Java code
            Domain/
            <specification bundles>

* * *

2. Java Package Rules (Mandatory)
   =================================

Java requires:
    package com.example.specifications.orders;

to live in:
    com/example/specifications/orders/

Therefore:

### ✔ Package names **must** match folder structure

### ✔ Folder names **must** be lowercase

### ✔ Package names **must** be lowercase

* * *

3. Java Domain Model Layout (Centralized)
   =========================================

All domain classes go into:
    generated/Domain/

But Java requires lowercase folder names, so the actual Java package is:
    generated/domain/

### Java package:

    package <project>.domain;

### Java folder:

    generated/domain/

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

### Why centralized?

* Domain is global
* Domain is stable
* Domain should not mirror specification folder structure
* IDEs expect a single domain package
* Java reflection works best with a single domain namespace

* * *

4. Java Specification Bundle Layout (Colocated)
   ===============================================

For each `.spectable` file:
    specifications/orders/Orders.spectable

Generate:
    generated/orders/
        Orders.spectablegen
        Orders_glue.java
        Test_Orders.java

### Java package:

    package <project>.specifications.orders;

### Java folder:

    generated/specifications/orders/

(We place generated code under `generated/specifications/` to keep package alignment.)

* * *

5. Java File Placement Summary
   ==============================

### Domain:

    generated/domain/*.java
    package <project>.domain;

### Specification bundles:

    generated/specifications/<path>/<Name>_glue.java
    generated/specifications/<path>/Test_<Name>.java
    generated/specifications/<path>/<Name>.spectablegen

### Packages:

    package <project>.specifications.<path>;
    package <project>.specifications.<path>.tests;

* * *

6. Java Naming Conventions
   ==========================

### ✔ Classes: PascalCase

### ✔ Fields: camelCase

### ✔ Packages: lowercase

### ✔ No underscores in package names

### ✔ Glue class: `<Name>_glue` is allowed because it is a class name, not a package

* * *

7. Java Code Generator Template
   ===============================

### (String class + Typed class)

Below is the **canonical Java template** for SpecTable v2.8.

* * *

**7.1 String Class Template (Java)**
------------------------------------

    package <project>.domain;
    
    import java.util.List;
    
    public class <Name>String {
    
        // Fields (all strings or List<String>)
        <stringFieldDeclarations>
    
        // Constructor
        public <Name>String(<stringCtorParams>) {
            <assignStringFields>
        }
    
        @Override
        public String toString() {
            return "<Name>String{" +
                <stringToStringFields> +
            "}";
        }
    }

### Generator rules:

* Single attribute → `String fieldName;`
* Multiples attribute → `List<String> fieldName;`
* Constructor parameters follow field order
* `toString()` is auto‑generated

* * *

**7.2 Typed Class Template (Java)**
-----------------------------------

    package <project>.domain;
    
    import java.util.List;
    import java.util.stream.Collectors;
    
    public class <Name>Typed {
    
        // Typed fields
        <typedFieldDeclarations>
    
        // Constructor
        public <Name>Typed(<typedCtorParams>) {
            <assignTypedFields>
        }
    
        // Conversion from String object
        public static <Name>Typed fromStringObject(<Name>String s) {
            return new <Name>Typed(
                <typedConversionExpressions>
            );
        }
    
        // Conversion back to String object
        public <Name>String toStringObject() {
            return new <Name>String(
                <reverseConversionExpressions>
            );
        }
    }

### Generator rules:

* Single attribute → typed scalar (int, boolean, LocalDate, etc.)
* Multiples attribute → `List<T>`
* Conversion uses:
  * `Integer.parseInt(s.field)`
  * `Boolean.parseBoolean(s.field)`
  * `list.stream().map(...).collect(Collectors.toList())`

* * *

8. Java Glue Template
   =====================
   
    package <project>.specifications.<path>;
    import <project>.domain.*;
    import java.util.List;
    public class <Name>_glue {
   
        public void <stepMethod>(List<<DataName>String> values) {
            for (<DataName>String s : values) {
                <DataName>Typed typed = <DataName>Typed.fromStringObject(s);
                // user logic here
            }
        }
   
    }

* * *

9. Java Unit Test Template
   ==========================
   
    package <project>.specifications.<path>.tests;
    import org.junit.Test;
    import java.util.List;
    import <project>.specifications.<path>.<Name>_glue;
    import <project>.domain.*;
    public class Test_<Name> {
   
        @Test
        public void test_<ScenarioName>() {
            <Name>_glue glue = new <Name>_glue();
       
            List<<DataName>String> list = List.of(
                new <DataName>String(<params>)
            );
       
            glue.<stepMethod>(list);
        }
   
    }

* * *

10. Summary (Java Version)
    ==========================

**Domain code → `generated/domain` → `package <project>.domain`**  
**Specification bundles → `generated/specifications/<path>` → `package <project>.specifications.<path>`**  
**Glue + Tests + .spectablegen colocated**  
**Defines expanded at compile time**  
**String/Typed classes follow Java naming + package rules**  
**Folder structure always matches package structure**

* * *

If you want, I can now generate:

### 🔹 A complete Java example project

### 🔹 A Maven or Gradle project skeleton

### 🔹 Java code generator pseudocode

### 🔹 Java code generator full implementation

Just tell me.
