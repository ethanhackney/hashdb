#include "hashdb.h"
#include <err.h>
#include <stdio.h>
#include <sysexits.h>

int
main(void)
{
        struct user {
                char uname[64];
                char pword[64];
        } users[] = {
                { "ethan", "dumbass" },
                { "chris", "boywonder" },
                { "sonny", "legend" },
                { "jon", "gamer" },
                { "luke", "og" },
                { "pat", "idiot savant" },
                { "" },
        };
        struct hashdb *db = NULL;
        struct user *up = NULL;
        hashdb_size_t nr_nodes = 4;
        hashdb_size_t nr_buckets = 4;

        db = hashdb_init("userdb",
                         HASHDB_FLAG_SANE_MODE,
                         nr_nodes,
                         nr_buckets,
                         sizeof(users[0].uname),
                         sizeof(users[0].pword),
                         NULL,
                         NULL,
                         0666);
        if (!db)
                err(EX_SOFTWARE, "hashdb_init()");

        for (up = users; *up->uname; ++up) {
                if (!hashdb_set(db, up->uname, up->pword)) {
                        perror("hashdb_set");
                        break;
                }
        }

        if (hashdb_free(&db, false))
                err(EX_SOFTWARE, "hashdb_free()");
}
