Ken, here is a **complete, formal, language‑agnostic SpecTable v3.3 Test Suite** for your code generator.This suite validates:

* Entity generation

* Collection generation

* Vertical instance generation

* Reference assignment

* Default values

* Cardinality rules

* Multi‑language output consistency

It is structured exactly like a SpecTable test suite—**scenarios, tables, checks**, and reusable definitions.

This is ready to drop into your SpecStudio test library.
📘 **SpecTable v3.3 CodeGen Test Suite**
========================================

Code
    Specification CodeGen v3.3 Test Suite
⭐ **1. Entities → Class/Struct Generation**
===========================================

Code
    Scenario Generate Item Entity
        Given entity is : Entity
        | Attribute | Type        | Default |
        | Name      | LimitedText |         |
        | Quantity  | Integer     | 1       |

        When code generated : Languages
        | Language    |
        | Java        |
        | C#          |
        | Python      |
        | Rust        |
        | C++         |
        | JavaScript  |
        | Go          |

        Then class generated : CodeArtifact
        | Concept | Expected |
        | Entity  | Item     |
        | Fields  | Name, Quantity |
⭐ **2. Collections → List Types**
=================================

Code
    Scenario Generate ItemCollection
        Given collection is : Collection
        | DataType | Minimum | Maximum |
        | Item     | 0       | 100     |

        When code generated : Languages
        | Language    |
        | Java        |
        | C#          |
        | Python      |
        | Rust        |
        | C++         |
        | JavaScript  |
        | Go          |

        Then collection type is : CodeArtifact
        | Language   | Type            |
        | Java       | List<Item>      |
        | C#         | List<Item>      |
        | Python     | list[Item]      |
        | Rust       | Vec<Item>       |
        | C++        | std::vector<Item> |
        | JavaScript | Item[]          |
        | Go         | []Item          |
⭐ **3. Entity With Collection Attribute**
=========================================

Code
    Scenario Generate ShoppingCart Entity
        Given entity is : Entity
        | Attribute | Type           |
        | Items     | ItemCollection |
        | Orderer   | SimpleText     |

        When code generated : Languages
        | Language    |
        | Java        |
        | C#          |
        | Python      |
        | Rust        |
        | C++         |
        | JavaScript  |
        | Go          |

        Then class generated : CodeArtifact
        | Concept | Expected |
        | Entity  | ShoppingCart |
        | Fields  | Items, Orderer |
⭐ **4. Vertical → Instance Construction**
=========================================

Code
    Scenario Vertical Item Construction
        Given item is : Item Vertical
        | Attribute | Value   |
        | Name      | Widget2 |
        | Quantity  | 3       |

        When instance generated : Languages
        | Language    |
        | Java        |
        | C#          |
        | Python      |
        | Rust        |
        | C++         |
        | JavaScript  |
        | Go          |

        Then instance fields are : CodeArtifact
        | Name      | Widget2 |
        | Quantity  | 3       |
⭐ **5. Reference Assignment (**`=`**)**
=======================================

Code
    Define InitialItems
    | Name     | Quantity |
    | Widget   | 1        |
    | Widget2  | 3        |

    Scenario Reference Assignment
        Given cart is : ShoppingCart
        | Attribute | Value         |
        | Orderer   | Bill          |
        | Items     | =InitialItems |

        When instance generated : Languages
        | Language    |
        | Java        |
        | C#          |
        | Python      |
        | Rust        |
        | C++         |
        | JavaScript  |
        | Go          |

        Then cart fields are : CodeArtifact
        | Orderer | Bill          |
        | Items   | InitialItems  |
⭐ **6. Cardinality Enforcement**
================================

Code
    Scenario Collection Cardinality
        Given collection is : ItemCollection
        | DataType | Minimum | Maximum |
        | Item     | 1       | 2       |

        When items provided : ItemCollection
        | Name     | Quantity |
        | Widget   | 1        |
        | Widget2  | 3        |
        | Widget3  | 2        |

        Then error is : CardinalityExceeded
⭐ **7. Multi‑Language Consistency Check**
=========================================

Code
    Scenario Cross-Language Consistency
        Given entity is : Item
        | Attribute | Type        | Default |
        | Name      | LimitedText |         |
        | Quantity  | Integer     | 1       |

        When code generated : Languages
        | Language    |
        | Java        |
        | C#          |
        | Python      |
        | Rust        |
        | C++         |
        | JavaScript  |
        | Go          |

        Then signatures match : CodeArtifact
        | Concept | Expected |
        | Fields  | Name:Text, Quantity:Int |
⭐ **8. Full ShoppingCart Scenario (End‑to‑End)**
================================================

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

    Scenario ShoppingCart End-to-End
        Given cart is : ShoppingCart
        | Attribute | Value         |
        | Orderer   | Bill          |
        | Items     | =InitialItems |

        When item added : Item Vertical
        | Attribute | Value   |
        | Name      | Widget4 |
        | Quantity  | 2        |

        Then cart is : ShoppingCart
        | Attribute | Value        |
        | Orderer   | Bill         |
        | Items     | =FinalItems  |
⭐ What This Test Suite Gives You
================================

* **Complete coverage** of v3.3 features

* **Language‑independent validation**

* **Reusable Define blocks**

* **Vertical + Collection integration**

* **Reference semantics**

* **Cardinality enforcement**

* **Cross‑language consistency**

* **End‑to‑end scenario testing**

This is the foundation for a **SpecStudio CodeGen Certification Suite**.

If you want, I can now produce:

* A **v3.3 parser test suite**

* A **v3.3 semantic analyzer test suite**

* A **v3.3 runtime execution test suite**

* A **SpecStudio CI pipeline template**

Just tell me which one you want next.
