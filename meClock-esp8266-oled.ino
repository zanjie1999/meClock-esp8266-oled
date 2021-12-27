
/*
咩时钟
梦飞翔 esp8266 oled
Sparkle 20211225

动这个屎山之前是有好好考虑过的，超级不规范的看着血压都上来了
因为配网库的屎山 Arduino的esp8266支持库必须是2.74
需要配合wifi_link_tool配网工具 地址：https://github.com/bilibilifmk/wifi_link_tool
所需库：
u8g2
ArduinoJson 5.13.5
time
大部分是来自 南湘小隐 和 发明控 的屎山代码
*/
#define FS_CONFIG
//兼容1.1.x版本库
#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <wifi_link_tool.h>
#include "bmps.h"

#include <WiFiClientSecure.h>

#include <Time.h>
#include <TimeLib.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

#include <WiFiClientSecureBearSSL.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

String keys = "887821c4003547a2a122c4995e33e7d6";  // 接口地址：https://console.heweather.com/app/index 自己去申请key
String dq = "101280102";                           //  填入城市编号  获取编号 https://where.heweather.com/index.html
String UID = "9992930";                            //  B站UID,用以显示粉丝数

String page = "3";  //设定显示界面1.B站粉丝界面/2.天气界面/ 3.天气第二界面/4.高考倒计时
#define sck 5       // D1
#define sda 4       // D2

const int CONFIG_HTTPPORT = 80;
const int CONFIG_HTTPSPORT = 443;

std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/sck, /* data=*/sda, /* reset=*/U8X8_PIN_NONE);
int col = 999;
String diqu = "";
int tq = 0;             //天气代码
String pm2 = "";        // pm2.5
String aqi = "";        //空气质量参数
String hum = "";        //空气湿度
String bfs = "NO UID";  // bilbili 粉丝数
String wendu = "";      //温度
String tim = "12:11";
String dat = "2001/01/02";
int previousday = 0;  //用于判断是否过了一天6
// int             previousminutes  =  0;    //用于判断分钟改变后刷新屏幕
int lastday = 1;  //倒计时天数

unsigned long prevMillis = 0;
const long interval = 10000; /* 默认10秒更新屏幕 */
unsigned long prevMillisTq = 0;
const long intervalTq = 180000; /* 半小时更新天气  */
unsigned long prevMillisBili = 0;
const long intervalBili = 300000; /* 粉丝数5分钟更新一次*/

int flag1 = 0;  //网页参数是否修改标记
int flag2 = 0;
int flag3 = 0;
int flag4 = 0;
int flag6 = 0;
int flag5 = 0;  //所有网页函数是否都运行完成

int pcboot = 0;                         // pc请求识别
IPAddress timeServer(120, 25, 115, 20); /* 阿里云ntp服务器 如果失效可以使用 120.25.115.19   120.25.115.20 */
#define STD_TIMEZONE_OFFSET +8          /* 设置中国 */
const int timeZone = 8;                 /* 修改北京时区 */
WiFiUDP Udp;

unsigned int localPort = 8266; /* 修改udp 有些路由器端口冲突时修改 */
int servoLift = 1500;

