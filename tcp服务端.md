# TCP服务端函数解析

本文档详细解析 `ggcloud_tcp_server_v2.c` 文件中各个函数的作用和实现细节。

## 宏定义

- **`PORT 8081`**: 定义TCP服务器监听的端口号为8081
- **`BUFFER_SIZE 1024`**: 定义接收缓冲区大小为1024字节

## 主要函数详解

### 1. `get_dev_mac(const char* data, char dev_name[])`

**功能**: 解析客户端发送的JSON数据，提取设备名称（"name"字段）

**参数**:
- `data`: 待解析的JSON字符串
- `dev_name`: 用于存储提取出的设备名称的缓冲区

**返回值**:
- `1`: 成功解析并提取设备名称
- `0`: JSON格式无效或"name"字段不存在/无效

**实现细节**:
1. 使用 `cJSON_Parse()` 解析输入的JSON字符串
2. 检查解析是否成功，失败则返回0
3. 使用 `cJSON_GetObjectItem()` 获取"name"字段
4. 验证"name"字段是否为有效的字符串类型
5. 使用 `strncpy()` 将设备名称复制到输出缓冲区，并确保以空字符结尾
6. 释放JSON对象内存，避免内存泄漏

### 2. `append_line_to_file(const char *filename, const char* line)`

**功能**: 将一行数据追加到指定文件末尾

**参数**:
- `filename`: 目标文件名
- `line`: 要写入的数据行

**实现细节**:
1. 以追加模式("a")打开文件
2. 使用 `fprintf()` 将数据写入文件，并自动添加换行符
3. 关闭文件
4. 包含错误处理，当文件打开失败时打印错误信息

### 3. `verify_cmd_to_client(char* data, const char* dev_name)`

**功能**: 验证命令的有效性，包括设备名称匹配和时间戳检查

**参数**:
- `data`: 待验证的JSON命令字符串
- `dev_name`: 期望的设备名称

**返回值**:
- `1`: 命令有效
- `0`: 命令无效

**实现细节**:
1. 解析JSON数据
2. 验证"name"字段与传入的`dev_name`是否匹配
3. 获取"time"字段的时间戳
4. 检查命令时间戳是否在6秒内（防止过期命令执行）
5. 如果时间戳超过6秒，认为命令超时无效

### 4. `read_last_line_from_file(const char *filename, char* last_line_buf, int last_line_bufsz)`

**功能**: 读取指定文件的最后一行内容

**参数**:
- `filename`: 源文件名
- `last_line_buf`: 存储最后一行内容的缓冲区
- `last_line_bufsz`: 缓冲区大小

**实现细节**:
1. 以只读模式打开文件
2. 循环读取每一行，覆盖之前的行内容
3. 最终`last_line_buf`中保存的是文件的最后一行
4. 包含错误处理和日志输出

### 5. `send_time_to_client(int client_sockfd, time_t t)`

**功能**: 向客户端发送时间响应

**参数**:
- `client_sockfd`: 客户端socket文件描述符
- `t`: 当前时间戳

**实现细节**:
1. 构造JSON响应字符串：`{"cmd":"time response","time":timestamp}`
2. 使用 `send()` 函数将响应发送给客户端

### 6. `send_cmd_to_client(int client_sockfd, const char* dev_name)`

**功能**: 向客户端发送该设备的最新命令

**参数**:
- `client_sockfd`: 客户端socket文件描述符
- `dev_name`: 设备名称

**返回值**:
- `0`: 发送成功
- `-1`: 发送失败或命令无效

**实现细节**:
1. 根据设备名称构造命令文件名：`cmd_{dev_name}.json`
2. 读取该命令文件的最后一行
3. 验证命令的有效性（调用`verify_cmd_to_client`）
4. 如果命令有效，发送给客户端

### 7. `parse_json_cmd(const char *json_str, char *cmd_out, size_t cmd_out_size)`

**功能**: 解析JSON字符串中的"cmd"字段

