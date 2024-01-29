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
    long port = 80;
    long recursionLevel = 1; // EXIT_RECURSION = 0; ERROR_RECURSION = -1; if you don't want any recursion set it to one
    char *path = NULL;
    char *url = NULL;
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
            case 'r':
                if (recursionSet) {
                    usage(argv[0]);
                }
                recursionSet = true;
                recursionLevel = parseRecursionLevel(optarg);
                break;
            case '?':
                usage(argv[0]);
                break;
            default:
                assert(0);
        }
    }

    // TODO: get rid of the casts for portability and stability
    long length = (long)log10((double)port) + 1;
    char strPort[length + 1];
    snprintf(strPort, sizeof(strPort), "%ld", port);

    if (argc - optind != 1) {
        usage(argv[0]);
    }

    if (recursionLevel == ERROR_RECURSION) {
        exit(EXIT_FAILURE);
    }

    if (recursionLevel == EXIT_RECURSION) {
        exit(EXIT_SUCCESS);
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
