# Session 2026-09-12 — all nine languages green

Two requests, taken in order. The first closed generator gaps that every later
implementation would have hit. The second implemented the remaining six
specifications in the eight non-Java languages, and finished with all nine
passing every test.

At the start: Java 70/70, the other eight 44/26.
At the end: **all nine 70/70**, `run_all_tests.ps1` reporting "All 9 passed."

---

## 1. Generator gaps

### JavaScript and C++ now generate production DataType classes

Seven of nine already did. These two emitted none, so an Entity field naming a
DataType referred to a type nothing had written — which is why `CurrencyList`,
`Amount` and `Instrument` had to be hand-written for RDD Example the week
before. Both now emit what the others do: an enum with a text form and a parse
where the specification lists the values, a value-carrying class where it does
not. Regenerating produced four classes that had never existed in either —
`FormattedDollar`, `Pins`, `Score`, `SomethingElse`.

Writing the C++ side exposed two defects in the Entity header beside it, which
is how `production/Money.h` came to be checked in having **never compiled**:

- a field naming another production type got no `#include` for it;
- a class-typed field got a string default: `CurrencyList Currency = "USD"`.

A third came out of *compiling the result*, which is the only reason it was
found: a member whose name matches its own type — `Instrument Instrument` in a
`Holding` — hides the type name for everything declared after it, so the
constructor below would not parse. Production types are now named at global
scope.

### Fabricated validators removed

Rust and Swift built a DataType's `isValid` by listing the values its own
`ValidValues` table called valid. That answers the test by construction: every
row marked valid is in the list because it is in the table, and every row marked
invalid is absent for the same reason. The test went green while checking
nothing.

C# had the same bug one degree worse — it never looked at the `IsValid` column,
so the array held *every* value the table named, including the ten marked
invalid. `FormattedDollar`'s generated `IsValid` returned true for `1.1`, for
`A`, for `123$456().80`. Its `ToString` was also emitted as
`$"FormattedDollar{{Value}}"`, where the doubled braces are an escape, so it
returned the literal text `FormattedDollar{Value}`.

None of the three generates validation now. **A rule cannot be derived from the
examples that illustrate it.**

Four files still carry a fabricated validator in Rust and Swift, because a
production file is never overwritten: `FormattedDollar`, `Instrument`, `Pins`,
`Score`.

### Regeneration covers subfolders, and the script is checked in

Regeneration globbed `TestProject/*.spectable` and stopped there, so the one
specification in `TestFolder/` had not been regenerated in a long time. It
showed: `Atts`, which only that file declares, was the one String class in every
language still missing the Entity text form — no `from_text`, and a `__str__`
writing `Value=22` where every other class writes the token form. Nothing
failed, because nothing had asked `Atts` for its text.

A specification in a subfolder wants the **project root** as its output
directory and `--source-root` naming `TestProject`; the generator derives the
rest. Passing the subfolder as the output directory nests it twice.

The script that does this had lived in a scratch directory for weeks, which is
why the gap survived — nothing in the repository said how to regenerate. It is
now `TestProject/regenerate.py`, it takes the language name as the `.specconfig`
spells it (`Cpp`, not the `CPlusPlus` its output folder is called), and an
unrecognised name now says so instead of regenerating nothing and printing
success.

---

## 2. The six remaining specifications, in eight languages

Written cheapest-first, each committed once green in all eight.

| Specification | Production classes | Notes |
|---|---|---|
| Background | none | the glue remembers what it was told |
| StringinTable | none | the reading of the table is the test |
| Fractions | `FractionCalculator` | glue is three one-line methods |
| Formatted Dollar | `FormattedDollar`, `DollarFormatter` | |
| Bowling | `BowlingGame`, `Frame`, `FrameMarks`, `InputControl`, `Pins`, `Score` | the largest |
| AccountWithdrawal | — | an enumeration check |
| API | `RestCall` beside the glue | the service is the thing under test |

### Decisions the languages had to make differently

