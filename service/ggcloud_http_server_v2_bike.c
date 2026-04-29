#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "cJSON.h"

#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct {
    float temp;             // 温度
    float humidity;         // 湿度
} th_data_t;                // 温湿度计的数据

typedef struct {
    float temp;             // 温度
    float humidity;         // 湿度
    int speed;              // 风扇转速
    int light;              // 灯光状态（1：开，0：关）
    char on_time[6];        // 风扇定时开启时间，格式 "HH:MM"
    char on_temp[6];        // 启动风扇的温度阈值，当环境温度高于此，则自动开启风扇
    int off_duration;       // 定时关闭的持续时间（分钟），范围：30/60/90/120，单位：分钟。
} fan_data_t;               // 电风扇的数据

typedef struct {
    float               total_distance_km;              /**< 骑行距离，单位：千米 */
    uint32_t            total_time_sec;                 /**< 骑行时间，单位：秒 */
    float               speed_kmh;                      /**< 实时速度，单位：千米每小时 */
    float               max_speed_kmh;                  /**< 最大时速 */
    float               average_speed_kmh;              /**< 平均时速 */
    float               humi;                           /**< 实时湿度 */
    float               temp;                           /**< 实时温度 */
    float               kcal;                           /**< 消耗卡路里 */
    float               latitude;                       /**< 实时纬度的原始数据，十进制gcj02 */
    float               longitude;                      /**< 实时经度的原始数据，十进制gcj02 */
} bike_data_t;              // 码表的数据

typedef struct {
    char dev_name[32];     // 设备名称
    char dev_type[8];      // 设备类型，目前支持fan、th
    time_t timestamp;      // 最新数据的更新时间戳
    union {
        fan_data_t fan;
        th_data_t th;
        bike_data_t bike;
    } d;                   // 数据字段，根据设备类型区分
} dev_info_t;

char* timestamp_to_time_str(time_t timestamp)
{
    // 这里使用静态数组，在函数退出后还能使用
    // 还能避免使用全局变量造成程序混乱
    static char time_str[18];
    struct tm *p_time;
    p_time = localtime(&timestamp);
    
    memset(time_str, 0, sizeof(time_str));
    sprintf(time_str, "%02d-%02d-%02d %02d:%02d:%02d",
            1900 + p_time->tm_year, 1 + p_time->tm_mon, p_time->tm_mday,  
            p_time->tm_hour, p_time->tm_min, p_time->tm_sec);

    return time_str;
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
    printf("last line:%s\n", last_line_buf);
}

