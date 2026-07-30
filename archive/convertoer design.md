Ken — excellent. I now understand exactly what you want:

### ✔ The **design must be altered** so that the **generated output looks exactly like your sample unit test file**,

### ✔ **Constructors** instead of builders,

### ✔ **List<TypedClass>** for ListOfObject steps,

### ✔ **List<List<string>>** for simple tables,

### ✔ **Glue file exactly like your example**,

### ✔ **Each Scenario = one test**,

### ✔ **Each test creates a new glue object**,

### ✔ **No shared context**,

### ✔ **And → Given/When/Then** depending on what it is “anding”.

I’ll now give you the **updated design**, rewritten so the generator produces output _identical_ to your sample.

* * *

⭐ **Updated SpecTable Test Generator Design (v2.2)**
====================================================

### _Matches your sample output exactly_

* * *

1. **Object Model (Final)**
   ===========================

### ✔ Each Data block produces a class with:

* Public fields (string typed)
* A constructor with one string parameter per attribute
* A `.ToXInternal()` method for typed conversion
* A `.ToString()` override (optional but useful)

### Example (FandC):

    public class FandC
    {
        public string f;
        public string c;
        public string notes;
    
        public FandC(string f, string c, string notes)
        {
            this.f = f;
            this.c = c;
            this.notes = notes;
        }
    
        public FandCInternal ToFandCInternal()
        {
            return new FandCInternal(
                int.Parse(f),
                int.Parse(c),
                notes
            );
        }
    }

### ✔ These classes **ARE overwritten** whenever the Data block changes.

* * *

2. **Internal Typed Classes (Final)**
   =====================================

Each Data block also produces:
    public class FandCInternal
    {
        public int f;
        public int c;
        public string notes;

        public FandCInternal(int f, int c, string notes)
        {
            this.f = f;
            this.c = c;
            this.notes = notes;
        }
    }

### ✔ Also overwritten when Data block changes.

* * *

3. **Unit Test Generation (Final)**
   ===================================

### ✔ Namespace:

    namespace gherkinexecutor.Feature_<FeatureName>

### ✔ Class:

    public class Feature_<FeatureName>

### ✔ One test per Scenario:

    [TestMethod]
    public void Test_Scenario_<ScenarioName>()

### ✔ Each test creates a new glue object:

    Feature_Examples_glue feature_Examples_glue_object =
        new Feature_Examples_glue();

### ✔ ListOfObject → List<TypedClass>

    List<FandC> objectList1 = new List<FandC>{
        new FandC("32", "0", "Freezing"),
        new FandC("212", "100", "Boiling"),
        new FandC("-40", "-40", "Below zero")
    };

### ✔ Simple tables → List<List<string>>

    List<List<string>> stringListList4 = new List<List<string>>{
        new List<string>{ "Q1234" }
    };

### ✔ Glue call:

    feature_Examples_glue_object.Calculation_Convert_F_to_C(objectList1);

* * *

4. **Glue File (Final)**
   ========================

Your glue file is now the **canonical template**.

### ✔ Glue class name:

    Feature_<FeatureName>_glue

### ✔ Glue methods:

* One per step
* Name = `<StepKeyword>_<StepText>`
* Parameter = `List<TypedClass>` or `List<List<string>>`
* Uses `.ToXInternal()`
* Calls production code
* Uses MSTest `Assert`

### ✔ Glue file is created once and never overwritten.

* * *

5. **Step Keyword Normalization (Final)**
   =========================================

You said:

> “A step with an And should be changed to Given/When/Or Then depending on what it is anding.”

### ✔ Rule:

* If previous step is **Given**, then `And` → `Given`
* If previous step is **When**, then `And` → `When`
* If previous step is **Then**, then `And` → `Then`

### ✔ Example:

    Given list of numbers
    And more numbers
    When filtered
    And sorted
    Then sum is
    And no errors

Becomes:
    Given_list_of_numbers(...)
    Given_more_numbers(...)
    When_filtered(...)
    When_sorted(...)
    Then_sum_is(...)
    Then_no_errors(...)

* * *

6. **Final Output Format (Matches Your Example Exactly)**
   =========================================================

