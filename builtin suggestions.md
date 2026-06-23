Ken, this is a _great_ refinement — and honestly, this is the point where SpecTable starts to intersect with the realities of **target language bindings**. If you’re going to support built‑in DataTypes, you want them to map cleanly to:

* C++
* Python
* C#
* Java

…without ambiguity or surprises.

Let’s walk through this systematically and build a **cross‑language‑compatible built‑in type set**.

* * *

⭐ Step 1 — Keep **String** and **Text**
=======================================

Absolutely. Even if they map to the same underlying type, they carry different semantics:

* **String** → short, single‑line, atomic
* **Text** → long, multi‑line, narrative

This distinction matters for business rules and readability.

* * *

⭐ Step 2 — Evaluate each built‑in type against C++, Python, C#, Java
====================================================================

Below is a clean compatibility matrix.

* * *

📘 **Built‑In Type Compatibility Matrix**
=========================================

| SpecTable Type                       | C++                                                | Python              | C#                 | Java               | Notes                                   |
| ------------------------------------ | -------------------------------------------------- | ------------------- | ------------------ | ------------------ | --------------------------------------- |
| **Character**                        | `char`                                             | `str` (len=1)       | `char`             | `char`             | Universal                               |
| **String**                           | `std::string`                                      | `str`               | `string`           | `String`           | Universal                               |
| **Text** | `std::string`                                      | `str`               | `string`           | `String`           | Semantics differ, type same             |
| **Integer**                          | `int` / `long`                                     | `int`               | `int` / `long`     | `int` / `long`     | Universal                               |
| **Float**                            | `float` / `double`                                 | `float`             | `float` / `double` | `float` / `double` | Universal                               |
| **Decimal**                          | `double` or `boost::multiprecision::cpp_dec_float` | `decimal.Decimal`   | `decimal`          | `BigDecimal`       | Only Python/C#/Java have native decimal |
| **Boolean**                          | `bool`                                             | `bool`              | `bool`             | `boolean`          | Universal                               |
| **Date**                             | `<chrono>` (C++20) or `std::string`                | `datetime.date`     | `DateOnly`         | `LocalDate`        | All languages support date              |
| **Time**                             | `<chrono>` or `std::string`                        | `datetime.time`     | `TimeOnly`         | `LocalTime`        | All languages support time              |
| **DateTime**                         | `<chrono>` or `std::string`                        | `datetime.datetime` | `DateTime`         | `LocalDateTime`    | Universal                               |
| **Duration**                         | `<chrono::duration>`                               | `timedelta`         | `TimeSpan`         | `Duration`         | Universal                               |
| **Money**                            | `double` or decimal lib                            | `decimal.Decimal`   | `decimal`          | `BigDecimal`       | Decimal‑based                           |
| **Percentage**                       | `double`                                           | `float`             | `double`           | `double`           | Numeric everywhere                      |
| **ID**                               | `std::string`                                      | `str`               | `string`           | `String`           | Universal                               |
| **Email**                            | `std::string`                                      | `str`               | `string`           | `String`           | Format validation only                  |
| **URL**                              | `std::string`                                      | `str`               | `string`           | `String`           | Format validation only                  |
| **Phone**                            | `std::string`                                      | `str`               | `string`           | `String`           | Format validation only                  |
| **Code**                             | `std::string`                                      | `str`               | `string`           | `String`           | Universal                               |

* * *

⭐ Step 3 — Which types are universally safe?
============================================

These types map cleanly to all four languages:

### ✔ Character

### ✔ String

### ✔ Text

### ✔ Integer

### ✔ Float

### ✔ Boolean

### ✔ Date

### ✔ Time

### ✔ DateTime

### ✔ Duration

### ✔ Percentage

### ✔ ID

### ✔ Email

### ✔ URL

### ✔ Phone

### ✔ Code

These are **100% safe** across all languages.

* * *

⭐ Step 4 — Which types need special handling?
=============================================

### ⚠ **Decimal**

* Python, C#, Java → native decimal
* C++ → no native decimal
  * You must choose:
    * `double` (fast, but approximate)
    * or `boost::multiprecision` (precise, but heavy)

