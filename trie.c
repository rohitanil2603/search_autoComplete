#include "trie.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TrieNode *createTrieNode(void) {
    TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
    int i;

    if (node == NULL) {
        return NULL;
    }

    for (i = 0; i < ALPHABET_SIZE; i++) {
        node->children[i] = NULL;
    }
    node->isEnd = 0;
    node->frequency = 0;
    return node;
}

void freeTrie(TrieNode *root) {
    int i;
    if (root == NULL) {
        return;
    }

    for (i = 0; i < ALPHABET_SIZE; i++) {
        freeTrie(root->children[i]);
    }
    free(root);
}

void insertWord(TrieNode *root, const char *word) {
    char normalized[MAX_WORD_LENGTH];
    size_t i;
    TrieNode *current = root;

    if (root == NULL || word == NULL) {
        return;
    }

    normalizeText(word, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return;
    }

    for (i = 0; normalized[i] != '\0'; i++) {
        int index = charToIndex(normalized[i]);
        if (index < 0) {
            continue;
        }
        if (current->children[index] == NULL) {
            current->children[index] = createTrieNode();
            if (current->children[index] == NULL) {
                fprintf(stderr, "Memory allocation failed while inserting.\n");
                return;
            }
        }
        current = current->children[index];
    }

    current->isEnd = 1;
    current->frequency++;
}

static TrieNode *traverseToNode(TrieNode *root, const char *text) {
    TrieNode *current = root;
    size_t i;

    for (i = 0; text[i] != '\0'; i++) {
        int index = charToIndex(text[i]);
        if (index < 0 || current->children[index] == NULL) {
            return NULL;
        }
        current = current->children[index];
    }
    return current;
}

static int hasChildren(const TrieNode *node) {
    int i;
    if (node == NULL) {
        return 0;
    }
    for (i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i] != NULL) {
            return 1;
        }
    }
    return 0;
}

static int deleteWordRecursive(TrieNode *node, const char *word, size_t depth, int *deleted) {
    int index;
    TrieNode *child;
    int shouldDeleteChild;

    if (node == NULL || word == NULL || deleted == NULL) {
        return 0;
    }

    if (word[depth] == '\0') {
        if (!node->isEnd) {
            return 0;
        }

        *deleted = 1;
        if (node->frequency > 1) {
            node->frequency--;
            return 0;
        }

        node->isEnd = 0;
        node->frequency = 0;
        return !hasChildren(node);
    }

    index = charToIndex(word[depth]);
    if (index < 0) {
        return 0;
    }

    child = node->children[index];
    if (child == NULL) {
        return 0;
    }

    shouldDeleteChild = deleteWordRecursive(child, word, depth + 1, deleted);
    if (shouldDeleteChild) {
        free(child);
        node->children[index] = NULL;
    }

    if (!(*deleted)) {
        return 0;
    }

    if (node->isEnd) {
        return 0;
    }

    return !hasChildren(node);
}

int deleteWord(TrieNode *root, const char *word) {
    char normalized[MAX_WORD_LENGTH];
    int deleted = 0;

    if (root == NULL || word == NULL) {
        return 0;
    }

    normalizeText(word, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return 0;
    }

    (void)deleteWordRecursive(root, normalized, 0, &deleted);
    return deleted;
}

int searchWord(TrieNode *root, const char *word) {
    char normalized[MAX_WORD_LENGTH];
    TrieNode *node;

    if (root == NULL || word == NULL) {
        return 0;
    }

    normalizeText(word, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return 0;
    }

    node = traverseToNode(root, normalized);
    return (node != NULL && node->isEnd);
}

int getFrequency(TrieNode *root, const char *word) {
    char normalized[MAX_WORD_LENGTH];
    TrieNode *node;

    if (root == NULL || word == NULL) {
        return 0;
    }

    normalizeText(word, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return 0;
    }

    node = traverseToNode(root, normalized);
    if (node != NULL && node->isEnd) {
        return node->frequency;
    }
    return 0;
}

static void dfsCollect(TrieNode *node, char *currentWord, size_t depth, SuggestionList *results) {
    int i;
    if (node == NULL) {
        return;
    }

    if (node->isEnd) {
        addSuggestion(results, currentWord, node->frequency);
    }

    if (depth + 1 >= MAX_WORD_LENGTH) {
        return;
    }

    for (i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i] != NULL) {
            currentWord[depth] = indexToChar(i);
            currentWord[depth + 1] = '\0';
            dfsCollect(node->children[i], currentWord, depth + 1, results);
        }
    }
}

int trieCollectCompletions(TrieNode *root, const char *prefix, SuggestionList *results) {
    char normalizedPrefix[MAX_WORD_LENGTH];
    char workingWord[MAX_WORD_LENGTH];
    TrieNode *prefixNode;

    if (root == NULL || prefix == NULL || results == NULL) {
        return -1;
    }

    freeSuggestionList(results);
    initSuggestionList(results);

    normalizeText(prefix, normalizedPrefix, sizeof(normalizedPrefix));
    if (normalizedPrefix[0] == '\0') {
        return 0;
    }

    prefixNode = traverseToNode(root, normalizedPrefix);
    if (prefixNode == NULL) {
        return 0;
    }

    strncpy(workingWord, normalizedPrefix, sizeof(workingWord) - 1);
    workingWord[sizeof(workingWord) - 1] = '\0';

    dfsCollect(prefixNode, workingWord, strlen(workingWord), results);

    if (results->size == 0) {
        return 0;
    }

    sortSuggestionsByFrequency(results->items, results->size);
    return (int)results->size;
}

int autocomplete(TrieNode *root, const char *prefix, int topK) {
    char normalizedPrefix[MAX_WORD_LENGTH];
    SuggestionList results;
    size_t printCount;
    size_t i;
    int n;

    if (root == NULL || prefix == NULL || topK <= 0) {
        return 0;
    }

    normalizeText(prefix, normalizedPrefix, sizeof(normalizedPrefix));
    if (normalizedPrefix[0] == '\0') {
        printf("Please provide a valid prefix.\n");
        return 0;
    }

    initSuggestionList(&results);
    n = trieCollectCompletions(root, prefix, &results);
    if (n <= 0) {
        printf("No suggestions found for \"%s\".\n", normalizedPrefix);
        freeSuggestionList(&results);
        return 0;
    }

    printCount = (results.size < (size_t)topK) ? results.size : (size_t)topK;

    printf("Suggestions for \"%s\":\n", normalizedPrefix);
    for (i = 0; i < printCount; i++) {
        printf("%zu. %s (%d)\n", i + 1, results.items[i].word, results.items[i].freq);
    }

    freeSuggestionList(&results);
    return (int)printCount;
}

int loadFromFile(TrieNode *root, const char *filename) {
    FILE *file;
    char line[MAX_INPUT_LENGTH];
    int insertedCount = 0;

    if (root == NULL || filename == NULL) {
        return -1;
    }

    file = fopen(filename, "r");
    if (file == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        trimNewline(line);
        trimOuterSpaces(line);
        if (line[0] == '\0') {
            continue;
        }
        insertWord(root, line);
        insertedCount++;
    }

    fclose(file);
    return insertedCount;
}
