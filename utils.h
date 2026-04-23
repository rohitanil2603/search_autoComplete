#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#define MAX_INPUT_LENGTH 512
#define MAX_WORD_LENGTH 256

typedef struct {
    char word[MAX_WORD_LENGTH];
    int freq;
} Suggestion;

typedef struct {
    Suggestion *items;
    size_t size;
    size_t capacity;
} SuggestionList;

void trimNewline(char *str);
void trimOuterSpaces(char *str);
void normalizeText(const char *input, char *output, size_t outputSize);
int charToIndex(char c);
char indexToChar(int index);

void initSuggestionList(SuggestionList *list);
void addSuggestion(SuggestionList *list, const char *word, int freq);
void sortSuggestionsByFrequency(Suggestion *items, size_t count);
void freeSuggestionList(SuggestionList *list);

/* Append one normalized line to a text file (creates file if missing). Returns 0 on success, -1 on error. */
int appendLineToFile(const char *filename, const char *text);
/* Remove one normalized matching line from a text file. Returns 1 if removed, 0 if not found, -1 on error. */
int removeOneLineFromFile(const char *filename, const char *text);

#endif
