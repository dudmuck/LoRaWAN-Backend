/* Copyright 2018 Wayne Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "as.h"

int
child(const char* mqName, const char* dbhostname, const char* dbuser, const char* dbpass, uint16_t dbport, const char *dbName)
{
    MYSQL *sql_conn;
    //struct mq_attr attr;
    char msg[MSGSIZE+1];
    unsigned pri;
    mqd_t mqrd = mq_open(mqName, O_RDONLY);
    if (mqrd == (mqd_t)-1) {
        perror("child mq_open");
        printf("mqName: %s\n", mqName);
        return -1;
    }

    sql_conn = mysql_init(NULL);
    if (sql_conn == NULL) {
        fprintf(stderr, "Failed to initialize: %s\n", mysql_error(sql_conn));
        return -1;
    }

    /* enable re-connect */
    my_bool reconnect = 1;
    if (mysql_options(sql_conn, MYSQL_OPT_RECONNECT, &reconnect) != 0) {
        fprintf(stderr, "mysql_options() failed\n");
        return -1;
    }

    printf("database connect %s\n", dbName);
    /* Connect to the server */
    if (!mysql_real_connect(sql_conn, dbhostname, dbuser, dbpass, dbName, dbport, NULL, 0))
    {
        fprintf(stderr, "Failed to connect to server: %s\n", mysql_error(sql_conn));
        return -1;
    }

    for (;;) {
        unsigned n;
        ssize_t s = mq_receive(mqrd, msg, sizeof(msg), &pri);
        if (s < 0) {
            perror("mq_receive");
            sleep(1);
            continue;
        }
        //printf("\e[7mchild %zu \"%s\" ", s, msg);
        if (mysql_query(sql_conn, msg)) {
            unsigned err = mysql_errno(sql_conn);
            printf("\n\e[31m############ child %d: %s ##############\e[0m\n", err, mysql_error(sql_conn));
            printf("\e[31m%s\e[0m\n", msg);
            break;
        }
        n = mysql_affected_rows(sql_conn);
        if (n == 0)
            printf("\e[31m");
        //printf(" %u affected rows\e[0m\n", n);
    }
    int i = mq_close(mqrd);
    printf("\e[31m ########### child-done %d ###########\e[0m\n", i);
    fflush(stdout);
    return 0;
}

