#include <Wire.h>
#include "RTClib.h"
#include <EtherCard.h>
#include "DHT.h"
#include <avr/wdt.h>

//RTC OBJECT & PIN
RTC_DS1307 rtc;
DHT dht(7, DHT11);
const byte dhtPowerPin PROGMEM = 8; //dht power pin

///LED MODULE PINS (PWM)
const byte rPin PROGMEM = 3; //Red
const byte gPin PROGMEM = 6; //Green
const byte bPin PROGMEM = 5; //Blue
byte rState = 0; byte gState = 0; byte bState = 0; //To make server more responsive, we don't need to turn ON an LED already ON.

////POWER SENSOR
const byte wPin PROGMEM = A0; //Power sensor input
byte wallPower = 0; //Wall Power sensor value
int outageTotal = 0; //Amount in seconds power was out for 1 days time.
int outageSeq = 0; //Amount of time in seconds power was out consecutively. If power comes back on, this will reset to 0.

//BUTTON INPUT
const byte iPin PROGMEM = 9; //Side input button

//ETHERNET ATTRIBUTES
const byte myip[] = { 192,168,200,5 };
const byte gwip[] = { 192,168,200,1 };
const byte dnsip[] = { 8,8,8,8 };
const byte netmask[] = { 255,255,255,0 };
const byte mymac[] = { 0x72,0x69,0x69,0x2D,0x38,0x31 };
unsigned int lastPort = 65000; //Webserver listens on port 65000. But website calls requires port 80.

//WEBITE ATTRIBUTES
const char dnsSite[] PROGMEM = "www.duckdns.org";
const byte dnsSiteIP[] PROGMEM = { 54,191,157,42 }; //this is a failsafe of duckdns's public ip. Used when DNS fails to resolve.
const char ipSite[] PROGMEM = "www.myexternalip.com";
const byte ipSiteIP[] PROGMEM = { 78,47,139,102 }; //this is a failsafe of myexternalip's public ip. Used when DNS fails to resolve.

//TIMERS USED IN LOOP
////These values are adjusted later in the loop upon success. Short intervals are nessisary for startup.
unsigned long cMillis = millis();
unsigned long pMillis1 = 0; unsigned long interval1 = 1000; //Used in power outage routine
unsigned long pMillis2 = 0; unsigned long interval2 = 10; //Surges Normal Blue LED to teal (Cosmetic)
unsigned long pMillis3 = 0; unsigned long interval3 = 4000; //When to check temp/hum from DHT11 
unsigned long pMillis4 = 0; unsigned long interval4 = 8000; //When to check public IP and update DNS

//GENERAL USAGE ARRAYS
int httpCalls[] = {0,0,0,0}; //HTTP call successes and fail populate this array. Used in debug output.
int newTime[] = {0,0,0,0,0,0}; //For the landing page /rtc/1/etc. Used to set time manually.

//RTC & DHT11
byte rtcMonth = ""; byte rtcDay = ""; int rtcYear = ""; byte rtcHour = 0; byte rtcMinute = 0; byte rtcSecond = 0;
byte temp; byte hum;

//
byte Ethernet::buffer[500]; // tcp/ip send and receive buffer
BufferFiller bfill;
char publicIP[16]; //Buffer for public IP
char conkat[100]; //See ctrl+f -> "sprintf"

//DNS CALL & CONTROL
byte debug = 0; //If 1,debug mode is enabled and display httpCalls[] data via HTTP /info
byte callCtrl = 0; //flag that idicates when and not to parse the public ip
int httpFails = 0; byte callStatus = 0; int index1; int index2;
byte callToogle = 0; //flips between myexternal and duckdns (1. get ip, 2. send ip if #1 was successful)
//

void setup(){
  Serial.begin(9600);
  MCUSR = 0; //wd reset

  //Turn RED LED on. If this setup pass, blink GREEN twice. Else just stay RED.
  pinMode(rPin, OUTPUT); pinMode(gPin, OUTPUT); pinMode(bPin, OUTPUT); pinMode(iPin, INPUT_PULLUP); pinMode(wPin, INPUT); pinMode(dhtPowerPin,OUTPUT);
  digitalWrite(rPin, HIGH); digitalWrite(dhtPowerPin, HIGH); delay(500);

  //rtc checks
  dht.begin();
  while (!rtc.begin()) { Serial.println(F("RTC(1)")); delay(250); }
  while (!rtc.isrunning()) { Serial.println(F("RTC(2)")); delay(250); } 
  while (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0) { Serial.println(F("Ether(0)")); delay(250); }
  ether.staticSetup(myip, gwip, dnsip, netmask);
  if (!ether.staticSetup(myip, gwip, dnsip, netmask)) { Serial.println(F("Static(0)")); while (1); }    

  //testdns
  if ((!ether.dnsLookup(dnsSite)) && (!ether.dnsLookup(ipSite))) { Serial.println("DNS failed"); while(1); }
  ether.hisport = 65000; //65535
  //ether.printIp("IP:  ", ether.myip); ether.printIp("GW:  ", ether.gwip); ether.printIp("DNS: ", ether.dnsip); ether.printIp("netmask: ", ether.netmask); ether.printIp("Port:  ", ether.hisport);
  digitalWrite(rPin, LOW); digitalWrite(gPin, HIGH); delay(1000); digitalWrite(gPin, LOW); delay(1000); digitalWrite(gPin, HIGH); delay(1000); digitalWrite(gPin, LOW);
}

