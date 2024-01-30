#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <regex.h>

#include "stringlist.h"

// "[a-zA-Z0-9_-]+\.(js|png|jpg|jpeg|css)";     // Basic file name pattern
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
 * @return the parsed number as a long integer; LONG_MAX and errno ERANGE if failed
 */
long convertStringToLong(char *string) {
    errno = 0;
    char *endptr;
    long longInteger = strtol(string, &endptr, 10);

    if ((errno == ERANGE && (longInteger == LONG_MAX || longInteger == LONG_MIN)) ||
        endptr == string || *endptr != '\0') {
        return LONG_MAX;
    }

    return longInteger;
}