void setup() {
    WiFiServer HTTPserver(CONFIG_HTTPPORT);
    WiFiServerSecure HTTPSserver(CONFIG_HTTPSPORT);
    Serial.begin(115200);

    rstb = 0;             /* 重置io */
    stateled = 2;         /* 指示灯io */
    Hostname = "meClock"; /* 设备名称 允许中文名称 不建议太长 */
    wxscan = false;        /* 是否被小程序发现设备 开启意味该设备具有后台 true开启 false关闭 */
    pinMode(rstb, INPUT_PULLUP);

    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_unifont_t_chinese2);
    u8g2.setFontDirection(0);
    u8g2.clearBuffer();
    u8g2.drawXBMP(0, 0, 128, 64, xyy);
    u8g2.sendBuffer();

    webServer.on("/pc", pc);  // pc请求
    /* 获取网页信息服务开启 1/3 --开启网页服务*/
    webServer.on("/zdws", zdws);  //和风keys
    webServer.on("/wsdw", wsdw);  //城市id
    webServer.on("/yggd", yggd);  // b站uid
    webServer.on("/txhm", txhm);  //界面选择：1.B站粉丝界面；2天气界面； 3天气第二界面
    webServer.on("/zgwd", zgwd);  //倒计时
    load();

    /* 初始化WiFi link tool */
    Serial.println("设置 UDP");
    Udp.begin(localPort);
    Serial.print("Local port: ");
    Serial.println(Udp.localPort());
    Serial.println("正在等待同步");

    if (link()) {
        /*获取网页信息服务开启 2/3 --判断flash中是有数据*/
        EEPROM.begin(4096);
        if (get_String(EEPROM.read(3000), 3001).length() > 50) {
            set_String(3000, 3001, keys);  //如果flash中的字符长度大于50，表明flash中没有数据，将keys的值写入flash
        } else {
            keys = get_String(EEPROM.read(3000), 3001);  //和风keys  如果flash中有参数，读取读取参数
        }

        if (get_String(EEPROM.read(3100), 3101).length() > 50) {
            set_String(3100, 3101, dq);
        } else {
            dq = get_String(EEPROM.read(3100), 3101);  //城市id
        }

        if (get_String(EEPROM.read(3200), 3201).length() > 50) {
            set_String(3200, 3201, UID);
        } else {
            UID = get_String(EEPROM.read(3200), 3201);  // b站uid
        }

        if (get_String(EEPROM.read(3300), 3301).length() == 1) {
            page = get_String(EEPROM.read(3300), 3301);  //界面选择：1.B站粉丝界面；2天气界面； 3天气第二界面
        } else {
            set_String(3300, 3301, page);
        }

        EEPROM.get(3400, lastday);  // 倒计时参数，如果flash中没有参数，也没必要使用默认值

        Serial.println("01-和风keys：");
        Serial.println(keys);
        Serial.println("01-城市id：");
        Serial.println(dq);
        Serial.println("01-b站uid：");
        Serial.println(UID);
        Serial.println("01-界面选择：");
        Serial.println(page);
        Serial.println("01-倒计时天数：");
        Serial.println(lastday);

        u8g2.setFont(u8g2_font_unifont_t_chinese2);
        u8g2.setFontDirection(0);
        u8g2.clearBuffer();
        u8g2.setCursor(0, 15);
        u8g2.print("网络");
        u8g2.setCursor(0, 30);
        u8g2.print("WiFi 好了!");
        u8g2.setCursor(0, 45);
        u8g2.print("IP:");
        u8g2.setCursor(0, 60);
        u8g2.print(WiFi.localIP());
        u8g2.sendBuffer();
        sjfx();
        //    delay( 1000 );
        setSyncProvider(getNtpTime);
        previousday = day();  //初始化倒计时天数
        shuaxin();

    } else {
        u8g2.setFont(u8g2_font_unifont_t_chinese2);
        u8g2.setFontDirection(0);
        u8g2.clearBuffer();
        u8g2.setCursor(0, 15);
        u8g2.print("WiFi:");
        u8g2.setCursor(0, 30);
        u8g2.print("wifi_link_tool");
        u8g2.setCursor(0, 45);
        u8g2.print("浏览器打开");
        u8g2.setCursor(0, 60);
        u8g2.print("6.6.6.6");
        u8g2.sendBuffer();
    }
}

