#if wiflyEnabled

#define TIME_BUFFER_SIZE 20 

char* sckWIFItime() {
  boolean ok=false;
  uint8_t count = 0;
  if (sckEnterCommandMode()) 
  {
    byte retry=0;
    while ((!sckOpen(WEB[0], 80))&&(retry<5)) 
      {
        retry++; //Serial.println("Retry!!");
      }
    if(retry<5)
    {
      for(byte i = 0; i<3; i++) Serial1.print(WEBTIME[i]); //Peticiones al servidor de tiempo
      if (sckFindInResponse("UTC:", 2000)) 
      {
        char newChar;
        byte offset = 0;
  
        while (offset < TIME_BUFFER_SIZE) {
          if (Serial1.available())
          {
            newChar = Serial1.read();
            if (newChar == '#') {
              ok = true;
              buffer[offset] = '\x00';
              break;
            } 
            else if (newChar != -1) {
              if (newChar==',') 
                {
                  if (count<2) buffer[offset]='-';
                  else if (count>2) buffer[offset]=':';
                  else buffer[offset]=' ';
                  count++;
                }
              else buffer[offset] = newChar;
              offset++;
            }
          }
        }
      }
    }
    if (connected) {
        sckClose();
      }
    //sckExitCommandMode();
  } 
  if (!ok)
    {
      buffer[0] = '#';
      buffer[1] = 0x00;
      //Serial.println("Fail!!");
    }
  sckExitCommandMode();
  return buffer;
} 

#define numbers_retry 5

boolean sckServer_connect()
  {
    uint16_t pos = sckReadintEEPROM(EE_ADDR_NUMBER_MEASURES);
    sckWriteData(DEFAULT_ADDR_MEASURES, pos + 1, sckScan());  //Wifi Nets
    sckWriteData(DEFAULT_ADDR_MEASURES, pos + 2, sckWIFItime());
    //Provisionalmente ajuste de reloj
    byte retry = 0;
    if (sckCheckRTC())
     {
      while (!sckRTCadjust(sckWIFItime())&&(retry<5))
       {
         retry = retry + 1;
       }
     }
    sckCheckData(); //Volvemos a verificar si datos correctos
    return sckServer_reconnect(); 
  }

boolean sckServer_reconnect()
  {
    char* mac_Address = sckMAC();
    //char* ApiKey = sckReadData(EE_ADDR_APIKEY, 0, 0);
    int retry = 0;
    boolean ok = false;   
    while ((!ok)&&(retry<numbers_retry)){
        if (sckOpen(WEB[0], 80)) ok = true;
        else 
          {
            retry++;
            if (retry >= numbers_retry) return ok;
          }
      }    
//    for (byte i = 1; i<5; i++) Serial1.print(WEB[i]);
//    Serial1.print(mac_Address);
//    for (byte i = 11; i<13; i++) Serial1.print(WEB[i]);


    for (byte i = 1; i<5; i++) Serial1.print(WEB[i]);
    Serial1.println(mac_Address);
    Serial1.print(WEB[5]);
    Serial1.println(sckReadData(EE_ADDR_APIKEY, 0, 0)); //Apikey
    Serial1.print(WEB[6]);
    Serial1.println(FirmWare);
    Serial1.print(WEB[7]);
    return ok; 
  }

void sckJson_update(uint16_t initial, boolean terminal)
  {  
    uint16_t updates = ((sckReadintEEPROM(EE_ADDR_NUMBER_MEASURES) + 1)/10); 
    if (!terminal)
    {
      if ((initial + POST_MAX) <= updates) updates = initial + POST_MAX;    
      if (updates > 0)
        {
          Serial1.print(F("["));  
          for (uint16_t pending = initial; pending < updates; pending++)
           { 
             byte i;
             for (i = 0; i<10; i++)
              {
                Serial1.print(SERVER[i]);
                Serial1.print(sckReadData(DEFAULT_ADDR_MEASURES, i + pending*10, 0));
              }  
             Serial1.print(SERVER[i]);
             if ((updates > 1)&&(pending < (updates-1))) Serial1.print(F(","));
           }
          Serial1.println(F("]"));
          Serial1.println();
        }
    }
    else
    {
        if (updates > 0)
          {
            Serial.print(F("["));  
            for (uint16_t pending = initial; pending < updates; pending++)
             { 
               byte i;
               for (i = 0; i<10; i++)
                {
                  Serial.print(SERVER[i]);
                  Serial.print(sckReadData(DEFAULT_ADDR_MEASURES, i + pending*10, 0));
                }  
               Serial.print(SERVER[i]);
               if ((updates > 1)&&(pending < (updates-1))) Serial.print(F(","));
             }
            Serial.println(F("]"));
          }
    }
  }  



