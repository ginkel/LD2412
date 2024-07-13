#include "MyLD2410.h"

/*** BEGIN LD2410 namespace ***/
namespace LD2410
{
  const char *tStatus[4]{"No target", "Moving only", "Stationary only", "Both moving and stationary"};
  const byte headData[4]{0xF4, 0xF3, 0xF2, 0xF1};
  const byte tailData[4]{0xF8, 0xF7, 0xF6, 0xF5};
  const byte headConfig[4]{0xFD, 0xFC, 0xFB, 0xFA};
  const byte tailConfig[4]{4, 3, 2, 1};
  const byte configEnable[6]{4, 0, 0xFF, 0, 1, 0};
  const byte configDisable[4]{2, 0, 0xFE, 0};
  const byte MAC[6]{4, 0, 0xA5, 0, 1, 0};
  const byte firmware[4]{2, 0, 0xA0, 0};
  const byte res[4]{2, 0, 0xAB, 0};
  const byte resCoarse[6]{4, 0, 0xAA, 0, 0, 0};
  const byte resFine[6]{4, 0, 0xAA, 0, 1, 0};
  const byte reset[4]{2, 0, 0xA2, 0};
  const byte reboot[4]{2, 0, 0xA3, 0};
  const byte BTon[6]{4, 0, 0xA4, 0, 1, 0};
  const byte BToff[6]{4, 0, 0xA4, 0, 0, 0};
  const byte param[4]{2, 0, 0x61, 0};
  const byte engOn[4]{2, 0, 0x62, 0};
  const byte engOff[4]{2, 0, 0x63, 0};
  byte gateParam[0x16]{0x14, 0, 0x64, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0};
  byte maxGate[0x16]{0x14, 0, 0x60, 0, 0, 0, 8, 0, 0, 0, 1, 0, 8, 0, 0, 0, 2, 0, 5, 0, 0, 0};

  String byte2hex(byte b, bool addZero = true)
  {
    String bStr(b, HEX);
    bStr.toUpperCase();
    if (addZero && (bStr.length() == 1))
      return "0" + bStr;
    return bStr;
  }
  void printBuf(const byte *buf, byte size)
  {
    for (byte i = 0; i < size; i++)
    {
      Serial.print(byte2hex(buf[i]));
      Serial.print(' ');
    }
    Serial.println();
    Serial.flush();
  }
  bool bufferEndsWith(const byte *buf, int iMax, const byte *other)
  {
    for (int j = 3; j >= 0; j--)
    {
      if (--iMax < 0)
        iMax = 3;
      if (buf[iMax] != other[j])
        return false;
    }
    return true;
  }
}
/*** END LD2410 namespace ***/

MyLD2410::Response MyLD2410::check()
{
  while (sensor->available())
  {
    headBuf[headBufI++] = byte(sensor->read());
    headBufI %= 4;
    if (LD2410::bufferEndsWith(headBuf, headBufI, LD2410::headConfig) && processAck())
      return ACK;
    if (LD2410::bufferEndsWith(headBuf, headBufI, LD2410::headData) && processData())
      return DATA;
  }
  return FAIL;
}

bool MyLD2410::sendCommand(const byte *command)
{
  byte size = command[0] + 2;
  // LD2410::printBuf(command, size);
  sensor->write(LD2410::headConfig, 4);
  sensor->write(command, size);
  sensor->write(LD2410::tailConfig, 4);
  sensor->flush();
  unsigned long giveUp = millis() + timeout;
  while (millis() < giveUp)
  {
    while (sensor->available())
    {
      headBuf[headBufI++] = byte(sensor->read());
      headBufI %= 4;
      if (LD2410::bufferEndsWith(headBuf, headBufI, LD2410::headConfig))
        return processAck();
    }
  }
  return false;
}
bool MyLD2410::readFrame()
{
  int frameSize = -1, bytes = 2;
  if (bytes > 0)
  {
    inBufI = 0;
    while (bytes)
    {
      if (sensor->available())
      {
        inBuf[inBufI++] = byte(sensor->read());
        bytes--;
      }
    }
    frameSize = 0;
    for (byte i = 0; i < inBufI; i++)
    {
      frameSize |= inBuf[i] << i * 8;
    }
  }
  if (frameSize <= 0)
    return false;
  frameSize += 4;
  inBufI = 0;
  while (frameSize > 0)
  {
    if (sensor->available())
    {
      inBuf[inBufI++] = byte(sensor->read());
      frameSize--;
    }
  }
  return true;
}

