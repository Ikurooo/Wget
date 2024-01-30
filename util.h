#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#include "beautifulsoup.h"

#define BUFFER_SIZE (32)
#define EXIT_RECURSION (0)

typedef struct {
    char *file;
    char *host;
    int success;
} URI;

void freeUri(URI *uri) {
    if (uri->file != NULL) {
        free(uri->file);
    }

    if (uri->host != NULL) {
        free(uri->host);
    }
}

// TODO: clean up validateDir()
// TODO: add recursion

/**
 * @brief Print a usage message to stderr and exit the process with EXIT_FAILURE.
 * @param process The name of the current process.
 */
void usage(const char *process) {
    fprintf(stderr, "[%s] USAGE: %s [-p PORT] [-d DIRECTORY | -o OUTPUT FILE] DOMAIN\n", process, process);
    exit(EXIT_FAILURE);
}

/**
 * @brief Parses the port from a string into an integer.
 * @param portStr the port you would like to convert
 * @return 0 if successful -1 otherwise
 */
long parsePort(const char *portStr) {
    errno = 0;
    char *endptr;
    long port = strtol(portStr, &endptr, 10);

    if ((errno == ERANGE && (port == LONG_MAX || port == LONG_MIN)) || (errno != 0 && port == 0) ||
        endptr == portStr || *endptr != '\0' || port < 0 || port > 65535) {
        return -1;
    }

    return 0;
}

/**
 * Takes a standard response and extracts the header as a dynamically allocated string.
 * @param response
 * @return a dynamically allocated string containing the header
 */
char* extractHeader(char *response) {

    char* position = strstr(response, "\r\n\r\n");

    if (position == NULL) {
        return strdup("ERROR MISSING HEADER");
    }

    size_t length = position - response;
    char* result = (char*)malloc(length + 1);
    strncpy(result, response, length);
    result[length] = '\0';

    return result;

}

/**
 * Takes a standard response and extracts the content as a dynamically allocated string.
 * @param response
 * @return a dynamically allocated string containing the content
 */
char* extractContent(char *response) {
    char* position = strstr(response, "\r\n\r\n");

    if (position == NULL) {
        return strdup("ERROR MISSING CONTENT");
    }

    size_t length = strlen(response) - (position - response + strlen("\r\n\r\n"));
    char* result = (char*)malloc(length + 1);

    strcpy(result, position + strlen("\r\n\r\n"));

    return result;
}

/**
 * Receives a response from a server and packs all of it into a dynamically allocated string.
 * @param serverSocket the serverSocket
 * @return a dynamically allocated string with the entire response
 */
char* receiveResponse(int serverSocket) {
    char *request = malloc(BUFFER_SIZE);
    char buffer[BUFFER_SIZE];

    size_t bytesRead = recv(serverSocket, request, BUFFER_SIZE - 1, 0);
    size_t totalBytesRead = bytesRead;
    request[bytesRead] = '\0';

    while ((bytesRead = recv(serverSocket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        totalBytesRead += bytesRead;

        char *temp = realloc(request, totalBytesRead + 1);
        if (temp == NULL) {
            free(request);
            return strdup("FAILED TO RECEIVE CONTENT FROM SERVER");
        }

        request = temp;
        if (strcat(request, buffer) == NULL) {
            free(request);
            return strdup("CLIENT ERROR");
        }
    }

    return request;
}

/**
 * @brief Validates the response.
 * @param protocol the protocol
 * @param status the status
 * @return 0 if everything went successful
 */
int validateResponse(char *response) {

    char *responseCopy = strdup(response);

    fprintf(stderr, "%s\n", responseCopy);

    char *protocol = strtok(responseCopy, " ");
    char *status = strtok(NULL, " ");
    char *misc = strtok(NULL, "\r\n");

    if (protocol == NULL || status == NULL || misc == NULL) {
        fprintf(stderr, "ERROR parsing first line of client socket as file.\n");
        free(responseCopy);
        return 2;
    }

    if (strncmp(protocol, "HTTP/1.1", 8) != 0) {
        fprintf(stderr, "%s %s\n", status, misc);
        free(responseCopy);
        return 2;
    }

    // Check if status contains only numeric characters
    if (strspn(status, "0123456789") != strlen(status)) {
        fprintf(stderr, "%s %s\n", status, misc);
        free(responseCopy);
        return 2;
    }

    if (strncmp(status, "200", 3) != 0) {
        fprintf(stderr, "%s %s\n", status, misc);
        free(responseCopy);
        return 3;
    }

    free(responseCopy);
    return 0;
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
 * @param dir the directory you would like to validate
 * @return 0 if successful -1 otherwise
 */
int createDir(char *dir) {
    if (strpbrk(dir, "/\\:*?\"<>|.") != NULL) {
        return -1;
    }

    struct stat st = {0};

    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0777) == -1) {
            return -1;
        }
    }

    return 0;
}

/**
 * Concatenates a file to a directory string
 * @implnote this function assumes that the directory does not have a / or any other special character at the end
 * additionally it is assumed that the file name does not contain any special characters
 * @param fileName the name o the file
 * @param dirName the name of the EXISTING directory
 * @return a "static" string consisting of the concatenated file and directory.
 */
char *catFileNameToDir(char *fileName, char *dirName) {

    if (fileName == NULL || dirName == NULL) {
        return NULL;
    }

    size_t dirNameLen = strlen(dirName);
    size_t fileNameLen = strlen(fileName);

    // Calculate required size for path
    size_t pathSize = dirNameLen + fileNameLen + 2; // 1 for '/' and 1 for null terminator

    // Add extra space if fileName is just "/"
    if (fileNameLen == 1 && fileName[0] == '/') {
        pathSize += strlen("index.html");
    }

    char *path = malloc(pathSize * sizeof(char));

    if (path == NULL) {
        return NULL;
    }

    if (strcmp(fileName, "/") == 0) {
        snprintf(path, pathSize, "%s/index.html", dirName);
    } else if (strncmp(fileName, "/", 1) == 0) {
        snprintf(path, pathSize, "%s%s", dirName, fileName);
    } else {
        snprintf(path, pathSize, "%s/%s", dirName, fileName);
    }

    fprintf(stderr, "PATH:%s\n", path);

    return path;
}
