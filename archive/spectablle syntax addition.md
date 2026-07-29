

----------------------------------------------

### 1.1 Attributes / Data blocks

Data tables now have:
    Data <Name>
    | Name   | Default  | DataType  | Multiples | Notes  |

* **Name:** attribute name.
* **Default:** default value (string).
* **DataType:** logical type (Integer, String, Boolean, ID, etc.).
* **Multiples:** controls cardinality:
  * `""` → exactly one value.
  * `Y`, `Yes`, `Any` → any number of values.
  * `N` (integer) → up to N values.
* **Notes:** documentation only.

### 1.2 Entities

Entities mirror the same idea:
    Entity <Name>
    | Attribute | Type       | Multiples | Notes |

Same Multiples semantics.

### 1.3 Define blocks

A `Define` (or `=` block) that is used in a step table:

* **Multiples empty:** must have exactly **1 data row**.
* **Multiples = N:** may have **1..N rows**.
* **Multiples = Any:** may have **any number of rows**.
* For **primitive DataTypes** (e.g., Integer) with Multiples:
  * Table may be **vertical** (one column, many rows) or **horizontal** (one row, many columns).

### 1.4 Step tables

When a step references a Data/Entity with Multiples:

* **Multiples empty:** step table must represent a **single instance**.
* **Multiples number/Any:** step table may represent **multiple instances**.
* Generator decides whether to emit:
  * A single `<Name>String` object, or
  * A `List<<Name>String>`.

### 1.5 Generated classes

For each Data block `<Name>`:

* If **Multiples empty**:
  * Attribute becomes a **single field**.
* If **Multiples number/Any**:
  * Attribute becomes a **`List<string>` field**.

Example:
    Data LabelValue
    | Name   | Default  | DataType  | Multiples | Notes  |
    | ID     |          | ID        |           |        |
    | Value  | 0        | Integer   | Any       |        |

Generates:
    public class LabelValueString
    {
        public string id;
        public List<string> value;
    }

    public class LabelValueTyped
    {
        public ID id;
        public List<int> value;
    }


    }

* * *

2. Updated end‑to‑end example including Multiples

-------------------------------------------------

Let’s tweak your feature slightly to show Multiples explicitly.

### 2.1 Feature (conceptual)

    Data LabelValue
    | Name   | Default  | DataType  | Multiples | Notes  |
    | ID     |          | ID        |           |        |
    | Value  | 0        | Integer   | Any       |        |

The rest of the feature stays as you had it.

### 2.2 Generated String/Typed classes

    namespace gherkinexecutor.Feature_Examples
    {
        public class LabelValueString
        {
            public string id;
            public List<string> value;
    
            public LabelValueString(string id, List<string> value)
            {
                this.id = id;
                this.value = value;
            }
    
            public LabelValueTyped ToLabelValueTyped()
            {
                return new LabelValueTyped(
                    new ID(id),
                    value.ConvertAll(v => int.Parse(v))
                );
            }
    
            public override string ToString()
            {
                return $"ID={id}, Values=[{string.Join(",", value)}]";
            }
        }
    
        public class LabelValueTyped
        {
            public ID id;
            public List<int> value;
    
            public LabelValueTyped(ID id, List<int> value)
            {
                this.id = id;
                this.value = value;
            }
        }
    }

For your original `LabelValue` (single Value per row), we keep:
    public class LabelValueString
    {
        public string id;
        public string value;

        public LabelValueString(string id, string value)
        {
            this.id = id;
            this.value = value;
        }

 
        }
    }

So Multiples is per attribute, not per whole Data block—you can mix single and multiple attributes.

### 2.3 Unit test generation (unchanged style, aware of Multiples)

For your existing feature, Multiples is effectively empty for all attributes, so the unit test file remains:
    namespace gherkinexecutor.Feature_Examples{
    using Microsoft.VisualStudio.TestTools.UnitTesting;
    using System.Collections.Generic;

    [TestClass]
    public class Feature_Examples{

    [TestMethod]
    public void Test_Scenario_Temperature_Conversion(){
         Feature_Examples_glue feature_Examples_glue_object = new Feature_Examples_glue();

         List<FandCString> objectList1 = new List<FandCString>{
             new FandCString("32","0","Freezing"),
             new FandCString("212","100","Boiling"),
             new FandCString("-40","-40","Below zero")
         };
         feature_Examples_glue_object.Calculation_Convert_F_to_C(objectList1);
    }

    // ... other scenarios unchanged ...
    }
    }

