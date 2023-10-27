/*ESP32-Si5351-FM-AM-TX-web-server    ver.: 01
 *Za kontrolo radijske postaje preko Wi Fija
 * dela na esp32 doit V1
 * var01 5.1.23 try to establish AM modulation first
 * created by stojanmar in Oct 2023
 * To control si5351 as transmitter for all HAM modulations
 * dodam se wifi vnos gaina in moči na dac2
 * naredim z interuptom ver10
 * ver upl2 umaknem interrupt v loop
 * ver ulp4 fm s CORRECTION na Si5351
 * final goal is to cover all modulation modes
 */

#include <Arduino.h>
#include <esp32/ulp.h>
#include <driver/rtc_io.h>
#include "driver/adc.h"
#include "driver/dac.h"
#include <soc/rtc.h>
#include <math.h>

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
//#include <ArduinoOTA.h>
#include <si5351.h>
#include <Wire.h>  //defaut sta pina 21 SDA in 22 SCL

#define SI5351_REF     25000000UL  //change this to the frequency of the crystal on your si5351’s PCB

//#define BAUD_RATE 9600
#ifndef STASSID
#define STASSID "xxxxxxx"  //define your SSID
#define STAPSK  "xxxxxxx"  //define your password
#endif
#define DELAY_BETWEEN_COMMANDS 1000
#define CORRECTION 115555 // See how to calibrate your Si5351A (0 if you do not want).162962 je dalo 1280 Hz prenizko
//#define DAC1 25  // bo na DAC pinu 25 
// OLED - Declaration for a SSD1306 display connected to I2C (SDA, SCL pins)
//SSD1306AsciiAvrI2c display;

// The Si5351 instance.
Si5351 si5351;

const char* ssid = STASSID;
const char* password = STAPSK;

const char* PARAM_INPUT_1 = "input1";
const char* PARAM_INPUT_2 = "input2";
const char* PARAM_INPUT_3 = "input3";
const char* PARAM_INPUT_4 = "input4";
const char* PARAM_INPUT_5 = "input5";
const char* PARAM_INPUT_6 = "input6";
const char* PARAM_INPUT_7 = "input7";

const int bufferSize = 1024;
const int bufferOffset = 1024;
const int indexOffset = bufferOffset - 1;
const int samplingRate = 20000;

int dacout = 0;
int txmode = 3;  //1 LSB, 2 USB, 3 AM, 4 FM .....
//String izhodonoff = "Off"; // On or Off
//String inString = "";    // string to hold input
int offsetadc = 0;
int dcoffset = 127;  // offset na dac izhodu za power level
int powerdac = 0;   //kot dodatni offset za povečat dac izhod v + ali -
int fmbase = 30; //bias napetost v FM načinu
int dif = 0;
int currentIndex = 0;
int32_t cal_factor = 0;  //uporabim za FM
float mojgain = 1.0;
float zacasna;
volatile int anvhod34;
volatile int micoffset = 1348;  // odvisno od srednje napetosti iz mikrofona not used yet
volatile int dacvalue = 0; // vrednost za na DAC izhod
unsigned long timenow = 0;
unsigned long timedif = 0;
unsigned long previousMicros = 0UL;
unsigned long interval = 80UL; // 12,5kHz
volatile double frekvin = 27125000; // 1xHz
volatile double frekvinfm;
double stepin = 1000; // za 1kHz

char buf1[10] = "27125000";
//uint8_t dacout = 0;
String strgain = "1.00";
String frekvenca = "200000000";  //do 200Mhz gre
String stepinc = "1000000"; //bi bil increment 1Mhz oz. 250Hz pri SDR
String strpowerdac = "255";

String readfrekvenca() {
  //int webfrekvenca = frekvin / 4;  // nazaj v readable frekvenco
    //Serial.println(frekvin); 
  return String(frekvin);
}