static void Callback (byte status, word off, word len) {
  if (status == 0) {
    char *data = (char *) Ethernet::buffer + off;
    if ((data[9] == 0x32) && (data[10] == 0x30) && (data[11] == 0x30)) { // 200 response
      if (callCtrl) {
        String ipBuff;
        //data extract 1. start 10 char from end. 2. find ends until whitespace. 3. add chars inbetween to ipbuff
        for (int x = (strlen(data)-10); x<(strlen(data)); x++) { if ((data[x] == 0x20 || (isspace(data[x])))) { index1 = x; break; } }      
        for (int x = (strlen(data)-10); x>0; x--) { if ((data[x] == 0x20 || (isspace(data[x])))) { index2 = x; break; } }      
        for (int x = index2; x<index1; x++) { if ((data[x] == 0x2e || (isdigit(data[x])))) { ipBuff += data[x]; } } //0x2e = decimal char
        if (ipBuff.length() >= 7) { //1.2.3.4 = 7+
          ipBuff.toCharArray(publicIP, 16);
          httpCalls[0] = httpCalls[0] + 1; //pass
          callStatus = 1;
        } else {
          publicIP[0] = {""}; // can check public ip before calling dns update
          httpCalls[1] = httpCalls[1] + 1; //fail
          callStatus = 0;
        }
      } else { //dns call 338-K 337-O 
        if ((data[337] == 0x4F) && (data[338] == 0x4B)) { //OK
          httpCalls[2] = httpCalls[2] + 1; //pass
          callStatus = 1;
        } else {
          httpCalls[3] = httpCalls[3] + 1; //fail          
          callStatus = 0;
        }
      }
    }
  if (debug) {Serial.println(data);}
  } else {
    httpFails += 1;
    callStatus = 0;
  }
}
void Call_Myexternal() {
      if (ether.dnsLookup(ipSite)) {
        ether.copyIp(ether.hisip, ether.hisip);
      } else {
        ether.copyIp(ether.hisip, ipSiteIP);
      }
      if (lastPort == 65000) { ether.hisport = 80; lastPort = 80; }
      ether.browseUrl(PSTR("/"), "raw", ipSite, Callback);
}
void Call_Duckdns() {
      if (ether.dnsLookup(dnsSite)) {
        ether.copyIp(ether.hisip, ether.hisip);
      } else {
        ether.copyIp(ether.hisip, dnsSiteIP);
      }
      if (lastPort == 65000) { ether.hisport = 80; lastPort = 80; }
      sprintf(conkat,"update?domains=[DUCK DOMAIN]&token=[DUCK API TOKEN]&ip=%s", publicIP); //<--------------EDIT---------------------ME----------
      ether.browseUrl(PSTR("/"), conkat, dnsSite, Callback);
}

