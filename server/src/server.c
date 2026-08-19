#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "cJSON.h"
#include "server.h"

char cmd[1024] = {0};
static int cli_socket_fd = -1;
static pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;

int send_command_to_device(const char *json_str, size_t len)
{
    if(json_str == NULL || len == 0) return -1;

    pthread_mutex_lock(&send_mutex);
    if(cli_socket_fd == -1)
    {
        pthread_mutex_unlock(&send_mutex);
        return -1;
    }
    int ret = send(cli_socket_fd, json_str, len, 0);
    pthread_mutex_unlock(&send_mutex);
    return ret;
}

/*接收客户端消息*/
void* recv_news(void* socket_fd)
{
    int* fd = (int*)socket_fd;
    char buf[1024] = {0};
    int ret = -1;
    while(1)
    {
        bzero(buf, sizeof(buf));
        ret = recv(*fd, buf, sizeof(buf), 0);
        if(ret == 0) break;

        cJSON *root = cJSON_Parse(buf);
        if(root)
        {
            cJSON *j_led  = cJSON_GetObjectItem(root, "LED");
            cJSON *j_beep = cJSON_GetObjectItem(root, "BEEP");
            cJSON *j_temp = cJSON_GetObjectItem(root, "temp");
            cJSON *j_humi = cJSON_GetObjectItem(root, "humi");
            cJSON *j_tt   = cJSON_GetObjectItem(root, "temp_threshold");
            cJSON *j_ht   = cJSON_GetObjectItem(root, "humi_threshold");

            /* 缺字段时给默认值 0，避免 NULL->valueint 段错误 */
            update_device_data(
                j_led  ? j_led->valueint    : 0,
                j_beep ? j_beep->valueint   : 0,
                j_temp ? (float)j_temp->valuedouble : 0.0f,
                j_humi ? (float)j_humi->valuedouble : 0.0f,
                j_tt   ? (float)j_tt->valuedouble   : 0.0f,
                j_ht   ? (float)j_ht->valuedouble   : 0.0f);
            cJSON_Delete(root);
        }
    }
    close(*fd);
    printf("client exited\n");
    return NULL;
}

void* tcp_server_thread(void* arg)
{
    int cli_fd[1024] = {-1};
    int cli_cnt = 0;
    printf("服务器线程启动\n");
    int optval = 1;
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1)
    {
        perror("socket");
        return NULL;
    }
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(8888);
    addr.sin_addr.s_addr = inet_addr("192.168.44.21");

    if(bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        close(socket_fd);
        return NULL;
    }
    printf("绑定成功\n");

    listen(socket_fd, 2);

    while(1)
    {
        struct sockaddr_in cli_addr;
        int len = sizeof(struct sockaddr);
        printf("等待客户端连接...\n");
        cli_socket_fd = accept(socket_fd, (struct sockaddr*)&cli_addr, &len);
        if(cli_socket_fd == -1)
        {
            perror("accept");
            close(socket_fd);
            return NULL;
        }
        cli_fd[cli_cnt] = cli_socket_fd;
        char* ip   = inet_ntoa(cli_addr.sin_addr);
        int   port = ntohs(cli_addr.sin_port);
        printf("设备连接: ip=%s port=%d\n", ip, port);

        printf("cli_fd[%d] = %d\n", cli_cnt, cli_fd[cli_cnt]);
        pthread_t recv_tid;
        pthread_create(&recv_tid, NULL, recv_news, (void*)&cli_fd[cli_cnt]);
        pthread_detach(recv_tid);
        cli_cnt++;
    }

    return NULL;
}
