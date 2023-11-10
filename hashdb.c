#include "hashdb.h"

static hashdb_size_t default_hashfn(const void *key, hashdb_size_t size);

struct hashdb *
hashdb_init(const char *path,
            uint64_t flags,
            hashdb_size_t nr_nodes,
            hashdb_size_t nr_buckets,
            hashdb_size_t key_size,
            hashdb_size_t value_size,
            hashdb_hashfn_t hashfn,
            hashdb_cmpfn_t cmpfn,
            mode_t mode)
{
        struct hashdb_header *hdr = NULL;
        struct hashdb_node *np = NULL;
        struct hashdb *hp = NULL;
        unsigned char *p = NULL;
        hashdb_size_t next;
        hashdb_size_t i;
        int saved_errno;

        errno = EINVAL;
        if (!path)
                goto ret;
        if (!*path)
                goto ret;
        if (!nr_nodes)
                goto ret;
        if (!nr_buckets)
                goto ret;
        if (!key_size)
                goto ret;
        if (!value_size)
                goto ret;
        errno = 0;

        hp = malloc(sizeof(*hp));
        if (!hp)
                goto ret;

        hp->hd_path = strdup(path);
        if (!hp->hd_path)
                goto free_hp;

        hp->hd_fd = open(path, O_CREAT | O_RDWR, mode);
        if (hp->hd_fd < 0)
                goto free_hd_path;

        /* allocate space for database */
        key_size = NEXT_MULTPLE_OF_8(key_size);
        value_size = NEXT_MULTPLE_OF_8(value_size);
        hp->hd_node_size = sizeof(next) + key_size + value_size;
        hp->hd_file_size = sizeof(hp->hd_hdr);
        hp->hd_file_size += hp->hd_node_size * (nr_nodes + 1);
        hp->hd_file_size += sizeof(hashdb_size_t) * nr_buckets;
        if (ftruncate(hp->hd_fd, hp->hd_file_size))
                goto close_fd_and_unlink;

        hp->hd_data = mmap(NULL,
                           hp->hd_file_size,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           hp->hd_fd,
                           0);
        if (!hp->hd_data)
                goto close_fd_and_unlink;

        /* initialize file header */
        hdr = &hp->hd_hdr;
        hdr->hh_nr_nodes = nr_nodes;
        hdr->hh_nr_buckets = nr_buckets;
        hdr->hh_key_size = key_size;
        hdr->hh_value_size = value_size;
        hdr->hh_free = 1;
        memcpy(hp->hd_data, hdr, sizeof(*hdr));

        /* initialize free list */
        p = UCHAR_P(hp->hd_data);
        hp->hd_actual = p + sizeof(*hdr);
        hp->hd_node_tab = hp->hd_actual + hp->hd_node_size;
        p = hp->hd_node_tab;
        for (i = 0; i < nr_nodes - 1; ++i) {
                np = HASHDB_NODE_P(p);
                np->hn_next = (i + 1) + 1;
                p += hp->hd_node_size;
        }
        np = HASHDB_NODE_P(p);
        np->hn_next = 0;

        /* initialize hash table */
        p = hp->hd_node_tab + (hp->hd_node_size * nr_nodes);
        hp->hd_hash_tab = p;
        memset(p, 0, sizeof(hashdb_size_t) * nr_buckets);

        hp->hd_hashfn = hashfn;
        if (!hp->hd_hashfn)
                hp->hd_hashfn = default_hashfn;

        hp->hd_cmpfn = cmpfn;
        if (!hp->hd_cmpfn)
                hp->hd_cmpfn = memcmp;

        hp->hd_flags = flags;
        goto ret;

close_fd_and_unlink:
        saved_errno = errno;
        close(hp->hd_fd);
        hp->hd_fd = -1;
        unlink(hp->hd_path);
        errno = saved_errno;

free_hd_path:
        free(hp->hd_path);
        hp->hd_path = NULL;
free_hp:
        free(hp);
        hp = NULL;
ret:
        return hp;
}

static hashdb_size_t
default_hashfn(const void *key, hashdb_size_t size)
{
        const unsigned char *p = key;
        hashdb_size_t hash = 0;
        hashdb_size_t i;

        for (i = 0; i < size; ++i)
                hash = hash * HASHDB_MULTIPLIER + *p++;

        return hash;
}

/* check sanity of hashdb */
static int hashdb_sanity(const struct hashdb *hp);

int
hashdb_free(struct hashdb **hpp, bool fully)
{
        struct hashdb *hp = NULL;

        if (hpp == NULL) {
                errno = EINVAL;
                return -1;
        }
        hp = *hpp;

        if (hashdb_sanity(hp))
                return -1;

        if (munmap(hp->hd_data, hp->hd_file_size))
                return -1;

        if (hp->hd_fd >= 0 && close(hp->hd_fd))
                return -1;

        if (hp->hd_fd >= 0 && fully && unlink(hp->hd_path))
                return -1;

        free(hp->hd_path);
        free(hp);
        *hpp = NULL;
        return 0;
}