If we had a step where `Value` is Multiples, the generator would emit something like:
    List<LabelValueString> objectList = new List<LabelValueString>{
        new LabelValueString("Q1234", new List<string>{ "1", "2", "3" })
    };

Glue stays the same pattern:
    public void Given_list_of_numbers(List<LabelValueString> values)
    {
        foreach (LabelValueString value in values)
        {
            LabelValueTyped typed = value.ToLabelValueTyped();
            solution.Add(typed);
        }
    }

* * *

3. Updated generator templates

------------------------------

### 3.1 Data class generator (String)

For each Data row:
    // Pseudocode template
    class <Name>String {
        // For each attribute:
        // if Multiples empty:
        public string <attrLower>;
        // else:
        public List<string> <attrLower>;

        // Constructor:
        public <Name>String(<ctorParams>) { ... }

        // ToTyped:
        public <Name>Typed To<Name>Typed() { ... }
    }

### 3.2 Typed class generator

    class <Name>Typed {
        // For each attribute:
        // if Multiples empty:
        public <TypedType> <AttrName>;
        // else:
        public List<<TypedType>> <AttrName>;
    }

* * *

4. Updated parser + semantic analyzer rules

-------------------------------------------

### Parser

* Recognize optional `Multiples` column in Data/Entity tables.
* Normalize values:
  * `Y`, `Yes`, `Any` → `Any`.
  * Numeric strings → integer.
  * Empty → `Single`.

### Semantic analyzer

* For each Data/Entity:
  * Attach `multiplesKind` to each attribute:
    * `Single`, `Any`, `Limited(N)`.
* For each Define:
  * Check row count against Multiples.
* For each step table:
  * Check row count against Multiples.
* For primitive DataTypes with Multiples:
  * Accept vertical or horizontal single‑column/row tables.

* * *

5. Updated class generation rules

---------------------------------

* **Single attribute (Multiples empty):**
  * `string` field in String class.
  * Typed field in Typed class.
* **Multiple attribute (Multiples Any or N):**
  * `List<string>` field in String class.
  * `List<TypedType>` field in Typed class.
* `.To<Name>Typed()`:
  * For single: parse one value.
  * For multiple: parse all values in the list.

* * *

6. Updated unit test generation rules

-------------------------------------

* For each Scenario:
  * One `[TestMethod]`.
  * New glue object per test.
* For each ListOfObject step:
  * If attribute Multiples empty:
    * Each row → one `<Name>String` with scalar fields.
  * If attribute Multiples non‑empty:
    * Row may represent:
      * A list in one column (vertical).
      * A list in one row (horizontal).
    * Generator builds `List<string>` for that attribute.
* For simple tables (no Data/Entity):
  * Still `List<List<string>>`.
* `And` steps:
  * Normalized to Given/When/Then based on previous keyword.

* * *

Got you—let’s finish the picture and keep it tight.

* * *

### Updated generator templates (concise)

**DataString class template:**
    public class <Name>String
    {
        // For each attribute:
        // Multiples empty:
        public string <attrLower>;
        // Multiples Any or N:
        public List<string> <attrLower>;

        public <Name>String(<ctorParams>)
        {
            // assign fields
        }

        public <Name>Typed To<Name>Typed()
        {
            return new <Name>Typed(
                // per-attribute conversion:
                // Single → parse one
                // Multiple → parse list
            );
        }
    }

**DataTyped class template:**
    public class <Name>Typed
    {
        // For each attribute:
        // Multiples empty:
        public <TypedType> <AttrName>;
        // Multiples Any or N:
        public List<<TypedType>> <AttrName>;

        public <Name>Typed(<typedCtorParams>)
        {
            // assign fields
        }
    }

* * *

### Updated parser rules (short)

* **Detect Multiples column** in Data/Entity tables.
* Normalize:
  * `""` → `Single`
  * `Y`, `Yes`, `Any` → `Any`
  * `N` (digits) → `Limited(N)`