int decode_json_to_dev(char* json_string, dev_info_t* dev)
{
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        printf("Error parsing JSON\n");
        return -1;
    }

    // 解析设备名称
    cJSON *name = cJSON_GetObjectItem(json, "name");
    if (cJSON_IsString(name) && (name->valuestring != NULL)) {
        strncpy(dev->dev_name, name->valuestring, sizeof(dev->dev_name) - 1);
        dev->dev_name[sizeof(dev->dev_name) - 1] = '\0';
    }

    // 解析设备类型
    cJSON *type = cJSON_GetObjectItem(json, "type");
    if (cJSON_IsString(type) && (type->valuestring != NULL)) {
        strncpy(dev->dev_type, type->valuestring, sizeof(dev->dev_type) - 1);
        dev->dev_type[sizeof(dev->dev_type) - 1] = '\0';
    }

    // 解析时间戳
    cJSON *time = cJSON_GetObjectItem(json, "time");
    if (cJSON_IsNumber(time)) {
        dev->timestamp = time->valueint;
    }

    // 解析数据字段
    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (data == NULL) {
        cJSON_Delete(json);
        printf("Error: data field is missing\n");
        return 0;
    }

    // 根据设备类型解析相应的数据
    if (strcmp(dev->dev_type, "fan") == 0) {
        // 电风扇数据解析
        cJSON *temp = cJSON_GetObjectItem(data, "temp");
        cJSON *humidity = cJSON_GetObjectItem(data, "humidity");
        cJSON *speed = cJSON_GetObjectItem(data, "speed");
        cJSON *light = cJSON_GetObjectItem(data, "light");
        cJSON *on_time = cJSON_GetObjectItem(data, "on_time");
        cJSON *on_temp = cJSON_GetObjectItem(data, "on_temp");
        cJSON *off_duration = cJSON_GetObjectItem(data, "off_duration");

        if (cJSON_IsNumber(temp))
            dev->d.fan.temp = temp->valuedouble;
        if (cJSON_IsNumber(humidity))
            dev->d.fan.humidity = humidity->valuedouble;
        if (cJSON_IsNumber(speed))
            dev->d.fan.speed = speed->valueint;
        if (cJSON_IsNumber(light))
            dev->d.fan.light = light->valueint;
        if (cJSON_IsString(on_time) && (on_time->valuestring != NULL)) {
            strncpy(dev->d.fan.on_time, on_time->valuestring, sizeof(dev->d.fan.on_time) - 1);
            dev->d.fan.on_time[sizeof(dev->d.fan.on_time) - 1] = '\0';
        }
        if (cJSON_IsString(on_temp) && (on_temp->valuestring != NULL)) {
            strncpy(dev->d.fan.on_temp, on_temp->valuestring, sizeof(dev->d.fan.on_temp) - 1);
            dev->d.fan.on_temp[sizeof(dev->d.fan.on_temp) - 1] = '\0';
        }
        if (cJSON_IsNumber(off_duration))
            dev->d.fan.off_duration = off_duration->valueint;
    }
    else if (strcmp(dev->dev_type, "th") == 0) {
        // 温湿度计数据解析
        cJSON *temp = cJSON_GetObjectItem(data, "temp");
        cJSON *humidity = cJSON_GetObjectItem(data, "humidity");

        if (cJSON_IsNumber(temp)) 
            dev->d.th.temp = temp->valuedouble;
        if (cJSON_IsNumber(humidity)) 
            dev->d.th.humidity = humidity->valuedouble;
    } else if (strcmp(dev->dev_type, "bike") == 0) {
        cJSON *temp = cJSON_GetObjectItem(data, "temp");
        cJSON *humidity = cJSON_GetObjectItem(data, "humidity");

        cJSON *speed = cJSON_GetObjectItem(data, "speed");
        cJSON *distance = cJSON_GetObjectItem(data, "distance");
        cJSON *total_time = cJSON_GetObjectItem(data, "total_time");

        cJSON *max_speed = cJSON_GetObjectItem(data, "max_speed");
        cJSON *average_speed = cJSON_GetObjectItem(data, "average_speed");
        cJSON *kcal = cJSON_GetObjectItem(data, "kcal");
        cJSON *lat = cJSON_GetObjectItem(data, "lat");
        cJSON *lng = cJSON_GetObjectItem(data, "lng");

        if (cJSON_IsNumber(temp)) 
            dev->d.bike.temp = temp->valuedouble;
        if (cJSON_IsNumber(humidity)) 
            dev->d.bike.humi = humidity->valuedouble;
        if (cJSON_IsNumber(speed)) 
            dev->d.bike.speed_kmh = speed->valuedouble;
        if (cJSON_IsNumber(total_time)) 
            dev->d.bike.total_time_sec = total_time->valuedouble;
        if (cJSON_IsNumber(distance)) 
            dev->d.bike.total_distance_km = distance->valuedouble;
        if (cJSON_IsNumber(max_speed)) 
            dev->d.bike.max_speed_kmh = max_speed->valuedouble;
        if (cJSON_IsNumber(average_speed)) 
            dev->d.bike.average_speed_kmh = average_speed->valuedouble;
        if (cJSON_IsNumber(kcal)) 
            dev->d.bike.kcal = kcal->valuedouble;
        if (cJSON_IsNumber(lat)) 
            dev->d.bike.latitude = lat->valuedouble;
        if (cJSON_IsNumber(lng)) 
            dev->d.bike.longitude = lng->valuedouble;
    } else {
        cJSON_Delete(json);
        printf("Error: Unknown device type\n");
        return 0;
    }

    cJSON_Delete(json);
    return 1;
}

