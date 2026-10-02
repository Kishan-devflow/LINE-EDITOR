#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LineNode {
    char *text;
    struct LineNode *next;
} LineNode;

typedef struct {
    LineNode *head;
    size_t count;
} Document;

typedef struct UndoNode {
    Document snapshot;
    struct UndoNode *next;
} UndoNode;

static void document_init(Document *document) {
    document->head = NULL;
    document->count = 0;
}

static void document_clear(Document *document) {
    LineNode *line = document->head;

    while (line != NULL) {
        LineNode *next = line->next;
        free(line->text);
        free(line);
        line = next;
    }

    document_init(document);
}

static bool document_append(Document *document, const char *text) {
    LineNode *new_line = malloc(sizeof(*new_line));
    LineNode **tail;

    if (new_line == NULL) {
        return false;
    }

    new_line->text = malloc(strlen(text) + 1);
    if (new_line->text == NULL) {
        free(new_line);
        return false;
    }
    strcpy(new_line->text, text);
    new_line->next = NULL;

    tail = &document->head;
    while (*tail != NULL) {
        tail = &(*tail)->next;
    }
    *tail = new_line;
    document->count++;
    return true;
}

static bool document_clone(const Document *source, Document *destination) {
    const LineNode *line;

    document_init(destination);
    for (line = source->head; line != NULL; line = line->next) {
        if (!document_append(destination, line->text)) {
            document_clear(destination);
            return false;
        }
    }
    return true;
}

static bool undo_push(UndoNode **undo_stack, const Document *document) {
    UndoNode *entry = malloc(sizeof(*entry));

    if (entry == NULL || !document_clone(document, &entry->snapshot)) {
        free(entry);
        return false;
    }

    entry->next = *undo_stack;
    *undo_stack = entry;
    return true;
}

static void undo_clear(UndoNode *undo_stack) {
    while (undo_stack != NULL) {
        UndoNode *next = undo_stack->next;
        document_clear(&undo_stack->snapshot);
        free(undo_stack);
        undo_stack = next;
    }
}

static char *read_line(FILE *stream, bool *read_error) {
    size_t capacity = 128;
    size_t length = 0;
    char *buffer = malloc(capacity);
    int character;

    if (buffer == NULL) {
        *read_error = true;
        return NULL;
    }

    while ((character = fgetc(stream)) != EOF && character != '\n') {
        if (length + 1 >= capacity) {
            if (capacity > SIZE_MAX / 2) {
                free(buffer);
                *read_error = true;
                return NULL;
            }
            size_t new_capacity = capacity * 2;
            char *resized = realloc(buffer, new_capacity);
            if (resized == NULL) {
                free(buffer);
                *read_error = true;
                return NULL;
            }
            buffer = resized;
            capacity = new_capacity;
        }
        buffer[length++] = (char)character;
    }

    if (character == EOF && ferror(stream)) {
        free(buffer);
        *read_error = true;
        return NULL;
    }
    if (character == EOF && length == 0) {
        free(buffer);
        return NULL;
    }
    if (length > 0 && buffer[length - 1] == '\r') {
        length--;
    }
    buffer[length] = '\0';
    return buffer;
}

static void trim_whitespace(char *text) {
    char *start = text;
    size_t length;

    while (isspace((unsigned char)*start)) {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        text[--length] = '\0';
    }
}

static char *unquote(char *text) {
    size_t length = strlen(text);

    if (length >= 2 && text[0] == '"' && text[length - 1] == '"') {
        text[length - 1] = '\0';
        return text + 1;
    }
    return text;
}

static bool parse_line_number(const char *text, size_t *line_number) {
    char *end;
    unsigned long long parsed;

    while (isspace((unsigned char)*text)) {
        text++;
    }
    if (*text == '\0' || *text == '-') {
        return false;
    }

    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || end == text || parsed > SIZE_MAX) {
        return false;
    }
    while (isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return false;
    }

    *line_number = (size_t)parsed;
    return true;
}