* Attach `multiplesKind` to each attribute.
* For each Define:
  * Enforce row count vs `multiplesKind`.
* For each step table:
  * Enforce row count vs `multiplesKind`.
* For primitive DataTypes with Multiples:
  * Accept vertical or horizontal single‑dimension tables.

* * *

### Updated semantic analyzer rules

* **Validate:**
  * Define blocks: rows vs Multiples.
  * Step tables: rows vs Multiples.
  * DataType multiples: shape (1D vertical/horizontal).
* **Infer:**
  * Whether generator should emit:
    * Single `<Name>String`
    * `List<<Name>String>`

* * *

### Updated class generation rules

* For each attribute:
  * If `Single` → `string` in String, typed scalar in Typed.
  * If `Any`/`Limited(N)` → `List<string>` in String, `List<T>` in Typed.
* `.To<Name>Typed()`:
  * Single: `int.Parse`, `bool.Parse`, etc.
  * Multiple: `list.ConvertAll(...)`.

* * *

### Updated unit test generation rules

* **Per Scenario:** one `[TestMethod]`, new glue object.
* **ListOfObject steps:**
  * If all attributes `Single`:
    * Each row → one `<Name>String` with scalar fields.
  * If some attributes `Multiple`:
    * Row may encode lists (vertical/horizontal); generator builds `List<string>` for those attributes.
* **Simple tables:** still `List<List<string>>`.
* **And steps:** normalized to Given/When/Then based on previous keyword.

Got you—let’s finish the picture and keep it tight.

* * *

### Updated generator templates (concise)

**DataString class template:**
    public class <Name>String
    {
        // For each attribute:
        // Multiples empty:
        public string <attrLower>;
        // Multiples Any or N:
        public List<string> <attrLower>;

        public <Name>String(<ctorParams>)
        {
            // assign fields
        }

        public <Name>Typed To<Name>Typed()
        {
            return new <Name>Typed(
                // per-attribute conversion:
                // Single → parse one
                // Multiple → parse list
            );
        }
    }

**DataTyped class template:**
    public class <Name>Typed
    {
        // For each attribute:
        // Multiples empty:
        public <TypedType> <AttrName>;
        // Multiples Any or N:
        public List<<TypedType>> <AttrName>;

        public <Name>Typed(<typedCtorParams>)
        {
            // assign fields
        }
    }

* * *

### Updated parser rules (short)

* **Detect Multiples column** in Data/Entity tables.
* Normalize:
  * `""` → `Single`
  * `Y`, `Yes`, `Any` → `Any`
  * `N` (digits) → `Limited(N)`
* Attach `multiplesKind` to each attribute.
* For each Define:
  * Enforce row count vs `multiplesKind`.
* For each step table:
  * Enforce row count vs `multiplesKind`.
* For primitive DataTypes with Multiples:
  * Accept vertical or horizontal single‑dimension tables.

* * *

### Updated semantic analyzer rules

* **Validate:**
  * Define blocks: rows vs Multiples.
  * Step tables: rows vs Multiples.
  * DataType multiples: shape (1D vertical/horizontal).
* **Infer:**
  * Whether generator should emit:
    * Single `<Name>String`
    * `List<<Name>String>`

* * *

### Updated class generation rules

* For each attribute:
  * If `Single` → `string` in String, typed scalar in Typed.
  * If `Any`/`Limited(N)` → `List<string>` in String, `List<T>` in Typed.
* `.To<Name>Typed()`:
  * Single: `int.Parse`, `bool.Parse`, etc.
  * Multiple: `list.ConvertAll(...)`.

* * *

### Updated unit test generation rules

* **Per Scenario:** one `[TestMethod]`, new glue object.
* **ListOfObject steps:**
  * If all attributes `Single`:
    * Each row → one `<Name>String` with scalar fields.
  * If some attributes `Multiple`:
    * Row may encode lists (vertical/horizontal); generator builds `List<string>` for those attributes.
* **Simple tables:** still `List<List<string>>`.
* **And steps:** normalized to Given/When/Then based on previous keyword.

* * *

### ✔ **String classes no longer know about Typed classes**

### ✔ **Typed classes now own both conversions:**

