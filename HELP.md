# C Line Editor Help

The editor stores each document line in memory and accepts one command at the
`line-editor>` prompt. Commands are lowercase. Line numbers start at **1**.

## Commands

| Command | Description |
| --- | --- |
| `insert <line>` | Insert a line at the given position. The editor prompts for its text. In an empty document, use `insert 1`. |
| `delete <line>` | Delete the line at the given position. |
| `display` | Display every line with its line number. An empty document is reported clearly. |
| `save <file.txt>` | Save the current document as a text file. |
| `load <file.txt>` | Load a text file and replace the current document. |
| `search <text>` | Find text and display each matching line. Search is case-sensitive. |
| `replace <old text> \| <new text>` | Replace every non-overlapping occurrence of the old text. The `|` separates the old and new text. |
| `count` | Display the number of lines and whitespace-separated words. |
| `undo` | Undo the most recent insert, delete, replace, or load. Undo can be used repeatedly. |
| `help` | Show the command summary in the program. |
| `exit` or `quit` | Exit the editor. |

File names and search text can be wrapped in double quotes, for example
`save "competition notes.txt"` or `search "line editor"`. Replacement text is
trimmed around the `|` delimiter; quote either side if its meaningful text
begins or ends with spaces.

## Example session

```text
line-editor> insert 1
Text for line 1: Welcome to the line editor.
Inserted line 1.
line-editor> insert 2
Text for line 2: This is the second line.
Inserted line 2.
line-editor> display
   1 | Welcome to the line editor.
   2 | This is the second line.
line-editor> search second
   2 | This is the second line.
Found text in 1 line(s).
line-editor> replace second | final
Replaced 1 occurrence(s).
line-editor> save notes.txt
Saved 2 line(s) to "notes.txt".
line-editor> delete 1
Deleted line 1.
line-editor> undo
Undo complete.
line-editor> display
   1 | Welcome to the line editor.
   2 | This is the final line.
line-editor> exit
```

Line-number errors, malformed commands, empty search/replacement terms, and
file errors are reported without terminating the editor.