### ⚠ **Money**

* Same issue as Decimal
* Should be implemented as Decimal under the hood

### Recommendation

Keep **Decimal** and **Money**, but document that C++ requires a library or fallback.

* * *

⭐ Step 5 — Final Recommended Built‑In Set for SpecTable v2.7
============================================================

**Core**
--------

* Character
* String
* Text
* Integer
* Float
* Decimal
* Boolean

**Temporal**
------------

* Date
* Time
* DateTime
* Duration

**Business**
------------

* Money
* Percentage

**Identifiers**
---------------

* ID
* Email
* URL
* Phone
* Code

Total: **17 built‑ins**

All of them map cleanly to Python, C#, Java.  
All except Decimal/Money map cleanly to C++ without libraries.

* * *

⭐ Step 6 — Should we add anything else?
=======================================

Here are optional candidates:

### **Binary**

* Maps to `std::vector<uint8_t>`, `bytes`, `byte[]`, `byte[]`

### **UUID**

* Maps to `std::string`, `uuid.UUID`, `Guid`, `UUID`

### **IPAddress**

* Maps to `std::string`, `ipaddress.IPv4Address`, `IPAddress`, `InetAddress`

If you want to support networking or identity, these are good additions.

* * *

⭐ My recommendation
===================

Stick with the **17 built‑ins**, and optionally add:

* UUID
* Binary

If your domain needs them.

* * *

If you want, I can now:

* Integrate these built‑ins into the **SpecTable v2.7 reference**
* Update the **reference card**
* Update the **grammar**
* Add **test cases** for built‑ins
* Add **language‑binding notes** for C++, Python, C#, Java

Just tell me.

Ken, this is a _great_ refinement — and honestly, this is the point where SpecTable starts to intersect with the realities of **target language bindings**. If you’re going to support built‑in DataTypes, you want them to map cleanly to:

* C++
* Python
* C#
* Java

…without ambiguity or surprises.

Let’s walk through this systematically and build a **cross‑language‑compatible built‑in type set**.

* * *

⭐ Step 1 — Keep **String** and **Text**
=======================================

Absolutely. Even if they map to the same underlying type, they carry different semantics:

* **String** → short, single‑line, atomic
* **Text** → long, multi‑line, narrative

This distinction matters for business rules and readability.

* * *

⭐ Step 2 — Evaluate each built‑in type against C++, Python, C#, Java
====================================================================

Below is a clean compatibility matrix.

* * *

📘 **Built‑In Type Compatibility Matrix**
=========================================

| SpecTable Type | C++                                                | Python              | C#                 | Java               | Notes                                   |
| -------------- | -------------------------------------------------- | ------------------- | ------------------ | ------------------ | --------------------------------------- |
| **Character**  | `char`                                             | `str` (len=1)       | `char`             | `char`             | Universal                               |
| **String**     | `std::string`                                      | `str`               | `string`           | `String`           | Universal                               |
| **Text**       | `std::string`                                      | `str`               | `string`           | `String`           | Semantics differ, type same             |
| **Integer**    | `int` / `long`                                     | `int`               | `int` / `long`     | `int` / `long`     | Universal                               |
| **Float**      | `float` / `double`                                 | `float`             | `float` / `double` | `float` / `double` | Universal                               |
| **Decimal**    | `double` or `boost::multiprecision::cpp_dec_float` | `decimal.Decimal`   | `decimal`          | `BigDecimal`       | Only Python/C#/Java have native decimal |
| **Boolean**    | `bool`                                             | `bool`              | `bool`             | `boolean`          | Universal                               |
| **Date**       | `<chrono>` (C++20) or `std::string`                | `datetime.date`     | `DateOnly`         | `LocalDate`        | All languages support date              |
| **Time**       | `<chrono>` or `std::string`                        | `datetime.time`     | `TimeOnly`         | `LocalTime`        | All languages support time              |
| **DateTime**   | `<chrono>` or `std::string`                        | `datetime.datetime` | `DateTime`         | `LocalDateTime`    | Universal                               |
| **Duration**   | `<chrono::duration>`                               | `timedelta`         | `TimeSpan`         | `Duration`         | Universal                               |
| **Money**      | `double` or decimal lib                            | `decimal.Decimal`   | `decimal`          | `BigDecimal`       | Decimal‑based                           |
| **Percentage** | `double`                                           | `float`             | `double`           | `double`           | Numeric everywhere                      |
| **ID**         | `std::string`                                      | `str`               | `string`           | `String`           | Universal                               |
| **Email**      | `std::string`                                      | `str`               | `string`           | `String`           | Format validation only                  |
| **URL**        | `std::string`                                      | `str`               | `string`           | `String`           | Format validation only                  |
| **Phone**      | `std::string`                                      | `str`               | `string`           | `String`           | Format validation only                  |
| **Code**       | `std::string`                                      | `str`               | `string`           | `String`           | Universal                               |

