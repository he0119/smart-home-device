#pragma once

#include "Arduino.h"

class Relay {
public:
  Relay(int pin);
  /**
   * @brief 设置继电器的状态
   *
   * @param status 继电器开关状态
   */
  void set_status(bool status);
  /**
   * @brief 设置延时关闭长度
   *
   * @param delay 单位 秒
   */
  void set_delay(unsigned long delay);
  /**
   * @brief 继电器状态
   */
  bool status();
  /**
   * @brief 继电器延时关闭时间（秒）
   */
  unsigned long delay();
  /**
   * @brief 打开继电器
   */
  void open();
  /**
   * @brief 关闭继电器
   */
  void close();
  /**
   * @brief 在开/关状态之间切换
   */
  void toggle();
  /**
   * @brief 在 loop() 中调用，以实现自动延时关闭功能。
   */
  void tick();

private:
  // 引脚
  int m_pin;
  // 继电器状态
  bool m_status;
  // 开启时间
  unsigned long m_open_at;
  // 是否启用自动关闭
  bool m_auto_close = false;
  // Auto Close Delay (seconds)
  unsigned long m_delay = 60;
};
