
#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int index_load(Index *index) {
    index->count = 0;
    return 0;
}

int index_add(Index *index, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);

    ObjectID id;
    object_write(OBJ_BLOB, data, size, &id);
    free(data);

    return 0;
}
int index_status(const Index *index) {
    printf("Index status:\n");

    if (index->count == 0) {
        printf("  (empty)\n");
        return 0;
    }

    for (int i = 0; i < index->count; i++) {
        printf("  %s\n", index->entries[i].path);
    }

    return 0;
}
