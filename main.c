#include <stdio.h>
#include <string.h>

#include "trie.h"
#include "utils.h"

#define DEFAULT_TOP_K 5
/* One phrase per line; loaded at startup, appended on each insert. */
#define DEFAULT_VOCAB_FILE "autofill_words.txt"

static void printHelp(void) {
    printf("\nAvailable commands:\n");
    printf("  insert <word or sentence>  (saved to %s)\n", DEFAULT_VOCAB_FILE);
    printf("  delete <word or sentence>\n");
    printf("  search <word or sentence>\n");
    printf("  autocomplete <prefix>\n");
    printf("  load <filename>\n");
    printf("  exit\n\n");
    printf("Vocabulary is restored from %s when the program starts.\n\n", DEFAULT_VOCAB_FILE);
}

int main(void) {
    TrieNode *root = createTrieNode();
    char line[MAX_INPUT_LENGTH];

    if (root == NULL) {
        fprintf(stderr, "Failed to initialize trie.\n");
        return 1;
    }

    printf("========================================\n");
    printf("  Terminal Autocomplete Search Engine\n");
    printf("  (case-insensitive, supports spaces)\n");
    printf("========================================\n");
    {
        int restored = loadFromFile(root, DEFAULT_VOCAB_FILE);
        if (restored > 0) {
            printf("Restored %d %s from %s\n\n", restored, restored == 1 ? "entry" : "entries", DEFAULT_VOCAB_FILE);
        }
    }
    printHelp();

    while (1) {
        char command[32];
        char argument[MAX_INPUT_LENGTH];
        char *argStart;

        printf("> ");
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\nExiting...\n");
            break;
        }

        trimNewline(line);
        trimOuterSpaces(line);
        if (line[0] == '\0') {
            continue;
        }

        if (sscanf(line, "%31s", command) != 1) {
            continue;
        }

        argStart = line + strlen(command);
        while (*argStart == ' ') {
            argStart++;
        }

        strncpy(argument, argStart, sizeof(argument) - 1);
        argument[sizeof(argument) - 1] = '\0';
        trimOuterSpaces(argument);

        if (strcmp(command, "exit") == 0) {
            printf("Goodbye!\n");
            break;
        } else if (strcmp(command, "insert") == 0) {
            char normalizedCheck[MAX_WORD_LENGTH];
            if (argument[0] == '\0') {
                printf("Usage: insert <word or sentence>\n");
                continue;
            }
            normalizeText(argument, normalizedCheck, sizeof(normalizedCheck));
            if (normalizedCheck[0] == '\0') {
                printf("Nothing to insert (no letters or spaces after normalization).\n");
                continue;
            }
            insertWord(root, argument);
            if (appendLineToFile(DEFAULT_VOCAB_FILE, argument) != 0) {
                fprintf(stderr, "Warning: could not append to %s (disk full or permissions?).\n", DEFAULT_VOCAB_FILE);
            }
            printf("Inserted: \"%s\"\n", argument);
        } else if (strcmp(command, "search") == 0) {
            int found;
            int freq;
            if (argument[0] == '\0') {
                printf("Usage: search <word or sentence>\n");
                continue;
            }
            found = searchWord(root, argument);
            if (found) {
                freq = getFrequency(root, argument);
                printf("Found: \"%s\" (frequency: %d)\n", argument, freq);
            } else {
                printf("Not found: \"%s\"\n", argument);
            }
        } else if (strcmp(command, "delete") == 0) {
            int deleted;
            if (argument[0] == '\0') {
                printf("Usage: delete <word or sentence>\n");
                continue;
            }
            deleted = deleteWord(root, argument);
            if (deleted) {
                printf("Deleted one occurrence of: \"%s\"\n", argument);
            } else {
                printf("Cannot delete, not found: \"%s\"\n", argument);
            }
        } else if (strcmp(command, "autocomplete") == 0) {
            if (argument[0] == '\0') {
                printf("Usage: autocomplete <prefix>\n");
                continue;
            }
            autocomplete(root, argument, DEFAULT_TOP_K);
        } else if (strcmp(command, "load") == 0) {
            int loaded;
            if (argument[0] == '\0') {
                printf("Usage: load <filename>\n");
                continue;
            }
            loaded = loadFromFile(root, argument);
            if (loaded < 0) {
                printf("Could not open file: %s\n", argument);
            } else {
                printf("Loaded %d entries from %s\n", loaded, argument);
            }
        } else if (strcmp(command, "help") == 0) {
            printHelp();
        } else {
            printf("Unknown command: %s\n", command);
            printHelp();
        }
    }

    freeTrie(root);
    return 0;
}