bool MyLD2410::processAck()
{
  if (!readFrame())
    return false;
  if (_debug)
    LD2410::printBuf(inBuf, inBufI);
  if (!LD2410::bufferEndsWith(inBuf, inBufI, LD2410::tailConfig))
    return false;
  unsigned long command = inBuf[0] | (inBuf[1] << 8);
  if (inBuf[2] | (inBuf[3] << 8))
    return false;
  switch (command)
  {
  case 0x1FF: // entered config mode
    isConfig = true;
    version = inBuf[4] | (inBuf[5] << 8);
    bufferSize = inBuf[6] | (inBuf[7] << 8);
    break;
  case 0x1FE: // exited config mode
    isConfig = false;
    break;
  case 0x1A5: // MAC
    for (int i = 0; i < 6; i++)
      MAC[i] = inBuf[i + 4];
    MACstr = LD2410::byte2hex(MAC[0]);
    for (int i = 1; i < 6; i++)
      MACstr += ":" + LD2410::byte2hex(MAC[i]);
    break;
  case 0x1A0: // Firmware
    firmware = LD2410::byte2hex(inBuf[7], false);
    firmware += "." + LD2410::byte2hex(inBuf[6]);
    firmware += "." + LD2410::byte2hex(inBuf[11]);
    firmware += LD2410::byte2hex(inBuf[10]);
    firmware += LD2410::byte2hex(inBuf[9]);
    firmware += LD2410::byte2hex(inBuf[8]);
    break;
  case 0x1AB: // Query Resolution
    fineRes = (inBuf[4]);
    break;
  case 0x1A3: // Reboot
    isEnhanced = false;
    isConfig = false;
    break;
  case 0x161: // Query parameters
    maxRange = inBuf[5];
    movingThresholds.N = inBuf[6];
    stationaryThresholds.N = inBuf[7];
    for (byte i = 0; i <= movingThresholds.N; i++)
      movingThresholds.values[i] = inBuf[8 + i];
    for (byte i = 0; i <= stationaryThresholds.N; i++)
      stationaryThresholds.values[i] = inBuf[17 + i];
    noOne_window = inBuf[26] | (inBuf[27] << 8);
    break;
  case 0x162:
    isEnhanced = true;
    break;
  case 0x163:
    isEnhanced = false;
    break;
  case 0x164:
    if (LD2410::gateParam[7] = 0xFF)
      LD2410::gateParam[7] = 0;
  }
  return (true);
}

bool MyLD2410::processData()
{
  if (!readFrame())
    return false;
  if (_debug)
    LD2410::printBuf(inBuf, inBufI);
  if (!LD2410::bufferEndsWith(inBuf, inBufI, LD2410::tailData))
    return false;
  if (((inBuf[0] == 1) || (inBuf[0] == 2)) && (inBuf[1] == 0xAA))
  { // Basic mode and Enhanced
    sData.timestamp = millis();
    sData.status = inBuf[2];
    sData.mTargetDistance = inBuf[3] | (inBuf[4] << 8);
    sData.mTargetSignal = inBuf[5];
    sData.sTargetDistance = inBuf[6] | (inBuf[7] << 8);
    sData.sTargetSignal = inBuf[8];
    sData.distance = inBuf[9] | (inBuf[10] << 8);
    if (inBuf[0] == 1)
    { // Enhanced mode only
      sData.mTargetSignals.N = inBuf[11];
      sData.sTargetSignals.N = inBuf[12];
      for (byte i = 0; i <= sData.mTargetSignals.N; i++)
        sData.mTargetSignals.values[i] = inBuf[13 + i];
      for (byte i = 0; i <= sData.sTargetSignals.N; i++)
        sData.sTargetSignals.values[i] = inBuf[24 + i];
    }
  }
  else
    return false;
  return true;
}