* * *

⭐ Step 3 — Which types are universally safe?
============================================

These types map cleanly to all four languages:

### ✔ Character

### ✔ String

### ✔ Text

### ✔ Integer

### ✔ Float

### ✔ Boolean

### ✔ Date

### ✔ Time

### ✔ DateTime

### ✔ Duration

### ✔ Percentage

### ✔ ID

### ✔ Email

### ✔ URL

### ✔ Phone

### ✔ Code

These are **100% safe** across all languages.

* * *

⭐ Step 4 — Which types need special handling?
=============================================

### ⚠ **Decimal**

* Python, C#, Java → native decimal
* C++ → no native decimal
  * You must choose:
    * `double` (fast, but approximate)
    * or `boost::multiprecision` (precise, but heavy)

### ⚠ **Money**

* Same issue as Decimal
* Should be implemented as Decimal under the hood

### Recommendation

Keep **Decimal** and **Money**, but document that C++ requires a library or fallback.

* * *

⭐ Step 5 — Final Recommended Built‑In Set for SpecTable v2.7
============================================================

**Core**
--------

* Character
* String
* Text
* Integer
* Float
* Decimal
* Boolean

**Temporal**
------------

* Date
* Time
* DateTime
* Duration

**Business**
------------

* Money
* Percentage

**Identifiers**
---------------

* ID
* Email
* URL
* Phone
* Code

Total: **17 built‑ins**

All of them map cleanly to Python, C#, Java.  
All except Decimal/Money map cleanly to C++ without libraries.

* * *

⭐ Step 6 — Should we add anything else?
=======================================

Here are optional candidates:

### **Binary**

* Maps to `std::vector<uint8_t>`, `bytes`, `byte[]`, `byte[]`

### **UUID**

* Maps to `std::string`, `uuid.UUID`, `Guid`, `UUID`

### **IPAddress**

* Maps to `std::string`, `ipaddress.IPv4Address`, `IPAddress`, `InetAddress`

If you want to support networking or identity, these are good additions.

* * *

⭐ My recommendation
===================

Stick with the **17 built‑ins**, and optionally add:

* UUID
* Binary

If your domain needs them.

* * *

If you want, I can now:

* Integrate these built‑ins into the **SpecTable v2.7 reference**
* Update the **reference card**
* Update the **grammar**
* Add **test cases** for built‑ins
* Add **language‑binding notes** for C++, Python, C#, Java

Just tell me.

Ken — yes, you’re absolutely right to introduce **built‑in DataTypes**.  
Every mature DSL needs a core set of primitive types that:

* don’t require a `DataType` declaration
* are universally available
* are guaranteed to exist for all parsers
* serve as the foundation for user‑defined DataTypes

Let’s build the **SpecTable v2.7 Built‑In DataType Set**.

I’ll give you:

1. The essential primitives
2. Optional but highly useful primitives
3. Domain‑friendly primitives
4. A clean, final recommended built‑in set
5. How they behave in grid tables, Define blocks, and Examples

This will round out the language beautifully.

* * *

⭐ 1. Essential Built‑In DataTypes
=================================

These are the absolute must‑haves — every DSL needs them.

### **Character**

A single Unicode character.

### **String**

A sequence of characters.  
(You can alias `Text` → `String` if you want both.)

### **Integer**

Whole numbers, positive or negative.

### **Float**