// HTML web page to handle 7 input fields (input1, input2, input3....)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Esp32 remote VFO</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <style>
body {
   background-color: #bb8899;
   font-family: Arial, Helvetica, sans-serif;
}
h1 {
   color: navy;
   margin-left: 20px;
   font-size: 150%
}
meter {
  width: 240px;
  height: 25px; 
}
input[type=submit] {
  width: 80px;
  height: 33px;
  background-color: #128F76;
  border: none;
  color: white;
  text-align: center;
  text-decoration: none;
  display: inline-block;
  font-size: 16px;
  margin: 4px 2px;
  cursor: pointer;
  border-radius: 5px;
}
input[type="text"] {
   margin: 0;
   height: 28px;
   background: white;
   font-size: 16px;
   width: 120px;
   appearance: none;
   box-shadow: none;
   border-radius: 5px;
   -webkit-border-radius: 5px;
   -moz-border-radius: 5px;
   -webkit-appearance: none;
   border: 1px solid black;
   border-radius: 10px;
}
</style>
<center>
  <h1>** Web VFO FM AM TX **</h1>
  <p>Augain: <meter  max="100" value="50" align="left"></meter></p>
  <form action="/get">
    Frekvenca : <input type="text" name="input1">
    <input type="submit" value="Submit">
  </form>
  <form action="/get">
    Stepincdec: <input type="text" name="input2" value="1000">
    <input type="submit" value="Submit">
  </form>
  <form action="/get">
    VFO power: <input type="text" name="input3" value="0">
    <input type="submit" value="Submit">
  </form>
  <form action="/get">
    Makestep+: <input type="text" name="input4" value="Up">
    <input type="submit" value="Submit">
  </form>
  <form action="/get">
    Makestep -: <input type="text" name="input5" value="Down">
    <input type="submit" value="Submit">
  </form>
  <form action="/get">
    Modulation: <input type="text" name="input6" value="A">
    <input type="submit" value="Submit">
  </form>
  <form action="/get">
    Augainf 1.0 <input type="text" name="input7" value="1.0">
    <input type="submit" value="Submit">
  </form>
  <br>
   <br>
   <span id="resultstr">%FREKVENCA%</span>
   <input type="text" width="200px" size="31" id="resultstr" placeholder="frekvenca v Hz"><br>
   <p>Freq and step in Hz, Step = Up or Down</p>
   <p>Stran kreirana Okt 2023 : stojanmar </p><br>
  </center>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("resultstr").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/resultstr", true);
  xhttp.send();
}, 2000) ;
</script>
</html)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
AsyncWebServer server(80);
//ESP8266WebServer server(80);

String processor(const String& var){
  //Serial.println(var);
  if(var == "FREKVENCA"){
    return readfrekvenca();
  }
  return String();
}

void startULPADC()
{
  //calculate ULP clock
  unsigned long rtc_8md256_period = rtc_clk_cal(RTC_CAL_8MD256, 1000);
  unsigned long rtc_fast_freq_hz = 1000000ULL * (1 << RTC_CLK_CAL_FRACT) * 256 / rtc_8md256_period;

  adc1_config_width(ADC_WIDTH_BIT_9);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);  //pin34
  adc1_ulp_enable();

  int loopCycles = 90;
  //Serial.print("Real RTC clock: ");
  //Serial.println(rtc_fast_freq_hz);
  int dt = (rtc_fast_freq_hz / samplingRate) - loopCycles;
  
  const ulp_insn_t adcProg[] = {
  I_MOVI(R0, 0),  //immediately 0 in R0, R0=0
  I_MOVI(R3, 0),  //R3=0
  I_DELAY(dt),                // 6 + dt
  I_ADC(R1, 0, 6), // 0 pomeni ADC1  1 bi bilo ADC2  in 6 pomeni pin34 store to R1
  //store sample
  I_ST(R1, R0, bufferOffset), // 8 spravi R1 v memory naslov R0+bufferOffset
  //increment addres
  I_ADDI(R0, R0, 1),          // 6 inkrement R0
  I_ANDI(R0, R0, 1023),       // 6 anda R0 to be 1023 max
  //store index
  I_ST(R0, R3, indexOffset),  // 8 spravi R0-inkrementiran R0 v naslov R3+1023
  //jump to start
  I_BXI(2)};                  // 4 jump to second instruction

  size_t load_addr = 0;
  size_t size = sizeof(adcProg)/ sizeof(ulp_insn_t);
  ulp_process_macros_and_load(load_addr, adcProg, &size);
