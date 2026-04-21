#include "commit.h"
#include "index.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out);

// ---------- SERIALIZE ----------
int commit_serialize(const Commit *c, void **data_out, size_t *len_out) {
    char tree_hex[65], parent_hex[65];
    hash_to_hex(&c->tree, tree_hex);

    char buf[8192];
    int n = 0;

    n += sprintf(buf+n, "tree %s\n", tree_hex);

    if (c->has_parent) {
        hash_to_hex(&c->parent, parent_hex);
        n += sprintf(buf+n, "parent %s\n", parent_hex);
    }

    n += sprintf(buf+n,
        "author %s %" PRIu64 "\n"
        "committer %s %" PRIu64 "\n\n%s",
        c->author, c->timestamp,
        c->author, c->timestamp,
        c->message);

    *data_out = malloc(n);
    memcpy(*data_out, buf, n);
    *len_out = n;
    return 0;
}

// ---------- HEAD ----------
int head_read(ObjectID *id_out) {
    FILE *f = fopen(".pes/refs/heads/main", "r");
    if (!f) return -1;

    char hex[65];
    if (!fgets(hex, sizeof(hex), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    hex[strcspn(hex, "\n")] = 0;
    return hex_to_hash(hex, id_out);
}

int head_update(const ObjectID *id) {
    mkdir(".pes", 0755);
    mkdir(".pes/refs", 0755);
    mkdir(".pes/refs/heads", 0755);

    FILE *f = fopen(".pes/refs/heads/main", "w");
    if (!f) return -1;

    char hex[65];
    hash_to_hex(id, hex);
    fprintf(f, "%s\n", hex);
    fclose(f);

    return 0;
}

// ---------- PARSE ----------
int commit_parse(const void *data, size_t len, Commit *c) {
    memset(c, 0, sizeof(*c));

    const char *p = (const char *)data;

    while (*p) {
        if (strncmp(p, "tree ", 5) == 0) {
            char hex[65];
            sscanf(p + 5, "%64s", hex);
            hex_to_hash(hex, &c->tree);
        }
        else if (strncmp(p, "parent ", 7) == 0) {
            char hex[65];
            sscanf(p + 7, "%64s", hex);
            hex_to_hash(hex, &c->parent);
            c->has_parent = 1;
        }
        else if (strncmp(p, "author ", 7) == 0) {
            sscanf(p + 7, "%255[^0-9] %lu", c->author, &c->timestamp);
        }
        else if (*p == '\n') {
            // message starts after blank line
            strncpy(c->message, p + 1, sizeof(c->message) - 1);
            break;
        }

        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }

    return 0;
}

// ---------- WALK ----------
int commit_walk(commit_walk_fn cb, void *ctx) {
    ObjectID id;

    if (head_read(&id) < 0) return -1;

    while (1) {
        ObjectType type;
        void *data;
        size_t len;

        if (object_read(&id, &type, &data, &len) < 0) return -1;

        Commit c;
        commit_parse(data, len, &c);
        free(data);

        cb(&id, &c, ctx);

        if (!c.has_parent) break;
        id = c.parent;
    }

    return 0;
}

// ---------- CREATE ----------
int commit_create(const char *message, ObjectID *id_out) {
    ObjectID tree_id;

    if (tree_from_index(&tree_id) < 0) {
        fprintf(stderr, "nothing to commit\n");
        return -1;
    }

    Commit c;
    memset(&c, 0, sizeof(c));

    c.tree = tree_id;
    c.timestamp = time(NULL);

    snprintf(c.author, sizeof(c.author), "%s", pes_author());
    snprintf(c.message, sizeof(c.message), "%s", message);

    ObjectID parent;
    if (head_read(&parent) == 0) {
        c.has_parent = 1;
        c.parent = parent;
    }

    void *data;
    size_t len;

    commit_serialize(&c, &data, &len);

    if (object_write(OBJ_COMMIT, data, len, id_out) < 0) {
        free(data);
        return -1;
    }

    free(data);

    return head_update(id_out);
}
