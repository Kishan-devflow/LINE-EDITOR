# C Programming Line Editor

A lightweight command-line text editor written in C for managing a document one
line at a time. It is designed to be easy to compile, demonstrate, and extend
for a 3rd Semester Coding Competition.

## Problem description

Basic text-editing operations are often performed on complete files or through
large graphical applications. This project demonstrates how a small C program
can keep text lines in memory and support common editing operations through a
simple command interface.

## Team members

- Add team member name(s) here.

## Data structure

The document is stored as a **singly linked list** of dynamically allocated
line nodes. Each node owns one line of text and points to the next node. This
structure supports insertion and deletion at a chosen position without shifting
an array of strings. The document also tracks its line count. Undo uses a stack
of linked-list snapshots, keeping each prior document state available.

## Features

- Insert a line at any valid position, including before or after existing lines.
- Delete a line by its 1-based line number.
- Display the document with clear line numbers.
- Save and load `.txt` files.
- Search case-sensitively and show matching line numbers.
- Replace all non-overlapping occurrences of text.
- Undo inserts, deletes, replacements, and file loads.
- Count document lines and whitespace-separated words.
- Handle invalid commands, line numbers, empty documents, and file errors
  without crashing.
- Accepts long input lines dynamically; file names and search terms may be
  quoted when they contain spaces.

## Commands and examples

| Command | Example |
| --- | --- |
| Insert | `insert 2`, then type the requested line |
| Delete | `delete 2` |
| Display | `display` |
| Save | `save notes.txt` |
| Load | `load notes.txt` |
| Search | `search "line editor"` |
| Replace | `replace old words | new words` |
| Count | `count` |
| Undo | `undo` |
| Help | `help` |
| Exit | `exit` |

See [HELP.md](HELP.md) for complete command descriptions and a sample session.

## Compile

With GCC:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic main.c -o line_editor
```

On Windows using MinGW GCC, create the Windows executable:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic main.c -o line_editor.exe
```

## Run

On Linux/macOS:

```sh
./line_editor
```

On Windows PowerShell:

```powershell
.\line_editor.exe
```

## Example input/output

```text
C Line Editor — type help to see available commands.
line-editor> insert 1
Text for line 1: First line
Inserted line 1.
line-editor> insert 2
Text for line 2: A line for the competition
Inserted line 2.
line-editor> display
   1 | First line
   2 | A line for the competition
line-editor> delete 1
Deleted line 1.
line-editor> display
   1 | A line for the competition
line-editor> save sample.txt
Saved 1 line(s) to "sample.txt".
line-editor> exit
```

## Project files

- `main.c` — editor implementation.
- `HELP.md` — detailed command help and example session.
- `README.md` — project overview, setup, and demonstration.