void txWiFly() {  
  
  uint16_t updates = (sckReadintEEPROM(EE_ADDR_NUMBER_MEASURES) + 3)/10; 
  boolean ok_sleep = false;
  
  if ((sleep)&&(updates>=NumUpdates)){
    #if debuggEnabled
      Serial.println(F("SCK Waking up..."));
    #endif
    ok_sleep = true;
    digitalWrite(AWAKE, HIGH);
  }
  if ((sckConnect())&&(updates>=NumUpdates))
   { 
      #if debuggEnabled
        Serial.println(F("SCK Connected!!")); 
      #endif   
      if (sckServer_connect())
      {
        uint16_t initial = 0; 
        #if debuggEnabled
          Serial.print(F("updates = "));
          Serial.println(updates-initial);
        #endif
        sckJson_update(initial, false);
        initial = initial + POST_MAX;
        #if debuggEnabled
          Serial.println(F("Posted to Server!")); 
        #endif
        while (updates > initial){
          sckServer_reconnect();
          #if debuggEnabled
            Serial.print(F("updates = "));
            Serial.println(updates-initial);
          #endif
          sckJson_update(initial, false);
          initial = initial + POST_MAX;
          #if debuggEnabled
            Serial.println(F("Posted to Server!")); 
          #endif
        }
       sckWriteintEEPROM(EE_ADDR_NUMBER_MEASURES, 0x0000);
      }
      else {
        #if debuggEnabled
          Serial.println(F("Error posting on Server..!"));
          uint16_t pos = sckReadintEEPROM(EE_ADDR_NUMBER_MEASURES);
          sckWriteData(DEFAULT_ADDR_MEASURES, pos - 1, sckRTCtime());
        #endif
      }
      if (connected) {
        #if debuggEnabled
          Serial.println(F("Old connection active. Closing..."));
        #endif
        sckClose();
      }
    }
   else 
    {
     uint16_t pos = sckReadintEEPROM(EE_ADDR_NUMBER_MEASURES);
     
     if (ok_sleep) sckWriteData(DEFAULT_ADDR_MEASURES, pos + 1, sckScan());  //Wifi Nets
     else sckWriteData(DEFAULT_ADDR_MEASURES, pos + 1, "0");  //Wifi Nets
     
     if (sckCheckRTC()) 
       {
         sckWriteData(DEFAULT_ADDR_MEASURES, pos + 2, sckRTCtime());
       }
     sckCheckData();//No hace falta que se guarda en memoria si falla la RTC
     #if debuggEnabled
       Serial.print(F("updates = "));
       Serial.println((sckReadintEEPROM(EE_ADDR_NUMBER_MEASURES)+1)/10);
       if (((sckReadintEEPROM(EE_ADDR_NUMBER_MEASURES) + 3)/10)>=NumUpdates) Serial.println(F("Error in connectionn!!"));
       else Serial.println(F("Saved in memory!!"));
     #endif
    }
  
  
  if ((sleep)&&(ok_sleep))
  {
    sckSleep();
    #if debuggEnabled
      Serial.println(F("SCK Sleeping")); 
      Serial.println(F("*******************"));
    #endif
    digitalWrite(AWAKE, LOW); 
  }
}

#endif

#if SDEnabled