//LED CONTROL
void RGBRESET() {
  rState = 0; gState = 0; bState = 0;
  digitalWrite(rPin, LOW); digitalWrite(gPin, LOW); digitalWrite(bPin, LOW); return;
}
void RGBLED(byte r, byte g, byte b) {
  byte bri;
  
       if ((r) && (!rState)) { for ( bri = 0; bri < 255; bri += 1 ){ analogWrite(rPin, bri); delay(5); } rState = 1; }
  else if ((g) && (!gState)) { for ( bri = 0; bri < 255; bri += 1 ){ analogWrite(gPin, bri); delay(5); } gState = 1; }
  else if ((b) && (!bState)) { for ( bri = 0; bri < 255; bri += 1 ){ analogWrite(bPin, bri); delay(5); } bState = 1; }  

       if ((!r) && (rState)) { for ( bri = 255; bri > 0; bri -= 1 ){ analogWrite(rPin, bri); delay(5); } rState = 0; }
  else if ((!g) && (gState)) { for ( bri = 255; bri > 0; bri -= 1 ){ analogWrite(gPin, bri); delay(5); } gState = 0; }
  else if ((!b) && (bState)) { for ( bri = 255; bri > 0; bri -= 1 ){ analogWrite(bPin, bri); delay(5); } bState = 0; }  
}
// http200 alert ok temp dns button
void ButtonHandler() {
    byte btnHold = 0;
    RGBRESET();
    while (!digitalRead(iPin)) {
      if (btnHold > 9) {
        //holding the button too long
        RGBRESET();
        for (byte i = 1; i < 5; i++) { digitalWrite(rPin, HIGH); delay(500); digitalWrite(rPin, LOW); delay(500); } return;
      } else {
        btnHold += 1;
        if ((btnHold > 0) && (btnHold <= 3))      { digitalWrite(rPin, HIGH); digitalWrite(gPin, HIGH); }
        else if ((btnHold > 3) && (btnHold <= 6)) { digitalWrite(gPin, LOW); digitalWrite(bPin, HIGH);  }
        else if ((btnHold > 6) && (btnHold <= 9)) { digitalWrite(gPin, HIGH); }
        delay(1000);
      }
    }
    if ((btnHold > 0) && (btnHold <= 3)) {
      //mode #1
      if (!callToogle) {
        callCtrl = 1; callToogle = 1;
        Call_Myexternal();
      } else {
        callCtrl = 1; callToogle = 0;
        Call_Duckdns();        
      }
    }
    else if ((btnHold > 3) && (btnHold <= 6)) {
      //mode #2
      digitalWrite(dhtPowerPin, LOW); delay(2000); digitalWrite(dhtPowerPin, HIGH); delay(2000); 
      GetTemps();
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      DateAndTime();
    }
    //mode #3
    else if ((btnHold > 6) && (btnHold <= 9)) { wdt_enable(WDTO_15MS); for(;;) { /*do nothing until reset*/ }; }
    else { RGBRESET(); for (byte i = 1; i < 5; i++) {  digitalWrite(rPin, HIGH); delay(500); digitalWrite(rPin, LOW); delay(500);  } return;}   
}


//TEMP and HUM FROM DHT11
void GetTemps() {
  byte fails = 0;
  while (fails < 2) {
    temp = dht.readTemperature(true);
    hum = dht.readHumidity();
    if (isnan(temp) || isnan(hum)) {
      fails += 1; temp = 0; hum = 0; delay(2000);
    } else {
      break;
    }
  }
  return;
}

