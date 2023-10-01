# Smart Home Device

智慧家庭的设备

## 自动浇水

家里使用的自动浇水设备

## 配置 cli

```shell
arduino-cli config set board_manager.additional_urls https://arduino.esp8266.com/stable/package_esp8266com_index.json https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
arduino-cli core install esp32:esp32
```

## OTA

```shell
# esp8266
arduino-cli compile --build-property build.extra_flags="-DENABLE_DEBUG -DESP8266" --fqbn esp8266:esp8266:nodemcuv2 -e ./autowatering/autowatering.ino
# esp32
arduino-cli compile --build-property build.defines=-DENABLE_DEBUG --fqbn esp32:esp32:esp32 -e ./autowatering/autowatering.ino
# 上传
espota.py -i 192.168.31.x -p 8266 --auth= -f autowatering.ino.bin
```