static int
hashdb_sanity(const struct hashdb *hp)
{
        const struct hashdb_header *hdr = NULL;
        const unsigned char *p = NULL;

        errno = EINVAL;
        if (!hp)
                return -1;
        if (!(hp->hd_flags & HASHDB_FLAG_SANE_MODE)) {
                errno = 0;
                return 0;
        }
        if (!hp->hd_path)
                return -1;
        if (hp->hd_fd < 0)
                return -1;
        if (!hp->hd_node_size)
                return -1;
        if (!hp->hd_file_size)
                return -1;
        if (!hp->hd_data)
                return -1;
        if (!hp->hd_node_tab)
                return -1;
        if (!hp->hd_actual)
                return -1;
        if (!hp->hd_hash_tab)
                return -1;
        if (!hp->hd_hashfn)
                return -1;
        if (!hp->hd_cmpfn)
                return -1;

        hdr = &hp->hd_hdr;
        p = hp->hd_data;
        if (hp->hd_actual != p + sizeof(*hdr))
                return -1;

        if (hp->hd_node_tab != p + sizeof(*hdr) + hp->hd_node_size)
                return -1;

        p = hp->hd_node_tab;
        if (hp->hd_hash_tab != p + (hp->hd_node_size * hdr->hh_nr_nodes))
                return -1;

        if (!hdr->hh_nr_nodes)
                return -1;
        if (!hdr->hh_nr_buckets)
                return -1;
        if (!hdr->hh_key_size)
                return -1;
        if (!hdr->hh_value_size)
                return -1;

        errno = 0;
        return 0;
}

struct hashdb *
hashdb_open(const char *path,
            uint64_t flags,
            hashdb_hashfn_t hashfn,
            hashdb_cmpfn_t cmpfn)
{
        struct hashdb_header *hdr = NULL;
        struct hashdb *hp = NULL;
        unsigned char *p = NULL;
        struct stat stats;
        hashdb_size_t next;
        int saved_errno;

        errno = 0;
        if (!path)
                goto ret;
        if (!*path)
                goto ret;
        errno = 0;

        hp = malloc(sizeof(*hp));
        if (!hp)
                goto ret;

        hp->hd_path = strdup(path);
        if (!hp->hd_path)
                goto free_hp;

        hp->hd_fd = open(path, O_RDWR);
        if (hp->hd_fd < 0)
                goto free_hd_path;

        if (fstat(hp->hd_fd, &stats))
                goto close_fd;

        hdr = &hp->hd_hdr;
        hp->hd_file_size = stats.st_size;
        hp->hd_data = mmap(NULL,
                           hp->hd_file_size,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           hp->hd_fd,
                           0);
        if (!hp->hd_data)
                goto close_fd;
        memcpy(hdr, hp->hd_data, sizeof(*hdr));

        hp->hd_node_size = sizeof(next) + hdr->hh_key_size + hdr->hh_value_size;

        p = UCHAR_P(hp->hd_data);
        hp->hd_actual = p + sizeof(*hdr);
        hp->hd_node_tab = hp->hd_actual + hp->hd_node_size;

        p = hp->hd_node_tab;
        hp->hd_hash_tab = p + (hp->hd_node_size * hdr->hh_nr_nodes);

        hp->hd_hashfn = hashfn;
        if (!hp->hd_hashfn)
                hp->hd_hashfn = default_hashfn;

        hp->hd_cmpfn = cmpfn;
        if (!hp->hd_cmpfn)
                hp->hd_cmpfn = memcmp;

        hp->hd_flags = flags;
        goto ret;

close_fd:
        saved_errno = errno;
        close(hp->hd_fd);
        hp->hd_fd = -1;
        errno = saved_errno;
free_hd_path:
        free(hp->hd_path);
        hp->hd_path = NULL;
free_hp:
        free(hp);
        hp = NULL;
ret:
        return hp;
}

int
hashdb_dump(struct hashdb *hp, FILE *fp)
{
        struct hashdb_header *hdr = NULL;
        hashdb_size_t freelistlen;
        hashdb_size_t curr;

        if (hashdb_sanity(hp))
                return -1;

        hdr = &hp->hd_hdr;
        fprintf(fp, "nr_nodes:    %zu\n", (size_t)hdr->hh_nr_nodes);
        fprintf(fp, "nr_buckets:  %zu\n", (size_t)hdr->hh_nr_buckets);
        fprintf(fp, "key_size:    %zu\n", (size_t)hdr->hh_key_size);
        fprintf(fp, "value_size:  %zu\n", (size_t)hdr->hh_value_size);
        fprintf(fp, "free:        %zu\n", (size_t)hdr->hh_free);

        curr = hdr->hh_free;
        freelistlen = 0;
        while (curr) {
                unsigned char *p = NULL;

                p = hp->hd_actual + (hp->hd_node_size * curr);
                curr = HASHDB_NODE_P(p)->hn_next;
                ++freelistlen;
        }

        fprintf(fp, "freelistlen: %zu\n", freelistlen);
        return 0;
}

