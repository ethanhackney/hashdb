#include "hashdb.h"
#include <err.h>
#include <stdio.h>
#include <sysexits.h>

int
main(void)
{
        struct hashdb *mydb = NULL;
        struct user {
                char name[64];
                char pword[64];
        } u = { "ethan", "password" };
        struct user *up;

        mydb = hashdb_init("mydb",
                           HASHDB_FLAG_SANE_MODE,
                           4,
                           4,
                           64,
                           64,
                           NULL,
                           NULL,
                           0666);
        if (!mydb)
                err(EX_SOFTWARE, "hashdb_init()");

        (void)hashdb_dump(mydb, stdout);

        up = hashdb_set(mydb, u.name, u.pword);
        if (up)
                printf("%s's password is %s\n\n", up->name, up->pword);

        if (hashdb_free(&mydb, false))
                err(EX_SOFTWARE, "hashdb_free()");

        mydb = hashdb_open("mydb", HASHDB_FLAG_SANE_MODE, NULL, NULL);
        if (!mydb)
                err(EX_SOFTWARE, "hashdb_open()");

        (void)hashdb_dump(mydb, stdout);

        up = hashdb_get(mydb, u.name);
        if (up)
                printf("%s's password is %s\n", up->name, up->pword);

        if (hashdb_free(&mydb, false))
                err(EX_SOFTWARE, "hashdb_free()");

}
