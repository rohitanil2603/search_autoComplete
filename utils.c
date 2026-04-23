#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void trimNewline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

void trimOuterSpaces(char *str) {
    size_t start = 0;
    size_t len = strlen(str);

    while (start < len && str[start] == ' ') {
        start++;
    }

    while (len > start && str[len - 1] == ' ') {
        len--;
    }

    if (start > 0) {
        memmove(str, str + start, len - start);
    }
    str[len - start] = '\0';
}

static char toLowerAscii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

void normalizeText(const char *input, char *output, size_t outputSize) {
    size_t j = 0;
    size_t i = 0;
    int previousWasSpace = 1;

    if (outputSize == 0) {
        return;
    }

    while (input[i] != '\0' && j + 1 < outputSize) {
        char c = toLowerAscii(input[i]);

        if (c >= 'a' && c <= 'z') {
            output[j++] = c;
            previousWasSpace = 0;
        } else if (c == ' ') {
            if (!previousWasSpace) {
                output[j++] = ' ';
                previousWasSpace = 1;
            }
        }
        i++;
    }

    if (j > 0 && output[j - 1] == ' ') {
        j--;
    }
    output[j] = '\0';
}

int charToIndex(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a';
    }
    if (c == ' ') {
        return 26;
    }
    return -1;
}

char indexToChar(int index) {
    if (index >= 0 && index <= 25) {
        return (char)('a' + index);
    }
    if (index == 26) {
        return ' ';
    }
    return '\0';
}

void initSuggestionList(SuggestionList *list) {
    list->items = NULL;
    list->size = 0;
    list->capacity = 0;
}

void addSuggestion(SuggestionList *list, const char *word, int freq) {
    if (list->size == list->capacity) {
        size_t newCapacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        Suggestion *newItems = (Suggestion *)realloc(list->items, newCapacity * sizeof(Suggestion));
        if (newItems == NULL) {
            fprintf(stderr, "Memory allocation failed while collecting suggestions.\n");
            return;
        }
        list->items = newItems;
        list->capacity = newCapacity;
    }

    strncpy(list->items[list->size].word, word, MAX_WORD_LENGTH - 1);
    list->items[list->size].word[MAX_WORD_LENGTH - 1] = '\0';
    list->items[list->size].freq = freq;
    list->size++;
}

static int compareSuggestions(const void *a, const void *b) {
    const Suggestion *s1 = (const Suggestion *)a;
    const Suggestion *s2 = (const Suggestion *)b;

    if (s1->freq != s2->freq) {
        return s2->freq - s1->freq;
    }
    return strcmp(s1->word, s2->word);
}

void sortSuggestionsByFrequency(Suggestion *items, size_t count) {
    qsort(items, count, sizeof(Suggestion), compareSuggestions);
}

void freeSuggestionList(SuggestionList *list) {
    free(list->items);
    list->items = NULL;
    list->size = 0;
    list->capacity = 0;
}

int appendLineToFile(const char *filename, const char *text) {
    char normalized[MAX_WORD_LENGTH];
    FILE *file;

    if (filename == NULL || text == NULL) {
        return -1;
    }

    normalizeText(text, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return 0;
    }

    file = fopen(filename, "a");
    if (file == NULL) {
        return -1;
    }

    if (fprintf(file, "%s\n", normalized) < 0) {
        fclose(file);
        return -1;
    }

    if (fclose(file) != 0) {
        return -1;
    }

    return 0;
}

int removeOneLineFromFile(const char *filename, const char *text) {
    char normalized[MAX_WORD_LENGTH];
    char line[MAX_INPUT_LENGTH];
    char tmpPath[MAX_INPUT_LENGTH];
    FILE *inFile;
    FILE *outFile;
    int removed = 0;

    if (filename == NULL || text == NULL) {
        return -1;
    }

    normalizeText(text, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return 0;
    }

    inFile = fopen(filename, "r");
    if (inFile == NULL) {
        return -1;
    }

    snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", filename);
    outFile = fopen(tmpPath, "w");
    if (outFile == NULL) {
        fclose(inFile);
        return -1;
    }

    while (fgets(line, sizeof(line), inFile) != NULL) {
        trimNewline(line);
        trimOuterSpaces(line);
        if (!removed && strcmp(line, normalized) == 0) {
            removed = 1;
            continue;
        }
        if (line[0] == '\0') {
            continue;
        }
        if (fprintf(outFile, "%s\n", line) < 0) {
            fclose(inFile);
            fclose(outFile);
            remove(tmpPath);
            return -1;
        }
    }

    if (fclose(inFile) != 0 || fclose(outFile) != 0) {
        remove(tmpPath);
        return -1;
    }

    if (rename(tmpPath, filename) != 0) {
        remove(tmpPath);
        return -1;
    }

    return removed;
}
