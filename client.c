/**
 * @file client.c
 * @author Ivan Cankov 12219400 <e12219400@student.tuwien.ac.at>
 * @date 07.01.2024
 * @brief A simple HTTP client in C
 **/

#include "util.h"

#define BUFFER_SIZE 32

typedef struct {
    char *file;
    char *host;
    int success;
} URI;

/**
 * @brief Print a usage message to stderr and exit the process with EXIT_FAILURE.
 * @param process The name of the current process.
 */
void usage(const char *process) {
    fprintf(stderr, "[%s] USAGE: %s [-p PORT] [-d DIRECTORY | -o OUTPUT FILE] DOMAIN\n", process, process);
    exit(EXIT_FAILURE);
}

/**
 * @brief Parses the the provided URL into a URI struct.
 * @param url the URL you would like to parse
 * @return the uri itself, if the conversion was successful the uri.success value will be 0
 */
static URI parseUrl(const char *url) {

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
 * @brief Validates the provided directory and if it is valid and does not yet exist it gets created.
 * @implnote This function mutates the original string if it is deemed a valid directory!
 * @param dir the directory you would like to validate
 * @return 0 if successful -1 otherwise
 */
static int validateDir(char **dir, URI uri) {
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

/**
 * @brief Validates the provided file.
 * @param file the file you would like to validate
 * @return 0 if successful -1 otherwise
 */
static int validateFile(char *file) {
    return (strspn(file, "/\\:*?\"<>|") != 0 || strlen(file) > 255) ? -1 : 0;
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
static char* receiveResponse(int serverSocket) {
    char *request = malloc(BUFFER_SIZE);
    char buffer[BUFFER_SIZE];

    size_t bytesRead = bytesRead = recv(serverSocket, request, sizeof(request) - 1, 0);
    size_t totalBytesRead = bytesRead;

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
static int validateResponse(char *response) {

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

// SYNOPSIS
//       client [-p PORT] [ -o FILE | -d DIR ] URL
// EXAMPLE
//       client http://www.example.com/

/**
 * @brief Entrypoint of the programme. (Sets up and runs client)
 * @details the program uses the HTTP protocol and can accept files that are in plain text.
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
    int port = 80;
    char *path = NULL;
    char *url = NULL;
    URI uri;

    bool portSet = false;
    bool fileSet = false;
    bool dirSet = false;

    int option;
    while ((option = getopt(argc, argv, "p:o:d:")) != -1) {
        switch (option) {
            case 'p':
                if (portSet) {
                    usage(argv[0]);
                }
                portSet = true;
                port = parsePort(optarg);
                if (port == -1) {
                    fprintf(stderr, "An error occurred while parsing the port.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'o':
                if (dirSet || fileSet) {
                    usage(argv[0]);
                }
                fileSet = true;
                path = optarg;
                break;
            case 'd':
                if (dirSet || fileSet) {
                    usage(argv[0]);
                }
                dirSet = true;
                path = optarg;
                break;
            case '?':
                usage(argv[0]);
                break;
            default:
                assert(0);
        }
    }

    int length = (int) log10(port) + 1;
    char strPort[length + 1];
    snprintf(strPort, sizeof(strPort), "%d", port);

    if (argc - optind != 1) {
        usage(argv[0]);
        fprintf(stderr, "URL is missing.\n");
    }

    url = argv[optind];
    uri = parseUrl(url);
    if (uri.success == -1) {
        fprintf(stderr, "An error occurred while parsing the URL.\n");
        exit(EXIT_FAILURE);
    }

    // source: https://www.youtube.com/watch?v=MOrvead27B4
    int clientSocket;
    struct addrinfo hints;
    struct addrinfo *results;
    struct addrinfo *record;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int error;
    if ((error = getaddrinfo(uri.host, strPort, &hints, &results)) != 0) {
        free(uri.host);
        free(uri.file);
        fprintf(stderr, "Failed getting address information. [%d]\n", error);
        exit(EXIT_FAILURE);
    }

    for (record = results; record != NULL; record = record->ai_next) {
        clientSocket = socket(record->ai_family, record->ai_socktype, record->ai_protocol);
        if (clientSocket == -1) continue;
        if (connect(clientSocket, record->ai_addr, record->ai_addrlen) != -1) break;
        close(clientSocket);
        fprintf(stderr, "Failed connecting to server.\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(results);
    if (record == NULL) {
        free(uri.host);
        free(uri.file);
        fprintf(stderr, "Failed connecting to server.\n");
        exit(EXIT_FAILURE);
    }

    char *request = NULL;
    asprintf(&request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", uri.file, uri.host);
    send(clientSocket, request, strlen(request), 0);
    free(request);
    free(uri.host);

    char *receivedResponse = receiveResponse(clientSocket);
    char *header = extractHeader(receivedResponse);
    char *file = extractContent(receivedResponse);

    free(receivedResponse);

    int responseCode = validateResponse(header);
    free(header);
    if (responseCode != 0) {
        free(file);
        free(uri.file);
        exit(responseCode);
    }

    if (fileSet == true) {
        if (validateFile(path) == -1) {
            free(file);
            free(uri.file);
            fprintf(stderr, "An error occurred while parsing the file.\n");
            exit(EXIT_FAILURE);
        }
    }

    if (dirSet == true) {
        if (validateDir(&path, uri) == -1) {
            free(file);
            free(uri.file);
            fprintf(stderr, "An error occurred while parsing the directory.\n");
            exit(EXIT_FAILURE);
        }
    }

    FILE *outfile = (dirSet == false && fileSet == false) ? stdout : fopen(path, "w");
    free(uri.file);

    if (dirSet == true) {
        free(path);
    }

    if (outfile == NULL)  {
        free(file);
        close(clientSocket);
        fprintf(stderr, "ERROR opening output file\n");
        exit(EXIT_FAILURE);
    }

    if (fprintf(outfile, "%s\n", file) == -1) {
        fprintf(stderr, "Failed saving content to file.\n");
    }

    free(file);
    close(clientSocket);
    exit(EXIT_SUCCESS);
}
