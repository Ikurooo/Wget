/**
 * @file client.c
 * @author Ivan Cankov 12219400 <e12219400@student.tuwien.ac.at>
 * @date 07.01.2024
 * @brief A simple HTTP client in C
 **/
#define _GNU_SOURCE

#include <sys/wait.h>
#include "util.h"

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
    long recursionLevel = 1; // EXIT_RECURSION = 0; if you don't want any recursion set it to one <- default value
    char *portStr = "80";
    char *url = NULL;
    char *path = NULL;
    URI uri;

    bool portSet = false;
    bool fileSet = false;
    bool dirSet = false;
    bool recursionSet = false;

    int option;
    while ((option = getopt(argc, argv, "p:o:d:r:")) != -1) {
        switch (option) {
            case 'p':
                if (portSet) {
                    fprintf(stderr, "port\n");
                    usage(argv[0]);
                }
                portSet = true;
                portStr = optarg;
                if (parsePort(portStr) == -1) {
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
            case 'r':
                if (recursionSet) {
                    usage(argv[0]);
                }
                recursionSet = true;
                recursionLevel = convertStringToLong(optarg);
                break;
            case '?':
                usage(argv[0]);
                break;
            default:
                assert(0);
        }
    }

    if (argc - optind != 1) {
        usage(argv[0]);
    }

    if (errno == ERANGE && recursionLevel == ULONG_MAX) {
        exit(EXIT_FAILURE);
    }

    if (recursionLevel == EXIT_RECURSION) {
        exit(EXIT_SUCCESS);
    }

    // Decrement recursion level for next iteration or early exit if no
    // recursion depth was specified to save on memory
    recursionLevel -= 1;

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
    if ((error = getaddrinfo(uri.host, portStr, &hints, &results)) != 0) {
        freeUri(&uri);
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
        freeUri(&uri);
        close(clientSocket);
        fprintf(stderr, "Failed connecting to server.\n");
        exit(EXIT_FAILURE);
    }

    char *request = NULL;
    asprintf(&request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", uri.file, uri.host);
    send(clientSocket, request, strlen(request), 0);
    free(request);

    uint8_t *message = NULL;
    ssize_t messageLength = receiveResponse(&message, clientSocket);

    // TODO fix error in receive response

    int responseCode = validateResponse(message);
    if (responseCode != 0) {
        free(message);
        freeUri(&uri);
        close(clientSocket);
        exit(responseCode);
    }

    uint8_t *content = NULL;
    size_t contentLength = extractContent(message, messageLength, &content);

    char *fullPath = NULL;

    if (fileSet == true) {
        if (validateFile(path) == -1) {
            free(message);
            freeUri(&uri);
            close(clientSocket);
            fprintf(stderr, "An error occurred while parsing the file.\n");
            exit(EXIT_FAILURE);
        }
        fullPath = strdup(path);
    }

    if (dirSet == true) {
        if (createDir(path) == -1) {
            free(message);
            freeUri(&uri);
            close(clientSocket);
            fprintf(stderr, "An error occurred while creating the directory.\n");
            exit(EXIT_FAILURE);
        }

        fullPath = catFileNameToDir(uri.file, path);

        if (fullPath == NULL) {
            free(message);
            freeUri(&uri);
            close(clientSocket);
            fprintf(stderr, "An error occurred while concatenating the directory.\n");
            exit(EXIT_FAILURE);
        }
    }

    FILE *outfile = (dirSet == false && fileSet == false) ? stdout : fopen(fullPath, "wb");
    free(fullPath);

    if (outfile == NULL)  {
        free(message);
        freeUri(&uri);
        close(clientSocket);
        fprintf(stderr, "ERROR opening output file\n");
        exit(EXIT_FAILURE);
    }

    if (fwrite(content, 1, contentLength, outfile) == -1) {
        free(message);
        freeUri(&uri);
        close(clientSocket);
        fprintf(stderr, "Failed saving content to file.\n");
        exit(EXIT_FAILURE);
    }

    stringList *urls = extractPattern((char*)content, "https?://[a-zA-Z0-9./?=_-]+");

    if (urls == NULL) {
        free(message);
        freeUri(&uri);
        close(clientSocket);
        fprintf(stderr, "An error occurred while extracting urls.\n");
        exit(EXIT_FAILURE);
    }

    stringList *additionalFileNames = extractPattern((char*)content, "(\"|\'){1}[a-zA-Z0-9_-]+\\.(js|png|jpg|jpeg|css)[0-9?_]*(\"|\'){1}");

    if (additionalFileNames == NULL) {
        free(message);
        freeUri(&uri);
        freeStringList(urls);
        close(clientSocket);
        fprintf(stderr, "An error occurred while extracting additional files.\n");
        exit(EXIT_FAILURE);
    }

    free(message);
    close(clientSocket);

    // TODO: make readable and remove magic values

    for (int i = 0; i < additionalFileNames->size; ++i) {
        fprintf(stderr, "ADDITIONAL FILE: %s\n", additionalFileNames->urls[i]);

        char *additionalRequest = NULL;
        asprintf(&additionalRequest, "%s/%s", uri.host, additionalFileNames->urls[i] + 1);
        additionalRequest[strlen(additionalRequest) - 1] = '\0';
        // hacky way for now but move pointer by one char since we know because
        // of the regex that every additional file is enclosed in quotes

        char *dArgument = (dirSet == true) ? path : "extra-files-needed-by-site";

        char *arguments[10];
        int j = 0;

        arguments[j++] = argv[0];
        arguments[j++] = "-p";
        arguments[j++] = portStr;
        arguments[j++] = "-d";
        arguments[j++] = dArgument;
        arguments[j++] = additionalRequest;
        arguments[j] = NULL;  // Mark the end of the argument list

        pid_t process = fork();

        if (process == 0) {
            execvp(arguments[0], arguments);
        } else {
            int worked = -1;
            wait(&worked);
            free(additionalRequest);
        }
    }

    freeUri(&uri);

    freeStringList(urls);
    freeStringList(additionalFileNames);

    exit(EXIT_SUCCESS);
}
// TODO: check for text/html text/css script/js for early exit
// TODO: implement gzip and .bin .png .jpeg etc. writable
// TODO: make multi-threaded with sync through unnamed semaphores
// TODO: implement function to get current working directory
