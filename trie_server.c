/*
 * Line protocol over stdin/stdout for the Node backend (one request per line).
 * Uses '|' as delimiter (normalized vocabulary does not contain '|').
 *
 * Client -> server:
 *   PING
 *   SEARCH|<query>
 *   AUTO|<topK>|<prefix>
 *   INSERT|<text>   (phrase; persisted to the same vocab file argv/env)
 *   DELETE|<text>   (deletes one occurrence from trie and vocab file)
 *
 * Server -> client:
 *   READY|<entries_loaded>
 *   PONG
 *   HIT|<frequency>   or   MISS
 *   ROW|<word>|<frequency>  (zero or more, sorted by frequency)
 *   END
 *   OK|<frequency>   (after INSERT — total frequency for that phrase)
 *   DELETED|<remaining_frequency>   or   MISS
 *   ERR|<message>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trie.h"
#include "utils.h"

#define DEFAULT_VOCAB_FILE "autofill_words.txt"

static const char *g_vocabPath;

static void sendErr(const char *msg) {
    printf("ERR|%s\n", msg);
    fflush(stdout);
}

static void handleSearch(TrieNode *root, const char *query) {
    int found;
    int freq;

    if (query[0] == '\0') {
        sendErr("empty query");
        return;
    }

    found = searchWord(root, query);
    if (found) {
        freq = getFrequency(root, query);
        printf("HIT|%d\n", freq);
    } else {
        printf("MISS\n");
    }
    fflush(stdout);
}

static void handleAuto(TrieNode *root, const char *spec) {
    int topK;
    const char *p;
    const char *prefix;
    SuggestionList results;
    size_t i;
    size_t limit;

    p = spec;
    topK = (int)strtol(p, (char **)&p, 10);
    if (topK <= 0 || p == spec || *p != '|') {
        sendErr("bad AUTO format");
        return;
    }
    prefix = p + 1;
    if (prefix[0] == '\0') {
        sendErr("empty prefix");
        return;
    }

    initSuggestionList(&results);
    if (trieCollectCompletions(root, prefix, &results) <= 0) {
        printf("END\n");
        fflush(stdout);
        freeSuggestionList(&results);
        return;
    }

    limit = (results.size < (size_t)topK) ? results.size : (size_t)topK;
    for (i = 0; i < limit; i++) {
        printf("ROW|%s|%d\n", results.items[i].word, results.items[i].freq);
    }
    printf("END\n");
    fflush(stdout);
    freeSuggestionList(&results);
}

static void handleInsert(TrieNode *root, const char *text) {
    char normalizedCheck[MAX_WORD_LENGTH];
    int freq;

    if (text == NULL || text[0] == '\0') {
        sendErr("empty text");
        return;
    }

    normalizeText(text, normalizedCheck, sizeof(normalizedCheck));
    if (normalizedCheck[0] == '\0') {
        sendErr("nothing to insert after normalization");
        return;
    }

    insertWord(root, text);
    if (appendLineToFile(g_vocabPath, text) != 0) {
        sendErr("could not append to vocabulary file");
        return;
    }

    freq = getFrequency(root, text);
    printf("OK|%d\n", freq);
    fflush(stdout);
}

static void handleDelete(TrieNode *root, const char *text) {
    char normalizedCheck[MAX_WORD_LENGTH];
    int deleted;
    int remainingFreq;
    int fileResult;

    if (text == NULL || text[0] == '\0') {
        sendErr("empty text");
        return;
    }

    normalizeText(text, normalizedCheck, sizeof(normalizedCheck));
    if (normalizedCheck[0] == '\0') {
        sendErr("nothing to delete after normalization");
        return;
    }

    deleted = deleteWord(root, text);
    if (!deleted) {
        printf("MISS\n");
        fflush(stdout);
        return;
    }

    fileResult = removeOneLineFromFile(g_vocabPath, text);
    if (fileResult < 0) {
        /* Keep trie and file consistent if file update fails. */
        insertWord(root, text);
        sendErr("could not update vocabulary file");
        return;
    }

    remainingFreq = getFrequency(root, text);
    printf("DELETED|%d\n", remainingFreq);
    fflush(stdout);
}

int main(int argc, char **argv) {
    TrieNode *root;
    const char *vocabPath;
    int loaded;
    char line[MAX_INPUT_LENGTH];

    vocabPath = getenv("TRIE_VOCAB");
    if (vocabPath == NULL || vocabPath[0] == '\0') {
        if (argc >= 2 && argv[1][0] != '\0') {
            vocabPath = argv[1];
        } else {
            vocabPath = DEFAULT_VOCAB_FILE;
        }
    }
    g_vocabPath = vocabPath;

    root = createTrieNode();
    if (root == NULL) {
        fprintf(stderr, "trie_server: failed to allocate trie root\n");
        return 1;
    }

    loaded = loadFromFile(root, vocabPath);
    if (loaded < 0) {
        fprintf(stderr, "trie_server: could not load vocabulary from %s\n", vocabPath);
    }

    printf("READY|%d\n", loaded >= 0 ? loaded : 0);
    fflush(stdout);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        trimNewline(line);
        if (line[0] == '\0') {
            continue;
        }

        if (strcmp(line, "PING") == 0) {
            printf("PONG\n");
            fflush(stdout);
        } else if (strncmp(line, "SEARCH|", 7) == 0) {
            handleSearch(root, line + 7);
        } else if (strncmp(line, "AUTO|", 5) == 0) {
            handleAuto(root, line + 5);
        } else if (strncmp(line, "INSERT|", 7) == 0) {
            handleInsert(root, line + 7);
        } else if (strncmp(line, "DELETE|", 7) == 0) {
            handleDelete(root, line + 7);
        } else if (strcmp(line, "EXIT") == 0) {
            break;
        } else {
            sendErr("unknown command");
        }
    }

    freeTrie(root);
    return 0;
}
