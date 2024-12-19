#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <mysql/mysql.h>

#define BUF_SIZE 100
#define NAME_SIZE 30 
#define ARR_CNT 6 

void* send_msg(void* arg);
void* recv_msg(void* arg);

char name[NAME_SIZE] = "[Default]";
char msg[BUF_SIZE];

int main(int argc, char* argv[])
{
    int sock, sock_blue;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread, mysql_thread;
    void* thread_return;

    if (argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    sprintf(name, "%s", argv[3]);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    sprintf(msg, "[%s:PASSWD]", name);
    write(sock, msg, strlen(msg));
    pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
    pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);

    // Bluetooth 연결 스레드 생성
    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);

    close(sock);
    return 0;
}

void* send_msg(void* arg)
{
    int* sock = (int*)arg;
    int str_len;
    int ret;
    fd_set initset, newset;
    struct timeval tv;
    char name_msg[NAME_SIZE + BUF_SIZE + 2];

    FD_ZERO(&initset);
    FD_SET(STDIN_FILENO, &initset);

    fputs("Input a message! [ID]msg (Default ID:ALLMSG)\n", stdout);
    while (1) {
        memset(msg, 0, sizeof(msg));
        name_msg[0] = '\0';
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        newset = initset;
        ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);
        if (FD_ISSET(STDIN_FILENO, &newset)) {
            fgets(msg, BUF_SIZE, stdin);
            if (!strncmp(msg, "quit\n", 5)) {
                *sock = -1;
                return NULL;
            }
            else if (msg[0] != '[') {
                strcat(name_msg, "[ALLMSG]");
                strcat(name_msg, msg);
            }
            else
                strcpy(name_msg, msg);
            if (write(*sock, name_msg, strlen(name_msg)) <= 0) {
                *sock = -1;
                return NULL;
            }
        }
        if (ret == 0) {
            if (*sock == -1)
                return NULL;
        }
    }
}

void* recv_msg(void* arg)
{
    MYSQL* conn;
    MYSQL_ROW sqlrow;
    MYSQL_RES* res_set;
    int res;
    char sql_cmd[200] = { 0 };
    char* host = "localhost";
    char* user = "iot";
    char* pass = "pwiot";
    char* dbname = "iotdb";
    char sendmsg[200] = { 0 };
    int* sock = (int*)arg;
    int i;
    char* pToken;
    char* pArray[ARR_CNT] = { 0 };

    char name_msg[NAME_SIZE + BUF_SIZE + 1];
    int str_len;
    char* space;
    char* stat;
    int temp;
    int humi;
    conn = mysql_init(NULL);
    puts("MYSQL startup");
    if (!(mysql_real_connect(conn, host, user, pass, dbname, 0, NULL, 0))) {
        fprintf(stderr, "ERROR : %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        exit(1);
    }
    else
        printf("Connection Successful!\n\n");

    while (1) {
        memset(name_msg, 0x0, sizeof(name_msg));
        str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE);
        if (str_len <= 0) {
            *sock = -1;
            return NULL;
        }
        name_msg[str_len] = 0;
        fputs(name_msg, stdout);

        pToken = strtok(name_msg, "[:@]");
        i = 0;
        while (pToken != NULL) {
            pArray[i] = pToken;
            if (++i >= ARR_CNT)
                break;
            pToken = strtok(NULL, "[:@]");
        }

        if (!strcmp(pArray[1], "SERVICE") && (i == 6)) {
            stat = pArray[3];
            space = pArray[2];
            temp = (int)(atoi(pArray[4]) * 0.95 + 0.5);
            humi = atoi(pArray[5]);

            // 데이터가 300행을 초과하는지 확인
            sprintf(sql_cmd, "SELECT COUNT(*) FROM service");
            res = mysql_query(conn, sql_cmd);
            if (res) {
                fprintf(stderr, "ERROR while counting rows: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
                continue;
            }

            res_set = mysql_store_result(conn);
            sqlrow = mysql_fetch_row(res_set);
            if (sqlrow && atoi(sqlrow[0]) >= 300) {
                // 300행을 초과하면 가장 오래된 행 삭제
                sprintf(sql_cmd, "DELETE FROM service WHERE date = (SELECT date FROM (SELECT date FROM service ORDER BY date ASC LIMIT 1) AS temp_table)");
                res = mysql_query(conn, sql_cmd);
                if (res) {
                    fprintf(stderr, "ERROR while deleting old rows: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
                    mysql_free_result(res_set);
                    continue;
                }
            }
            mysql_free_result(res_set);

            // 새로운 데이터 삽입
            sprintf(sql_cmd, "INSERT INTO service(name, date, state, temp, humi) VALUES(\"%s\", now(), \"%s\", %d, %d)", space, stat, temp, humi);
            res = mysql_query(conn, sql_cmd);
            if (!res) {
                printf("Inserted %lu rows\n", (unsigned long)mysql_affected_rows(conn));
            }
            else {
                fprintf(stderr, "ERROR inserting row: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
            }

        }
        else {
            continue;
        }
    }

    mysql_close(conn);
}

void error_handling(char* msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