//  this is how to get the opcodes
//  for(int i = 0; i < size; i++)
//    Serial.println(RTC_SLOW_MEM[i], HEX);
    
  //start
  RTC_SLOW_MEM[indexOffset] = 0;
  ulp_run(0);  
}

void setup() {
    
  Serial.begin(115200);
  Serial.println("Setup process.");
  delay(100); //da se napetosti stabilizirajo
 
  // Enable DAC Channel's Outputs
  dac_output_enable(DAC_CHANNEL_1);
  dac_output_enable(DAC_CHANNEL_2);
  
  Wire.begin(21, 22, 800000);  // Fast mode ali Fm+ mode do 800000 1mhz ne dela sigurno
  //analogValue = analogRead(34);  // samo zato da nastavi pin za ADC funkcijo
  
  // Set your Static IP address
  IPAddress local_IP(192, 168, 1, xxx);  //set your static IP you want
  // Set your Gateway IP address
  IPAddress gateway(192, 168, 1, 1);

  IPAddress subnet(255, 255, 0, 0);
  IPAddress primaryDNS(8, 8, 8, 8);   //optional
  IPAddress secondaryDNS(8, 8, 4, 4); //optional
   // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("StaticIP Failed to configure");
      }
  wifiMulti.addAP("xxxxxxx", password);   // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("yyyyyyy", password);
  //wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");
  Serial.println("Connecting");
  int i = 0;
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(250);
    Serial.print('.');
  }
  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer

    if (MDNS.begin("VFO for TX")) {
    Serial.println("MDNS Responder Started");
  }

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/resultstr", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readfrekvenca().c_str());
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      frekvenca = inputMessage;
      frekvin = frekvenca.toInt(); // morda funkcija atol() tudi dela
      //si5351.set_freq(frekvin * 100, SI5351_CLK0);
      //timenow = micros();
      si5351.set_freq(frekvin * 100, SI5351_CLK1); //traja 1350 us
      //timedif = micros() - timenow;
      //Serial.println(timedif); // trajanje
               
    }
    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
      stepinc = inputMessage;
      stepin = stepinc.toInt();
            
    }
    // GET input3 value on <ESP_IP>/get?input3=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_3)) {
      inputMessage = request->getParam(PARAM_INPUT_3)->value();
      inputParam = PARAM_INPUT_3;
      strpowerdac = inputMessage;
      powerdac = strpowerdac.toInt();
      powerdac = constrain(powerdac, 0, 255);
      //dacWrite(26, powerdac);
      dac_output_voltage(DAC_CHANNEL_2, powerdac);          
    }
    // GET input4 value on <ESP_IP>/get?input4=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_4)) {
      inputMessage = request->getParam(PARAM_INPUT_4)->value();
      inputParam = PARAM_INPUT_4;
      if (inputMessage == "Up") {frekvin = frekvin + stepin;} // povišaj frekvenco     
      //si5351.set_freq(frekvin * 100, SI5351_CLK0);
      si5351.set_freq(frekvin * 100, SI5351_CLK1); 
    }
    // GET input5 value on <ESP_IP>/get?input5=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_5)) {
      inputMessage = request->getParam(PARAM_INPUT_5)->value();
      inputParam = PARAM_INPUT_5;
      if (inputMessage == "Down") {frekvin = frekvin - stepin;} // znizaj
      //si5351.set_freq(frekvin * 100, SI5351_CLK0);
      si5351.set_freq(frekvin * 100, SI5351_CLK1); 
    }
    // GET input6 value on <ESP_IP>/get?input6=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_6)) {
      inputMessage = request->getParam(PARAM_INPUT_6)->value();
      inputParam = PARAM_INPUT_6;
      //if (inputMessage == "L") {swSer.println("L");} // LSB mode 1 todo
      //if (inputMessage == "U") {swSer.println("U");} // USB mode 2 todo
      if (inputMessage == "A") {
         txmode = 3;
         si5351.set_correction(CORRECTION, SI5351_PLL_INPUT_XO);   // zamika frekvenco?
         interval = 80UL;} // AM mode 3, sampling 12,5kHz
      if (inputMessage == "F") {txmode = 4; interval = 200UL; dac_output_voltage(DAC_CHANNEL_1, fmbase);} // FM-N mode 4, sampling 5kHz
      
      /*if (modulacija == "W") {xx=7;} // FM-W mode to do
      if (modulacija == "C") {xx=2;} // CW mode todo*/ 
    }
    // GET input7 value on <ESP_IP>/get?input7=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_7)) {
      inputMessage = request->getParam(PARAM_INPUT_7)->value();
      inputParam = PARAM_INPUT_7;
      strgain = inputMessage;
      mojgain = strgain.toFloat();
 
    }
    
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    
      request->send_P(200, "text/html", index_html);                                                                
  });
  server.onNotFound(notFound);
  server.begin();
  Serial.println("HTTP Server Started");

  // Initiating the Signal Generator (si5351)
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, SI5351_REF, CORRECTION); //nastavi tudi wire na 100kHz
  
  si5351.set_freq(frekvin * 100, SI5351_CLK1);
  
  //si5351.set_phase(SI5351_CLK0, 0);
  //si5351.set_phase(SI5351_CLK1, 50); tko jih das za 90 deg
  si5351.set_phase(SI5351_CLK1, 0);
  // We need to reset the PLL before they will be in phase alignment
  si5351.pll_reset(SI5351_PLLA);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_4MA); // Set for max power default is 2MA  max je 8
  //si5351.set_clock_pwr(SI5351_CLK0, 0); // Disable the clock initially
  // To see later
  // si5351.set_freq(bfoFreq, SI5351_CLK1);
  //si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 0);  // ugasnem tetji izhod

  si5351.update_status();
  // Show the initial system information
  Serial.println("Module initializated");

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  //findoffset();
  //dacWrite(26, powerdac);  //vse do tu vzame cca xxus
  startULPADC();
  
  Serial.println("Clock speed");
  Serial.println(Wire.getClock());
  //Wire.getClock();
} // konec setup