// 获取网页信息服务开启 3/3 对应1的处理函数
void zdws() {  // 和风keys
    String a = webServer.arg("zdws");
    if (a != "" && a != get_String(EEPROM.read(3000), 3001))  //如果网页获取的参数不是空值 且 与flash中的值不同
    {
        keys = a;
        set_String(3000, 3001, keys);
        flag1 = 1;  // 标志着网页参数1改变
    }
    flag5 = flag5 + 1;  //本函数运行之后 ，标志位加1
    Serial.println("和风keys");
    Serial.println(keys);
}
void wsdw() {  //城市id
    String b = webServer.arg("wsdw");
    if (b != "" && b != get_String(EEPROM.read(3100), 3101)) {
        dq = b;
        set_String(3100, 3101, dq);
        flag2 = 1;  // 标志着网页参数2改变
    }
    flag5 = flag5 + 1;  //本函数运行之后 ，标志位加1
    Serial.println("城市id");
    Serial.println(dq);
}
void yggd() {  // b站uid
    String c = webServer.arg("yggd");
    if (c != "" && c != get_String(EEPROM.read(3200), 3201)) {
        UID = c;
        set_String(3200, 3201, UID);
        flag3 = 1;  // 标志着网页参数3改变
    }
    flag5 = flag5 + 1;  //本函数运行之后 ，标志位加1
    Serial.println("b站uid");
    Serial.println(UID);
}
void txhm() {  //界面选择：1.B站粉丝界面；2天气界面； 3天气第二界面
    String d = webServer.arg("txhm");
    if (d != "" && d != get_String(EEPROM.read(3300), 3301)) {
        page = d;
        set_String(3300, 3301, page);
        flag4 = 1;  // 标志着网页参数4改变
    }
    flag5 = flag5 + 1;  //本函数运行之后 ，标志位加1
    Serial.println("界面选择");
    Serial.println(page);
}
void zgwd() {  // 倒计时数据处理
    String e = webServer.arg("zgwd");
    EEPROM.get(3400, lastday);
    if (e != "" && e.toInt() != lastday) {
        lastday = e.toInt();
        EEPROM.put(3400, lastday);
        EEPROM.commit();
        flag6 = 1;
    }
    Serial.println("倒计时");
    Serial.println(lastday);
    flag5 = flag5 + 1;  //本函数运行之后 ，标志位加1
    Serial.println("网页函数运行数量");
    Serial.println(flag5);
    if (flag5 > 4)  // 5个函数都运行了,执行重启的动作
    {
        if (flag1 == 1 || flag2 == 1 || flag3 == 1 || flag4 == 1 || flag6 == 1)  //任一一个参数发生改变
        {
            setok();        //设定完成后屏幕提示完成
                            //        flag1 = 0;  //重启后，这些参数都恢复默认值
                            //        flag2 = 0;
                            //        flag3 = 0;
                            //        flag4 = 0;
                            //        flag5 = 0;
            ESP.restart();  //判断逻辑是，如果都执行了且有参数改变后才重启
        }
    }
}

//写入字符串函数
// a写入字符串长度位置，b是起始位，str为要保存的字符串
void set_String(int a, int b, String str) {
    EEPROM.write(a, str.length());  // EEPROM第a位，写入str字符串的长度
    //把str所有数据逐个保存在EEPROM
    for (int i = 0; i < str.length(); i++) {
        EEPROM.write(b + i, str[i]);
    }
    EEPROM.commit();
}

//读字符串函数
// a位是字符串长度，b是起始位
String get_String(int a, int b) {
    String data = "";
    //从EEPROM中逐个取出每一位的值，并链接
    for (int i = 0; i < a; i++) {
        data += char(EEPROM.read(b + i));
    }
    return data;
}

void loop() {
    // 按键按下动作，剩下的长按重置pant会做
    // if (!digitalRead(0)) {
    //     Serial.println("按钮触发");
    //     if (page == "4") {
    //         page = "1";
    //     } else {
    //         String(page.toInt() + 1);
    //     }
    //     shuaxin();
    // }
    pant(); /* WiFi link tool 服务维持函数  请勿修改位置 */
    if (link()) {
        djs();  // 高考倒计时函数

        unsigned long currentMillis = millis();
        if (currentMillis - prevMillis >= interval) {
            prevMillis = currentMillis;
            shuaxin();
        }

        //天气刷新
        if (currentMillis - prevMillisTq >= intervalTq) {
            prevMillisTq = currentMillis;
            sjfx();
        }
    }
}