char* encode_dev_to_json(const dev_info_t* dev)
{
    // 创建 JSON 根对象
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }
    // 添加命令
    cJSON_AddStringToObject(json, "cmd", "download cmd");
    // 添加设备名称
    cJSON_AddStringToObject(json, "name", dev->dev_name);
    // 添加设备类型
    cJSON_AddStringToObject(json, "type", dev->dev_type);
    // 添加时间戳
    cJSON_AddNumberToObject(json, "time", dev->timestamp);
    // 添加数据对象
    cJSON *data = cJSON_CreateObject();
    if (data == NULL) {
        cJSON_Delete(json);
        return NULL;
    }
    cJSON_AddItemToObject(json, "data", data);

    // 根据设备类型添加字段
    if (strcmp(dev->dev_type, "fan") == 0) {
        // 电风扇设备的数据字段
        cJSON_AddNumberToObject(data, "speed", dev->d.fan.speed);
        cJSON_AddNumberToObject(data, "light", dev->d.fan.light);
        cJSON_AddStringToObject(data, "on_time", dev->d.fan.on_time);
        cJSON_AddStringToObject(data, "on_temp", dev->d.fan.on_temp);
        cJSON_AddNumberToObject(data, "off_duration", dev->d.fan.off_duration);
    } else if (strcmp(dev->dev_type, "th") == 0) { // 温湿度计没有需要下发的数据
        // 温湿度计设备的数据字段
        //cJSON_AddNumberToObject(data, "temp", dev->d.th.temp);
        //cJSON_AddNumberToObject(data, "humidity", dev->d.th.humidity);
    }

    // 将 JSON 对象转换为字符串
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);  // 释放 JSON 对象

    return json_str;
}

/* 定义操作函数的类型 */
typedef void(*send_html_body_t)(const char *, int);

