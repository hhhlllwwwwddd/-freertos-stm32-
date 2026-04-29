# HTTP服务端代码分析

## 文件概述

`ggcloud_http_server_v2_bike.c` 是一个基于C语言实现的HTTP服务器，主要用于显示和管理物联网设备数据，支持温湿度计、电风扇和骑行码表三种设备类型。

## 数据结构定义

### th_data_t - 温湿度计数据结构

```c
typedef struct {
    float temp;             // 温度
    float humidity;         // 湿度
} th_data_t;
```

用于存储温湿度计的实时数据。

### fan_data_t - 电风扇数据结构

```c
typedef struct {
    float temp;             // 温度
    float humidity;         // 湿度
    int speed;              // 风扇转速
    int light;              // 灯光状态（1：开，0：关）
    char on_time[6];        // 风扇定时开启时间，格式 "HH:MM"
    char on_temp[6];        // 启动风扇的温度阈值
    int off_duration;       // 定时关闭的持续时间（分钟）
} fan_data_t;
```

用于存储电风扇的实时状态和控制参数。

### bike_data_t - 骑行码表数据结构

```c
typedef struct {
    float total_distance_km;              // 骑行距离，单位：千米
    uint32_t total_time_sec;              // 骑行时间，单位：秒
    float speed_kmh;                      // 实时速度，单位：千米每小时
    float max_speed_kmh;                  // 最大时速
    float average_speed_kmh;              // 平均时速
    float humi;                           // 实时湿度
    float temp;                           // 实时温度
    float kcal;                           // 消耗卡路里
    float latitude;                       // 实时纬度的原始数据，十进制gcj02
    float longitude;                      // 实时经度的原始数据，十进制gcj02
} bike_data_t;
```

用于存储骑行码表的实时骑行数据和环境数据。

### dev_info_t - 设备信息结构

```c
typedef struct {
    char dev_name[32];     // 设备名称
    char dev_type[8];      // 设备类型，目前支持fan、th、bike
    time_t timestamp;      // 最新数据的更新时间戳
    union {
        fan_data_t fan;
        th_data_t th;
        bike_data_t bike;
    } d;                   // 数据字段，根据设备类型区分
} dev_info_t;
```

通用的设备信息结构，使用union来存储不同类型设备的数据。

## 核心函数分析

### 1. timestamp_to_time_str()

**功能**：将时间戳转换为格式化的时间字符串

**实现细节**：

- 使用静态数组存储结果，避免使用全局变量
- 调用`localtime()`将时间戳转换为本地时间结构
- 格式化输出为"YYYY-MM-DD HH:MM:SS"格式
- 返回静态字符串指针，调用者无需释放内存

**参数**：

- `timestamp`: time_t类型的时间戳

**返回值**：

- 格式化的时间字符串指针

---

### 2. read_last_line_from_file()

**功能**：读取文件的最后一行（最新数据）

**实现细节**：

- 以只读模式打开文件
- 使用`fgets()`循环读取每一行，每次都用新内容覆盖缓冲区
- 最终缓冲区中保存的就是文件的最后一行
- 处理文件打开失败的情况，输出错误信息

**参数**：

- `filename`: 文件路径
- `last_line_buf`: 存储最后一行的缓冲区
- `last_line_bufsz`: 缓冲区大小

**特点**：

- 简单高效，但需要完整读取文件
- 适合存储设备历史数据的小文件

---

### 3. decode_json_to_dev() 【重点函数】

**功能**：将JSON字符串解析为设备信息结构

**实现细节**：

1. **JSON解析**：使用cJSON库解析JSON字符串
2. **基础字段解析**：
   - 解析设备名称(`name`)
   - 解析设备类型(`type`)
   - 解析时间戳(`time`)
3. **数据字段解析**：根据设备类型分别解析

**电风扇数据解析**：

- `temp`: 温度
- `humidity`: 湿度
- `speed`: 风扇转速
- `light`: 灯光状态
- `on_time`: 定时开启时间
- `on_temp`: 温度阈值
- `off_duration`: 定时关闭时长

**温湿度计数据解析**：

- `temp`: 温度
- `humidity`: 湿度

**骑行码表数据解析**：

- `temp`: 温度
- `humidity`: 湿度
- `speed`: 实时速度
- `distance`: 骑行距离
- `total_time`: 骑行时间
- `max_speed`: 最大速度
- `average_speed`: 平均速度
- `kcal`: 消耗卡路里
- `lat`: 纬度
- `lng`: 经度