//高考倒计时函数
void djs() {
    int currentday = day();
    if (currentday != previousday) {
        previousday = currentday;
        if (lastday != 0) {
            lastday = lastday - 1;
            EEPROM.put(3400, lastday);  //将此值写入flash
            EEPROM.commit();
        }
        // Serial.println("previousday");
        // Serial.println(previousday);
        // Serial.println("lastday");
        // Serial.println(lastday);
    }
}

//参数设定确认界面
void setok() {
    u8g2.clearBuffer();
    u8g2.drawXBMP(0, 5, 20, 20, ok1);  //参数设置完成
    u8g2.drawXBMP(20, 5, 20, 20, ok2);
    u8g2.drawXBMP(40, 5, 20, 20, ok3);
    u8g2.drawXBMP(60, 5, 20, 20, ok4);
    u8g2.drawXBMP(80, 5, 20, 20, ok5);
    u8g2.drawXBMP(100, 5, 20, 20, ok6);
    u8g2.drawXBMP(30, 30, 20, 20, ok7);  //重启中
    u8g2.drawXBMP(50, 30, 20, 20, ok8);
    u8g2.drawXBMP(70, 30, 20, 20, ok9);
    u8g2.sendBuffer();
    delay(2000);
}

//天气刷新函数
void sjfx() {
    delay(1000);
    xx();
    delay(1000);
    pm();
    delay(1000);
    col = tq;
    Serial.println("天气刷新中...");
    Serial.println("当前地区： " + diqu);
    Serial.println("天气代码： ");
    Serial.println(tq);
    Serial.println("体感温度： " + wendu);
    Serial.println("PM2.5： " + pm2);
}