char html_buf[4096];
char* generate_html_body_th(dev_info_t* dev)
{
    memset(html_buf, 0, sizeof(html_buf));

    snprintf(html_buf, sizeof(html_buf),
        "<body>\r\n"
            "<p>温湿度计名称: %s</p>\r\n"
            "<p>温度: %.2f°C</p>\r\n"
            "<p>湿度: %.2f%%</p>\r\n"
            "<p>数据更新时间: %s</p>\r\n"
            "<hr>\r\n"
        "</body>\r\n",
        dev->dev_name, dev->d.th.temp, dev->d.th.humidity,
        timestamp_to_time_str(dev->timestamp)
    );

    return html_buf;
}
char* generate_html_body_fan(dev_info_t* dev)
{
    memset(html_buf, 0, sizeof(html_buf));

    snprintf(html_buf, sizeof(html_buf),
        "<body>\r\n"
            "<p>电风扇名称: %s</p>\r\n"
            "<p>温度: %.2f°C</p>\r\n"
            "<p>湿度: %.2f%%</p>\r\n"
            "<p>数据更新时间: %s</p>\r\n"
            "<form action=\"/fan\" method=\"post\">\r\n"
                "<p>\r\n"
                    "<input type=\"hidden\" id=\"devName\" name=\"devName\" value=\"%s\">\r\n"
                "</p>\r\n"
                "<p>\r\n"
                "<label for=\"light\">灯光开关:</label>\r\n"
                "<select id=\"light\" name=\"light\">\r\n"
                    "<option value=\"on\" %s>开启</option>\r\n"
                    "<option value=\"off\" %s>关闭</option>\r\n"
                "</select>\r\n"
                "</p>\r\n"
                "<p>\r\n"
                    "<label for=\"speedInput\">风速 (0~4):</label>\r\n"
                    "<input type=\"number\" id=\"speedInput\" name=\"Speed\" min=\"0\" max=\"4\" value=\"%d\">\r\n"
                "</p>\r\n"
                "<p>\r\n"
                    "<label for=\"temp\">温度阈值：</label>\r\n"
                    "<input type=\"text\" id=\"temp\" name=\"onTemp\" value=\"%s\">\r\n"
                    "<label >℃</label>\r\n"
                "</p>\r\n"
                "<p>\r\n"
                    "<label for=\"timeInput\">定时开启:</label>\r\n"
                    "<input type=\"time\" id=\"timeInput\" name=\"onTime\" value=\"%s\">\r\n"
                "</p>\r\n"
                "<p>\r\n"
                    "<label for=\"offDuration\">倒计时关闭:</label>\r\n"
                    "<select id=\"offDuration\" name=\"offDuration\">\r\n"
                        "<option value=\"0\" %s>不启用</option>\r\n"
                        "<option value=\"30\" %s>30分</option>\r\n"
                        "<option value=\"60\" %s>1小时</option>\r\n"
                        "<option value=\"90\" %s>1.5小时</option>\r\n"
                        "<option value=\"120\" %s>2小时</option>\r\n"
                    "</select>\r\n"
                "</p>\r\n"
                "<button type=\"submit\">提交</button>\r\n"
            "</form>\r\n"
            "<hr>\r\n"
        "</body>\r\n",
        dev->dev_name, dev->d.th.temp, dev->d.th.humidity,
        timestamp_to_time_str(dev->timestamp),dev->dev_name,
        (dev->d.fan.light == 1) ? "selected" : "",  // 动态设置灯光开关选项
        (dev->d.fan.light == 0) ? "selected" : "",
        dev->d.fan.speed,  // 设置风速默认值
        dev->d.fan.on_temp,
        dev->d.fan.on_time,
        (dev->d.fan.off_duration == 0) ? "selected" : "",
        (dev->d.fan.off_duration == 30) ? "selected" : "",
        (dev->d.fan.off_duration == 60) ? "selected" : "",
        (dev->d.fan.off_duration == 90) ? "selected" : "",
        (dev->d.fan.off_duration == 120) ? "selected" : ""
    );

    return html_buf;
}
// 秒数转成 hh:mm:ss 字符串
char* seconds_to_hhmmss(int seconds, char* result)
{
    if (seconds < 0 || result == NULL) {
        return NULL;
    }

    int hours = seconds / 3600;      // 计算小时
    int minutes = (seconds % 3600) / 60; // 计算分钟
    int secs = seconds % 60;        // 剩余秒数

    // 将结果格式化到字符串中
    sprintf(result, "%02d:%02d:%02d", hours, minutes, secs);
    return result;
}
char* generate_html_body_bike(dev_info_t* dev)
{
    char hhmmss[9] = {0};
    memset(html_buf, 0, sizeof(html_buf));
    char * gaode_key = xxxxxxxx; // 替换成你自己的秘钥
    // 为了能够定时刷新地图，这里使用了高德提供的 JavaScript 脚本
    // 文档在：https://lbs.amap.com/api/javascript-api-v2/documentation
    // 示例在：https://lbs.amap.com/demo/javascript-api-v2/example/marker/replaying-historical-running-data
    // 大家知道即可，不用深究
    snprintf(html_buf, sizeof(html_buf),
        "<body>\r\n"
            "<p id=\"deviceName\">码表名称: %s</p>"
            "<p id=\"temperature\">实时温度: %.2f°C</p>"
            "<p id=\"humidity\">实时湿度: %.2f%%</p>"
            "<p id=\"speed\">实时速度: %.2f</p>"
            "<p id=\"distance\">骑行距离: %.2f</p>"
            "<p id=\"totalTime\">骑行时长: %s</p>"
            "<p id=\"maxSpeed\">最大时速: %.2f</p>"
            "<p id=\"avgSpeed\">平均时速: %.2f</p>"
            "<p id=\"calories\">消耗卡路里: %.2f</p>"
            "<p id=\"latitude\">实时纬度: %.6f</p>"
            "<p id=\"longitude\">实时经度: %.6f</p>"
            "<p id=\"updateTime\">数据更新时间: %s</p>"
            "<button id=\"startBtn\">刷新数据</button>"
            "<button id=\"stopBtn\" disabled>停止刷新</button>"
            "<div id=\"container\"></div>"
"<script type=\"text/javascript\" src=\"https://webapi.amap.com/maps?v=2.0&key=%s\"></script><script>AMap.plugin('AMap.MoveAnimation',function(){var map=new AMap.Map(\"container\",{resizeEnable:true,center: [116.397428, 39.90923],zoom: 17});map.setFitView();var lineArr=[];var marker=null;function fetchCoordinates() {try {var currentUrl = window.location.href;var targetUrl=new URL('index.html', currentUrl).href;console.log('url:', targetUrl);fetch(targetUrl).then(response => response.text()).then(html=>{const parser = new DOMParser();const doc = parser.parseFromString(html,'text/html');const latitude=parseFloat(doc.querySelector('p#latitude').textContent.replace('实时纬度: ',''));const longitude=parseFloat(doc.querySelector('p#longitude').textContent.replace('实时经度: ',''));console.log('Latitude:', latitude);console.log('Longitude:', longitude);if(latitude===0.0||longitude===0.0){return;}newPoint = [longitude, latitude];lineArr.push(newPoint);if (!marker) {marker = new AMap.Marker({map: map,position: newPoint,icon: \"https://a.amap.com/jsapi_demos/static/demo-center-v2/car.png\",offset: new AMap.Pixel(-13, -26),});} else {   marker.moveTo(newPoint, {duration: 500,delay: 100,autoRotation: true,});}map.setCenter(marker.getPosition(),true);document.querySelector('#deviceName').textContent=doc.querySelector('#deviceName').textContent;document.querySelector('#temperature').textContent=doc.querySelector('#temperature').textContent;document.querySelector('#humidity').textContent=doc.querySelector('#humidity').textContent;document.querySelector('#speed').textContent = doc.querySelector('#speed').textContent;document.querySelector('#distance').textContent=doc.querySelector('#distance').textContent;document.querySelector('#totalTime').textContent=doc.querySelector('#totalTime').textContent;document.querySelector('#maxSpeed').textContent=doc.querySelector('#maxSpeed').textContent;document.querySelector('#avgSpeed').textContent=doc.querySelector('#avgSpeed').textContent;document.querySelector('#calories').textContent=doc.querySelector('#calories').textContent;document.querySelector('#latitude').textContent = doc.querySelector('#latitude').textContent;document.querySelector('#longitude').textContent=doc.querySelector('#longitude').textContent;document.querySelector('#updateTime').textContent=doc.querySelector('#updateTime').textContent;}).catch(error => console.error('Error fetching data:', error));} catch (error){console.error('Error in fetchCoordinates:', error);}}let intervalId;document.getElementById('startBtn').addEventListener('click', function() {intervalId= setInterval(fetchCoordinates, 2000);document.getElementById('startBtn').disabled = true;document.getElementById('stopBtn').disabled = false;});document.getElementById('stopBtn').addEventListener('click', function() {clearInterval(intervalId);document.getElementById('startBtn').disabled = false;document.getElementById('stopBtn').disabled=true;marker=null;if(lineArr.length<=2){return;}var passedPolyline=new AMap.Polyline({map: map,strokeColor: \"#AF5\",strokeWeight: 6, });passedPolyline.setPath(lineArr);});});</script>"
            "<hr>"
        "</body>\r\n",
        dev->dev_name, dev->d.bike.temp, dev->d.bike.humi,
        dev->d.bike.speed_kmh, dev->d.bike.total_distance_km,
        seconds_to_hhmmss(dev->d.bike.total_time_sec,hhmmss) , 
        dev->d.bike.max_speed_kmh, dev->d.bike.average_speed_kmh, 
        dev->d.bike.kcal, dev->d.bike.latitude, dev->d.bike.longitude,
        timestamp_to_time_str(dev->timestamp), gaode_key
    );

    return html_buf;
}

