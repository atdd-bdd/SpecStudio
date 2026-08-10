# Getting Started with AlignThree™

*(formerly named SpecStudio)*

Read this first. It gets you to a working specification without deciding
anything you would have to undo later.

The short version: **use a shared file system, take the Java configuration you
are given, and write a specification.** Everything else can wait, and none of it
changes what you write.

---

## 0. Installing: unblock the download first

If you downloaded the **zip**, unblock it *before* you extract it:

> Right-click `AlignThree-<version>-windows-x64.zip` → **Properties** → tick
> **Unblock** at the bottom → **OK**. Then extract.

Windows tags every file you download from the internet, and the tag is copied to
everything extracted from a zip. Miss this and the first run of `AlignThree.exe`
brings up a security warning; unblocking afterwards means doing it file by file.

The **installer** needs no unblocking. It will ask for administrator permission,
which is normal — it installs into `Program Files`.

Either way you may see a **"Windows protected your PC"** panel from SmartScreen.
AlignThree is signed by *Ken Pugh, Inc.*, and clicking **More info** shows that
publisher; **Run anyway** then proceeds. SmartScreen shows this until enough
people have installed a given release, so it fades with each version rather than
indicating anything is wrong. Check the publisher before you click through — on
this or on anything else.

---

## 1. Sharing: pick the shared file system

The first thing AlignThree asks is how your team will share the work — a
**shared file system** or **GitHub**.

**Choose the shared file system**, unless you already use git comfortably.

A shared folder is somewhere you and your team can all reach: a network drive,
OneDrive, Dropbox, a shared folder on a server. AlignThree makes no git calls at
all in this mode. Nothing to install, no account, no sign-in, nothing to go
wrong while you are trying to learn the tool.

**You can switch to git later.** Sharing is a property of the solution, not of
your specifications, and changing it does not alter a single line of what you
have written. Nobody has to start again. When you are ready — or when someone on
your team who knows git offers — see `Git Setup.md`.

If you already use git daily and would rather start there, that is fine too.
Just do not let it be the first thing you fight.

---

## 2. Configuration: take the Java default

Create a project and AlignThree writes a `Java.specconfig` for you: Java, JUnit,
tests into `src/test/java/spectable`, production classes into
`src/main/java/production`.

**Leave it alone to begin with.** You do not need to understand it to write a
specification, and it is one file to change later — not a decision baked into
your work.

This matters more than it sounds, and here is why.

---

## 3. A specification does not know what language you use

This is the point worth internalising early.

**There is nothing in a `.spectable` file that says Java, or C#, or Python.**
Not a hint. The same specification generates tests in any of the nine supported
languages — Java, C#, C++, Go, JavaScript, Python, Rust, Swift, TypeScript — and
the file itself is identical in every case.

```
Calculation Add_two_numbers
Description adds number1 to number2 giving result
Examples: Adder
| number1 | number2 | result |
| 2       | 3       | 5      |
| 10      | 20      | 30     |
| -1      | 1       | 0      |
```

That is the whole thing. It says what the software should do. It says nothing
about how, and nothing about what it is written in.

The language lives entirely in the `.specconfig` — one small JSON file, changed
in a moment, with no effect on your specifications.

So starting with the Java default costs you nothing even if your team writes
Python. The specifications you write today are exactly the ones you would have
written having thought about it for a week.

---

## 4. If you are the customer or business analyst

You are probably the person who most needs the specification and least wants to
configure a build. Good news: **you do not have to.**

Write specifications. Use the shared file system. Ignore the configuration file.

The other members of your triad — the developer and the tester — can, whenever
they are ready:

- set up git sharing, and connect it to your team's repository,
- replace or add a `.specconfig` for the language they actually build in,
- point the generated tests at their source tree.

None of that requires you to change anything you wrote, or to be present when
they do it. The specification is the part that needs your judgement; the rest is
plumbing, and it is their plumbing.

If you are the developer or tester reading this over their shoulder:
`Configuration Guide.md` has every setting, and `Git Setup.md` covers sharing.

---

## 5. Your first specification, end to end

1. **File → New Solution…** — choose **shared file system**, and pick a folder
   your team can reach.
2. **File → New Project…** — give it a name. You get `Java.specconfig`; leave it.
3. **File → New File…** — name it `Calculator.spectable`.
4. Type the `Calculation` above, with its `Attributes` block:

   ```
   Specification Calculator

   Calculation Add_two_numbers
   Description adds number1 to number2 giving result
   Examples: Adder
   | number1 | number2 | result |
   | 2       | 3       | 5      |
   | 10      | 20      | 30     |
   | -1      | 1       | 0      |

   Attributes Adder
   | Name    | Default | Datatype |
   | number1 | 0       | Integer  |
   | number2 | 0       | Integer  |
   | result  | 0       | Integer  |
   ```

5. **Right-click in the examples table → Run Examples…** — this checks your rows
   without generating or compiling anything. It is the fastest way to see whether
   what you wrote makes sense.
6. **Analyze → Solution** (`Shift+F7`) — reports anything undeclared or
   inconsistent across the whole solution.

That is a complete, useful loop, and you have installed no compiler and signed in
to nothing.

When a developer is ready to make the tests run, they use **Build**, implement
the generated glue methods, and the examples above become real tests. A freshly
generated test suite is deliberately **all red** — an unimplemented step must not
report success.

---

## What to read next

| | |
|---|---|
| `User Guide.md` | the IDE in full — editing, tables, navigation, analysis |
| `spectable syntax v3.3a.md` | every keyword, with examples |
| `Configuration Guide.md` | `.specconfig` settings, other languages, specs in a separate repository |
| `Git Setup.md` | moving to git sharing when you are ready |
| `README.md` | why AlignThree works this way, and where the format comes from |