//屏幕刷新函数
void shuaxin() {
    if (pcboot == 0) {
        /* setSyncProvider(getNtpTime); */
        String th = "";
        String zod = "";
        // 转12小时制
        if (hour() < 12) {
            th = String(hour());
        } else {
            th = String(hour() - 12);
        }
        if (minute() < 10) {
            tim = th + ":0" + String(minute());
        } else {
            tim = th + ":" + String(minute());
        }
        if (month() < 10) {
            zod = "0";
        }
        if (day() < 10) {
            dat = String(year()) + "/" + zod + String(month()) + "/0" + String(day());
        } else {
            dat = String(year()) + "/" + zod + String(month()) + "/" + String(day());
        }

        Serial.print(tim); /* 输出当前网络分钟 */
        Serial.print(dat); /* 输出当前日期 */
        u8g2.clearBuffer();
        tubiao();  //天气图标

        if (page == "1")  // 1.B站粉丝界面
        {
            unsigned long currentMillis = millis();
            if (prevMillisBili == 0 || currentMillis - prevMillisBili >= intervalBili) {
                prevMillisBili = currentMillis;
                blbluid();  //刷新b站粉丝
            }
            u8g2.setFont(u8g2_font_ncenB18_tf);
            u8g2.setFontDirection(0);
            u8g2.setCursor(65, 53);
            u8g2.print(wendu);  //温度
            u8g2.setFont(u8g2_font_wqy12_t_chinese2);
            u8g2.setFontDirection(0);
            u8g2.setCursor(63, 64);
            u8g2.print(dat);  //日期
            u8g2.drawXBMP(103, 37, 16, 16, col_ssd1);
            u8g2.setFont(u8g2_font_ncenR12_tf);
            u8g2.setFontDirection(0);
            u8g2.setCursor(0, 50);
            u8g2.print("bilibili");

            // u8g2.drawXBMP( 0, 39, 12, 12, id1 ); //稚晖君
            // u8g2.drawXBMP( 12, 39, 12, 12, id2 );
            // u8g2.drawXBMP( 24, 39, 12, 12, id3 );

            u8g2.setFont(u8g2_font_bauhaus2015_tn);
            u8g2.setCursor(0, 64);
            u8g2.print(bfs);  // 刷新粉丝
            u8g2.setFont(u8g2_font_fub25_tf);
            u8g2.setFontDirection(0);
            u8g2.setCursor(40, 32);
            u8g2.print(tim);
            u8g2.sendBuffer();
        } else if (page == "2")  // 2.默认天气界面  pm2.5
        {
            u8g2.setFont(u8g2_font_ncenB18_tf);
            u8g2.setFontDirection(0);
            u8g2.setCursor(65, 53);
            u8g2.print(wendu);  //温度
            u8g2.setFont(u8g2_font_wqy12_t_chinese2);
            u8g2.setFontDirection(0);
            u8g2.setCursor(63, 64);
            u8g2.print(dat);  //日期
            u8g2.drawXBMP(103, 37, 16, 16, col_ssd1);
            u8g2.setFont(u8g2_font_ncenR12_tf);
            u8g2.setFontDirection(0);
            u8g2.setCursor(0, 48);
            u8g2.print("pm2.5");
            u8g2.setCursor(10, 62);
            u8g2.print(pm2);  // pm2.5 浓度
            u8g2.setFont(u8g2_font_fub25_tf);
            u8g2.setFontDirection(0);
            u8g2.setCursor(40, 32);
            u8g2.print(tim);
            u8g2.sendBuffer();

        } else if (page == "3")  // 3.天气界面  温湿度  aqi
        {
            if (wendu.length() < 3) {
                u8g2.drawXBMP(30, 38, 16, 16, col_ssd1);
                u8g2.setFont(u8g2_font_VCR_OSD_tf);
                u8g2.setFontDirection(0);
                u8g2.setCursor(5, 53);
                u8g2.print(wendu);  //体感温度
            } else {
                u8g2.setFont(u8g2_font_VCR_OSD_tf);
                u8g2.setFontDirection(0);
                u8g2.setCursor(5, 53);
                u8g2.print(wendu);  //体感温度
            }
            u8g2.setFont(u8g2_font_8x13_t_symbols);
            u8g2.setFontDirection(0);
            u8g2.setCursor(3, 64);
            u8g2.print(dat);  //日期
            u8g2.setFont(u8g2_font_8x13_t_symbols);
            u8g2.setFontDirection(0);
            u8g2.setCursor(102, 64);
            u8g2.print("AQI");  //空气质量符号

            if (aqi.length() < 3) {
                u8g2.setFont(u8g2_font_VCR_OSD_tf);
                u8g2.setFontDirection(0);
                u8g2.setCursor(52, 53);
                u8g2.print(hum);  //实际湿度
                u8g2.setFont(u8g2_font_luBS12_tr);
                u8g2.setFontDirection(0);
                u8g2.setCursor(77, 53);
                u8g2.print("%");
                u8g2.setFont(u8g2_font_VCR_OSD_tf);
                u8g2.setFontDirection(0);
                u8g2.setCursor(100, 53);
                u8g2.print(aqi);  //空气质量数值
            } else {
                u8g2.setFont(u8g2_font_VCR_OSD_tf);
                u8g2.setFontDirection(0);
                u8g2.setCursor(50, 53);
                u8g2.print(hum);  //实际湿度
                u8g2.setFont(u8g2_font_luBS12_tr);
                u8g2.setFontDirection(0);
                u8g2.setCursor(75, 53);
                u8g2.print("%");
                u8g2.setFont(u8g2_font_VCR_OSD_tf);
                u8g2.setFontDirection(0);
                u8g2.setCursor(92, 53);
                u8g2.print(aqi);  //空气质量数值
            }
            u8g2.setFont(u8g2_font_fub25_tf);  // u8g2_font_fub25_tf
            u8g2.setFontDirection(0);
            u8g2.setCursor(40, 30);
            u8g2.print(tim);  //时间
            u8g2.sendBuffer();
        } else if (page == "4")  // 4.倒计时界面
        {
            u8g2.setFont(u8g2_font_ncenB18_tf);
            u8g2.setFontDirection(0);
            u8g2.setCursor(65, 53);
            u8g2.print(wendu);                         //温度
            u8g2.setFont(u8g2_font_wqy12_t_chinese2);  // u8g2_font_6x10_tf
            u8g2.setFontDirection(0);
            u8g2.setCursor(63, 64);
            u8g2.print(dat);  //日期
            u8g2.drawXBMP(103, 37, 16, 16, col_ssd1);
            u8g2.setCursor(0, 50);
            u8g2.print("还有");
            u8g2.setCursor(40, 62);
            u8g2.print("天");
            u8g2.setFont(u8g2_font_bauhaus2015_tn);
            u8g2.setCursor(6, 64);
            u8g2.print(lastday);  // 刷新倒计时天数
            u8g2.setFont(u8g2_font_fub25_tf);
            u8g2.setFontDirection(0);
            u8g2.setCursor(40, 32);
            u8g2.print(tim);
            u8g2.sendBuffer();
        }
    } else {
        pcboot = 0;
    }
}

