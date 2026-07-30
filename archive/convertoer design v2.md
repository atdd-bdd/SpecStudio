Absolutely, Ken — this is a clean and easy adjustment, and it actually makes the entire system more consistent and predictable.

You want:

### ✔ All **external-facing** data classes to end in **String**

(e.g., `LabelValueString`, `FandCString`, `ResultValueString`)

### ✔ All **internal typed** classes to end in **Typed**

(e.g., `LabelValueTyped`, `FandCTyped`, `ResultValueTyped`)

### ✔ Glue methods should receive **List<…String>**

### ✔ Glue methods should convert using `.To…Typed()`

### ✔ Internal typed classes replace the old “…Internal” naming

This is a clean, systematic naming convention — and it aligns perfectly with the architecture you’re building.

Below is the **updated full example output**, rewritten to use the new naming convention.

* * *

⭐ UPDATED FULL EXAMPLE OUTPUT
=============================

### Using `<Name>String` and `<Name>Typed` naming

### EXACTLY matching your required unit test + glue style

* * *

1. **Generated Data Classes (String classes)**
   ==============================================

(Overwritten every run)
📘 `FandCString.cs`
-------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class FandCString
        {
            public string f;
            public string c;
            public string notes;
    
            public FandCString(string f, string c, string notes)
            {
                this.f = f;
                this.c = c;
                this.notes = notes;
            }
    
            public FandCTyped ToFandCTyped()
            {
                return new FandCTyped(
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

📘 `ValueValidString.cs`
------------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class ValueValidString
        {
            public string value;
            public string valid;
            public string notes;
    
            public ValueValidString(string value, string valid, string notes)
            {
                this.value = value;
                this.valid = valid;
                this.notes = notes;
            }
    
            public ValueValidTyped ToValueValidTyped()
            {
                return new ValueValidTyped(
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

📘 `LabelValueString.cs`
------------------------

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
    
            public LabelValueTyped ToLabelValueTyped()
            {
                return new LabelValueTyped(
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

📘 `FilterValueString.cs`
-------------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class FilterValueString
        {
            public string value;
    
            public FilterValueString(string value)
            {
                this.value = value;
            }
    
            public FilterValueTyped ToFilterValueTyped()
            {
                return new FilterValueTyped(
                    new ID(value)
                );
            }
    
            public override string ToString()
            {
                return $"Value={value}";
            }
        }
    }

📘 `ResultValueString.cs`
-------------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class ResultValueString
        {
            public string sum;
    
            public ResultValueString(string sum)
            {
                this.sum = sum;
            }
    
            public ResultValueTyped ToResultValueTyped()
            {
                return new ResultValueTyped(
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
📘 `FandCTyped.cs`
------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class FandCTyped
        {
            public int f;
            public int c;
            public string notes;
    
            public FandCTyped(int f, int c, string notes)
            {
                this.f = f;
                this.c = c;
                this.notes = notes;
            }
        }
    }

📘 `ValueValidTyped.cs`
-----------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class ValueValidTyped
        {
            public string value;
            public bool valid;
            public string notes;
    
            public ValueValidTyped(string value, bool valid, string notes)
            {
                this.value = value;
                this.valid = valid;
                this.notes = notes;
            }
        }
    }

📘 `LabelValueTyped.cs`
-----------------------

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
        }
    }

📘 `FilterValueTyped.cs`
------------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class FilterValueTyped
        {
            public ID value;
    
            public FilterValueTyped(ID value)
            {
                this.value = value;
            }
        }
    }

📘 `ResultValueTyped.cs`
------------------------

    namespace gherkinexecutor.Feature_Examples
    {
        public class ResultValueTyped
        {
            public int sum;
    
            public ResultValueTyped(int sum)
            {
                this.sum = sum;
            }
        }
    }

* * *

3. **Generated Unit Test File (Updated for String/Typed naming)**
   =================================================================

(Overwritten every run)
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

    [TestMethod]
    public void Test_Scenario_Domain_Term_ID(){
         Feature_Examples_glue feature_Examples_glue_object = new Feature_Examples_glue();

         List<ValueValidString> objectList2 = new List<ValueValidString>{
             new ValueValidString("Q1234","true",""),
             new ValueValidString("Q123","false","Too short"),
             new ValueValidString("Q12345","false","Too long"),
             new ValueValidString("A1234","false","Must begin with Q")
         };
         feature_Examples_glue_object.Rule_ID_must_have_exactly_5_letters_and_begin_with_Q(objectList2);
    }

    [TestMethod]
    public void Test_Scenario_Filter_Data(){
         Feature_Examples_glue feature_Examples_glue_object = new Feature_Examples_glue();

         List<LabelValueString> objectList3 = new List<LabelValueString>{
             new LabelValueString("Q1234","1"),
             new LabelValueString("Q9999","2"),
             new LabelValueString("Q1234","3")
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

         List<LabelValueString> objectList6 = new List<LabelValueString>{
             new LabelValueString("Q1234","1"),
             new LabelValueString("Q9999","2"),
             new LabelValueString("Q1234","3")
         };
         feature_Examples_glue_object.Given_list_of_numbers(objectList6);

         List<FilterValueString> objectList7 = new List<FilterValueString>{
             new FilterValueString("Q1234")
         };
         feature_Examples_glue_object.When_filtered_by(objectList7);

         List<ResultValueString> objectList8 = new List<ResultValueString>{
             new ResultValueString("4")
         };
         feature_Examples_glue_object.Then_result(objectList8);
    }
    }
    }

* * *

4. **Glue File (Updated for String/Typed naming)**
   ==================================================

(Not overwritten)

Here is your glue file rewritten with the new naming:
    namespace gherkinexecutor.Feature_Examples
    {
        using System;
        using System.Collections.Generic;
        using static Microsoft.VisualStudio.TestTools.UnitTesting.Assert;

        public class Feature_Examples_glue
        {
            const string DNCString = "?DNC?";

            SolutionForListOfNumber solution = new SolutionForListOfNumber();

            public void Calculation_Convert_F_to_C(List<FandCString> values)
            {
                Console.WriteLine("---  " + "Calculation_Convert_F_to_C");
                foreach (FandCString value in values)
                {
                    Console.WriteLine(value);
                    FandCTyped i = value.ToFandCTyped();
                    int c = TemperatureCalculations.ConvertFahrenheitToCelsius(i.f);
                    AreEqual(i.c, c, i.notes);
                }
            }

            public void Rule_ID_must_have_exactly_5_letters_and_begin_with_Q(List<ValueValidString> values)
            {
                Console.WriteLine("---  " + "Rule_ID_must_have_exactly_5_letters_and_begin_with_Q");
                foreach (ValueValidString value in values)
                {
                    Console.WriteLine(value);

                    bool expectedException = !bool.Parse(value.valid);
                    try
                    {
                        new ID(value.value);
                        if (expectedException)
                        {
                            Fail("Invalid value did not throw exeception "
                                    + value.value + " " + value.notes);
                        }
                    }
                    catch (Exception e)
                    {
                        if (!expectedException)
                            Fail("Valid value threw exeception " + e
                                    + value.value + " " + value.notes);
                    }
                }
            }

            public void Given_list_of_numbers(List<LabelValueString> values)
            {
                Console.WriteLine("---  " + "Given_list_of_numbers");
                foreach (LabelValueString value in values)
                {
                    Console.WriteLine(value);
                    LabelValueTyped i = value.ToLabelValueTyped();
                    solution.Add(i);
                }
            }

            public void When_filtered_by_ID_with_value(List<List<string>> values)
            {
                Console.WriteLine("---  " + "When_filtered_by_ID_with_value");
                string id = values[0][0];
                Console.WriteLine("ID is " + id);
                solution.SetFilterValue(new ID(id));
            }

            public void Then_sum_is(List<List<string>> values)
            {
                Console.WriteLine("---  " + "Then_sum_is");
                int expected = Int32.Parse(values[0][0]);
                Console.WriteLine("    expecting " + expected);
                int result = solution.Sum();
                AreEqual(expected, result);
            }

            public void When_filtered_by(List<FilterValueString> values)
            {
                Console.WriteLine("---  " + "When_filtered_by");
                foreach (FilterValueString value in values)
                {
                    Console.WriteLine(value);
                    FilterValueTyped i = value.ToFilterValueTyped();
                    solution.SetFilterValue(i.value);
                }
            }

            public void Then_result(List<ResultValueString> values)
            {
                Console.WriteLine("---  " + "Then_result");
                foreach (ResultValueString value in values)
                {
                    Console.WriteLine(value);
                    ResultValueTyped i = value.ToResultValueTyped();
                    int actual = solution.Sum();
                    AreEqual(i.sum, actual);
                }
            }
        }
    }

* * *

⭐ Ken — this is now the **final, correct, complete output**
===========================================================

Everything matches:

* Your naming convention
* Your glue file style
* Your unit test style
* Your constructor‑based object creation
* Your internal typed conversion
* Your ListOfObject vs List<List<string>> distinction
* Your step naming rules

If you want next:

### ✔ The generator templates

### ✔ The parser + semantic analyzer skeleton

### ✔ The full SpecTableExecutor runtime

### ✔ A complete end‑to‑end example with production code stubs

Just say the word.
