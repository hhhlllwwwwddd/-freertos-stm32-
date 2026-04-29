#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "cJSON.h"

#define PORT 8081
#define BUFFER_SIZE 1024

// 检查客户端发送的数据格式是否合法（是否是有效json格式）
int get_dev_mac(const char* data, char dev_name[])
{
    cJSON *json = cJSON_Parse(data);
    if (json == NULL) {
        // 无效的 JSON
        return 0;
    }
    // 检查 "name" 字段
    cJSON *name = cJSON_GetObjectItem(json, "name");
    if (!cJSON_IsString(name) || (name->valuestring == NULL)) {
        cJSON_Delete(json);
        return 0;  // "name" 字段无效
    }
    // 拷贝 "name" 的值到 dev_name
    strncpy(dev_name, name->valuestring, 31);
    dev_name[31] = '\0';  // 确保 dev_name 以空字符结尾

    // 释放解析后的 JSON 对象
    cJSON_Delete(json);
    return 1;  // 有效的 JSON
}
// 追加一行到文件结尾
void append_line_to_file(const char *filename, const char* line)
{
    // 打开文件，模式为 "a" 表示追加内容到文件末尾
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        printf("fopen failed, [%d], %s\n", errno, strerror(errno));
        return ;
    }

    // 写入一行数据到文件末尾，注意，json数据没有\r\n结尾
    fprintf(file, "%s\n", line);

    // 关闭文件
    fclose(file);

    printf("write [%s] to [%s] successfully\n\n", line, filename);
}

int verify_cmd_to_client(char* data, const char* dev_name)
{
    cJSON *json = cJSON_Parse(data);
    if (json == NULL) {
        printf("Error parsing JSON\n");
        return 0;
    }

    // 解析设备名称
    cJSON *name = cJSON_GetObjectItem(json, "name");
    if (cJSON_IsString(name) && (name->valuestring != NULL)) {
        if(strcmp(dev_name, name->valuestring) != 0)
            return 0;
    } else {
        return 0;
    }

    // 解析时间戳
    time_t timestamp = time(NULL);
    cJSON *time_json = cJSON_GetObjectItem(json, "time");
    if (cJSON_IsNumber(time_json)) {
        if(timestamp - time_json->valueint > 6) {// 如果是6秒内的命令，则认为是有效命令
            printf("cmd time out\n\n");
            return 0;
        }
    } else {
        return 0; // 没有时间戳，解析错误
    }

    cJSON_Delete(json);
    return 1;
}
// 读取文件的最后一行，因为最后一行是最新数据
void read_last_line_from_file(const char *filename, char* last_line_buf, int last_line_bufsz)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("fopen failed, [%d], %s\n", errno, strerror(errno));
        return;
    }

    char line[256];

    // 循环读取每一行，直到最后一行，并保存到 `last_line_buf`
    while (fgets(line, sizeof(line), file) != NULL && strlen(line) > 0) {
        //printf("line:%s\n", line);
        strncpy(last_line_buf, line, last_line_bufsz);  // 记录当前行为最后一行
    }

    fclose(file);

    // 日志输出文件的最后一行
    printf("last line:%s", last_line_buf);
}

void send_time_to_client(int client_sockfd, time_t t)
{
    char buf[BUFFER_SIZE] = {0};
    
    sprintf(buf, "{\"cmd\":\"time response\",\"time\":%ld}", t);

    // strlen 计算的字符串长度包含 \r\n
    send(client_sockfd, buf, strlen(buf), 0);
}

int send_cmd_to_client(int client_sockfd, const char* dev_name)
{
    char file_name[48] = {0};
    snprintf(file_name, sizeof(file_name), "%s%s%s", "cmd_", dev_name, ".json");

    char last_line[512] = {0};
    read_last_line_from_file(file_name, last_line, sizeof(last_line));
    
    if(!verify_cmd_to_client(last_line, dev_name)) {
        return -1;
    }
    printf("send cmd %s to client\n", last_line);
    send(client_sockfd, last_line, strlen(last_line), 0);

    return 0;
}
// 判断字符串是否为合法 JSON，并解析 "cmd" 字段
// 解析成功返回1，否则返回0
int parse_json_cmd(const char *json_str, char *cmd_out, size_t cmd_out_size)
{
    // 检查输入参数的合法性
    if (!json_str || !cmd_out || cmd_out_size == 0) {
        return 0; // 参数无效
    }

    // 尝试解析 JSON
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        return 0; // 解析失败
    }

    // 尝试获取 "cmd" 字段
    cJSON *cmd = cJSON_GetObjectItem(json, "cmd");
    if (!cmd || !cJSON_IsString(cmd)) {
        cJSON_Delete(json);
        return 0; // 没有 "cmd" 字段，或字段类型不是字符串
    }

    // 将 "cmd" 的值复制到输出缓冲区（确保不会越界）
    strncpy(cmd_out, cmd->valuestring, cmd_out_size - 1);
    cmd_out[cmd_out_size - 1] = '\0'; // 确保字符串以 '\0' 结尾

    // 释放 JSON 对象
    cJSON_Delete(json);

    // 返回成功标志
    return 1;
}