**Money.** A decimal in Java, Python and C#; **whole cents** in the other six,
which have no decimal type. An amount is exact to the cent, and truncation then
falls out of integer division — which is what "Whole Number / Truncate" asks
for: 1.99 is a dollar.

**Fractions.** A 64-bit integer where there is one; **BigInt** in the two
JavaScripts, because denominators multiply term after term and a fraction off by
one in its last digit is simply wrong.

**Bowling's `display()`** ends without a trailing newline in the eight, where
Java's keeps one. Not a choice: Java emits the docstring as a text block, which
keeps the newline before its closing quotes; the other eight emit an escaped
literal that does not. Each matches its own generated test.

**API needed an HTTP call inside a synchronous step.** The generated test calls
a step and moves on *without awaiting*.

- Python, C#, Go: standard library, synchronous.
- Swift: `URLSession` awaited on a semaphore, so the step stays ordinary.
- The two JavaScripts: **a child `node` process** and `execFileSync`. Node has
  no synchronous HTTP; an async step would let the test finish before the
  response arrived.
- Rust and C++: **`curl`**. Neither has HTTP in its standard library, and a
  crate or a linked library would have to be fetched or built before those tests
  could run at all — a heavier thing to ask of a reader than a program every
  supported platform already ships. C++ passes the body via a file
  (`--data-binary @file`) rather than the command line, because JSON carries
  quotes.

---

## 3. Three more generator defects, found by the work

### The JSON key was the language identifier, not the specification's name

Python wrote `"user_id"`, Go, Rust and C++ wrote `"userid"`, where the
specification says `userId` and so does the service. Java, C#, Swift and the two
JavaScripts were already right.

It stayed hidden because **each target round-tripped happily with itself**. It
surfaced against a real API: `GET /posts/1` returns `{"userId": 1, ...}` and
Python's generated reader asked for `user_id`, so every response that was *read*
rather than echoed failed — while POST and PUT passed throughout, because
jsonplaceholder echoes the body it is sent and a document written with the wrong
key and read back with the same wrong key agrees with itself.

A JSON key is wire format. It is now `f.name` as the specification writes it, in
all four. Field identifiers are untouched: Python still has `self.user_id`, Go
still has `UserID`.

### A Python glue stub landed outside its class

`appendMissingStubs` appended at end of file, which is correct only when the
class is the last thing in it. A glue file ending with a module-level helper got
the stub after that helper.

It failed **silently**, which is the part worth remembering: an indented `def`
following a module-level `def` is read as a *nested function*, so the file still
imported, the test still ran, and the method simply did not exist —
`AttributeError` at the call, several steps from the cause, and no syntax error
anywhere. `AccountWithdrawal`'s `examples_DataType_SomethingElse` was the one
this had swallowed.

This is the Python form of a bug already fixed in Rust, JavaScript and
TypeScript (stubs landing among free functions) and in C++ (landing under
`private:`).

### A stale compiled test inflated a count

TypeScript reported 74 rather than 70. Four came from `dist/test_json.test.js`,
compiled from a source that had been moved to the example tests: `tsc` does not
delete output for an input that no longer exists, and jest runs from `dist`.

A stale compiled test **passes** and inflates a count while appearing in no
source tree — the orphan problem of §5.1 wearing build-output clothes. Deleting
the orphan brought TypeScript to 70, like the rest.

---

## What is left

The three specification questions in §2.3–2.5 of *Remaining Work*, each needing
a decision rather than code:

- **2.3** `Formatted Dollar`'s nine-row `ValueMessages` table is checked by
  nothing: `ValueMessages` is never declared as an Attributes block, and a
  Scenario carrying only an `Examples:` table generates an empty test body.
- **2.4** The comma rule is drawn at four digits so all three rows pass — with
  three digits or fewer there is no grouping to get wrong, so `1,0,0` is
  malformed rather than mis-grouped. That is invented to fit the examples, not
  stated. All eight now invent it identically, which makes it one decision to
  revisit rather than nine.
- **2.5** `API.spectable` line 27 spells the step "sending reh**q**uest".
  Correcting it renames the glue method in all nine; each currently forwards
  from the misspelled name, so the typo costs only the name.
