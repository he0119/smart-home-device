# Smart Home Device

智慧家庭的设备

## 自动浇水

家里使用的自动浇水设备

## 配置 cli

```shell
arduino-cli core update-index
arduino-cli lib update-index
```

## OTA

```shell
# esp8266
arduino-cli compile --build-property build.extra_flags="-DENABLE_DEBUG -DESP8266" --profile esp8266 -e ./autowatering/autowatering.ino
# esp32
arduino-cli compile --build-property build.defines=-DENABLE_DEBUG --profile esp32 -e ./autowatering/autowatering.ino
# 上传
espota.py -i 192.168.31.x -p 8266 --auth= -f autowatering.ino.bin
```