* `FromStringObject(<Name>String s)`
* `ToStringObject()`

### ✔ **Unit tests never reference Typed classes**

### ✔ **Glue code can choose when to convert**

### ✔ **String classes become pure DTOs**

### ✔ **Typed classes become the canonical domain objects**

This is a _very_ good design evolution.

Below is the **updated specification**, followed by the **updated end‑to‑end example**, and then the **updated generator templates, parser rules, semantic rules, class generation rules, and unit test rules**.

* * *

⭐ 1. UPDATED SPECIFICATION

### (String → Typed conversion moved entirely into Typed classes)

1.1 String classes

Generated from Data blocks.

### ✔ Pure data holders

### ✔ Only string fields

### ✔ Only a constructor

### ✔ No `.ToTyped()` method

### ✔ No knowledge of Typed classes

Example: public class LabelValueString { public string id; public string value; public LabelValueString(string id, string value) { this.id = id; this.value = value; } public override string ToString() { return $"ID={id}, Value={value}"; } }

* * *

1.2 Typed classes

Generated from Data blocks.

### ✔ Contain typed fields

### ✔ Contain domain‑level types (ID, Money, etc.)

### ✔ Contain **two conversion methods**:

### 1. `static <Name>Typed FromStringObject(<Name>String s)`

Converts a String object → Typed object.

### 2. `<Name>String ToStringObject()`

Converts a Typed object → String object.

Example: public class LabelValueTyped { public ID id; public int value; public LabelValueTyped(ID id, int value) { this.id = id; this.value = value; } public static LabelValueTyped FromStringObject(LabelValueString s) { return new LabelValueTyped( new ID(s.id), int.Parse(s.value) ); } public LabelValueString ToStringObject() { return new LabelValueString( id.ToString(), value.ToString() ); } }

* * *

1.3 Multiples

Same rules as before, but now:

* String class uses `List<string>`
* Typed class uses `List<T>`
* Conversion happens inside Typed class

* * *

1.4 Glue code

Glue code now calls: var typed = LabelValueTyped.FromStringObject(value);

instead of: var typed = value.ToLabelValueTyped();

* * *

1.5 Unit tests

Unit tests **never reference Typed classes**.They only construct `<Name>String` objects.

* * *

⭐ 2. UPDATED END‑TO‑END EXAMPLE

### (Using your original feature file, but with new conversion rules)

* * *

2.1 Generated String classes

_(Overwritten every run)_`LabelValueString.cs`
    namespace gherkinexecutor.Feature_Examples
    {
        public class LabelValueString
        {
            public string id;
            public string value;

            public LabelValueString(string id, string value)
            {
                this.id = id;
                this.value = value;
            }

            public override string ToString()
            {
                return $"ID={id}, Value={value}";
            }
        }
    }

* * *

2.2 Generated Typed classes

_(Overwritten every run)_`LabelValueTyped.cs`
    namespace gherkinexecutor.Feature_Examples
    {
        public class LabelValueTyped
        {
            public ID id;
            public int value;

            public LabelValueTyped(ID id, int value)
            {
                this.id = id;
                this.value = value;
            }

            public static LabelValueTyped FromStringObject(LabelValueString s)
            {
                return new LabelValueTyped(
                    new ID(s.id),
                    int.Parse(s.value)
                );
            }

            public LabelValueString ToStringObject()
            {
                return new LabelValueString(
                    id.ToString(),
                    value.ToString()
                );
            }
        }
    }

* * *

2.3 Updated Glue File

_(Never overwritten)_ public void Given_list_of_numbers(List<LabelValueString> values) { Console.WriteLine("--- " + "Given_list_of_numbers"); foreach (LabelValueString value in values) { Console.WriteLine(value); LabelValueTyped typed = LabelValueTyped.FromStringObject(value); solution.Add(typed); } }

* * *

2.4 Unit Test File

_(Overwritten every run — unchanged except for class names)_ List<LabelValueString> objectList3 = new List<LabelValueString>{ new LabelValueString("Q1234","1"), new LabelValueString("Q9999","2"), new LabelValueString("Q1234","3") };

* * *

⭐ 3. UPDATED GENERATOR TEMPLATES