void tubiao() {
    if (col == 100 || col == 150) {  //晴
        u8g2.drawXBMP(0, 0, 40, 40, col_100);

    } else if (col == 102 || col == 101) {
        u8g2.drawXBMP(0, 0, 40, 40, col_102);  //云

    } else if (col == 103 || col == 153) {
        u8g2.drawXBMP(0, 0, 40, 40, col_103);  //晴间多云

    } else if (col == 104 || col == 154) {
        u8g2.drawXBMP(0, 0, 40, 40, col_104);  //阴

    } else if (col >= 300 && col <= 301) {
        u8g2.drawXBMP(0, 0, 40, 40, col_301);  //阵雨

    } else if (col >= 302 && col <= 303) {
        u8g2.drawXBMP(0, 0, 40, 40, col_302);  //雷阵雨

    } else if (col == 304) {
        u8g2.drawXBMP(0, 0, 40, 40, col_304);  //冰雹

    } else if (col == 399 || col == 314 || col == 305 || col == 306 || col == 307 || col == 315 || col == 350 || col == 351) {
        u8g2.drawXBMP(0, 0, 40, 40, col_307);  //雨

    } else if ((col >= 308 && col <= 313) || (col >= 316 && col <= 318)) {
        u8g2.drawXBMP(0, 0, 40, 40, col_310);  //暴雨

    } else if ((col >= 402 && col <= 406) || col == 409 || col == 410 || col == 400 || col == 401 || col == 408 || col == 499 || col == 456) {
        u8g2.drawXBMP(0, 0, 40, 40, col_401);  //雪

    } else if (col == 407 || col == 457) {
        u8g2.drawXBMP(0, 0, 40, 40, col_407);  //阵雪

    } else if (col >= 500 && col <= 502) {
        u8g2.drawXBMP(0, 0, 40, 40, col_500);  //雾霾

    } else if (col >= 503 && col <= 508) {
        u8g2.drawXBMP(0, 0, 40, 40, col_503);  //沙尘暴

    } else if (col >= 509 && col <= 515) {
        u8g2.drawXBMP(0, 0, 40, 40, col_509);  //不适宜生存

    } else {
        u8g2.drawXBMP(0, 0, 40, 40, col_999);  //未知
    }
}

void pc() {
    pcboot = 1;
    String clk = webServer.arg("clk");
    String cpu = webServer.arg("cpu");
    String ram = webServer.arg("ram");
    String cput = webServer.arg("cput");
    webServer.arg("cpuv");
    u8g2.clearBuffer();
    u8g2.drawXBMP(0, 0, 128, 64, pctp);
    u8g2.setFont(u8g2_font_crox5hb_tf);
    u8g2.setFontDirection(0);
    u8g2.setCursor(33, 25);
    u8g2.print(cpu);  // cpu
    u8g2.setCursor(33, 57);
    u8g2.print(cput);  // cput
    u8g2.setCursor(95, 57);
    u8g2.print(ram);  // ram
    u8g2.setFont(u8g2_font_ncenR10_tf);
    u8g2.setFontDirection(0);
    u8g2.setCursor(96, 16);
    u8g2.print(clk);  // mhz
    u8g2.setCursor(94, 30);
    u8g2.print("MHz");
    u8g2.sendBuffer();
    webServer.send(200, "text/plain", "ojbk");
}

