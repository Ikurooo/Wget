/**
 * @file client.c
 * @author Ivan Cankov 12219400 <e12219400@student.tuwien.ac.at>
 * @date 07.01.2024
 * @brief A simple HTTP client in C
 **/

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
        if (createDir(path) == -1) {
            free(file);
            free(uri.file);
            fprintf(stderr, "An error occurred while creating the directory.\n");
            exit(EXIT_FAILURE);
        }

        char* fullPath = catFileNameToDir(uri.file, path);
        path = fullPath;

        if (fullPath == NULL) {
            free(file);
            fprintf(stderr, "An error occurred while concatenating the directory.\n");
            exit(EXIT_FAILURE);
        }
    }

    free(uri.file);

    FILE *outfile = (dirSet == false && fileSet == false) ? stdout : fopen(path, "w");

    if (outfile == NULL)  {
        free(file);
        close(clientSocket);
        fprintf(stderr, "ERROR opening output file\n");
        exit(EXIT_FAILURE);
    }

    if (fprintf(outfile, "%s", file) == -1) {
        fprintf(stderr, "Failed saving content to file.\n");
        free(file);
        exit(EXIT_FAILURE);
    }

    stringList *urls = extractPattern(file, "https?://[a-zA-Z0-9./?=_-]+");

    if (urls == NULL) {
        fprintf(stderr, "An error occurred while extracting urls.\n");
        free(file);
        exit(EXIT_FAILURE);
    }

    stringList *additionalFileNames = extractPattern(file, "[a-zA-Z0-9_-]+\\.(js|png|jpg|jpeg|css)");

    if (additionalFileNames == NULL) {
        fprintf(stderr, "An error occurred while extracting additional files.\n");
        free(file);
        exit(EXIT_FAILURE);
    }

    // TODO: remove debug lines
    for (int i = 0; i < additionalFileNames->size; ++i) {
        printf("%s\n", additionalFileNames->urls[i]);
    }

    free(file);
    close(clientSocket);

//    for (int i = 0; i < additionalFileNames->size; ++i) {
//        pid_t process = fork();
//
//        if (process == 0) {
//            char *arguments = prepareArguments();
//
//            char *pathArgument = NULL;
//
//            if (dirSet) {
//                error = snprintf(pathArgument, strlen(path) + 4, "-p %s", path);
//            } else if (fileSet) {
//                error = snprintf(pathArgument, strlen() + 4, "-o %s", );
//            }
//
//            execlp(argv[0], argv[0], pathArgument, NULL);
//        } else {
//
//        }
//    }

    freeStringList(urls);
    freeStringList(additionalFileNames);

    exit(EXIT_SUCCESS);
}