3.1 String class template
    public class <Name>String
    {
        // For each attribute:
        public <stringOrList> <attrLower>;

        public <Name>String(<ctorParams>)
        {
            // assign
        }

        public override string ToString()
        {
            // auto‑generated
        }
    }

* * *

3.2 Typed class template
    public class <Name>Typed
    {
        // For each attribute:
        public <typedOrList> <AttrName>;

        public <Name>Typed(<typedCtorParams>)
        {
            // assign
        }

        public static <Name>Typed FromStringObject(<Name>String s)
        {
            return new <Name>Typed(
                // per‑attribute conversion
            );
        }

        public <Name>String ToStringObject()
        {
            return new <Name>String(
                // per‑attribute reverse conversion
            );
        }
    }

* * *

⭐ 4. UPDATED PARSER + SEMANTIC RULES

### Parser:

* Detect Multiples column.
* Normalize Multiples values.
* Build attribute metadata.

### Semantic analyzer:

* Validate Define block row count vs Multiples.
* Validate step table row count vs Multiples.
* Determine whether attribute is:
  * Single
  * List (Any)
  * List (Limited N)
* Determine whether DataType multiples allow vertical/horizontal.

* * *

⭐ 5. UPDATED CLASS GENERATION RULES

### String class:

* Single → `string`
* Multiple → `List<string>`

### Typed class:

* Single → typed scalar
* Multiple → `List<T>`

### Conversion:

* Typed.FromStringObject handles all parsing.
* Typed.ToStringObject handles reverse conversion.

* * *

⭐ 6. UPDATED UNIT TEST GENERATION RULES

### Unit tests:

* Only reference `<Name>String` classes.
* Never reference Typed classes.
* Never call `.ToTyped()`.
* Always pass String objects to glue.

### Glue:

* Converts using `Typed.FromStringObject`.

* * *

* * *

1. Updated specification (including Multiples)





Ken — yes, **“Multiples” is the right name**, and the rules you’re describing fit beautifully into the SpecTable model.  
Let me walk you through a clean, rigorous design that incorporates **Multiples** into:

* **Attributes**
* **Entities**
* **Data blocks**
* **Define blocks**
* **Step table validation**
* **Generated String/Typed classes**
* **Unit test generation**

This will give you a **complete, end‑to‑end, unambiguous specification** for how Multiples works.

* * *

⭐ 1. Why “Multiples” is the right name
======================================

Yes — “Multiples” is the correct and intuitive name because:

* It describes _how many instances_ of an attribute/entity/data item may appear.
* It generalizes cleanly across:
  * Entities
  * Attributes
  * DataTypes
  * Define blocks
  * Step tables
* It is readable by business users and developers.
* It avoids overloaded terms like “List”, “Array”, “Collection”, “Many”, etc.

**Conclusion:**  
✔ “Multiples” is the right name.  
✔ It should be a column in both **Attributes** and **Entity** definitions.

* * *

⭐ 2. Multiples Column — Allowed Values
======================================

The **Multiples** column may contain:

| Value             | Meaning                          |
| ----------------- | -------------------------------- |
| `""` (empty)      | Exactly 1 instance (default)     |
| `N` (integer)     | Up to N instances                |
| `Y`, `Yes`, `Any` | Unlimited instances              |
| `0`               | Not allowed (rare, but possible) |

### Normalization:

* `"Y"` → `"Any"`
* `"Yes"` → `"Any"`
* `"Any"` → `"Any"`
* `"1"` → treated as single instance
* `"0"` → allowed but means “must not appear” (edge case)

* * *

⭐ 3. How Multiples Affects Define Blocks
========================================

A **Define block** is a reusable table.  
Multiples determines **how many rows** the Define block may contain.

### ✔ If Multiples = empty (default)

Define block must have **exactly 1 data row**.

### ✔ If Multiples = number (e.g., `3`)

Define block may have **1 to 3 rows**.

### ✔ If Multiples = `Any`

Define block may have **any number of rows**.

### ✔ If Multiples applies to a DataType (e.g., Integer)

Define block may be:

* A **single column** with multiple rows
* A **single row** with multiple columns

Example:
    =AllowedAges
    | 18 |
    | 21 |
    | 65 |

or
    =AllowedAges
    | 18 | 21 | 65 |