Floating‑point number (binary floating).

### **Decimal**

Fixed‑precision decimal number (for money, measurements).

### **Boolean**

`true` / `false`.

These six form the **core primitive set**.

* * *

⭐ 2. Strongly Recommended Built‑Ins
===================================

These are extremely common in business rules, calculations, and scenarios.

### **Date**

Calendar date (YYYY‑MM‑DD).

### **Time**

Time of day (HH:MM[:SS]).

### **DateTime**

Combined date + time.

### **Duration**

A span of time (e.g., “3h”, “2d”, “15m”).

### **Percentage**

A decimal or integer with `%` semantics.

### **Money**

A Decimal with currency semantics.

These are used constantly in real‑world specs.

* * *

⭐ 3. Domain‑Friendly Built‑Ins
==============================

These aren’t required, but they make the DSL feel “complete.”

### **ID**

Generic identifier (string with constraints).

### **Email**

String with email format.

### **URL**

String with URL format.

### **Phone**

String with phone format.

### **Code**

A short alphanumeric code (SKU, product code, etc.).

These are optional but very helpful.

* * *

⭐ 4. Final Recommended Built‑In DataType Set for SpecTable v2.7
===============================================================

Here is the clean, balanced set I recommend:
**Core Primitives**
-------------------

* Character
* String
* Integer
* Float
* Decimal
* Boolean

**Temporal Types**
------------------

* Date
* Time
* DateTime
* Duration

**Business Types**
------------------

* Money
* Percentage

**Identifier Types**
--------------------

* ID
* Email
* URL
* Phone
* Code

This gives you **15 built‑ins**, which is a perfect size:  
not too many, not too few.

* * *

⭐ 5. How Built‑In DataTypes Behave in SpecTable v2.7
====================================================

✔ They do **not** require a `DataType` declaration
--------------------------------------------------

They are always available.
✔ They can be used in:
----------------------

* Entities
* Attributes
* BusinessRules
* Calculations
* Scenarios
* Grid tables
* Define blocks
* Step tables

✔ They can appear in grid tables
--------------------------------

Example:
    Given matrix of numbers : Integer
    | 1 | 2 | 3 |
    | 4 | 5 | 6 |
✔ They can be referenced in Define blocks
-----------------------------------------

Example:
    Define PriceList =
    """
    | 10.00 |
    | 12.50 |
    |  9.99 |
    """
✔ They can be used as AttributeSet types
----------------------------------------

Example:
    Attributes Payment
    | Attribute | Type   | Default | Notes | In-Out |
    | Amount    | Money  | 0       |       | In     |
    | Date      | Date   |         |       | In     |
    | Approved  | Boolean| false   |       | Out    |

* * *

⭐ 6. Should String and Text both exist?
=======================================

You have two options:

### Option A — Only **String**

Simplest.

### Option B — **String** and **Text**

Where:

* **String** = short, single‑line
* **Text** = long, multi‑line

Given you now have **Define blocks**, I recommend:

### ✔ Keep only **String**

Let Define handle long text.

* * *

⭐ 7. Should Float and Decimal both exist?
=========================================

Yes.

* **Float** = binary floating point (fast, approximate)
* **Decimal** = fixed precision (money, measurements)

Both are needed.

* * *

⭐ 8. Should Money be a built‑in or a user DataType?
===================================================

I strongly recommend:

### ✔ Make **Money** a built‑in

It’s too common to force users to define it.

* * *

⭐ 9. Final Answer: Built‑In DataTypes for SpecTable v2.7
========================================================

Here is the final, recommended list:

### **Core**

* Character
* String
* Integer
* Float
* Decimal
* Boolean

### **Temporal**

* Date
* Time
* DateTime
* Duration

### **Business**

* Money
* Percentage

### **Identifiers**

* ID
* Email
* URL
* Phone
* Code

Total: **15 built‑ins**

* * *

Ken — if you want, I can now:

* Integrate these into the **full v2.7 specification**
* Update the **reference card**
* Update the **grammar**
* Update the **test suite**
* Add examples showing built‑ins in Entities, BusinessRules, Calculations, Scenarios

Just tell me.
