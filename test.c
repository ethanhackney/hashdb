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
                { "ethan", "dumby" },
                { "chris", "boywonder" },
                { "sonny", "legend" },
                { "jon", "gamer" },
                { "luke", "og" },
                { "pat", "savant" },
                { "" },
        };
        struct hashdb *db = NULL;
        struct user *up = NULL;
        struct user *upp = NULL;
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
                hashdb_dump(db, stdout);
                if (!hashdb_set(db, up->uname, up->pword)) {
                        perror("hashdb_set");
                        break;
                }
        }

        for (upp = users; upp < up; ++upp) {
                struct user *p = NULL;

                p = hashdb_get(db, upp->uname);
                if (!p)
                        err(EX_SOFTWARE, "hashdb_get(%s)", upp->uname);

                printf("%s's password is %s\n", upp->uname, p->pword);
        }

        if (hashdb_free(&db, false))
                err(EX_SOFTWARE, "hashdb_free()");
}