void send_html_body(const char *filename, int client_sockfd)
{
    char *body;
    dev_info_t dev;
    char last_line[512] = {0};
    
    read_last_line_from_file(filename, last_line, sizeof(last_line));
    if(!decode_json_to_dev(last_line, &dev)) {
        printf("decode ggijp failed\n");
        return ;
    }
    // 响应体
    // 根据设备类型解析相应的数据
    if (strcmp(dev.dev_type, "fan") == 0) {
        body = generate_html_body_fan(&dev);
    } else if (strcmp(dev.dev_type, "th") == 0) {
        body = generate_html_body_th(&dev);
    }  else if (strcmp(dev.dev_type, "bike") == 0) {
        body = generate_html_body_bike(&dev);
    } else {
        printf("Error: Unknown device type\n");
        return ;
    }
    send(client_sockfd, body, strlen(body), 0);
}

int traverse_dir(const char* dir_path, send_html_body_t send_html_body, int client_sockfd)
{
    DIR *dirp;
    struct dirent *entry;
    char filepath[2048] = { 0 };   

    // 打开目录
    dirp = opendir(dir_path);
    if (dirp == NULL) {
        printf("opendir error\n");
        return -1;
    }

    // 读取目录项
    while ((entry = readdir(dirp)) != NULL) {
        // 过滤掉当前目录. 和 父目录..
        if (entry->d_name[0] == '.') {
            continue;
        }
        // 查找以 data_dev 开头、以 .json 结尾的文件
        //printf("file name:%s\n", entry->d_name);
        if (strncmp(entry->d_name, "data_dev", 8) == 0) {
            // 查找文件名中最后一个 '.' 的位置
            char *dot = strrchr(entry->d_name, '.');
            // 是否以 json 结尾
            if (dot && strcmp(dot, ".json") == 0) {
                // 将文件内容解析成 html body 段，发送给浏览器
                send_html_body(entry->d_name, client_sockfd);
            }
        }

    }

    // 关闭目录
    closedir(dirp);

    return 0;
}