/* //////////////////////////////////////////////////天气数据 */
void xx() {
    String line;
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    HTTPClient https;
    if (https.begin(*client, "https://devapi.heweather.net/v7/weather/now?gzip=n&location=" + dq + "&key=" + keys)) {
        int httpCode = https.GET();
        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                line = https.getString();
            }
        }
        https.end();
    } else {
        Serial.printf("[HTTPS]请求链接失败\n");
    }
    Serial.println("接口返回" + line);
    DynamicJsonBuffer jsonBuffer(1024);
    JsonObject& res_json = jsonBuffer.parseObject(line);
    String r1 = res_json["basic"]["location"];  //地区
    int r2 = res_json["now"]["icon"];           //天气
    String r3 = res_json["now"]["temp"];        //温度
    String r4 = res_json["now"]["humidity"];    // humidity湿度
    jsonBuffer.clear();
    diqu = r1; /* 地区 */
    if (r2 != 0) {
        tq = r2;
    }
    if (r3 != "") {
        wendu = r3;  //体感温度
        hum = r4;
    }
}

//空气质量获取函数

void pm() {
    String line;
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    HTTPClient https;
    if (https.begin(*client, "https://devapi.heweather.net/v7/air/now?gzip=n&location=" + dq + "&key=" + keys)) {
        int httpCode = https.GET();
        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
                line = https.getString();
            }
        }
        https.end();
    } else {
        Serial.printf("[HTTPS]请求链接失败\n");
    }

    Serial.println("接口返回" + line);
    DynamicJsonBuffer jsonBuffer(1024);
    JsonObject& res_json = jsonBuffer.parseObject(line);
    String r1 = res_json["now"]["pm2p5"];  // pm25
    String r2 = res_json["now"]["aqi"];    //空气质量
    jsonBuffer.clear();
    if (r1 != "") {
        pm2 = r1;  // pm2.5
        aqi = r2;  //空气质量
    }
}

// b站粉丝采集函数
void blbluid() {
    WiFiClient client;
    HTTPClient http;
    if (http.begin(client, "http://api.bilibili.com/x/relation/stat?vmid=" + UID)) {
        int httpCode = http.GET();
        String payload = http.getString();
        http.end();
        DynamicJsonBuffer jsonBuffer;
        JsonObject& res_json = jsonBuffer.parseObject(payload);
        String follower = res_json["data"]["follower"];
        if (follower != "") {
            bfs = follower;
        }
        jsonBuffer.clear();
    }
}

void digitalClockDisplay() {
    /*  */
    Serial.print(hour());
    printDigits(minute());
    Serial.println();
}

void printDigits(int digits) {
    Serial.print(":");
    if (digits < 10)
        Serial.print('0');
    Serial.print(digits);
}

const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime() {
    while (Udp.parsePacket() > 0)
        ;
    Serial.println("连接时间服务器");
    sendNTPpacket(timeServer);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE) {
            Serial.println("时间服务器应答");
            Udp.read(packetBuffer, NTP_PACKET_SIZE);
            unsigned long secsSince1900;
            /* convert four bytes starting at location 40 to a long integer */
            secsSince1900 = (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            return (secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
        }
    }
    Serial.println("No NTP Response :-(");
    return (0);
}

void sendNTPpacket(IPAddress& address) {
    memset(packetBuffer, 0, NTP_PACKET_SIZE);

    packetBuffer[0] = 0b11100011;
    packetBuffer[1] = 0;
    packetBuffer[3] = 0xEC;

    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    Udp.beginPacket(address, 123);
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}
