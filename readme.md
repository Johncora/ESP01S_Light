# 编译
arduino-cli compile --fqbn esp8266:esp8266:generic .

# 上传
arduino-cli upload -p COM3 --fqbn esp8266:esp8266:generic .

# 串口监视
arduino-cli monitor -p COM3 -b 115200
arduino-cli monitor -p COM3 --config baudrate=115200