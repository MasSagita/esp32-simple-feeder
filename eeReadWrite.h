#include <EEPROM.h>

//take 2 bytes
void memWriteInt(int value, int address) {
  byte highByteValue = highByte(value);
  byte lowByteValue = lowByte(value);

  EEPROM.write(address * 2, highByteValue); // Multiply by 2 to store 2 bytes per integer
  EEPROM.write(address * 2 + 1, lowByteValue);
}

int memReadInt(int address) {
  byte highByteValue = EEPROM.read(address * 2);
  byte lowByteValue = EEPROM.read(address * 2 + 1);

  return (int(highByteValue) << 8) | int(lowByteValue);
}

//take 4 bytes
void memWriteFloat(float *num, int MemPos) {
  byte ByteArray[4];
  memcpy(ByteArray, num, 4);
  for (int x = 0; x < 4; x++) EEPROM.write((MemPos * 4) + x, ByteArray[x]);
}

void memReadFloat(float *num, int MemPos) {
  byte ByteArray[4];
  for (int x = 0; x < 4; x++) ByteArray[x] = EEPROM.read((MemPos * 4) + x);
  memcpy(num, ByteArray, 4);
}