Here is the **canonical output** the generator must produce:
    namespace gherkinexecutor.Feature_Examples{
    [TestClass]
    public class Feature_Examples{

    [TestMethod]
    public void Test_Scenario_Temperature_Conversion(){
         Feature_Examples_glue feature_Examples_glue_object = new Feature_Examples_glue();

         List<FandC> objectList1 = new List<FandC>{
             new FandC("32","0","Freezing"),
             new FandC("212","100","Boiling"),
             new FandC("-40","-40","Below zero")
         };
         feature_Examples_glue_object.Calculation_Convert_F_to_C(objectList1);
    }

…and so on for each scenario.

* * *

7. **Updated Generator Architecture (Final)**
   =============================================
   
    SpecTableTestGenerator/
   
        Parsing/
        Semantics/
        Expansion/
        Generators/
            DataClassGenerator.cs
            InternalClassGenerator.cs
            UnitTestGenerator.cs
            GlueStubGenerator.cs
        IO/

### ✔ DataClassGenerator

Creates `<AttributeSet>.cs` with constructor + fields + ToInternal.

### ✔ InternalClassGenerator

Creates `<AttributeSet>Internal.cs`.

### ✔ UnitTestGenerator

Creates unit test file exactly like your example.

### ✔ GlueStubGenerator

Creates glue file only if missing.

* * *

8. **Ready to Generate the Full Example Output**
   ================================================

Now that the design is locked in, I can generate:

### ✔ The full set of generated classes for your sample feature

### ✔ The full generated unit test file

### ✔ The full generated glue stub file

### ✔ The full internal typed classes

### ✔ The full SpecTableExecutor skeleton

Just say:

**“Generate the full example output.”**











Ken — perfect. Let’s lock this down cleanly and update the design so it matches your new constraints:

* **CustomerString** (and all `<AttributeSet>String` classes)
  * Must have **one property per attribute**, not Attribute/Value pairs
  * Must use **lowercase first letter** for property names
  * Must be **overwritten every run**
* **CustomerTyped** (and all `<AttributeSet>Typed` classes)
  * Must have **typed properties**
  * Must be **overwritten if the AttributeSet or Entity changes**
* **Each Scenario = one unit test**
* **No shared context**
* **Each test creates a new glue object**
* **Glue file persists and is never overwritten**

Below is the **updated, complete design document**, rewritten to reflect these rules.

* * *

📘 **SpecTable Test Generator — Updated Full Design (v2.1)**
============================================================

### _String classes have attribute‑named fields; Typed classes regenerate; each test has its own glue instance; no shared context._

* * *

⭐ 1. Overview
=============

This version of the SpecTable Test Generator (STG) produces:

### ✔ Generated Every Run

* Unit test files
* `<AttributeSet>String` classes
* `<AttributeSet>Typed` classes
* Converters
* Test data arrays

### ✔ Generated Once (Never Overwritten)

* Glue file
* Default glue methods

### ✔ Execution Model

* Each Scenario = one unit test
* Each test creates a **new glue object**
* No shared context
* Glue methods receive `<AttributeSet>String` objects
* Glue methods convert to `<AttributeSet>Typed`
* Glue methods call production code

* * *

⭐ 2. AttributeSetString (Updated)
=================================

### ✔ One property per attribute

### ✔ Lowercase first letter

### ✔ All properties are strings

### ✔ Overwritten every run

Example for:
    Attributes Customer
    | Attribute | Type    |
    | Name      | String  |
    | Age       | Integer |

Generated:
    public class CustomerString
    {
        public string name;
        public string age;
    }

* * *

⭐ 3. AttributeSetTyped (Updated)
================================

### ✔ One property per attribute

### ✔ Typed according to AttributeSet

### ✔ Overwritten when AttributeSet changes

Generated:
    public class CustomerTyped
    {
        public string Name;
        public int Age;
    }

* * *

⭐ 4. Converter (Updated)
========================

### ✔ Converts CustomerString → CustomerTyped

### ✔ Handles missing attributes

### ✔ Handles arbitrary order

### ✔ Applies defaults

### ✔ Overwritten when AttributeSet changes

Generated:
    public static class CustomerConverter
    {
        public static CustomerTyped Convert(CustomerString s)
        {
            var obj = new CustomerTyped();

            // Defaults
            obj.Age = 0;

            if (s.name != null)
                obj.Name = s.name;

            if (s.age != null)
                obj.Age = int.Parse(s.age);

            return obj;
        }
    }

* * *

⭐ 5. Unit Test Generation (Updated)
===================================

### ✔ One test per Scenario

### ✔ No shared context

### ✔ Each test creates a new glue object

### ✔ Test passes `<AttributeSet>String` object(s)

Generated:
    [TestMethod]
    public void CustomerInfo()
    {
        var glue = new Customer_Glue();

        var customer = new CustomerString {
            name = "Ken",
            age = "42"
        };

        glue.Given_customer(customer);
        glue.Then_valid_customer(new CustomerString());
    }

* * *

⭐ 6. Glue File (Updated)
========================

### ✔ Created once

### ✔ Never overwritten

### ✔ One method per step

### ✔ Receives `<AttributeSet>String`

### ✔ Converts to `<AttributeSet>Typed`

### ✔ Calls production code

Generated once:
    public class Customer_Glue
    {
        public void Given_customer(CustomerString s)
        {
            var typed = CustomerConverter.Convert(s);
            // Developer adds production code call here
        }

        public void Then_valid_customer(CustomerString s)
        {
            var typed = CustomerConverter.Convert(s);
            // Developer adds assertions here
        }
    }

* * *

⭐ 7. Table Conversion Rules (Updated)
=====================================

### ✔ Normal table

    | Name | Age |
    | Ken  | 42  |

Becomes:
    new CustomerString {
        name = "Ken",
        age = "42"
    };

### ✔ Vertical table

    | Name | Ken |
    | Age  | 42  |

Becomes the same:
    new CustomerString {
        name = "Ken",
        age = "42"
    };

### ✔ Missing attributes allowed

    | Name | Ken |

Becomes:
    new CustomerString {
        name = "Ken",
        age = null
    };

Converter fills defaults.

* * *

⭐ 8. No Shared Context
======================

### ✔ Each test creates a new glue object

### ✔ Glue object has no state

### ✔ All state is local to glue method

This is the simplest and safest model.

* * *

⭐ 9. Background + Cleanup Injection
===================================

### ✔ Background steps run first

### ✔ Cleanup steps run last

### ✔ Each step is a glue call

Example:
    Background:
    Given system initialized

    Scenario:
    Given customer
    Then valid customer

    Cleanup:
    Then no errors occurred

Unit test:
    var glue = new Customer_Glue();

    glue.Given_system_initialized(new SystemString());
    glue.Given_customer(customer);
    glue.Then_valid_customer(new CustomerString());
    glue.Then_no_errors_occurred(new ErrorString());

* * *

⭐ 10. Define Block Resolution
=============================

    =Address1

Becomes:
    new AddressString {
        street = "123 Main",
        city = "Cary",
        state = "NC"
    };

* * *

⭐ 11. BusinessRule and Calculation Tests
========================================

Each Example row becomes a test.
    Examples: ShippingCheck
    | Address | Valid |
    | =Addr1  | Y     |

Unit test:
    var glue = new Shipping_Glue();

    var data = new ShippingCheckString {
        address = "=Addr1",
        valid = "Y"
    };

    glue.BusinessRule_ValidateAddress(data);

* * *

⭐ 12. DataType Tests
====================

Enumerations:
    DataType Color
    Examples: EnumerationValues
    | Value | Notes |
    | Red   |       |
    | Blue  |       |

Generate:

* One test per value
* Glue method: `Validate_Color(ColorString s)`

* * *

⭐ 13. File Overwrite Rules (Updated)
====================================

### Overwritten every run:

* Unit tests
* `<AttributeSet>String` classes
* `<AttributeSet>Typed` classes
* Converters

### Never overwritten:

* Glue files
* Developer code inside glue

* * *

⭐ 14. Summary of Updated Model
==============================

### ✔ `<AttributeSet>String` has one property per attribute

### ✔ Lowercase first letter

### ✔ `<AttributeSet>Typed` has typed properties

### ✔ Both regenerated when AttributeSet changes

### ✔ Each Scenario = one unit test

### ✔ Each test creates a new glue object

### ✔ No shared context

### ✔ Glue receives String objects

### ✔ Glue converts to Typed objects

### ✔ Glue calls production code

### ✔ Missing attributes allowed

### ✔ Arbitrary order allowed

### ✔ Vertical tables normalized

### ✔ Background + Cleanup injected

* * *

Ken — this is the **updated, complete design** exactly matching your new requirements.

If you want next:

* A **sample full project folder**
* A **reference implementation in C# or C++**
* A **SpecTableExecutor runtime library**
* A **code generator template set**

Just say the word.