static void display_document(const Document *document) {
    const LineNode *line = document->head;
    size_t line_number = 1;

    if (line == NULL) {
        puts("Document is empty.");
        return;
    }

    while (line != NULL) {
        printf("%4zu | %s\n", line_number++, line->text);
        line = line->next;
    }
}

static bool insert_line(Document *document, UndoNode **undo_stack,
                        size_t line_number, const char *text) {
    LineNode *new_line;
    LineNode **position;

    if (line_number == 0 || line_number > document->count + 1) {
        printf("Error: line number must be between 1 and %zu.\n",
               document->count + 1);
        return false;
    }

    new_line = malloc(sizeof(*new_line));
    if (new_line == NULL) {
        puts("Error: not enough memory to insert the line.");
        return false;
    }
    new_line->text = malloc(strlen(text) + 1);
    if (new_line->text == NULL) {
        free(new_line);
        puts("Error: not enough memory to insert the line.");
        return false;
    }
    strcpy(new_line->text, text);

    if (!undo_push(undo_stack, document)) {
        free(new_line->text);
        free(new_line);
        puts("Error: unable to save the undo snapshot.");
        return false;
    }

    position = &document->head;
    for (size_t current = 1; current < line_number; current++) {
        position = &(*position)->next;
    }
    new_line->next = *position;
    *position = new_line;
    document->count++;
    printf("Inserted line %zu.\n", line_number);
    return true;
}

static bool delete_line(Document *document, UndoNode **undo_stack,
                        size_t line_number) {
    LineNode **position = &document->head;
    LineNode *removed;

    if (line_number == 0 || line_number > document->count) {
        printf("Error: line number must be between 1 and %zu.\n",
               document->count);
        return false;
    }
    if (!undo_push(undo_stack, document)) {
        puts("Error: unable to save the undo snapshot.");
        return false;
    }

    for (size_t current = 1; current < line_number; current++) {
        position = &(*position)->next;
    }
    removed = *position;
    *position = removed->next;
    free(removed->text);
    free(removed);
    document->count--;
    printf("Deleted line %zu.\n", line_number);
    return true;
}

static void search_document(const Document *document, const char *query) {
    const LineNode *line = document->head;
    size_t line_number = 1;
    size_t matches = 0;

    if (*query == '\0') {
        puts("Error: search text cannot be empty.");
        return;
    }

    while (line != NULL) {
        if (strstr(line->text, query) != NULL) {
            printf("%4zu | %s\n", line_number, line->text);
            matches++;
        }
        line = line->next;
        line_number++;
    }
    if (matches == 0) {
        printf("No lines contain \"%s\".\n", query);
    } else {
        printf("Found text in %zu line(s).\n", matches);
    }
}

static bool replace_in_line(char **line_text, const char *old_text,
                            const char *new_text, size_t *replacements) {
    const size_t old_length = strlen(old_text);
    const size_t new_length = strlen(new_text);
    const char *cursor = *line_text;
    size_t output_length = 0;
    size_t count = 0;
    char *match;
    char *result;
    char *output;

    while ((match = strstr(cursor, old_text)) != NULL) {
        size_t prefix_length = (size_t)(match - cursor);
        if (output_length > SIZE_MAX - prefix_length ||
            output_length + prefix_length > SIZE_MAX - new_length) {
            return false;
        }
        output_length += prefix_length + new_length;
        count++;
        cursor = match + old_length;
    }
    if (output_length > SIZE_MAX - strlen(cursor)) {
        return false;
    }
    output_length += strlen(cursor);
    if (count == 0) {
        return true;
    }
    if (output_length == SIZE_MAX) {
        return false;
    }

    result = malloc(output_length + 1);
    if (result == NULL) {
        return false;
    }
    cursor = *line_text;
    output = result;
    while ((match = strstr(cursor, old_text)) != NULL) {
        size_t prefix_length = (size_t)(match - cursor);
        memcpy(output, cursor, prefix_length);
        output += prefix_length;
        memcpy(output, new_text, new_length);
        output += new_length;
        cursor = match + old_length;
    }
    strcpy(output, cursor);

    free(*line_text);
    *line_text = result;
    *replacements += count;
    return true;
}