**参数**：

- `json_string`: JSON格式字符串
- `dev`: 设备信息结构指针

**返回值**：

- 1: 解析成功
- 0: 解析失败或未知设备类型
- -1: JSON解析错误

---

### 4. encode_dev_to_json() 【重点函数】

**功能**：将设备信息结构编码为JSON字符串

**实现细节**：

1. **创建JSON对象**：使用cJSON创建根对象
2. **添加基础字段**：
   - `cmd`: 固定为"download cmd"
   - `name`: 设备名称
   - `type`: 设备类型
   - `time`: 时间戳
3. **添加数据字段**：根据设备类型添加相应的控制数据
4. **转换为字符串**：使用`cJSON_PrintUnformatted()`生成紧凑的JSON字符串
5. **内存管理**：释放JSON对象，返回字符串指针

**电风扇JSON数据**：

- `speed`: 风扇转速
- `light`: 灯光状态
- `on_time`: 定时开启时间
- `on_temp`: 温度阈值
- `off_duration`: 定时关闭时长

**温湿度计**：

- 无需下发的控制数据（注释掉了temp和humidity）

**参数**：

- `dev`: 设备信息结构指针

**返回值**：

- JSON字符串指针（需要调用者释放）
- NULL: 创建失败

---

### 5. generate_html_body_th()

**功能**：生成温湿度计的HTML显示内容

**实现细节**：

- 使用全局缓冲区`html_buf`存储HTML内容
- 显示设备名称、温度、湿度和更新时间
- 使用`timestamp_to_time_str()`格式化时间戳
- 添加水平分割线分隔不同设备

**参数**：

- `dev`: 设备信息结构指针

**返回值**：

- HTML内容字符串指针

---

### 6. generate_html_body_fan()

**功能**：生成电风扇的HTML控制界面

**实现细节**：

- 显示设备基本信息（名称、温度、湿度、更新时间）
- 创建HTML表单，支持POST请求到"/fan"
- 包含隐藏字段：设备名称
- 提供控制选项：
  - **灯光开关**：下拉选择框（开启/关闭）
  - **风速控制**：数字输入框（0~4）
  - **温度阈值**：文本输入框
  - **定时开启**：时间选择器
  - **倒计时关闭**：下拉选择框（不启用/30分/1小时/1.5小时/2小时）
- 动态设置表单控件的默认值（根据当前设备状态）

**参数**：

- `dev`: 设备信息结构指针

**返回值**：

- HTML内容字符串指针

---

### 7. seconds_to_hhmmss()

**功能**：将秒数转换为HH:MM:SS格式的时间字符串

**实现细节**：

- 计算小时数：`seconds / 3600`
- 计算分钟数：`(seconds % 3600) / 60`
- 计算秒数：`seconds % 60`
- 格式化输出为两位数格式

**参数**：

- `seconds`: 总秒数
- `result`: 存储结果的缓冲区

**返回值**：

- 格式化的时间字符串指针

---

### 8. generate_html_body_bike() 【重点函数】

**功能**：生成骑行码表的HTML显示界面，包含实时地图

**实现细节**：

1. **数据显示**：
   - 设备名称、温度、湿度
   - 实时速度、骑行距离、骑行时长
   - 最大速度、平均速度、消耗卡路里
   - 实时经纬度、更新时间

2. **控制按钮**：
   - "刷新数据"按钮：开始定时刷新
   - "停止刷新"按钮：停止定时刷新

3. **地图集成**：
   - 使用高德地图JavaScript API
   - 集成`AMap.MoveAnimation`插件实现轨迹动画
   - 初始化地图容器，设置中心点和缩放级别
   - 创建标记点（marker）显示当前位置

4. **定时刷新机制**：
   - 使用`setInterval()`每3秒获取一次数据
   - 通过`fetch()`请求当前页面获取最新数据
   - 使用DOM解析器提取经纬度信息
   - 更新地图标记位置和页面数据显示
   - 实现轨迹回放功能

5. **数据处理**：
   - 过滤无效坐标（0.0, 0.0）
   - 维护轨迹数组`lineArr`
   - 自动旋转标记点方向
   - 动态更新所有数据字段

**参数**：

- `dev`: 设备信息结构指针

**返回值**：

- HTML内容字符串指针（包含JavaScript代码）

**特点**：

- 集成了完整的地图可视化功能
- 实现了实时数据刷新机制
- 提供了轨迹回放功能
- 需要配置高德地图API密钥

---

