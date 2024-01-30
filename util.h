#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <limits.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <stdbool.h>
#include <math.h>

#include "beautifulsoup.h"

#define BUFFER_SIZE (32)
#define EXIT_RECURSION (0)
#define ERROR_RECURSION (-1)

typedef struct {
    char *file;
    char *host;
    int success;
} URI;

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

    return port;
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

    size_t length = strlen(position + strlen("\r\n\r\n"));
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

    size_t bytesRead = recv(serverSocket, request, sizeof(request) - 1, 0);
    size_t totalBytesRead = bytesRead;
    request[bytesRead] = '\0';

    while ((bytesRead = recv(serverSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        totalBytesRead += bytesRead;

        char *temp = realloc(request, totalBytesRead);
        if (temp == NULL) {
            free(request);
            return strdup("FAILED TO RECEIVE CONTENT FROM SERVER");
        }
        request = temp;
        strcat(request, buffer);
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
 * @return a "static" string consisting of the concatenated file and directory. Note that some IDEs will complain
 * that "the address of the local variable 'path' may escape the function".
 */
char *catFileNameToDir(char *fileName, char *dirName) {

    if (fileName == NULL || dirName == NULL) {
        return NULL;
    }

    size_t pathSize = strlen(dirName) + strlen(fileName) + 2; // 1 for '/' and 1 for null terminator

    if (pathSize <= 0) {
        return NULL;
    }

    char path[pathSize];

    if (strncmp(fileName, "/", 1) == 0) {
        snprintf(path, pathSize, "%s/index.html", dirName);
    } else {
        snprintf(path, pathSize, "%s%s", dirName, fileName);
    }

    return path;
}
