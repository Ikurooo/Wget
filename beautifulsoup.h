#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <regex.h>


#include "stringlist.h"

typedef struct {
    char *file;
    char *host;
    int success;
} URI;

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

char *prepareArguments(void) {
    return NULL;
}


/**
 * Converts a string to an integer - extracts the recursion level
 * @param recursionLevelString
 * @return the recursion level on success else ERROR_RECURSION
 */
long parseStringToLong(char *string) {
    errno = 0;
    char *endptr;
    long longInteger = strtol(string, &endptr, 10);

    if ((errno == ERANGE && (longInteger == LONG_MAX || longInteger == LONG_MIN)) ||
        endptr == string || *endptr != '\0' || longInteger < 0) {
        return -1;
    }

    return longInteger;
}

/**
 * @brief Parses the the provided URL into a URI struct.
 * @param url the URL you would like to parse
 * @return the uri itself, if the conversion was successful the uri.success value will be 0
 */
URI parseUrl(const char *url) {

    URI uri = {
            .file = NULL,
            .host = NULL,
            .success = -1
    };

    int hostOffset = 0;

    if (strncasecmp(url, "http://", 7) == 0) {
        hostOffset = 7;
    }

    if (strncasecmp(url, "https://", 8) == 0) {
        hostOffset = 8;
    }

    if ((strlen(url) - hostOffset) == 0) {
        return uri;
    }

    char* s = strpbrk(url + hostOffset, ";/?:@=&");
    u_long fileLength = -1;

    if (s == NULL) {
        if (asprintf(&uri.file, "/") == -1) {
            return uri;
        }
        fileLength = 0;
    } else if (s[0] != '/') {
        if (asprintf(&uri.file, "/%s", s) == -1) {
            return uri;
        }
        fileLength = strlen(uri.file);
    } else {
        if (asprintf(&uri.file, "%s", s) == -1) {
            return uri;
        }
        fileLength = strlen(uri.file);
    }

    // TODO: get rid of the casts for portability and stability
    if (asprintf(&uri.host, "%.*s", (int)(strlen(url) - hostOffset - fileLength), (url + hostOffset)) == -1) {
        free(uri.file);
        return uri;
    }

    if (strlen(uri.host) == 0) {
        free(uri.host);
        free(uri.file);
        return uri;
    }

    uri.success = 0;
    return uri;
}

/**
 * @brief Validates the provided file.
 * @param file the file you would like to validate
 * @return 0 if successful -1 otherwise
 */
int validateFile(char *file) {
    return (strspn(file, "/\\:*?\"<>|") != 0 || strlen(file) > 255) ? -1 : 0;
}

/**
 * @brief Validates the provided directory and if it is valid and does not yet exist it gets created.
 * @implnote This function mutates the original string if it is deemed a valid directory!
 * @param dir the directory you would like to validate
 * @return 0 if successful -1 otherwise
 */
int validateDir(char **dir, URI uri) {
    if (strpbrk(*dir, "/\\:*?\"<>|.") != NULL) {
        return -1;
    }

    struct stat st = {0};

    if (stat(*dir, &st) == -1) {
        mkdir(*dir, 0777);
    }

    char *tempDir = NULL;

    if (strncmp(uri.file, "/", 1) == 0) {
        asprintf(&tempDir, "%s/index.html", *dir);
    } else {
        asprintf(&tempDir, "%s%s", *dir, uri.file);
    }

    *dir = tempDir;

    return 0;
}