### 9. send_html_body()

**功能**：根据设备类型发送相应的HTML内容

**实现细节**：

1. **读取设备数据**：
   - 调用`read_last_line_from_file()`读取文件最后一行
   - 调用`decode_json_to_dev()`解析JSON数据

2. **生成HTML内容**：
   - 根据设备类型调用相应的生成函数
   - `fan`: 调用`generate_html_body_fan()`
   - `th`: 调用`generate_html_body_th()`
   - `bike`: 调用`generate_html_body_bike()`

3. **发送数据**：
   - 使用`send()`函数将HTML内容发送给客户端

**参数**：

- `filename`: 设备数据文件名
- `client_sockfd`: 客户端socket描述符

**特点**：

- 统一的HTML内容发送接口
- 自动处理不同设备类型
- 错误处理和日志输出

---

### 10. traverse_dir() 【重点函数】

**功能**：遍历目录，查找并处理设备数据文件

**实现细节**：

1. **目录操作**：
   - 使用`opendir()`打开指定目录
   - 使用`readdir()`循环读取目录项

2. **文件过滤**：
   - 过滤掉当前目录(`.`)和父目录(`..`)
   - 查找以`data_dev`开头的文件
   - 检查文件扩展名是否为`.json`

3. **文件处理**：
   - 对每个匹配的文件调用`send_html_body()`函数
   - 将生成的HTML内容发送给客户端

4. **资源清理**：
   - 使用`closedir()`关闭目录

**参数**：

- `dir_path`: 要遍历的目录路径
- `send_html_body`: 处理文件的函数指针
- `client_sockfd`: 客户端socket描述符

**返回值**：

- 0: 成功
- -1: 目录打开失败

**特点**：

- 支持批量处理多个设备文件
- 灵活的文件名匹配机制
- 统一的错误处理

---

### 11. send_html() 【重点函数】

**功能**：发送完整的HTTP响应

**实现细节**：

1. **发送HTTP响应头**：
   - 状态行：`HTTP/1.1 200 OK`
   - 内容类型：`text/html; charset=utf-8`
   - 必须的空行分隔头部和主体

2. **发送HTML头部**：
   - HTML文档开始标签
   - 头部元信息（title、style）
   - 设置页面和地图容器的样式

3. **遍历设备目录**：
   - 调用`traverse_dir()`处理当前目录下的所有设备文件
   - 为每个设备生成并发送HTML内容

**参数**：

- `client_sockfd`: 客户端socket描述符

**HTTP响应格式**：

```
HTTP/1.1 200 OK\r\n
Content-Type: text/html; charset=utf-8\r\n
\r\n
<html>
<head>
    <title>果果云</title>
    <style>...</style>
</head>
<body>
    <!-- 设备HTML内容 -->
</body>
</html>
```

**特点**：

- 完整的HTTP协议实现
- 支持多设备显示
- 统一的页面样式

---

## 程序工作流程

1. **启动服务器**：监听8080端口
2. **接收客户端连接**：建立TCP连接
3. **处理HTTP请求**：
   - 发送HTTP响应头
   - 发送HTML头部
   - 遍历设备数据文件
   - 为每个设备生成HTML内容
   - 发送完整的HTML页面
4. **客户端显示**：浏览器渲染HTML页面
5. **交互控制**：
   - 电风扇：通过表单提交控制参数
   - 骑行码表：通过JavaScript定时刷新数据
   - 温湿度计：仅显示数据

## 技术特点

1. **模块化设计**：每个功能都有独立的函数
2. **多设备支持**：使用union实现不同设备类型的数据存储
3. **JSON数据处理**：使用cJSON库进行JSON解析和生成
4. **动态HTML生成**：根据设备数据动态生成HTML内容
5. **地图集成**：集成高德地图API实现轨迹可视化
6. **实时更新**：JavaScript定时刷新机制
7. **文件系统交互**：读取JSON格式的设备数据文件

## 依赖库

- **cJSON**: JSON数据解析和生成库
- **标准C库**: 文件操作、字符串处理、网络编程等

## 配置说明

- **端口**: 8080
- **数据文件**: 以`data_dev`开头、`.json`结尾的文件
- **高德地图API**: 需要配置有效的API密钥

## 注意事项

1. 文件读取需要完整的文件路径
2. JSON数据格式必须符合预期结构
3. 高德地图API密钥需要替换为有效值
4. 缓冲区大小需要根据实际数据量调整
5. 网络通信需要处理各种错误情况