static void replace_text(Document *document, UndoNode **undo_stack,
                         const char *old_text, const char *new_text) {
    Document updated;
    LineNode *line;
    size_t replacements = 0;

    if (*old_text == '\0') {
        puts("Error: text to find cannot be empty.");
        return;
    }
    if (!document_clone(document, &updated)) {
        puts("Error: not enough memory to prepare replacement.");
        return;
    }

    for (line = updated.head; line != NULL; line = line->next) {
        if (!replace_in_line(&line->text, old_text, new_text, &replacements)) {
            document_clear(&updated);
            puts("Error: replacement could not be completed.");
            return;
        }
    }
    if (replacements == 0) {
        document_clear(&updated);
        printf("No occurrences of \"%s\" were found.\n", old_text);
        return;
    }
    if (!undo_push(undo_stack, document)) {
        document_clear(&updated);
        puts("Error: unable to save the undo snapshot.");
        return;
    }

    document_clear(document);
    *document = updated;
    printf("Replaced %zu occurrence(s).\n", replacements);
}

static size_t count_words(const char *text) {
    size_t words = 0;
    bool inside_word = false;

    while (*text != '\0') {
        if (isspace((unsigned char)*text)) {
            inside_word = false;
        } else if (!inside_word) {
            words++;
            inside_word = true;
        }
        text++;
    }
    return words;
}

static void count_document(const Document *document) {
    const LineNode *line = document->head;
    size_t words = 0;

    while (line != NULL) {
        words += count_words(line->text);
        line = line->next;
    }
    printf("Lines: %zu\nWords: %zu\n", document->count, words);
}

static bool save_document(const Document *document, const char *filename) {
    const LineNode *line;
    FILE *file = fopen(filename, "w");
    bool success = true;

    if (file == NULL) {
        perror("Error opening file for saving");
        return false;
    }
    for (line = document->head; line != NULL; line = line->next) {
        if (fputs(line->text, file) == EOF || fputc('\n', file) == EOF) {
            success = false;
            break;
        }
    }
    if (fclose(file) == EOF) {
        success = false;
    }
    if (!success) {
        perror("Error writing file");
        return false;
    }
    printf("Saved %zu line(s) to \"%s\".\n", document->count, filename);
    return true;
}

static bool load_document(Document *document, UndoNode **undo_stack,
                          const char *filename) {
    Document loaded;
    FILE *file = fopen(filename, "r");
    bool read_error = false;
    char *text;

    if (file == NULL) {
        perror("Error opening file for loading");
        return false;
    }
    document_init(&loaded);
    while ((text = read_line(file, &read_error)) != NULL) {
        if (!document_append(&loaded, text)) {
            free(text);
            document_clear(&loaded);
            fclose(file);
            puts("Error: not enough memory to load the file.");
            return false;
        }
        free(text);
    }

    if (fclose(file) == EOF) {
        read_error = true;
    }
    if (read_error) {
        document_clear(&loaded);
        puts("Error: the file could not be read completely.");
        return false;
    }
    if (!undo_push(undo_stack, document)) {
        document_clear(&loaded);
        puts("Error: unable to save the undo snapshot.");
        return false;
    }

    document_clear(document);
    *document = loaded;
    printf("Loaded %zu line(s) from \"%s\".\n", document->count, filename);
    return true;
}

static void undo_last_change(Document *document, UndoNode **undo_stack) {
    UndoNode *entry = *undo_stack;

    if (entry == NULL) {
        puts("Nothing to undo.");
        return;
    }
    *undo_stack = entry->next;
    document_clear(document);
    *document = entry->snapshot;
    free(entry);
    puts("Undo complete.");
}