int handle_client_json(int client_sockfd, const char* json_str)
{
    char cmd[32] = {0}; // 用于存储解析到的 cmd 值

    if (parse_json_cmd(json_str, cmd, sizeof(cmd))) {
        printf("receive cmd [%s] from json:%s\n", cmd, json_str);
        
        if (strcmp(cmd, "time request") == 0) {
            time_t timestamp = time(NULL);
            send_time_to_client(client_sockfd, timestamp);
        } else if (strcmp(cmd, "upload data") == 0) {
            char dev_name[32] = {0};

            if(get_dev_mac(json_str, dev_name)) {
                char file_name[48] = {0};
                snprintf(file_name, sizeof(file_name), "%s%s%s", "data_", dev_name, ".json");
                append_line_to_file(file_name, json_str);
            } else {// 数据不合法，不再继续接收
                printf("data format wrong!\n");
            }
            send_cmd_to_client(client_sockfd, dev_name);
        } else {
            printf("cmd not support\n");
        }

        return 0;
    } else {
        // 没有收到一条完整的json
        return -1;
    }
}

void handle_client(int client_sockfd)
{
    char recv_buf[BUFFER_SIZE];      // 接收缓冲区
    int total_len = 0;               // 缓冲区收到数据总长度
    int recv_len;                    // 单次接收数据长度

    while (1) {
        // 阻塞接收数据
        recv_len = recv(client_sockfd, recv_buf + total_len, sizeof(recv_buf) - total_len - 1, 0);

        if (recv_len < 0) {
            printf("recv failed, [%d], %s\n", errno, strerror(errno));
            return ;
        } else if (recv_len == 0) {
            printf("tcp closed\n");
            return ;
        } else {
            total_len += recv_len;
            recv_buf[total_len] = '\0'; // 确保字符串以NULL结尾
            //printf("receive [%d] bytes data:%s\n", total_len, recv_buf);
            if (handle_client_json(client_sockfd, recv_buf) == 0) {
                total_len = 0;            // 清空缓冲区
            }  else {
                // 收到的数据不是一个完整 json 字符串
                //printf("Invalid JSON, waiting for more data...\n");
            }
        }
    }
}

int main(void)
{
    int server_sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // 创建一个 TCP IPv4 套接字
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (server_sockfd == -1) {
        printf("Failed to create server socket, [%d], %s\n", errno, strerror(errno));
        return -1;
    }
    printf("file descriptor of server socket is [%d]\n", server_sockfd);

    int opt = 1; // 套接字选项
    // 设置 SO_REUSEADDR 选项
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        printf("setsockopt failed, [%d], %s\n", errno, strerror(errno));
        close(server_sockfd);
        return -1;
    }

    // 套接字的地址协议族, AF_INET表示 IPv4 地址
    server_addr.sin_family = AF_INET;
    // 套接字的IP地址，INADDR_ANY 相当于 0.0.0.0
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // 套接字的端口号，需要转换为网络序
    server_addr.sin_port = htons(PORT);
    // 将套接字绑定到指定的地址和端口
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed to bind, [%d], %s\n", errno, strerror(errno));
        close(server_sockfd);
        return -1;
    }
    printf("bind to server %s:%u successfully!\n", "0.0.0.0", PORT);

    // 在绑定的套接字上监听连接请求
    if (listen(server_sockfd, 2) < 0) {
        printf("Failed to listen server, [%d], %s\n", errno, strerror(errno));
        close(server_sockfd);
        return -1;
    }
    printf("listen to server %s:%u successfully!\n", "0.0.0.0", PORT);

    while (1) {
        // 阻塞接收客户端连接
        client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sockfd < 0) {
            printf("accept failed, [%d], %s\n", errno, strerror(errno));
            continue;
        }
        // 当有客户端发起连接时，退出阻塞
        printf("client socket [%d], ip: %s, port: %d connect successfully\n", 
            client_sockfd, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
        // 处理客户端请求
        handle_client(client_sockfd);
        
        // 关闭客户端连接
        close(client_sockfd);
    }

    // 关闭服务端 Socket
    if (close(server_sockfd) == -1) {
        printf("Failed to close server socket, [%d], %s\n", errno, strerror(errno));
        return -1;
    }
    printf("server socket [%d] closed successfully\n", server_sockfd);
    return 0;
}
