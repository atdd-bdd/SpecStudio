# Response to "AlignThree findings from a real project"

Written 2026-10-23 against the current repository, which is ahead of the 0.9.2
you were using. Every claim below was checked by running the converter or
`analyze_cli` from this tree, or by reading the code that produces the
behaviour; where a repro is quoted, it was run.

Thank you for the detail. The two items that cost you the most — the headline
"analyses clean, does not compile" case and the dropped empty table — are both
already closed, and the one that is still open (#11) is open exactly where you
say it is, for exactly the reason you give.

## Summary

| # | Finding | Status |
|---|---|---|
| 1 | `EveryCell` on a bare DataType generates uncompilable glue | **Fixed** |
| 2 | The CLI does not warn about step-name collisions | **Not reproducible** — see below |
| 3 | `analyze_cli` reports a file as a duplicate of itself | **Fixed, in two places** |
| 4 | Tilde-for-space is not in the syntax document | **It is, in your own copy** |
| 5 | `as previous` silently produces an empty table | **Not a feature of the language** |
| 6 | A table with a heading and no rows is dropped | **Fixed, as you proposed** |
| 7 | Stub appending works well and deserves documentation | **Documented** |
| 8 | Examples heading mismatch gives two messages | **Fixed** — bullet 1; see below |
| 9 | `$tag` filtering is excellent and undocumented | **Fixed**, both halves; its bare-`\` note **documented** |
| 10 | Named comment continuation; a stray word ends a block | Both **shipped** |
| 11 | 0.9.2 makes a cell a struct and comparisons stop compiling | **Fixed** — see below |
| 12 | `And` works and is in no document | **Fixed**, with two rulings |

## Fixed since 0.9.2

**#1 — `EveryCell` on a bare DataType.** Your repro, run here for C++:

```
DataType TransactionRef
Examples: ValidValues
| Value | IsValid | Notes |
| T1    | Yes     |       |

When transaction deleted : TransactionRef EveryCell
| T1 |
```

now writes `common/transactionref_string.h` and `common/transactionref_typed.h`,
and generates

```cpp
void when_transaction_deleted(const std::vector<TransactionRefString>& values)
```

called from the test as `TransactionRefString("T1")`. No `from_string_struct`,
no undeclared identifier, 0 diagnostics. Option 1 of the three you listed is
what shipped: a DataType used with `EveryCell` gets its own `String`/`Typed`
pair, so the documented language works. The eleven one-attribute `Attributes`
blocks you wrote as a workaround can be removed.

**#3 — a file reported as a duplicate of itself.** Both causes are addressed.
`analyze_cli` now skips a generated copy and says so:

```
-- 0 diagnostic(s) over 3 file(s), 1 generated copy/copies skipped (--include-generated to analyse them)
```

Forced on with `--include-generated`, the message names the other file rather
than itself (`Two.spectable:7 ... is also declared in generated/Two.spectable:7`),
so a symbol is never its own duplicate. Separately, the converter no longer
copies the `.spectable` into the output at all unless asked: the `.specconfig`
default became `copySpectable: false` on 2026-10-11 and the command line
followed on 2026-10-20 — it is `--copy-spectable` to ask for the copy now, and
`--no-copy-spectable` is still accepted and does nothing, so your scripts keep
running.

**#4 — the tilde.** `spectable syntax v3.3a.md` as shipped inside 0.9.2 contains
the rule, in the section on tables: a cell is trimmed, a tilde stands for a
space, and every generator does it, so it holds in all nine languages. It looks
as though the copy you searched was 0.9.1's.

**#6 — a heading with no rows.** Generated now from
`Then rule list rows are : RuleListRow` with a heading row and nothing else:

```cpp
std::vector<RuleListRowString> objectList1 = {
};
glue.then_rule_list_rows_are(objectList1);
```

The step is generated and the glue is handed an empty table — an empty table is
a statement, not an absence. The two spec tables you left written the correct
way are now real assertions without being edited.

**#7 — stub appending.** It is in the User Guide, under *Glue is yours*: each
glue method is created once as a stub and thereafter only appended to, never
rewritten or removed, together with the cost of that safety (glue for a deleted
step is listed as a warning until you remove it).

**#9 — `$tag` and `--tag-filter`.** Both halves are closed. The syntax file
carries the `TagLine` grammar, the rule that a blank line clears a pending tag,
the end-of-file case, and the filter's case-insensitivity and tolerance of a `$`
inside the expression; the User Guide carries the workflow and the fact that a
`$tag` above `Specification` covers the whole file; the Configuration Guide
documents `tagFilter` and `--tag-filter`. And your third bullet — a tag that
attaches to nothing — is now reported at the line you wrote it on:

```
$WIP marks nothing -- a blank line ends a tag, so what follows is generated as
though it were untagged. Put the tag immediately above the block it belongs to.
```

A tag written above a `ScenarioGroup` is dropped the same way, with its own
message, since a group generates no test for a tag to mark.

## Not reproducible

**#2 — the CLI is silent about step-name collisions.** It is not. Your two
steps, in one file, generated by `SpecTableConverter.exe` with no `--context`
and no other arguments:

```
WARNING:17:Step 'when_account_added' expects a AccountWithOpeningBalance table here,
but the same glue method elsewhere in this project expects a Account table -- rename
one of the steps, or switch this project to naming glue methods after the table they
take
```

The check lives in all nine generators, which is what the CLI runs; there is no
path where the IDE has it and the command line does not. What differs is the
input. The IDE passes every other specification in the project as `--context`,
so a collision whose two steps are in different files is visible to it. A CLI
run over one file at a time cannot see the other file's step, and that is almost
certainly what happened with your three. Pass the siblings as `--context` — which
you want anyway, since a DataType declared in one file has to be visible to
another — and the warning appears.

Your second half of #2 is done: `--step-name-attrset-on-collision`, used with
`--step-name-attrset`, appends the attribute set only to the glue methods that
would otherwise collide. Every other method keeps the name the step reads as.

## Not a feature

**#5 — `as previous`.** There is no such construct in the language. The phrase
appears in no generator, nowhere in the IDE, in no shipped document and in no
example specification; it was invented on your side. `Given postings are as
previous` is simply a step whose text ends in those words, with no table under
it, so it generates a no-argument call — the same as any other table-less step.

That also means there are not two readings of it. Nothing resolves to the
`Background`: a `Background`'s own steps, with their tables, are inlined into
every test in the file, so the state you saw was established by the Background
itself and not by the `as previous` step. In a file without one, nothing
establishes anything, which is the 72 you counted.

The repair is what you were told and what you did: a table-form `Define`,
referenced as `=SomePostings` from each scenario that needs it. That is the
construct the language has for sharing a table, and your mechanical replacement
of the six Background-less files is the right outcome. No tool change follows
from this one.

## Fixed because of this report

**#11 — the cell wrapper. Your C++ reasoning is right, and this is now done.**
The `...String` struct had no `operator==`, no `operator!=` and no `empty()`,
and your analysis of why the promise in its comment did not hold is correct.
The generator made the same argument itself eight lines further down, for
streaming:

> Streaming needs its own operator: the one for `std::string` is a template, and
> template argument deduction does not apply a user-defined conversion, so
> `message << cell` would not find it.

That is exactly why `cell == expected` cannot reach `std::string`'s
`operator==` either — so the ambiguity the comment guards against cannot arise,
and omitting the operator removed the only comparison that would have worked.

What the wrapper now carries: `empty()`, and `operator==` / `operator!=`
against its own type, against `std::string` and against `const char*`, in both
operand orders. Both orders because the generated code targets C++17, where a
reversed candidate is not synthesised; and the `const char*` pair because it is
what answers the original worry — with it, `cell == "x"` is an exact match
rather than a choice between converting the literal to `std::string` and
converting it to the cell type, which is the ambiguity that was the argument for
having none of these.

Verified by compiling and running, not by reading: a GoogleTest probe over
every shape in your table — `cell == expected`, `cell != DNCString`,
`DNCString == cell`, `cell == "T1"`, `"T1" == cell`, `cell == OtherCell("T1")`,
`cell.empty()`, `EXPECT_EQ(cell, expected)`, `EXPECT_EQ(cell, "T1")`,
`EXPECT_NE(cell, DNCString)`, your line `cell != DNCString && !cell.empty()`,
and the streaming and `dnc_equal` forms that already worked — compiles clean and
passes. Regenerating the C++ corpus changed exactly the eleven
`common/*_string.h` wrappers and no test or glue file.

Three notes on the rest of #11:

- **`std::stod(cell)` already compiles against a `...String`** — checked, both
  before and after this change, because a conversion to `const std::string&` is
  considered for an ordinary function parameter. Your ten `std::stod` sites and
  eleven `Money(cell)` sites were therefore `...Typed` values, where `.text` is
  the accessor. That distinction is the one your `tools/accessors.py` already
  makes, and it is the right way to drive the migration — thank you for writing
  down why the compiler log alone is not enough to do it.
- **The `.value` / `.text` split is staying.** Renaming either would break glue
  a second time. It belongs in the release notes, and will be there.
- **No `--cells-as-strings`.** With comparison restored, most of the 214 errors
  do not happen.

For what it is worth, C++ was the only one of the nine with this gap. The
TypeScript cell is a `type XString = string` and the JavaScript one is a plain
string; Go's is `type XString string`; C#'s is a readonly struct with implicit
conversions in both directions and `Equals` overridden, and C# — unlike C++
template deduction — does consider that conversion for `==`; Swift's is
`Equatable` and `ExpressibleByStringLiteral`; Rust's newtype has a `PartialEq`
impl and derefs to `str`; Java has no operators to overload and the field is
`value`. The C++ wrapper was the one place where naming the type in a signature
cost the behaviour of the text.

**#12 — `And`. Done.** You are right that it worked and was undocumented:
`normalizeKeyword` maps `and` to the keyword of the step above it and the step
pattern accepts it, which is why your converted scenarios generate
byte-identical tests. Section 4.6 of the syntax document now states it, with
the grammar production, and two rulings came with it:

- **A leading `And` — one with no `Given`, `When` or `Then` above it in its
  block — is now reported.** It used to become a `Given` in silence, which is a
  keyword the parser chose and the file never did:

  ```
  WARNING:8:'And' continues the step above it, and this is the first step of the
  Scenario -- it is read as a Given. Write the keyword you mean.
  ```

  It still generates, as a `Given`; nothing that works today stops working.
- **`But` is not supported, and will not be.** A comment in the parser listed
  it; the code never did, and the comment has been corrected. A line beginning
  with `But` is an unrecognized keyword — and, as your finding #10 points out,
  it ends the block it is in, so its table rows are then orphans:

  ```
  WARNING:19:Unrecognized keyword 'But'
  WARNING:20:Unexpected table row (no active block)
  ```

  `And` is the only continuation. The document says so now, because a reader
  coming from Gherkin will reach for `But`.

**#10, request 2 — a stray line is an error now, and ends nothing.** You asked
for one of two repairs, and this is the second: the line is an error rather than
a warning. Your repro, run here before the change, was three messages and a
rule that generated nothing while the converter exited 0. Now:

```
ERROR:10:'longest' is not a keyword -- a misspelled one, or a wrapped line that needs a leading '\' to carry on the comment above it. The line is dropped, and nothing it was part of ends here.
ERROR:11:'needs' is not a keyword -- a misspelled one, or a wrapped line that needs a leading '\' to carry on the comment above it. The line is dropped, and nothing it was part of ends here.
```

exit 1, and no files written. Your shape is in the `AnalyzeErrors` corpus now,
so it stays fixed.

The cause was ordering, not severity. `parseLines` closed an open named block
on *any* line it had not already handled, and only afterwards, having run out
of branches, said the first word was not a keyword -- so the block was gone
before anyone asked whether the line was a keyword at all. The question is
asked first now (`isKnownKeyword`), which is why the third of your three
warnings has disappeared: the `BusinessRule` keeps its `Examples:` table, so
Analyze no longer reports it as having none. Two messages, both about the two
lines that are actually wrong.

We did not take your other option -- reading the stray line as a continuation --
because it guesses. It would swallow a misspelled keyword into prose, and the
leading `\` exists precisely so that continuation is declared rather than
inferred. The error message names both causes for the same reason.

One more message changed while we were in there. A line whose first word *is* a
keyword, in a place where it cannot be used, used to be reported as
"Unrecognized keyword 'Given'", which is not true of `Given`. It now says which
of the two things it is: a step with no `Scenario`, `Background` or `Cleanup`
open for it, or a keyword with nothing after it.

**#8, first bullet — one heading, one message.** Your own case was already
fixed before you wrote, and the version you were on says which: the pairing
landed on 2026-10-11, so 0.9.1 gave you the warning and the error, and 0.9.2
gives one message. The quoted texts are the giveaway — *column 'Assumed here'
… will be ignored* is the 0.9.1 wording.

What was still two messages is a heading that is not the same letters: a typo.
Run here before the change, on an `Examples:` table whose attribute is
`AssumedHere`:

```
WARNING:16:Examples table has column 'Asumed Hree', which is not an attribute of 'Withdrawal' -- its values are ignored
ERROR:16:Examples table for 'Withdrawal' has no column 'AssumedHere', and 'AssumedHere' has no default value
```

and after it:

```
ERROR:16:Examples table for 'Withdrawal' has column 'Asumed Hree' where attribute 'AssumedHere' is expected -- a heading must match the attribute name exactly, so the column is ignored and 'AssumedHere' is left with no value
```

Both halves of the check now go through one function, where they were two
copies of it before -- identical, and unlikely to have stayed so. A heading
pairs with
an attribute when the two are the same word written differently — a space, a
capital, a hyphen — or when they are within a budget of single-character edits
that grows with the name: one for a short name, three for a name of eleven
letters. Beyond that the two messages stay, because then they really are two
problems, and so they do for the three cases where pairing would be a guess: a
heading equally close to two attributes names neither of them (`Addend3`, where
`Addend1` and `Addend2` are both unfilled); a heading differing from an
attribute only in its digits is a different attribute, since a number in a name
says which one and not how it is spelled (`Day2` is not `Day1` misspelled); and
a name of three letters or fewer must match exactly, because at that length one
edit reaches a word nobody meant (`Foe` is not `Fee`). Each of those keeps the
warning and the error it had, which say only what was seen.

Section 4.2.1 of the syntax document now states the rule the finding called
right but undocumented — what a heading must say, what happens to one that
names nothing, to an attribute with no column, and to the one mistake that is
both. Two cases went into the `AnalyzeErrors` corpus, which had none; with the
one added for #10 below, its count is 67 where you had 64.

Your other two bullets need nothing: the empty-`Scenario` message stays as it
is, and `--fail-every-test` stays the default.

**#9's paragraph break — documented, and not given a marker of its own.** A
line holding only `\` is the stand-in you found, and you are right that it was
folklore: it works, and nothing said so. Syntax 4.1.1 and the User Guide state
it now. The shapes were run before they were written down -- two in a row, one
straight after the keyword line, one as the last line of a comment, and one
between a step and its table -- and all of them are silent, with the table
still attached to its step.

We did not give it a marker of its own. It already reads as what it is, every
specification that has one keeps working, and a second marker is one more thing
to misspell. What 4.1.1 adds instead is what it is *not*: the pieces are still
joined with a single space, so the break is for a reader of the specification
and not something the comment's text carries. That is free today, because
nothing is generated from a named comment -- and if a rendered form of one is
ever wanted, this is the line that will have to mean something there. The
grammar now allows a continuation with no text, which it did not.

## And the one thing that was not a finding

The most-vexing-parse line — `T(cell);` inside a `try` declaring a variable
instead of calling a constructor, so every "this value is invalid" row passed
vacuously — is in the glue documentation now: *User Guide*, "Checking that a
value is refused", at the end of the section on how a column reaches the glue.
You are right that the generated stub shape invites it, which makes it ours to
warn about even though the generator had nothing to do with the bug.

Writing it from a run rather than from the rule turned up something we would
have got wrong. The parse is half of it. The other half is the step's no-throw
path: a refusal step whose `catch` is its whole assertion passes whatever
happens — including a value the production class accepts — and that is what lets
the parse hide. Put `ADD_FAILURE()` after the construction and the parse
mistake fails loudly the first time it runs. The section says both, in that
order, because the second one is the defect that outlives this particular typo.

The rest is what the three compilers to hand actually do: MSVC 19.44 `/W4`,
g++ 15.2 and clang++ 21.1 all compile `T(cell);` and all three report it
refusing nothing, while `T{cell};` and `const T parsed(cell);` throw. The
warning is in every one of them and an error in none (`-Wvexing-parse`,
`-Wparentheses`, C4457 — the last only at `/W4`, and only because the name
shadows a parameter), and the line compiles only when the type is
default-constructible: without that it fails with *no matching function for call
to `T::T()`*, an error about a no-argument constructor on a line that appears
to pass one.
