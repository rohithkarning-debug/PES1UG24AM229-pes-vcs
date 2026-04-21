#include "index.h"
#include "pes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

// ---------- PROVIDED ----------

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    if (index->count == 0) printf("  (nothing to show)\n");

    for (int i = 0; i < index->count; i++)
        printf("  staged:     %s\n", index->entries[i].path);

    printf("\nUnstaged changes:\n  (nothing to show)\n\n");

    printf("Untracked files:\n");

    DIR *dir = opendir(".");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (!strcmp(ent->d_name, ".") ||
                !strcmp(ent->d_name, "..") ||
                !strcmp(ent->d_name, ".pes"))
                continue;

            int tracked = 0;
            for (int i = 0; i < index->count; i++)
                if (!strcmp(index->entries[i].path, ent->d_name))
                    tracked = 1;

            if (!tracked)
                printf("  untracked:  %s\n", ent->d_name);
        }
        closedir(dir);
    }

    return 0;
}

// ---------- CORE ----------

static int cmp(const void *a, const void *b) {
    return strcmp(((IndexEntry*)a)->path, ((IndexEntry*)b)->path);
}

int index_load(Index *index) {
    index->count = 0;

    FILE *f = fopen(".pes/index", "r");
    if (!f) return 0;

    char hex[65], path[512];
    unsigned int mode, size;
    unsigned long long mtime;

    while (fscanf(f, "%o %64s %llu %u %511s",
                  &mode, hex, &mtime, &size, path) == 5) {

        IndexEntry *e = &index->entries[index->count++];

        e->mode = mode;
        e->mtime_sec = mtime;
        e->size = size;
        strncpy(e->path, path, sizeof(e->path)-1);

        hex_to_hash(hex, &e->hash);
    }

    fclose(f);
    return 0;
}

int index_save(const Index *index) {
    // 🔥 FIX: avoid stack overflow
    Index *sorted = malloc(sizeof(Index));
    *sorted = *index;

    qsort(sorted->entries, sorted->count, sizeof(IndexEntry), cmp);

    mkdir(".pes", 0755);

    FILE *f = fopen(".pes/index", "w");
    if (!f) return -1;

    char hex[65];

    for (int i = 0; i < sorted->count; i++) {
        hash_to_hex(&sorted->entries[i].hash, hex);

        fprintf(f, "%o %s %llu %u %s\n",
            sorted->entries[i].mode,
            hex,
            (unsigned long long)sorted->entries[i].mtime_sec,
            sorted->entries[i].size,
            sorted->entries[i].path);
    }

    fclose(f);
    free(sorted);
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

    struct stat st;
    stat(path, &st);

    IndexEntry *e = index_find(index, path);
    if (!e)
        e = &index->entries[index->count++];

    e->mode = 0100644;
    e->hash = id;
    e->mtime_sec = st.st_mtime;
    e->size = st.st_size;

    strncpy(e->path, path, sizeof(e->path)-1);

    return index_save(index);
}
