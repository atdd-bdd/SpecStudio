Ken — perfect.  
I will now generate the **full example output**, exactly matching your required format, using:

* **Constructors instead of builders**
* **Typed classes with string fields**
* **Internal typed classes**
* **List<TypedClass>** for ListOfObject steps
* **List<List<string>>** for simple tables
* **Glue file exactly like your example**
* **One test per scenario**
* **And → Given/When/Then normalization**
* **Namespace + class naming exactly as in your sample**

Everything below is **precisely what your generator should output** for the sample feature file you provided.

* * *

⭐ FULL EXAMPLE OUTPUT
=====================

### For Feature: _Examples_

### Matching your exact glue file + unit test style

* * *

1. **Generated Data Classes**
   =============================

(Overwritten every run)
📘 `FandC.cs`
-------------

    namespace gherkinexecutor.Feature_Examples
    {
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
    
            public override string ToString()
            {
                return $"F={f}, C={c}, Notes={notes}";
            }
        }
    }

📘 `ValueValid.cs`
------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class ValueValid
        {
            public string value;
            public string valid;
            public string notes;
    
            public ValueValid(string value, string valid, string notes)
            {
                this.value = value;
                this.valid = valid;
                this.notes = notes;
            }
    
            public ValueValidInternal ToValueValidInternal()
            {
                return new ValueValidInternal(
                    value,
                    bool.Parse(valid),
                    notes
                );
            }
    
            public override string ToString()
            {
                return $"Value={value}, Valid={valid}, Notes={notes}";
            }
        }
    }

📘 `LabelValue.cs`
------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class LabelValue
        {
            public string id;
            public string value;
    
            public LabelValue(string id, string value)
            {
                this.id = id;
                this.value = value;
            }
    
            public LabelValueInternal ToLabelValueInternal()
            {
                return new LabelValueInternal(
                    new ID(id),
                    int.Parse(value)
                );
            }
    
            public override string ToString()
            {
                return $"ID={id}, Value={value}";
            }
        }
    }

📘 `FilterValue.cs`
-------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class FilterValue
        {
            public string value;
    
            public FilterValue(string value)
            {
                this.value = value;
            }
    
            public FilterValueInternal ToFilterValueInternal()
            {
                return new FilterValueInternal(
                    new ID(value)
                );
            }
    
            public override string ToString()
            {
                return $"Value={value}";
            }
        }
    }

📘 `ResultValue.cs`
-------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class ResultValue
        {
            public string sum;
    
            public ResultValue(string sum)
            {
                this.sum = sum;
            }
    
            public ResultValueInternal ToResultValueInternal()
            {
                return new ResultValueInternal(
                    int.Parse(sum)
                );
            }
    
            public override string ToString()
            {
                return $"Sum={sum}";
            }
        }
    }

* * *

2. **Generated Internal Typed Classes**
   =======================================

(Overwritten every run)
📘 `FandCInternal.cs`
---------------------

    namespace gherkinexecutor.Feature_Examples
    {
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
    }

📘 `ValueValidInternal.cs`
--------------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class ValueValidInternal
        {
            public string value;
            public bool valid;
            public string notes;
    
            public ValueValidInternal(string value, bool valid, string notes)
            {
                this.value = value;
                this.valid = valid;
                this.notes = notes;
            }
        }
    }

📘 `LabelValueInternal.cs`
--------------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class LabelValueInternal
        {
            public ID id;
            public int value;
    
            public LabelValueInternal(ID id, int value)
            {
                this.id = id;
                this.value = value;
            }
        }
    }

📘 `FilterValueInternal.cs`
---------------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class FilterValueInternal
        {
            public ID value;
    
            public FilterValueInternal(ID value)
            {
                this.value = value;
            }
        }
    }

📘 `ResultValueInternal.cs`
---------------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class ResultValueInternal
        {
            public int sum;
    
            public ResultValueInternal(int sum)
            {
                this.sum = sum;
            }
        }
    }

* * *

3. **Generated Unit Test File**
   ===============================

(Overwritten every run — EXACTLY matches your sample)
    namespace gherkinexecutor.Feature_Examples{
    using Microsoft.VisualStudio.TestTools.UnitTesting;
    using System.Collections.Generic;

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

    [TestMethod]
    public void Test_Scenario_Domain_Term_ID(){
         Feature_Examples_glue feature_Examples_glue_object = new Feature_Examples_glue();

         List<ValueValid> objectList2 = new List<ValueValid>{
             new ValueValid("Q1234","true",""),
             new ValueValid("Q123","false","Too short"),
             new ValueValid("Q12345","false","Too long"),
             new ValueValid("A1234","false","Must begin with Q")
         };
         feature_Examples_glue_object.Rule_ID_must_have_exactly_5_letters_and_begin_with_Q(objectList2);
    }

    [TestMethod]
    public void Test_Scenario_Filter_Data(){
         Feature_Examples_glue feature_Examples_glue_object = new Feature_Examples_glue();

         List<LabelValue> objectList3 = new List<LabelValue>{
             new LabelValue("Q1234","1"),
             new LabelValue("Q9999","2"),
             new LabelValue("Q1234","3")
         };
         feature_Examples_glue_object.Given_list_of_numbers(objectList3);

         List<List<string>> stringListList4 = new List<List<string>>{
            new List<string>{ "Q1234" }
         };
         feature_Examples_glue_object.When_filtered_by_ID_with_value(stringListList4);

         List<List<string>> stringListList5 = new List<List<string>>{
            new List<string>{ "4" }
         };
         feature_Examples_glue_object.Then_sum_is(stringListList5);
    }

    [TestMethod]
    public void Test_Scenario_Filter_Data_Another_Way(){
         Feature_Examples_glue feature_Examples_glue_object = new Feature_Examples_glue();

         List<LabelValue> objectList6 = new List<LabelValue>{
             new LabelValue("Q1234","1"),
             new LabelValue("Q9999","2"),
             new LabelValue("Q1234","3")
         };
         feature_Examples_glue_object.Given_list_of_numbers(objectList6);

         List<FilterValue> objectList7 = new List<FilterValue>{
             new FilterValue("Q1234")
         };
         feature_Examples_glue_object.When_filtered_by(objectList7);

         List<ResultValue> objectList8 = new List<ResultValue>{
             new ResultValue("4")
         };
         feature_Examples_glue_object.Then_result(objectList8);
    }
    }
    }

* * *

4. **Glue File**
   ================

(Not overwritten — your exact file)

I will not repeat it here unless you want it regenerated.

* * *

⭐ Ken — this is the **complete, final, exact output** your generator must produce.
==================================================================================

If you want next:

### ✔ The generator code templates

### ✔ The full SpecTableExecutor runtime library

### ✔ The parser + semantic analyzer skeleton

### ✔ A full end‑to‑end example including production code stubs

Just say the word.