/**
@brief Construct from a serial stream object
*/
MyLD2410::MyLD2410(Stream &serial, bool debug)
{
  sensor = &serial;
  _debug = debug;
}

bool MyLD2410::begin()
{
  // Wait for the sensor to come online, or to timeout.
  unsigned long giveUp = millis() + timeout;
  bool online = false;
  while (millis() < giveUp)
  {
    if (check())
    {
      online = true;
      break;
    }
    delay(110);
  }
  return online;
}

void MyLD2410::end()
{
  isConfig = false;
  isEnhanced = false;
}

bool MyLD2410::inConfigMode()
{
  return isConfig;
}

bool MyLD2410::inBasicMode()
{
  return !isEnhanced;
}

bool MyLD2410::inEnhancedMode()
{
  return isEnhanced;
}
bool MyLD2410::isDataValid()
{
  return (millis() - sData.timestamp < dataLifespan);
}

bool MyLD2410::presenceDetected()
{
  return isDataValid() && (sData.status);
}

bool MyLD2410::stationaryTargetDetected()
{
  return isDataValid() && ((sData.status == 2) || (sData.status == 3)) &&
         (sData.sTargetDistance) && (sData.sTargetSignal);
}

const unsigned long MyLD2410::stationaryTargetDistance()
{
  return sData.sTargetDistance;
}

const byte MyLD2410::stationaryTargetSignal()
{
  return sData.sTargetSignal;
}

const MyLD2410::ValuesArray &MyLD2410::getStationarySignals()
{
  return sData.sTargetSignals;
}

bool MyLD2410::movingTargetDetected()
{
  return isDataValid() && ((sData.status == 1) || (sData.status == 3)) &&
         (sData.mTargetDistance) && (sData.mTargetSignal);
}

const unsigned long MyLD2410::movingTargetDistance()
{
  return sData.mTargetDistance;
}

const byte MyLD2410::movingTargetSignal()
{
  return sData.mTargetSignal;
}

const MyLD2410::ValuesArray &MyLD2410::getMovingSignals()
{
  return sData.mTargetSignals;
}

const unsigned long MyLD2410::detectedDistance()
{
  return sData.distance;
}

const byte *MyLD2410::getMAC()
{
  if (MACstr.isEmpty())
    requestMAC();
  return MAC;
}

String MyLD2410::getMACstr()
{
  if (MACstr.isEmpty())
    requestMAC();
  return MACstr;
}

String MyLD2410::getFirmware()
{
  if (firmware.isEmpty())
    requestFirmware();
  return firmware;
}

unsigned long MyLD2410::getVersion()
{
  return version;
}

const MyLD2410::SensorData &MyLD2410::getSensorData()
{
  return sData;
}

const char *MyLD2410::statusString()
{
  return LD2410::tStatus[sData.status];
}

const MyLD2410::ValuesArray &MyLD2410::getMovingThresholds()
{
  if (!maxRange)
    requestParameters();
  return movingThresholds;
}

const MyLD2410::ValuesArray &MyLD2410::getStationaryThresholds()
{
  if (!maxRange)
    requestParameters();
  return stationaryThresholds;
}

byte MyLD2410::getRange()
{
  if (!maxRange)
    requestParameters();
  return maxRange;
}

byte MyLD2410::getNoOneWindow()
{
  if (!maxRange)
    requestParameters();
  return noOne_window;
}

bool MyLD2410::configMode(bool enable)
{
  if (enable && !isConfig)
    return sendCommand(LD2410::configEnable);
  if (!enable && isConfig)
    return sendCommand(LD2410::configDisable);
  return false;
}