void txSD() {
  myFile = SD.open("post.csv", FILE_WRITE);
  // if the file opened okay, write to it:
  if (myFile) {
    #if debuggEnabled
      Serial.println(F("Writing...")); 
    #endif 
    
    float dec = 0;
    for (int i=0; i<8; i++)
      {
        if (i<4) dec = 10;
        else if (i<7) dec = 1000;
        else if (i<8) dec = 100;
        else dec = 1;
        
        myFile.print(i);
        myFile.print(", ");
        myFile.print(SENSORvalue[i]/dec);
        myFile.print(", ");
      }
    myFile.print(sckRTCtime());
    myFile.println();
    // close the file:
    myFile.close();
    #if debuggEnabled
      Serial.println(F("Closing...")); 
    #endif 
  }
}

#endif

char* SENSOR[10]={
                  "Temperature: ",
                  "Humidity: ",
                  "Light: ",
                  "Battery: ",
                  "Solar Panel: ",
                  "Carbon Monxide: ",
                  "Nitrogen Dioxide: ",
                  "Noise: ",
                  "Wifi Spots: ",
                  "UTC: " 
             };
   
char* UNITS[10]={
                  #if F_CPU == 8000000 
                    #if DataRaw
                      " C RAW",
                      " % RAW",
                    #else
                      " C",
                      " %",
                    #endif
                  #else
                    " C",
                    " %",
                  #endif
                  #if F_CPU == 8000000 
                    " lx",
                  #else
                    " %",
                  #endif
                  " %",
                  " V",
                  " kOhm",
                  " kOhm",
                  #if DataRaw
                    " mV",
                  #else
                    " dB",
                  #endif
                  "",
                  "" 
             };            

#if USBEnabled             
  void txDebug() {
    uint8_t dec = 0;
    uint16_t pos = (sckReadintEEPROM(EE_ADDR_NUMBER_MEASURES) + 1)/10;
    if (pos>0)pos = (pos - 1)*10;
      for(int i=0; i<10; i++) 
       {
         #if F_CPU == 8000000 
           #if DataRaw
             if (i<2) dec = 0;
             else if (i<4) dec = 1;
           #else 
             if (i<4) dec = 1;
           #endif
           else if (i<7) dec = 3;
           #if DataRaw
             else if (i<8) dec = 0;
           #else 
             else if (i<8) dec = 2;
           #endif
         #else
           if (i<4) dec = 1;
           else if (i<7) dec = 3;
           #if DataRaw
             else if (i<8) dec = 0;
           #else 
             else if (i<8) dec = 2;
           #endif
         #endif
         else dec = 0;
         Serial.print(SENSOR[i]); Serial.print(sckReadData(DEFAULT_ADDR_MEASURES, pos + i, dec)); Serial.println(UNITS[i]);
       }
        Serial.println(F("*******************"));     
  }
#endif

#if SDEnabled
  void updateSensorsSD() {
     #if F_CPU == 8000000 
      sckGetSHT21();
      SENSORvalue[0] = lastTemperature; // C
      SENSORvalue[1] = lastHumidity; // %
    #else
      if (sckDHT22(IO3))
      {
        SENSORvalue[0] = lastTemperature; // C
        SENSORvalue[1] = lastHumidity; // %
      }
    #endif
    sckGetMICS();
    SENSORvalue[2] = sckGetLight(); // %
    SENSORvalue[3] = sckGetBattery(); //%
    SENSORvalue[4] = sckGetPanel();  // %
    SENSORvalue[5] = sckGetCO(); //Ohm
    SENSORvalue[6] = sckGetNO2(); //Ohm
    SENSORvalue[7] = sckGetNoise(); //dB    
  }
  
 void txDebugSD() {
  float dec = 0;
    for(int i=0; i<8; i++) 
     {
       if (i<4) dec = 10;
       else if (i<7) dec = 1000;
       else if (i<8) dec = 100;
       else dec = 1;
       Serial.print(SENSOR[i]); Serial.print((SENSORvalue[i])/dec); Serial.println(UNITS[i]);
     }
     Serial.print(SENSOR[9]);
     Serial.println(sckRTCtime());
     Serial.println(F("*******************"));     
}
#endif


