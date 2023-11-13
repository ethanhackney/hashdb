#include "hashdb.h"
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <sysexits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum {
        /* default number of nodes in node table */
        DEFAULT_NR_NODE         = 8192,
        /* default number of buckets in hash table */
        DEFAULT_NR_BUCKET       = 4096,
        /* max size of word */
        WORD_SIZE               = 127,
};

/* word frequency */
struct word_freq {
        char word[WORD_SIZE + 1];
        hashdb_size_t count;
};

/* strtol with error checking */
static long e_strtol(const char *nptr, char **endptr, int base);

/* print usage and exit */
static void usage(char *progname);

/* hash table for storing word frequencies */
static struct hashdb *g_wordfreq;

/* list of all words read */
static char     **words;
static size_t   words_len;
static size_t   words_cap;

/* key comparison */
static int word_cmp(const void *a, const void *b, hashdb_size_t size);

static size_t word_hash(const void *key, hashdb_size_t size);

/* increment word frequency */
static void word_inc(char *base, char *end);

int
main(int argc, char **argv)
{
        hashdb_size_t nr_nodes = 0;
        hashdb_size_t nr_buckets = 0;
        size_t i;
        char buf[BUFSIZ];
        int c;

        while ((c = getopt(argc, argv, "n:b:")) != -1) {
                switch (c) {
                case 'n':
                        nr_nodes = e_strtol(optarg, NULL, 10);
                        break;
                case 'b':
                        nr_buckets = e_strtol(optarg, NULL, 10);
                        break;
                default:
                        usage(argv[0]);
                        /* does not return */
                }
        }

        if (!nr_nodes)
                nr_nodes = DEFAULT_NR_NODE;
        if (!nr_buckets)
                nr_buckets = DEFAULT_NR_BUCKET;

        g_wordfreq = hashdb_init("wordfreq",
                                 HASHDB_FLAG_SANE_MODE,
                                 nr_nodes,
                                 nr_buckets,
                                 (WORD_SIZE + 1),
                                 sizeof(int),
                                 word_hash,
                                 word_cmp,
                                 0666);
        if (!g_wordfreq)
                err(EX_SOFTWARE, "wordfreq()");

        while (fgets(buf, sizeof(buf), stdin)) {
                char *base, *end;

                if (buf[strlen(buf) - 1] == '\n')
                        buf[strlen(buf) - 1] = 0;

                for (base = end = buf; *end; ++end) {
                        if (isalpha(*end) || *end == '_')
                                continue;
                        word_inc(base, end);
                        base = end + 1;
                }
                word_inc(base, end);
        }

        if (hashdb_free(&g_wordfreq, false))
                err(EX_SOFTWARE, "hashdb_free()");

        g_wordfreq = hashdb_open("wordfreq", HASHDB_FLAG_SANE_MODE,
                        word_hash, word_cmp);
        if (!g_wordfreq)
                err(EX_SOFTWARE, "hashdb_open()");

        for (i = 0; i < words_len; ++i) {
                struct word_freq *wf;
                char *word = words[i];

                wf = hashdb_get(g_wordfreq, word);
                if (!wf)
                        err(EX_SOFTWARE, "hashdb_get(%s)", word);

                printf("%ld: %s\n", (long)wf->count, wf->word);
                free(word);
        }
        free(words);

        if (hashdb_free(&g_wordfreq, false))
                err(EX_SOFTWARE, "hashdb_free()");
}

static long
e_strtol(const char *nptr, char **endptr, int base)
{
        long res;

        errno = 0;
        res = strtol(nptr, endptr, base);
        if (errno)
                err(EX_SOFTWARE, "strtol(%s)", nptr);

        return res;
}

static void
usage(char *progname)
{
        fprintf(stderr, "%s: Usage\n", progname);
        fprintf(stderr, "\t-n:  number of nodes in node table\n");
        fprintf(stderr, "\t-b:  number of buckets in hash table\n");
        fprintf(stderr, "\t-k:  key size\n");
        exit(EXIT_FAILURE);
}

static void
word_inc(char *base, char *end)
{
        struct word_freq *wf = NULL;

        /* empty word */
        if (!(end - base))
                return;

        /* word that is too big */
        *end = 0;
        if ((end - base) >= WORD_SIZE) {
                errno = E2BIG;
                warn("\"%s\" too long", base);
                return;
        }

        wf = hashdb_get(g_wordfreq, base);
        if (!wf) {
                char *dup;
                hashdb_size_t count = 0;

                wf = hashdb_set(g_wordfreq, base, &count);
                if (!wf)
                        err(EX_SOFTWARE, "hashdb_set(%s)", base);

                if (words_len == words_cap) {
                        words_cap = words_cap ? words_cap * 2 : 1;
                        words = realloc(words, sizeof(*words) * words_cap);
                        if (!words)
                                err(EX_SOFTWARE, "realloc()");
                }

                dup = strdup(base);
                if (!dup)
                        err(EX_SOFTWARE, "strdup(%s)", base);
                words[words_len++] = dup;
        }
        ++wf->count;
}

static int
word_cmp(const void *a, const void *b, hashdb_size_t size)
{
        const char *astr = a;
        const char *bstr = b;
        return strcmp(astr, bstr);
}

static size_t
word_hash(const void *key, hashdb_size_t size)
{
        const char *kp = key;
        hashdb_size_t hash = 0;

        while (*kp)
                hash = hash * 31 + *kp++;

        return hash;
}
