#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ========== WiFi 配置（STA 模式） ==========
const char* ssid = "WiFi";
const char* password = "12345678";

// ESP-01S 可用 GPIO：0, 1(TX), 2, 3(RX)
// GPIO0 控制灯（LED 负极接 GPIO0，正极串 220Ω 电阻接 3.3V）
const int LED_PIN = 0;

ESP8266WebServer server(80);
bool ledState = false;

// 内嵌网页（单开关）
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP-01S 灯控</title>
    <style>
        body { font-family: Arial; text-align: center; margin-top: 50px; background: #1a1a2e; color: white; }
        .btn { display: inline-block; padding: 25px 50px; font-size: 28px; border: none; border-radius: 15px; cursor: pointer; transition: 0.3s; }
        .on { background: #00d26a; color: white; }
        .off { background: #e94560; color: white; }
        .btn:hover { transform: scale(1.05); }
        #status { font-size: 28px; margin: 20px; }
    </style>
</head>
<body>
    <h1>ESP-01S 智能灯控</h1>
    <div id="status">灯状态: 加载中...</div>
    <button id="sw" class="btn off" onclick="toggle()">开关</button>
    <script>
        function updateStatus() {
            fetch('/status').then(r => r.text()).then(t => {
                let on = t == '1';
                document.getElementById('status').innerText = '灯状态: ' + (on ? '开' : '关');
                let btn = document.getElementById('sw');
                btn.innerText = on ? '关灯' : '开灯';
                btn.className = 'btn ' + (on ? 'on' : 'off');
            });
        }
        function toggle() {
            fetch('/toggle').then(() => updateStatus());
        }
        setInterval(updateStatus, 2000);
        updateStatus();
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", htmlPage);
}

void handleToggle() {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? LOW : HIGH);  // GPIO0 低电平亮，高电平灭
    server.send(200, "text/plain", ledState ? "1" : "0");
}

void handleStatus() {
    server.send(200, "text/plain", ledState ? "1" : "0");
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("BOOT");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  // 默认关灯

    // 连接 WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());

    server.on("/", handleRoot);
    server.on("/toggle", handleToggle);
    server.on("/status", handleStatus);
    server.begin();
    
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
}