//DATE AND TIME FROM RTC
void DateAndTime() {
    DateTime now = rtc.now();
    rtcMonth = now.month(); rtcDay = now.day(); rtcYear = now.year();
    rtcHour = now.hour(); rtcMinute = now.minute(); rtcSecond = now.second();
}
static word http_function() {
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Retry-After: 600\r\n"
    "\r\n"
    "$D"), callStatus);
  return bfill.position();
}
static word http_debug() {
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Retry-After: 600\r\n"
    "\r\n"
    "$D^$D^$D^$D^$D^$D"), debug, httpCalls[0], httpCalls[1], httpCalls[2], httpCalls[3], httpFails);
  return bfill.position();
}
static word http_Info() {
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Retry-After: 600\r\n"
    "\r\n"
    "$D/$D/$D^$D:$D:$D^$D^$D^$D^$D^$D"), rtcMonth, rtcDay, rtcYear, rtcHour, rtcMinute, rtcSecond, temp, hum, wallPower, outageSeq, outageTotal);
  return bfill.position();
}
static word http_NotFound() {
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 404 Not Found\r\n"
    "Content-Type: text/html\r\n\r\n"));
  return bfill.position();
}
/*_________________________________________________________________________________________________________*/
void loop(){
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);
  if (pos){
    char *data = (char *) Ethernet::buffer + pos;
    String buff;
    for (int x = 5; x<100; x++) {
      if (isspace(data[x])) {
        break;
      } else {
        buff += data[x];
      }
    }
    if (buff == "info"){
      if (!debug) { DateAndTime(); ether.httpServerReply(http_Info()); } else { ether.httpServerReply(http_debug()); } RGBLED(1,1,1);
    } else if (buff.substring(0,6) == "rtc/0/") {
      newTime[0] = 0; newTime[1] = 0; newTime[2] = 0; newTime[3] = 0; newTime[4] = 0; newTime[5] = 0; index2 = 0;
      buff = buff.substring(6);
      for (byte i = 0; i < buff.length(); i++) {
        if (buff[i] == 0x2D) { //hyphen
          if      (newTime[0] == 0) { newTime[0] = buff.substring(0, i).toInt();        index2 = i;}
          else if (newTime[1] == 0) { newTime[1] = buff.substring(index2+1, i).toInt(); index2 = i;} 
          else if (newTime[2] == 0) { newTime[2] = buff.substring(index2+1, i).toInt(); index2 = i;} 
          else if (newTime[3] == 0) { newTime[3] = buff.substring(index2+1, i).toInt(); index2 = i;} 
          else if (newTime[4] == 0) { newTime[4] = buff.substring(index2+1, i).toInt(); newTime[5] = buff.substring(i+1).toInt();} else { break; }
        }
      }
      index2 = 0;
      if ((newTime[0] != 0) && (newTime[1] != 0) && (newTime[2] != 0)) {
        rtc.adjust(DateTime(newTime[0], newTime[1], newTime[2], newTime[3], newTime[4], newTime[5])); //1/2/2017^3:4:13^75^19^1^0^0
        callStatus = 1;
      } else {
        callStatus = 0;
      }
      ether.httpServerReply(http_function());
      RGBLED(1,1,1);
    } else if (buff.substring(0,5) == "rtc/1") {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      callStatus = 1;
      ether.httpServerReply(http_function());
      RGBLED(1,1,1);
    } else if (buff == "dht") {
      callStatus = 1;
      ether.httpServerReply(http_function());
      RGBLED(1,1,1);
      digitalWrite(dhtPowerPin, LOW); delay(1000); digitalWrite(dhtPowerPin, HIGH); delay(2000);
      GetTemps();
    } else if (buff == "reset") {
      callStatus = 1;
      ether.httpServerReply(http_function());
      RGBLED(1,1,1); delay(1000); RGBLED(1,0,0); delay(3000);
      wdt_enable(WDTO_15MS); for(;;) { /*do nothing until reset*/ }
    } else if (buff == "dns") {
      ether.httpServerReply(http_Info());
      RGBLED(1,1,1);
      if (!callToogle) { callCtrl = 1; Call_Myexternal();
        if (callStatus) { callToogle = 1; }
      } else { callCtrl = 0; RGBLED(1,1,0); Call_Duckdns(); callToogle = 0; }
    } else if (buff == "debug") {
      if (debug) { debug = 0; } else { debug = 1; }
      callStatus = 1;
      ether.httpServerReply(http_function());
      RGBLED(1,1,1);
    } else {
      callStatus = 0;
      ether.httpServerReply(http_NotFound());
      RGBLED(1,0,0);
    }
  } else {
    if (!digitalRead(iPin)){
      ButtonHandler();
    } else {
      wallPower = digitalRead(wPin);
      if (! wallPower) {
        if (cMillis - pMillis1 >= interval1) { //1 sec
          outageTotal += 1;
          outageSeq += 1;
          pMillis1 = cMillis;
          digitalWrite(rPin, HIGH); 
        } else {
          RGBRESET();
        }
      } else {
        outageSeq = 0;
        if(cMillis - pMillis2 >= interval2) { //LED ROUTINE
          if ((rtcHour == 24) && (rtcMinute == 0) && (rtcSecond <= 30)) {
            outageTotal = 0;
          } else {
            if (lastPort == 80) { ether.hisport = 65000;  lastPort = 65000; }
            RGBLED(0,0,1); RGBLED(0,1,1); RGBLED(0,0,1); interval2 = 15000;
          }
          pMillis2 = cMillis;
        } else if (cMillis - pMillis3 >= interval3) { //GET TEMPS
          RGBLED(1,0,1);
          GetTemps();
          if ((temp) && (hum)) {
            interval3 = (60000 * 10); //1800000;
          } else {
            interval3 = 60000; //900000;
          }
          pMillis3 = cMillis;
        } else if (cMillis - pMillis4 >= interval4) { //SET DNS
          if (!callToogle) {
            callCtrl = 1; //tells to parse ip out
            Call_Myexternal();
            if (callStatus) {
              callToogle = 1;
              RGBLED(0,1,0);
            } else {
              RGBLED(1,0,0);
            }
          } else {
            callCtrl = 0; // dont parse ip
            RGBLED(1,1,0);
            Call_Duckdns();
            if (callStatus) {
              interval4 = (3600000 * 6); //6 hours
              callToogle = 0;
              if (lastPort == 80) { ether.hisport = 65000;  lastPort = 65000; }
              RGBLED(0,1,0);
            } else {
              interval4 = 1800000; //30 mins
              RGBLED(1,0,0);
            }
          }
        pMillis4 = cMillis;
        }
      }
    }
  }
  delay(31); //If timers end in an even number... 
  cMillis = millis();
}
