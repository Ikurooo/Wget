/**
 * @file client.c
 * @author Ivan Cankov 12219400 <e12219400@student.tuwien.ac.at>
 * @date 13.02.2024
 * @brief A simple wget implementation in C
 * @version 1.0.0
 **/
#define _GNU_SOURCE

#include <assert.h>
#include "util.h"

/**
 * @brief Entrypoint of the programme. (Sets up and runs client)
 * @details the program uses the HTTP protocol to download files and websites
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
    char *portStr = "443";
    char *url = NULL;
    char *path = NULL;
    URI uri;

    bool portSet = false;
    bool fileSet = false;
    bool dirSet = false;

    int option;
    while ((option = getopt(argc, argv, "p:o:d:")) != -1) {
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

    // Initialize OpenSSL
    SSL_library_init();
    SSL_CTX *sslContext = SSL_CTX_new(SSLv23_client_method());
    SSL *ssl = SSL_new(sslContext);
    SSL_set_fd(ssl, clientSocket);

    // Perform SSL/TLS handshake
    if (SSL_connect(ssl) != 1) {
        fprintf(stderr, "SSL/TLS handshake failed.\n");
        SSL_free(ssl);
        SSL_CTX_free(sslContext);
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    char *request = NULL;

    if (asprintf(&request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", uri.file, uri.host) == -1) {
        SSL_free(ssl);
        SSL_CTX_free(sslContext);
        close(clientSocket);
        fprintf(stderr, "Error creating HTTP request");
        exit(EXIT_FAILURE);
    }

    size_t requestLength = strlen(request);
    ssize_t bytesSent = SSL_write(ssl, request, (int)strlen(request));

    if (bytesSent == -1 || bytesSent < requestLength) {
        SSL_free(ssl);
        SSL_CTX_free(sslContext);
        close(clientSocket);
        fprintf(stderr, "Error sending HTTP request");
        exit(EXIT_FAILURE);
    }
    free(request);

    FILE *outfile = parseOutputFile(fileSet, dirSet, path, uri.file);

    if (outfile == NULL)  {
        SSL_free(ssl);
        SSL_CTX_free(sslContext);
        freeUri(&uri);
        fprintf(stderr, "ERROR opening output file\n");
        exit(EXIT_FAILURE);
    }

    if (receiveHeaderAndWriteToFile(outfile, ssl) == -1) {
        freeUri(&uri);
        SSL_free(ssl);
        SSL_CTX_free(sslContext);
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    SSL_free(ssl);
    SSL_CTX_free(sslContext);
    close(clientSocket);
    freeUri(&uri);
    exit(EXIT_SUCCESS);
}