Both are valid.

* * *

⭐ 4. How Multiples Affects Step Tables
======================================

When a step references an AttributeSet or Entity with Multiples:

### ✔ If Multiples = empty

Step table must contain **exactly 1 row**.

### ✔ If Multiples = number

Step table must contain **≤ that number of rows**.

### ✔ If Multiples = Any

Step table may contain **any number of rows**.

### ✔ If Multiples applies to a DataType

Step table may be:

* A vertical list
* A horizontal list
* A grid (if DataType is primitive)

* * *

⭐ 5. How Multiples Affects Generated Classes
============================================

### ✔ If Multiples = empty

Generate a **single object**:
    public LabelValueString labelValue;

### ✔ If Multiples = number or Any

Generate a **List<T>**:
    public List<LabelValueString> labelValue;

### ✔ Typed version:

    public List<LabelValueTyped> labelValue;

### ✔ Conversion method:

    public List<LabelValueTyped> ToLabelValueTypedList()
    {
        return labelValue.Select(v => v.ToLabelValueTyped()).ToList();
    }

* * *

⭐ 6. How Multiples Affects Unit Test Generation
===============================================

### ✔ If Multiples = empty

Generate:
    new LabelValueString("Q1234", "1")

### ✔ If Multiples = number or Any

Generate:
    List<LabelValueString> list = new List<LabelValueString>{
        new LabelValueString("Q1234","1"),
        new LabelValueString("Q9999","2"),
        new LabelValueString("Q1234","3")
    };

### ✔ Glue method receives:

    public void Given_list_of_numbers(List<LabelValueString> values)

* * *

⭐ 7. How Multiples Affects Glue Code
====================================

Glue code does **not** change — it already expects:

* `List<TString>` for multiple rows
* `TString` for single rows
* `.To<Typed>()` conversion

Multiples simply determines which one the generator emits.

* * *

⭐ 8. How Multiples Affects Parsing & Semantics
==============================================

### Parser must:

* Read Multiples column
* Normalize values
* Store Multiples in AttributeSet/Entity metadata

### Semantic analyzer must:

* Validate Define block row count
* Validate step table row count
* Validate DataType multiple-value tables
* Normalize horizontal/vertical tables for DataTypes

* * *

⭐ 9. Example: AttributeSet with Multiples
=========================================

    Data LabelValue
    | Name   | Default  | DataType  | Multiples | Notes  |
    | ID     |          | ID        |           |        |
    | Value  | 0        | Integer   | Any       |        |

Meaning:

* ID → single value
* Value → list of integers

### Generated String class:

    public class LabelValueString
    {
        public string id;
        public List<string> value;
    }

### Generated Typed class:

    public class LabelValueTyped
    {
        public ID id;
        public List<int> value;
    }

* * *

⭐ 10. Example: Entity with Multiples
====================================

    Entity Order
    | Attribute | Type       | Multiples |
    | Items     | OrderItem  | Any       |
    | Total     | Money      |           |

Generated:
    public class OrderString
    {
        public List<OrderItemString> items;
        public string total;
    }

* * *

⭐ 11. Example: Define Block with Multiples
==========================================

    Data AllowedAges
    | Name | Default | DataType | Multiples |
    | Age  | 0       | Integer  | Any       |
    
    =AllowedAges
    | 18 |
    | 21 |
    | 65 |

Valid.

* * *

⭐ 12. Summary — Final Rules for Multiples
=========================================

### ✔ Multiples column allowed in Attributes and Entities

### ✔ Values: empty, number, Y/Yes/Any

### ✔ Controls how many rows a Define block may have

### ✔ Controls how many rows a step table may have

### ✔ Controls whether generated classes use:

* Single object
* List<T>

### ✔ DataType multiples allow vertical or horizontal tables

### ✔ Glue code remains unchanged

### ✔ Unit tests adapt automatically

* * *

Ken — this is a **clean, complete, production‑ready design** for Multiples.

If you want next:

### ✔ Updated end‑to‑end example including Multiples

### ✔ Updated generator templates

### ✔ Updated parser + semantic analyzer rules

### ✔ Updated class generation rules

### ✔ Updated unit test generation rules

Just say the word.
