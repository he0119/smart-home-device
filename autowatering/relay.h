#pragma once

#include "Arduino.h"

class Relay
{
public:
  Relay(int pin);
  void set_status(bool status);
  void set_delay(unsigned long delay);
  bool status();
  unsigned long delay();
  void open();
  void close();
  // 在开/关状态之间切换
  void toggle();
  void tick();

private:
  int m_pin;
  bool m_status;
  unsigned long m_open_at;
  bool m_auto_close = false;
  unsigned long m_delay = 60; // Auto Close Delay (seconds)
};
