# Smart Home Device

智慧家庭的设备

## 自动浇水

家里使用的自动浇水设备

## OTA

```bash
# esp8266
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 ./autowatering/autowatering.ino -e
# esp32
arduino-cli compile --build-property --fqbn esp32:esp32:esp32 ./autowatering/autowatering.ino -e
# 上传
espota.py -i 192.168.31.x -p 8266 --auth= -f autowatering.ino.bin
```