void *
hashdb_set(struct hashdb *hp, void *key, void *value)
{
        struct hashdb_header *hdr = &hp->hd_hdr;
        unsigned char *p = NULL;
        hashdb_size_t *bp = NULL;
        hashdb_size_t hash;
        hashdb_size_t bucket;
        hashdb_size_t curr;
        hashdb_size_t save;
        hashdb_size_t free;
        hashdb_size_t next;
        void *keyp = NULL;
        void *valp = NULL;

        if (hashdb_sanity(hp))
                return NULL;

        hash = hp->hd_hashfn(key, hdr->hh_key_size);
        bucket = hash % hdr->hh_nr_buckets;
        bp = (hashdb_size_t *)(hp->hd_hash_tab +
                        (sizeof(hashdb_size_t) * bucket));
        memcpy(&curr, bp, sizeof(curr));
        save = curr;

        while (curr) {
                p = hp->hd_actual + (hp->hd_node_size * curr);
                next = HASHDB_NODE_P(p)->hn_next;
                keyp = p + sizeof(hashdb_size_t);
                valp = UCHAR_P(keyp) + hdr->hh_key_size;
                if (!hp->hd_cmpfn(key, keyp, hdr->hh_key_size)) {
                        memcpy(valp, value, hdr->hh_value_size);
                        return keyp;
                }

                curr = next;
        }

        if (!hdr->hh_free) {
                errno = ENOMEM;
                return NULL;
        }

        free = hdr->hh_free;
        p = hp->hd_actual + (hp->hd_node_size * free);
        next = HASHDB_NODE_P(p)->hn_next;
        keyp = p + sizeof(hashdb_size_t);
        valp = UCHAR_P(keyp) + hdr->hh_key_size;
        memcpy(keyp, key, hdr->hh_key_size);
        memcpy(valp, value, hdr->hh_value_size);
        hdr->hh_free = next;
        memcpy(hp->hd_data, hdr, sizeof(*hdr));
        HASHDB_NODE_P(p)->hn_next = save;
        memcpy(bp, &free, sizeof(free));
        return keyp;
}

void *
hashdb_get(struct hashdb *hp, void *key)
{
        struct hashdb_header *hdr = &hp->hd_hdr;
        unsigned char *p = NULL;
        hashdb_size_t *bp = NULL;
        hashdb_size_t hash;
        hashdb_size_t bucket;
        hashdb_size_t curr;
        hashdb_size_t next;
        void *keyp = NULL;

        if (hashdb_sanity(hp))
                return NULL;

        hash = hp->hd_hashfn(key, hdr->hh_key_size);
        bucket = hash % hdr->hh_nr_buckets;
        bp = (hashdb_size_t *)(hp->hd_hash_tab +
                        (sizeof(hashdb_size_t) * bucket));
        memcpy(&curr, bp, sizeof(curr));

        while (curr) {
                p = hp->hd_actual + (hp->hd_node_size * curr);
                next = HASHDB_NODE_P(p)->hn_next;
                keyp = p + sizeof(hashdb_size_t);
                if (!hp->hd_cmpfn(key, keyp, hdr->hh_key_size))
                        return keyp;
                curr = next;
        }

        return NULL;
}

int
hashdb_rm(struct hashdb *hp, void *key)
{
        struct hashdb_header *hdr = &hp->hd_hdr;
        unsigned char *p = NULL;
        unsigned char *elemp = p;
        unsigned char *bkt_p = NULL;
        hashdb_size_t hash;
        hashdb_size_t bucket;
        hashdb_size_t curr;
        hashdb_size_t prev;
        hashdb_size_t chainlen;

        hash = hp->hd_hashfn(key, hdr->hh_key_size);
        bucket = hash % hdr->hh_nr_buckets;
        bkt_p = hp->hd_hash_tab + (sizeof(hashdb_size_t) * bucket);
        memcpy(&curr, bkt_p, sizeof(curr));

        prev = 0;
        chainlen = 0;
        while (curr) {
                void *keyp = NULL;

                ++chainlen;
                prev = curr;
                p = hp->hd_actual + (hp->hd_node_size * curr);
                keyp = p + sizeof(hashdb_size_t);
                if (!hp->hd_cmpfn(key, keyp, hdr->hh_key_size))
                        break;

                curr = HASHDB_NODE_P(p)->hn_next;
        }

        if (!curr) {
                errno = ENOENT;
                return -1;
        }

        elemp = p;
        if (chainlen > 1) {
                p = hp->hd_actual + (hp->hd_node_size * prev);
                HASHDB_NODE_P(p)->hn_next = HASHDB_NODE_P(elemp)->hn_next;
        } else {
                memset(bkt_p, 0, sizeof(hashdb_size_t));
        }

        HASHDB_NODE_P(elemp)->hn_next = hdr->hh_free;
        hdr->hh_free = curr;
        memcpy(hp->hd_data, hdr, sizeof(*hdr));
        return 0;
}