**参数**:
- `json_str`: 待解析的JSON字符串
- `cmd_out`: 存储解析出的命令字符串的缓冲区
- `cmd_out_size`: 缓冲区大小

**返回值**:
- `1`: 解析成功
- `0`: 解析失败

**实现细节**:
1. 参数合法性检查
2. 使用cJSON库解析JSON
3. 提取"cmd"字段并验证其为字符串类型
4. 安全地复制命令字符串到输出缓冲区（防止缓冲区溢出）
5. 释放JSON对象内存

### 8. `handle_client_json(int client_sockfd, const char* json_str)`

**功能**: 处理客户端发送的JSON命令

**参数**:
- `client_sockfd`: 客户端socket文件描述符
- `json_str`: 完整的JSON字符串

**返回值**:
- `0`: 处理成功
- `-1`: JSON格式无效

**实现细节**:
1. 调用`parse_json_cmd`解析命令类型
2. 根据不同的命令类型执行相应操作：
   - `"time request"`: 调用`send_time_to_client`发送当前时间
   - `"upload data"`: 
     - 调用`get_dev_mac`获取设备名称
     - 将数据保存到`data_{dev_name}.json`文件
     - 调用`send_cmd_to_client`发送该设备的最新命令
   - 其他命令：打印不支持提示
3. 返回处理结果

### 9. `handle_client(int client_sockfd)`

**功能**: 处理单个客户端连接的主循环

**参数**:
- `client_sockfd`: 客户端socket文件描述符

**实现细节**:
1. 初始化接收缓冲区和长度计数器
2. 进入无限循环，持续接收客户端数据
3. 使用`recv()`阻塞接收数据
4. 错误处理：
   - 接收失败：打印错误并返回
   - 连接关闭（recv_len == 0）：打印信息并返回
5. 将接收到的数据拼接到缓冲区，并确保字符串以NULL结尾
6. 调用`handle_client_json`处理完整的JSON数据
7. 如果处理成功，清空缓冲区准备接收下一条消息
8. 如果JSON不完整，继续等待更多数据（支持分包接收）

### 10. `main(void)`

**功能**: TCP服务器主函数

**实现细节**:
1. 创建TCP socket（IPv4，流式）
2. 设置`SO_REUSEADDR`选项，允许快速重启服务器
3. 配置服务器地址结构：
   - 地址族：AF_INET（IPv4）
   - IP地址：INADDR_ANY（监听所有接口）
   - 端口：8081（转换为网络字节序）
4. 绑定socket到指定地址和端口
5. 开始监听连接请求（最大队列长度为2）
6. 进入主循环：
   - 使用`accept()`阻塞等待客户端连接
   - 连接成功后打印客户端信息（IP和端口）
   - 调用`handle_client`处理客户端请求
   - 客户端断开连接后关闭客户端socket
7. 程序退出时关闭服务器socket

## 整体架构特点

1. **基于JSON的通信协议**: 所有客户端和服务端通信都使用JSON格式
2. **文件存储机制**: 
   - 设备数据存储在`data_{设备名}.json`文件中
   - 设备命令存储在`cmd_{设备名}.json`文件中
3. **命令时效性**: 命令必须在6秒内执行，防止过期命令
4. **分包处理**: 支持接收不完整的JSON数据，等待后续数据包完成
5. **设备绑定**: 通过设备名称（"name"字段）区分不同设备的数据和命令
6. **简单可靠**: 采用同步阻塞I/O模型，代码简洁易懂

## 支持的命令类型

1. **`time request`**: 客户端请求服务器时间，服务器回复当前时间戳
2. **`upload data`**: 客户端上传设备数据，服务器保存数据并回复最新命令

## 数据流向

1. 客户端连接 → 服务器接受连接
2. 客户端发送JSON命令 → 服务器接收并解析
3. 根据命令类型执行相应操作：
   - 时间请求 → 回复时间
   - 数据上传 → 保存数据 + 发送命令
4. 客户端断开 → 服务器关闭连接，等待新连接