void loop() {
  
  unsigned long currentMicros = micros();

  if(currentMicros - previousMicros > interval)
  {
  switch(txmode)
           {
  case 3:   //AM modulacija sampling 12,5kHz
    //timenow = micros();
  currentIndex = (RTC_SLOW_MEM[indexOffset] + 100) & 1023;   //1024 - 100 samples away
  anvhod34 = RTC_SLOW_MEM[currentIndex + bufferOffset] & 0xffff;  //9 bitno
  //anvhod34 = anvhod34>>1;  //8 bitno
  dacout = anvhod34 - 255; //sredina 9 bitno
  zacasna = dacout * mojgain;
  dacvalue = ((int) zacasna>>1) + dcoffset; // + powerdac; // dcofset postavi na sredino powerdc doda + ali -
  dacvalue = constrain(dacvalue, 0, 255);
  dac_output_voltage(DAC_CHANNEL_1, dacvalue);
    //timedif = micros() - timenow;
    break;
  case 4:   //FM modulacija se spremeni sampling na 5kHz
    //timenow = micros();
    
   currentIndex = (RTC_SLOW_MEM[indexOffset] + 100) & 1023;   //100 samples away
   anvhod34 = RTC_SLOW_MEM[currentIndex + bufferOffset] & 0xffff;  //9 bitno
   //anvhod34 = anvhod34>>1;  //8 bitno
   dacout = anvhod34 - 255; //sredina 9 bitno
   zacasna = dacout * mojgain;
   dif = (int) zacasna; // deviacija frekvence od nosilca
   cal_factor = (int32_t)(dif*200 + CORRECTION);
    //timenow = micros();
   si5351.set_correction2(cal_factor, SI5351_PLL_INPUT_XO);   // zamika frekvenco?
   //timedif = micros() - timenow;
   // Serial.println(timedif); // trajanje
  
    break;

  default:
    // statements
    break;
           }

  previousMicros = currentMicros;
  }
    //delay(1);
}

// Function finding zero offsets of the ADC, not used yet
/*void findoffset(){
    const int calibration_rounds = 1000;     
    // read the adc voltage 1000 times ( arbitrary number )
    for (int i = 0; i < calibration_rounds; i++) {
      offsetadc = offsetadc + local_adc1_read(ADC1_CHANNEL_6); // - micoffset;  //Je za pin 34 mikrofon vhod
        delay(1);
    }
    // calculate the mean offsets
    offsetadc = offsetadc / calibration_rounds;    
}*/
