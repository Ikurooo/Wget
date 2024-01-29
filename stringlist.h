#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>

typedef struct {
    char** urls;
    size_t size;
    size_t capacity;
} stringList;

//stringList* createDynamicStringArray(void);
//
//void freeStringList(stringList* array);
//
//int pushUrl(stringList* array, const char* url);

stringList* createDynamicStringArray(void) {
    stringList* array = malloc(sizeof(stringList));
    array->urls = NULL;
    array->size = 0;
    array->capacity = 0;
    return array;
}

void freeStringList(stringList* array) {
    for (size_t i = 0; i < array->size; ++i) {
        free(array->urls[i]);
    }
    free(array->urls);
    free(array);
}

int pushUrl(stringList* array, const char* url) {
    if (array->size >= array->capacity) {
        // Double the capacity when needed
        size_t newCapacity = (array->capacity == 0) ? 1 : array->capacity * 2;
        char **temp = realloc(array->urls, newCapacity * sizeof(char*));
        if (temp == NULL) {
            freeStringList(array);
            return -1;
        }
        array->urls = temp;
        array->capacity = newCapacity;
    }

    char* newUrl = strdup(url);
    if (newUrl == NULL) {
        freeStringList(array);
        return -1;
    }

    array->urls[array->size] = newUrl;
    array->size++;

    return 0;
}


