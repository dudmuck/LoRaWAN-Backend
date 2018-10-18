/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "ns.h"

MYSQL*
dbConnect(const char* dbhostname, const char* dbuser, const char* dbpass, uint16_t dbport, const char *dbName)
{
    MYSQL *ret;
    ret = mysql_init(NULL);
    if (ret == NULL) {
        fprintf(stderr, "child Failed to initialize: %s\n", mysql_error(ret));
        return NULL;
    }

    /* enable re-connect */
    my_bool reconnect = 1;
    if (mysql_options(ret, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
        fprintf(stderr, "child mysql_options() failed\n");
        return NULL;
    }

    /* Connect to the server */
    if (!mysql_real_connect(ret, dbhostname, dbuser, dbpass, dbName, dbport, NULL, 0))
    {
        fprintf(stderr, "child Failed to connect to server: %s\n", mysql_error(ret));
        return NULL;
    }

    return ret;
}

int
child(const char* mqName, const char* dbhostname, const char* dbuser, const char* dbpass, uint16_t dbport, const char *dbName)
{
    MYSQL *sql_conn;
    struct mq_attr attr;
    char msg[MQ_MSGSIZE+1];
    unsigned pri;
    bool dbConnected;
    //mqd_t mqd = mq_open(mqName, O_RDONLY | O_NONBLOCK);
    mqd_t mqd = mq_open(mqName, O_RDONLY);
    if (mqd == (mqd_t)-1) {
        perror("child mq_open");
        printf("mqName: %s\n", mqName);
        return -1;
    }

    do {
        if (mq_getattr(mqd, &attr) < 0) {
            perror("mq_getattr");
            return -1;
        }
        printf("curmsgs:%lu\n", attr.mq_curmsgs);
        if (attr.mq_curmsgs > 0) {
            ssize_t s = mq_receive(mqd, msg, sizeof(msg), NULL);
            if (s == -1) {
                perror("mq_receive");
                sleep(1);
                continue;
            } else
                printf("stale msg (%zd) \"%s\"\n", s, msg);
        }
    } while (attr.mq_curmsgs > 0);

    sql_conn = dbConnect(dbhostname, dbuser, dbpass, dbport, dbName);
    if (sql_conn == NULL)
        return -1;

    dbConnected = true;

    for (;;) {
        struct timespec tm;
        clock_gettime(CLOCK_REALTIME, &tm);
        tm.tv_sec += 600;
        ssize_t s = mq_timedreceive(mqd, msg, sizeof(msg), &pri, &tm);
        if (s == -1) {
            if (errno == ETIMEDOUT) {
                if (dbConnected) {
                    mysql_close(sql_conn);
                    dbConnected = false;
                }
            } else {
                perror("child mq_receive");
                sleep(1);
            }
            continue;
        } else if (!dbConnected) {
            sql_conn = dbConnect(dbhostname, dbuser, dbpass, dbport, dbName);
            if (sql_conn == NULL) {
                printf("\e[31mchild: failed to reconnect\e[0m\n");
                continue;
            }
            dbConnected = true;
        }
        if (mysql_query(sql_conn, msg)) {
            unsigned err = mysql_errno(sql_conn);
            printf("\n\e[31m############ child %d: %s ##############\e[0m\n", err, mysql_error(sql_conn));
            break;
        }
    } // ..for (;;)

    int i = mq_close(mqd);
    printf("\e[31m ########### child-done %d ###########\e[0m\n", i);
    fflush(stdout);
    return 0;
}
