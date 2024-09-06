const byte ph[] =   {0x01, 0x03 , 0x00 , 0x06 , 0x00 , 0x01 , 0x64 , 0x0B};
const byte nitro[] ={0x01 , 0x03 , 0x00 , 0x1E , 0x00 , 0x01 , 0xE4 , 0x0C};
const byte phos[]  ={0x01 , 0x03 , 0x00 , 0x1F , 0x00 , 0x01 , 0xB5 , 0xCC};
const byte pota[] = {0x01 , 0x03 , 0x00 , 0x20 , 0x00 , 0x01 , 0x85 , 0xC0};
const byte soilTemp[] = {0x01 , 0x03 , 0x00 , 0x13 , 0x00 , 0x01 , 0x75 , 0xCF};
const byte vwc[] = {0x01 , 0x03 , 0x00 , 0x12 , 0x00 , 0x01 , 0x24 , 0x0F};
const byte ec[] ={0x01 , 0x03 , 0x00 , 0x15 , 0x00 , 0x01 , 0x95 , 0xCE};
const byte salinity[]={0x01 , 0x03 , 0x00 , 0x07 , 0x00 , 0x01 , 0x35 , 0xCB};
const byte tds[]={0x01 , 0x03 , 0x00 , 0x08 , 0x00 , 0x01 , 0x05 , 0xC8};

byte values[11];
int getNitrogen() {
  delay(150);
  for (uint8_t i = 0; i < sizeof(nitro); i++ ) mod.write( nitro[i] );

  delay(150);

  // read in the received bytes
  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
    Serial.print(values[i], HEX);
    Serial.print(' ');
  }
  int nitrogen = int(values[3]<<8|values[4]);
  return nitrogen;
}

int getPhosphorous() {
  delay(150);
  for (uint8_t i = 0; i < sizeof(phos); i++ ) mod.write( phos[i] );

  delay(150);

  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
    Serial.print(values[i], HEX);
    Serial.print(' ');
  }
  int phosphorous = int(values[3]<<8|values[4]);
  return phosphorous;

}

int getPotassium() {
  delay(150);
  for (uint8_t i = 0; i < sizeof(pota); i++ ) mod.write( pota[i] );

  delay(150);
  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
    Serial.print(values[i], HEX);
    Serial.print(' ');
  }
  int potassium = int(values[3]<<8|values[4]);
  return potassium;
}

int getPH() {
  delay(150);
  for (uint8_t i = 0; i < sizeof(ph); i++ ) mod.write( ph[i] );

  delay(150);
  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
    Serial.print(values[i], HEX);
    Serial.print(' ');
  }
  int pH = int(values[3]<<8|values[4]);
  return pH;
}

int getSoilTemp() {
  delay(150);
  for (uint8_t i = 0; i < sizeof(soilTemp); i++ ) mod.write( soilTemp[i] );

  delay(150);
  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
    Serial.print(values[i], HEX);
    Serial.print(' ');
  }
  int soilTemperature = int(values[3]<<8|values[4]);
  return soilTemperature;
}


int getVWC() {
  delay(150);
  for (uint8_t i = 0; i < sizeof(vwc); i++ ) mod.write( vwc[i] );

  delay(150);
  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
    Serial.print(values[i], HEX);
    Serial.print(' ');
  }
  int VWC = int(values[3]<<8|values[4]);
  return VWC;
}

int getEC() {
  delay(150);
  for (uint8_t i = 0; i < sizeof(ec); i++ ) mod.write( ec[i] );

  delay(150);
  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
    Serial.print(values[i], HEX);
    Serial.print(' ');
  }
  int EC = int(values[3]<<8|values[4]);
  return EC;
}

int getSalinity() {
  delay(150);
  for (uint8_t i = 0; i < sizeof(salinity); i++ ) mod.write( salinity[i] );

  delay(150);
  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
    Serial.print(values[i], HEX);
    Serial.print(' ');
  }
  int soilSalinity = int(values[3]<<8|values[4]);
  return soilSalinity;
}

int getTDS() {
  delay(150);
  for (uint8_t i = 0; i < sizeof(tds); i++ ) mod.write( tds[i] );

  delay(150);
  for (byte i = 0; i < 7; i++) {
    values[i] = mod.read();
    Serial.print(values[i], HEX);
    Serial.print(' ');
  }
  int TDS = int(values[3]<<8|values[4]);
  return TDS;
}