void send_html(int client_sockfd)
{
    // 响应行和响应头
    char *http_header = "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html; charset=utf-8\r\n\r\n";
    send(client_sockfd, http_header, strlen(http_header), 0);

    char *html_head = "<html>\r\n"
        "<head>\r\n"
            "<title>果果云</title>\r\n"
            "<style>html, body {height: 100%;}"
            "#container {height: 70%;width: 80%;}</style>"
        "</head>\r\n"
        "<body>\r\n";
    send(client_sockfd, html_head, strlen(html_head), 0);
    
    traverse_dir(".", send_html_body, client_sockfd);

    char *html_tail = "</body>\r\n</html>\r\n";    
    send(client_sockfd, html_tail, strlen(html_tail), 0);
}
// 追加一行到文件结尾
void append_line_to_file(const char *filename, char* line)
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

    printf("write [%s] to [%s] successfully\n", line, filename);
}

// URL解码函数
void url_decode(char *dst, const char *src)
{
    while (*src) {
        if (*src == '%') {
            int code;
            if (sscanf(src + 1, "%2x", &code) == 1) {
                *dst++ = code;
                src += 3;
            }
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}
// 解析查询字符串并填充 fan_data_t 结构体
void decode_fan_data(const char *query)
{
    char decoded_query[256];
    dev_info_t dev;
    // 先进行URL解码
    // 将：light=on&speed=1&temp=27.00&time=09%3A44&offDuration=30
    // 解码成：light=on&speed=1&temp=27.00&time=09:44&offDuration=30
    url_decode(decoded_query, query);
    char key[32], value[32];
    // 按 & 分割字符串
    char *token = strtok(decoded_query, "&");
    while (token) {
        memset(key, 0, sizeof(key));
        memset(value, 0, sizeof(value));
        // 解析 xxx=xxx 格式内容
        sscanf(token, "%[^=]=%s", key, value);

        // 根据键名填充 fan_data_t 结构体
        if (strcmp(key, "devName") == 0) {
            strncpy(dev.dev_name, value, sizeof(dev.dev_name) - 1);
            dev.dev_name[sizeof(dev.dev_name) - 1] = '\0';
        } else if (strcmp(key, "Speed") == 0) {
            dev.d.fan.speed = atoi(value);
        } else if (strcmp(key, "light") == 0) {
            dev.d.fan.light = strcmp(value, "on") == 0 ? 1 : 0;
        } else if (strcmp(key, "onTime") == 0) {
            if(strlen(value) == 0)
                strcpy(value, "--:--");
            strncpy(dev.d.fan.on_time, value, sizeof(dev.d.fan.on_time) - 1);
            dev.d.fan.on_time[sizeof(dev.d.fan.on_time) - 1] = '\0';
        } else if (strcmp(key, "onTemp") == 0) {
            strncpy(dev.d.fan.on_temp, value, sizeof(dev.d.fan.on_temp) - 1);
            dev.d.fan.on_temp[sizeof(dev.d.fan.on_temp) - 1] = '\0';
        } else if (strcmp(key, "offDuration") == 0) {
            dev.d.fan.off_duration = atoi(value);
        }
        
        token = strtok(NULL, "&");  // 继续解析下一个键值对
    }
    
    strcpy(dev.dev_type, "fan");
    dev.timestamp = time(NULL);

    char* ggijp = encode_dev_to_json(&dev);
    if(ggijp){
        char file_name[48];
        snprintf(file_name, sizeof(file_name), "%s%s%s", "cmd_", dev.dev_name, ".json");
        append_line_to_file(file_name, ggijp);
        free(ggijp); // 释放cJSON库申请的堆内存
    }
}

int decode_http_post(const char *path, const char *query)
{
    if (strncmp(path, "/fan", 4) == 0) {
        decode_fan_data(query);
    } else {
        printf("Unknown post type\n");
    }
}

void handle_http_request(const char *request, int client_sockfd)
{
    char method[8], path[256];
    // HTTP GET请求：  GET / HTTP/1.1
    // HTTP POST请求：  POST /submit HTTP/1.1
    // 解析 HTTP 请求行的方法和路径，sscanf遇到空格或换行时结束
    sscanf(request, "%s %s", method, path);
    printf("\nmethod:[%s] path:[%s]\n", method, path);
    if (strncmp(method, "GET", 4) == 0) {
        printf("Received a GET request\n");
        // 进一步解析 GET 请求的路径和参数
        // 处理 GET 请求的逻辑...
        // 不论收到什么GET请求，都发送固定的内容
        send_html(client_sockfd);
    } else if (strncmp(method, "POST", 5) == 0) {
        printf("Received a POST request\n");
        // 查找 Content-Length 获取 POST 数据的长度
        const char *content_length_str = strstr(request, "Content-Length: ");
        int content_length = 0;
        if (content_length_str) {
            sscanf(content_length_str, "Content-Length: %d", &content_length);
        }
        // 获取 POST 数据主体部分
        const char *body = strstr(request, "\r\n\r\n");
        if (body) {
            body += 4; // 跳过头部和空行的分隔
            printf("POST body: %.*s\n", content_length, body);
        }
        // 处理 POST 请求的逻辑
        decode_http_post(path, body);
        // 不论收到什么POST请求，都发送固定的内容
        send_html(client_sockfd);
    } else {
        printf("Unknown request type\n");
    }
}

void handle_client(int client_sockfd)
{
    char recvbuf[BUFFER_SIZE];
    ssize_t bytes_recv;
    
    // 阻塞接收数据
    bytes_recv = recv(client_sockfd, recvbuf, sizeof(recvbuf) - 1, 0);

    if (bytes_recv < 0) {
        printf("recv failed, [%d], %s\n", errno, strerror(errno));
    } else if (bytes_recv == 0) {
        printf("tcp closed\n");
    } else {
        recvbuf[bytes_recv] = '\0';
        printf("receive [%zd] bytes data:\n%s", bytes_recv, recvbuf);
        handle_http_request(recvbuf, client_sockfd);
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
        printf("close client socket [%d]\n", client_sockfd);
    }

    // 关闭服务端 Socket
    if (close(server_sockfd) == -1) {
        printf("Failed to close server socket, [%d], %s\n", errno, strerror(errno));
        return -1;
    }
    printf("server socket [%d] closed successfully\n", server_sockfd);
    return 0;
}
