#include "relay.h"

void upload(bool reset);

Relay::Relay(int pin)
{
  m_pin = pin;
};

void Relay::set_status(bool status)
{
  m_status = status;
  digitalWrite(m_pin, m_status);
};

void Relay::set_delay(unsigned long delay)
{
  m_delay = delay;
};

bool Relay::status()
{
  return m_status;
};

unsigned long Relay::delay()
{
  return m_delay;
};

void Relay::open()
{
  m_auto_close = true;
  m_open_at = millis(); // Reset timer
  m_status = true;
  digitalWrite(m_pin, m_status);
  upload(0);
};

void Relay::close()
{
  m_auto_close = false;
  m_status = false;
  digitalWrite(m_pin, m_status);
  upload(0);
};

void Relay::toggle()
{
  if (m_status)
  {
    close();
  }
  else
  {
    open();
  }
};

void Relay::tick()
{
  if (m_auto_close && millis() - m_open_at > 1000 * m_delay)
  {
    close();
  }
};
