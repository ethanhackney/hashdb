#ifndef HASHDB_H
#define HASHDB_H

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* we do alot of casting to unsigned char * */
#define UCHAR_P(p) \
        ((unsigned char *)(p))

/* we also do alot for hashdb_node casting */
#define HASHDB_NODE_P(p) \
        ((struct hashdb_node *)(p))

/* get next multiple of 8 (used for aligning data) */
#define NEXT_MULTPLE_OF_8(n) \
        (((n) + 7) & (-8))

/* misc constants */
enum {
        /* multiplier used for hashing */
        HASHDB_MULTIPLIER       = 31,
};

/* database flags */
enum {
        /* enable sanity mode */
        HASHDB_FLAG_SANE_MODE   = 1,
};

/* used for sizes and pointers */
typedef uint64_t hashdb_size_t;

/* type of hash function */
typedef hashdb_size_t (*hashdb_hashfn_t)(const void *, hashdb_size_t);

/* type of key comparison function */
typedef int (*hashdb_cmpfn_t)(const void *, const void *, hashdb_size_t);

/* hashdb element */
struct hashdb_node {
        /* next on free list or hash list */
        hashdb_size_t   hn_next;
        /* everything after this is user data */
};

/* hashdb file header */
struct hashdb_header {
        /* number of nodes in node table minus the NULL node */
        hashdb_size_t   hh_nr_nodes;
        /* number of buckets in hash table */
        hashdb_size_t   hh_nr_buckets;
        /* size of key in key/value pair */
        hashdb_size_t   hh_key_size;
        /* size of value in key/value pair */
        hashdb_size_t   hh_value_size;
        /* index in node table of head of free list */
        hashdb_size_t   hh_free;
};

/* hash table based database */
struct hashdb {
        /* file header */
        struct hashdb_header    hd_hdr;
        /* hash function */
        hashdb_hashfn_t         hd_hashfn;
        /* key comparison function */
        hashdb_cmpfn_t          hd_cmpfn;
        /* pointer in hd_data to node table + sizeof(NULL node)*/
        unsigned char           *hd_node_tab;
        /* pointer to actual location of node table */
        unsigned char           *hd_actual;
        /* pointer in hd_data to hash table */
        unsigned char           *hd_hash_tab;
        /* size of hashdb_nodes */
        hashdb_size_t           hd_node_size;
        /* total size of database file */
        hashdb_size_t           hd_file_size;
        /* flags for modifying behavior */
        uint64_t                hd_flags;
        /* mmap()'ed file data */
        void                    *hd_data;
        /* pathname of database */
        char                    *hd_path;
        /* database file descriptor */
        int                     hd_fd;
};

/**
 * Initialize a hashdb:
 *
 * args:
 *      @path:          pathname of database
 *      @flags:         flags
 *      @nr_nodes:      number of nodes in node table
 *      @nr_buckets:    number of buckets in hash table
 *      @key_size:      key size
 *      @value_size:    value size
 *      @hashfn:        hash function (optional)
 *      @cmpfn:         key comparison function (optional)
 *      @mode:          file permissions
 * ret:
 *      @success:       pointer to hashdb
 *      @failure:       NULL and errno set
 */
extern struct hashdb *hashdb_init(const char *path,
                                  uint64_t flags,
                                  hashdb_size_t nr_nodes,
                                  hashdb_size_t nr_buckets,
                                  hashdb_size_t key_size,
                                  hashdb_size_t value_size,
                                  hashdb_hashfn_t hashfn,
                                  hashdb_cmpfn_t cmpfn,
                                  mode_t mode);

/**
 * Free hashdb context:
 *
 * args:
 *      @hpp:   pointer to pointer to hashdb
 *      @fully: should we fully destroy database (e.g., remove files)
 * ret:
 *      @success:       0 and *hpp set to NULL
 *      @failure:       -1 and errno set
 */
extern int hashdb_free(struct hashdb **hp, bool fully);

/**
 * Open an existing hashdb:
 *
 * args:
 *      @path:          pathname of database
 *      @flags:         flags
 *      @hashfn:        hash function (optional)
 *      @cmpfn:         key comparison function (optional)
 * ret:
 *      @success:       pointer to hashdb
 *      @failure:       NULL and errno set
 */
extern struct hashdb *hashdb_open(const char *path,
                                  uint64_t flags,
                                  hashdb_hashfn_t hashfn,
                                  hashdb_cmpfn_t cmpfn);

/**
 * Dump hashdb state:
 *
 * args:
 *      @hp:    pointer to hashdb
 *      @fp:    file to write state to
 * ret:
 *      @success:       0
 *      @failure:       -1 and errno set
 */
extern int hashdb_dump(struct hashdb *hp, FILE *fp);

/**
 * Add key/value pair to hashdb:
 *
 * args:
 *      @hp:    pointer to hashdb
 *      @key:   key
 *      @value: value
 * ret:
 *      @success:       pointer to key/value pair
 *      @failure:       NULL and errno set
 *
 */
extern void *hashdb_set(struct hashdb *hp, void *key, void *value);

/**
 * Retrieve key/value pair from hashdb:
 *
 * args:
 *      @hp:    pointer to hashdb
 *      @key:   key
 * ret:
 *      @success:       pointer to key/value pair
 *      @failure:       NULL (if error happens than errno is set)
 */
extern void *hashdb_get(struct hashdb *hp, void *key);

/**
 * Remove key/value pair from hashdb:
 *
 * args:
 *      @hp:    pointer to hashdb
 *      @key:   key of pair to remove
 * ret:
 *      @success:       0
 *      @failure:       -1 and errno set
 */
extern int hashdb_rm(struct hashdb *hp, void *key);

#endif
