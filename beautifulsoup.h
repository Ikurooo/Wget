#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <regex.h>


#include "stringlist.h"

// "\\b(?:[a-zA-Z0-9_-]+\\.(?:js|png|jpg|jpeg))\\b";     // Basic file name pattern
// "https?://[a-zA-Z0-9./?=_-]+";                       // Basic URL pattern

/**
 * Finds all occurrences of a pattern in a plain text string.
 * @param plainText the plain text
 * @param pattern the pattern you wish to find
 * @return a dynamic array of strings containing all matches; NULL if something went wrong
 */
stringList *extractPattern(char *plainText, const char* pattern) {
    stringList *matchingPatterns = createDynamicStringArray();

    if (matchingPatterns == NULL) {
        fprintf(stderr, "Error creating dynamic array\n");
        return NULL;
    }

    regex_t regex;
    regmatch_t match;

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Error compiling regex pattern\n");
        freeStringList(matchingPatterns);
        return NULL;
    }

    while (regexec(&regex, plainText, 1, &match, 0) == 0) {
        if (match.rm_so == -1) {
            break;
        }

        char* matchingPattern = strndup(plainText + match.rm_so, match.rm_eo - match.rm_so);

        if (matchingPattern == NULL) {
            fprintf(stderr, "Error duplicating string before adding it to the dynamic array.\n");
            freeStringList(matchingPatterns);
            return NULL;
        }

        if (pushUrl(matchingPatterns, matchingPattern) == -1) {
            fprintf(stderr, "Error in adding string to the dynamic array.\n");
            freeStringList(matchingPatterns);
            return NULL;
        }

        plainText += match.rm_eo;
        free(matchingPattern);
    }

    (void) regfree(&regex);

    return matchingPatterns;
}

/**
 * Converts a string to an integer
 * @param string
 * @return the parsed number in a u_long; -1 if failed
 */
u_long convertStringToUlong(char *string) {
    errno = 0;
    char *endptr;
    u_long ulongInteger = strtoul(string, &endptr, 10);

    if ((errno == ERANGE && ulongInteger == ULONG_MAX) ||
        endptr == string || *endptr != '\0') {
        return ULONG_MAX;
    }

    return ulongInteger;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
/**
 * Converts an unsigned long to a string
 * @param recursionLevelString
 * @return a "static" string containing the number; if failed returns NULL.
 * Note that some IDEs will complain that "the address of the local variable 'path' may escape the function".
 */
char *convertUlongIntegerToString(u_long longInteger) {
    // C rounds to zero
    long length = (long) log10((double_t) longInteger) + 1;
    char string[length + 1];
    if (snprintf(string, sizeof(string), "%lu", longInteger) == -1) {
        return NULL;
    }

    return string;
}
#pragma GCC diagnostic pop
