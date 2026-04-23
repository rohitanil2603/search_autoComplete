#ifndef TRIE_H
#define TRIE_H

#include "utils.h"

#define ALPHABET_SIZE 27

typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE];
    int isEnd;
    int frequency;
} TrieNode;

TrieNode *createTrieNode(void);
void freeTrie(TrieNode *root);

void insertWord(TrieNode *root, const char *word);
/* Deletes one occurrence of word if present; returns 1 if deleted, else 0. */
int deleteWord(TrieNode *root, const char *word);
int searchWord(TrieNode *root, const char *word);
int getFrequency(TrieNode *root, const char *word);
int autocomplete(TrieNode *root, const char *prefix, int topK);
/* Collects all completions under prefix into results (sorted by frequency). Returns count or -1 on bad args. Caller must initSuggestionList before first use; function replaces list contents. */
int trieCollectCompletions(TrieNode *root, const char *prefix, SuggestionList *results);
int loadFromFile(TrieNode *root, const char *filename);

#endif