static void print_help(void) {
    puts("Commands:");
    puts("  insert <line>       Insert a new line at this position (then enter text).");
    puts("  delete <line>       Delete the line at this position.");
    puts("  display             Display all lines with line numbers.");
    puts("  save <file.txt>     Save the document to a text file.");
    puts("  load <file.txt>     Replace the document with a text file.");
    puts("  search <text>       Find text and display matching lines.");
    puts("  replace <old> | <new>");
    puts("                      Replace every occurrence; spaces are allowed.");
    puts("  count               Show the number of lines and words.");
    puts("  undo                Undo the most recent change or load.");
    puts("  help                Show this command list.");
    puts("  exit                Quit the editor.");
    puts("");
    puts("Examples:");
    puts("  insert 1");
    puts("  search \"hello world\"");
    puts("  replace old text | new text");
    puts("  save notes.txt");
}

static void process_command(char *input, Document *document,
                            UndoNode **undo_stack, bool *should_exit) {
    char *command = input;
    char *arguments;

    trim_whitespace(command);
    if (*command == '\0') {
        return;
    }

    arguments = command;
    while (*arguments != '\0' && !isspace((unsigned char)*arguments)) {
        arguments++;
    }
    if (*arguments != '\0') {
        *arguments++ = '\0';
        while (isspace((unsigned char)*arguments)) {
            arguments++;
        }
    }

    if (strcmp(command, "insert") == 0) {
        size_t line_number;
        bool read_error = false;
        char *text;

        if (!parse_line_number(arguments, &line_number)) {
            puts("Error: use insert <positive-line-number>.");
            return;
        }
        if (line_number == 0 || line_number > document->count + 1) {
            printf("Error: line number must be between 1 and %zu.\n",
                   document->count + 1);
            return;
        }
        printf("Text for line %zu: ", line_number);
        fflush(stdout);
        text = read_line(stdin, &read_error);
        if (text == NULL) {
            puts(read_error ? "Error: unable to read line text."
                            : "Input ended; insert cancelled.");
            return;
        }
        insert_line(document, undo_stack, line_number, text);
        free(text);
    } else if (strcmp(command, "delete") == 0) {
        size_t line_number;
        if (!parse_line_number(arguments, &line_number)) {
            puts("Error: use delete <positive-line-number>.");
            return;
        }
        delete_line(document, undo_stack, line_number);
    } else if (strcmp(command, "display") == 0) {
        display_document(document);
    } else if (strcmp(command, "save") == 0) {
        trim_whitespace(arguments);
        arguments = unquote(arguments);
        if (*arguments == '\0') {
            puts("Error: use save <file-name>.");
            return;
        }
        save_document(document, arguments);
    } else if (strcmp(command, "load") == 0) {
        trim_whitespace(arguments);
        arguments = unquote(arguments);
        if (*arguments == '\0') {
            puts("Error: use load <file-name>.");
            return;
        }
        load_document(document, undo_stack, arguments);
    } else if (strcmp(command, "search") == 0) {
        trim_whitespace(arguments);
        search_document(document, unquote(arguments));
    } else if (strcmp(command, "replace") == 0) {
        char *separator = strchr(arguments, '|');
        if (separator == NULL) {
            puts("Error: use replace <old text> | <new text>.");
            return;
        }
        *separator++ = '\0';
        trim_whitespace(arguments);
        trim_whitespace(separator);
        replace_text(document, undo_stack, unquote(arguments),
                     unquote(separator));
    } else if (strcmp(command, "count") == 0) {
        count_document(document);
    } else if (strcmp(command, "undo") == 0) {
        undo_last_change(document, undo_stack);
    } else if (strcmp(command, "help") == 0) {
        print_help();
    } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        *should_exit = true;
    } else {
        printf("Error: unknown command \"%s\". Type help for commands.\n",
               command);
    }
}

int main(void) {
    Document document;
    UndoNode *undo_stack = NULL;
    bool should_exit = false;

    document_init(&document);
    puts("C Line Editor - type help to see available commands.");
    while (!should_exit) {
        bool read_error = false;
        char *input;

        printf("line-editor> ");
        fflush(stdout);
        input = read_line(stdin, &read_error);
        if (input == NULL) {
            if (read_error) {
                puts("\nError: unable to read command input.");
            } else {
                putchar('\n');
            }
            break;
        }
        process_command(input, &document, &undo_stack, &should_exit);
        free(input);
    }

    document_clear(&document);
    undo_clear(undo_stack);
    return 0;
}