bool MyLD2410::enhancedMode(bool enable)
{
  if (isConfig)
    return sendCommand(((enable) ? LD2410::engOn : LD2410::engOff));
  else
    return configMode() && sendCommand(((enable) ? LD2410::engOn : LD2410::engOff)) && configMode(false);
}

bool MyLD2410::requestMAC()
{
  if (isConfig)
    return sendCommand(LD2410::MAC);
  return configMode() && sendCommand(LD2410::MAC) && configMode(false);
}

bool MyLD2410::requestFirmware()
{
  if (isConfig)
    return sendCommand(LD2410::firmware);
  return configMode() && sendCommand(LD2410::firmware) && configMode(false);
}

bool MyLD2410::requestResolution()
{
  if (isConfig)
    return sendCommand(LD2410::res);
  return configMode() && sendCommand(LD2410::res) && configMode(false);
}

bool MyLD2410::setResolution(bool fine)
{
  if (isConfig && sendCommand(((fine) ? LD2410::resFine : LD2410::resCoarse)))
    return sendCommand(LD2410::res);
  return configMode() && sendCommand(((fine) ? LD2410::resFine : LD2410::resCoarse)) && sendCommand(LD2410::res) && configMode(false);
}

bool MyLD2410::requestParameters()
{
  if (isConfig)
    return sendCommand(LD2410::param);
  return configMode() && sendCommand(LD2410::param) && configMode(false);
}

bool MyLD2410::setGateParameters(byte gate, byte movingThreshold, byte staticThreshold)
{
  if (movingThreshold > 100)
    movingThreshold = 100;
  if (staticThreshold > 100)
    staticThreshold = 100;
  if ((gate == 1) || (gate > 8))
    gate = 0; // 0 means all
  byte *cmd = LD2410::gateParam;
  if (gate)
    cmd[6] = gate;
  else
  {
    cmd[6] = 0xFF;
    cmd[7] = 0xFF;
  }
  cmd[12] = movingThreshold;
  cmd[18] = staticThreshold;
  if (isConfig && sendCommand(cmd))
    return sendCommand(LD2410::param);
  return configMode() && sendCommand(cmd) && sendCommand(LD2410::param) && configMode(false);
}

bool MyLD2410::setMaxGate(byte movingGate, byte staticGate, byte noOneWindow)
{
  if (movingGate > 8)
    movingGate = 8;
  if (staticGate > 8)
    staticGate = 8;
  byte *cmd = LD2410::maxGate;
  cmd[6] = movingGate;
  cmd[12] = staticGate;
  cmd[18] = noOneWindow;
  if (isConfig && sendCommand(cmd))
    return sendCommand(LD2410::param);
  return configMode() && sendCommand(cmd) && sendCommand(LD2410::param) && configMode(false);
}

bool MyLD2410::requestReset()
{
  if (isConfig)
    return sendCommand(LD2410::reset) && sendCommand(LD2410::param);
  return configMode() && sendCommand(LD2410::reset) && sendCommand(LD2410::param) && configMode(false);
}

bool MyLD2410::requestReboot()
{
  if (isConfig)
    return sendCommand(LD2410::reboot);
  return false;
}

bool MyLD2410::requestBTon()
{
  if (isConfig)
    return sendCommand(LD2410::BTon);
  return configMode() && sendCommand(LD2410::BTon) && configMode(false);
}

bool MyLD2410::requestBToff()
{
  if (isConfig)
    return sendCommand(LD2410::BToff);
  return configMode() && sendCommand(LD2410::BToff) && configMode(false);
}

byte MyLD2410::getResolution()
{
  if (fineRes >= 0)
    return ((fineRes == 1) ? 20 : 75);
  if (isConfig)
  {
    if (sendCommand(LD2410::res))
      return getResolution();
  }
  else
  {
    if (configMode() && sendCommand(LD2410::res) && configMode(false))
      return getResolution();
  }
  return 0;
}