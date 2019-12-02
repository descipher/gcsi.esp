/**
 *	File:       	GCWebApp.ino
 *	Version:  	1.0
 *	Date:       	2019
 *	License:	GPL v3
 *	Description:	Geiger.Counter.1 esp8266 main code
 *	Project:	Geiger.Counter.1, WiFi, Logging, USB Interface and low power field operations.
 *
 *  	Copyright 2019 by Gelidus Research Inc, mike.laspina@gelidus.ca
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 * 	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * 	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include "NTPClient.h"
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "TimeLib.h"
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "gcio.h"
#include <stdio.h>
#include <stdlib.h>

GC gc;

uint32_t last_milli_stamp = 0;
uint32_t current_milli_stamp = 0;
char _pagebuff[8192];
int utc_offset = -6 * 3600;
DynamicJsonDocument jdoc(4096);

ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, utc_offset);
IPAddress ip;
String ipStr;
const char * ssid = "GC.1"; //default auto MDNS config ssid
const char * host = "gc1";  //default host name
String _argName;
char jsonBuff[512] = {0};
char timeStr[9];
char SSID[9];
char dateStr[11];
char gctimeStr[9];
char gcdateStr[11];
char idBuff[9];
uint16_t cps[60];
uint16_t cpm[60];
uint16_t geigerCPM = 0;
uint16_t geigerCPH = 0;
uint8_t gcCPSIndex = 0;
uint8_t gcCPMIndex = 0;
String sendjcmd;
String ackjcmd;
uint8_t jcmdq = 0;
bool configFlag = true;
const int parms = 11;
DynamicJsonDocument gcdoc(1024);

void setup(void) {
  
  Serial.begin(38400, SERIAL_8N1);
  WiFi.hostname(host);
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(300);
  wifiManager.setDebugOutput(false);
  
  //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  //wifiManager.setSTAStaticIPConfig(IPAddress(192,168,0,99), IPAddress(192,168,0,1), IPAddress(255,255,255,0));
  //WiFiManagerParameter custom_text("<p>This is just a text paragraph</p>");
  //wifiManager.addParameter(&custom_text);
  
  wifiManager.autoConnect(ssid);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
  }
  server.on("/", handleRoot);
  server.on("/gcdata", handleData);
  server.on("/monitor", handleMonitor);
  server.on("/config", handleConfig);
  server.onNotFound(handleNotFound);
  server.begin();
  timeClient.begin();
  ip = WiFi.localIP();
  MDNS.begin(host);
  MDNS.addService("http", "tcp", 80);
  ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  getTime();
  gcdoc["voltage"] = 0;
  gcdoc["cpm"] = 0;
  gcdoc["dose"] = 0.0;
  gcdoc["cps"] = 0; 
}

void getTime(){
  if (timeClient.forceUpdate()) {
    unsigned long NTPstamp = timeClient.getEpochTime();
    time_t utcCalc = NTPstamp - 2208988800UL ;
    timeClient.getFormattedTime().toCharArray(timeStr, 9);
    timeClient.getFormattedDate().toCharArray(dateStr, 11);
  } else {
  memcpy (timeStr, "00:00:00\0",9);
  memcpy (dateStr, "01/01/1900\0",11);
  }
}

void sendTime() {
    Serial.print("{\"time\":\"");
    Serial.print(timeStr);
    Serial.print("\"}\n");
}
void sendDate() {    
    Serial.print("{\"date\":\"");
    Serial.print(dateStr);
    Serial.print("\"}\n");
}

void getConfig() {
    Serial.print("{\"get\":\"config\"}\n");
    Serial.println();
}

void sendIP() {
    Serial.print("{\"ip\":\"");
    Serial.print(ipStr);
    Serial.print("\"}\n");
}

void sendSSID() {
    Serial.print("{\"ssid\":\"");
    Serial.print(ssid);
    Serial.print("\"}\n");
}

//..........................................................................
const char root[] =
  "<html>\n\
  <style>\n\
  body{background-color:black;border:4px double white;padding:8px; }\n\
  table{color:white;font-family:monospace;font-size:1.2em;}\n\
  h1{text-align:center;color:blue;font-family:verdana;font-size:1.4em; }\n\
  nav{color:blue;font-family:verdana;font-size:1em;}\n\
  a{color:blue;font-family:verdana;font-size:1em; }\n\
  th,td{border:1px solid white;font-family:monospace;padding:4px; }\n\
  </style>\n\
  <link rel=icon href=data:;base64,=>\n\
  <nav><a href=/monitor>Monitor</a>   <a href=/config>Config</a></nav>\n\
  <h1>Gelidus Research Inc.</h1>\n\
  <h1>GC1 Monitor</h1>\n\
  </html>";
  
void handleData() {
  serializeJsonPretty(gcdoc, _pagebuff);
  server.send(200, "application/json", _pagebuff);
}

void handleMonitor() {
  char _ip[17];
  ipStr.toCharArray(_ip,17);
  snprintf(_pagebuff, 8192, "<html> \n\
  <head> \n\
  <style> \n\
  html{min-width: 100%; min-height:100%}\n\
  #canvas1{background: #000000; margin-left:-2px; margin-top:2px; border:1px solid #ffffff;}\n\
  #canvas2{background: #000000; margin-left:-2px; margin-top:2px; border:1px solid #ffffff;}\n\
  #svg1{width: 14px; margin-left:2px;}\n\
  #svg2{width: 20px; margin-left:-2px;}\n\
  #div1{color:blue;;text-align: center;font-family:monospace;font-size:1.5em;border:1px solid #ffffff;width:292px;margin-left:2px;margin-bottom:2px}\n\
  #div2{color:blue;;text-align: center;font-family:monospace;font-size:1.5em;border:1px solid #ffffff;width:292px;margin-left:2px;}\n\
  #div3{border:1px solid #ffffff;width:292px;margin-left:2px;}\n\
  body{width: 300px; height: 475px; background-color:black;padding:2px;}\n\
  table{color:white;font-family:monospace;font-size:1.2em;}\n\
  nav{color:blue;font-family:monospace;font-size:1em;width:288px;}\n\
  p{color:red;font-family:courier;text-align:right;margin-right:60;font-size:1em;}\n\
  a{color:blue;font-family:verdana;font-size:1em;}\n\
  th,td{border:1px solid white;width:140px; font-family:monospace;padding:2px;}\n\
  </style>\n\
  </head>\n\
  <body onLoad=\"loadJSON()\">\n\
  <link rel=icon href=data:;base64,=>\n\
  <nav align=\"center\"><a href=/monitor>Monitor</a>   <a href=/config>Config</a></nav>\n\
  <table><div id=div1>Gelidus Research Inc.</div>\n\
  <div id=div2>GC Monitor</div>\n\
  <tr><th>Data</th><th>Value</th></tr>\n\
  <tr><td>CPS Reading</td><td id=cps>%u</td></tr>\n\
  <tr><td>CPM Reading</td><td id=cpm>%lu</td></tr>\n\
  <tr><td id=doseScale>Dose uSv/h</td>\n\
  <td id=dose>%f</td></tr>\n\   
  <tr><td>Tube Voltage</td>\n\
  <td id=voltage>%lu</td></tr>\n\      
  </table>\n\
  <div id=div3>\n\
    <svg id=\"svg1\" height=\"100\">\n\
        <text id=cpsScale font-size=\"10px\" x=\"2\" y=\"8\" style=\"stroke:rgb(255,255,255);\">10</text>\n\
        <text font-size=\"10px\" x=\"9\" y=\"98\" style=\"stroke:rgb(255,255,255);\">0</text>\n\
        <text font-family=\"monospace\" transform = \"rotate(-90)\" font-size=\"12px\" x=\"-55\" y=\"11\" style=\"stroke:rgb(255,255,255)\">CPS</text>\n\
    </svg>\n\
    <canvas id=\"canvas1\" height=\"100\" width=\"236\">\n\
    </canvas>\n\
    <canvas id=\"canvas2\" height=\"100\" width=\"10\">\n\
    </canvas>\n\
     <svg id=\"svg2\" height=\"100\">\n\
        <text id=cpmScale font-size=\"10px\" x=\"2\" y=\"8\" style=\"stroke:rgb(255,255,255);\">100</text>\n\
        <text font-size=\"10px\" x=\"2\" y=\"98\" style=\"stroke:rgb(255,255,255)\">0</text>\n\
        <text font-family=\"monospace\" transform = \"rotate(-90)\" font-size=\"12px\" x=\"-55\" y=\"10\" style=\"stroke:rgb(255,255,255)\">CPM</text>\n\
     </svg>\n\  
  </div>\n\
  <script>\n\
  var gcArray;\n\
  var gcCPS=[60];\n\
  for (i=0;i<60;i++) {\n\
    gcCPS[i]=0;\n\
  }\n\
  var gcCPSIndex=0;\n\
  var gcValueIndex=0;\n\
  var xmlhttp = new XMLHttpRequest();\n\
  var canvas1;\n\
  var ctx1;\n\
  var canvas2;\n\
  var ctx2;\n\
  var y1Scale=1;\n\
  var y2Scale=1;\n\
  var maxCPS = 0;\n\
  canvas1 = document.getElementById(\"canvas1\");\n\
  ctx1 = canvas1.getContext(\"2d\");\n\
  ctx1.translate(0,canvas1.height);\n\    
  canvas2 = document.getElementById(\"canvas2\");\n\
  ctx2 = canvas2.getContext(\"2d\");\n\
  ctx2.translate(0,canvas2.height);\n\    
  function drawGraph() {\n\
    if ((gcArray.cpm / y2Scale) > 100) y2Scale++;\n\
    if ((y2Scale > 1) && (gcArray.cpm < 49)) y2Scale--;\n\ 
    if ((maxCPS / (y1Scale/10)) > 99) y1Scale++;\n\
    if (((y1Scale) > 1) && ((maxCPS / (y1Scale/10)) < 49 )) y1Scale--;\n\ 
    leftShiftGraph1();\n\
    ctx1.beginPath();\n\
    ctx1.lineWidth=2;\n\
    ctx1.strokeStyle=\"black\";\n\
    ctx1.moveTo(59*4,1);\n\
    ctx1.lineTo(59*4,-1*canvas1.height);\n\
    ctx1.stroke();\n\
    ctx1.beginPath();\n\
    ctx1.strokeStyle=\"blue\";\n\
    ctx1.moveTo(59*4,1);\n\
    ctx1.lineTo(59*4,(-1*(gcCPS[59] / (y1Scale/10))));\n\
    ctx1.stroke();\n\
    ctx2.beginPath();\n\
    ctx2.lineWidth=12;\n\
    ctx2.strokeStyle=\"black\";\n\
    ctx2.moveTo(4,1);\n\
    ctx2.lineTo(4,-1*canvas2.height );\n\
    ctx2.stroke();\n\
    ctx2.beginPath();\n\
    ctx2.strokeStyle=\"blue\";\n\
    ctx2.moveTo(4,1);\n\
    ctx2.lineTo(4,( -1*(gcArray.cpm / y2Scale )));\n\    
    ctx2.stroke();\n\
  }\n\
  function loadJSON() {\n\   
    var xmlhttp = new XMLHttpRequest();\n\
    xmlhttp.overrideMimeType(\"application/json\");\n\
    xmlhttp.onreadystatechange=function() {\n\
      if (this.readyState == 4 && this.status == \"200\") {\n\
        gcArray = JSON.parse(this.responseText);\n\
        gcCPS[59]=gcArray.cps;\n\
        document.getElementById(\"cps\").innerHTML = gcArray.cps;\n\
        document.getElementById(\"cpm\").innerHTML = gcArray.cpm;\n\
        document.getElementById(\"dose\").innerHTML = gcArray.dose;\n\
        document.getElementById(\"voltage\").innerHTML = gcArray.voltage;\n\
        document.getElementById(\"cpsScale\").innerHTML = (y1Scale/10)*100;\n\
        document.getElementById(\"cpmScale\").innerHTML = y2Scale*100;\n\
      }\n\
    }\n\
    xmlhttp.open(\"GET\", \"/gcdata\", true);\n\
    xmlhttp.send(null);\n\
  }\n\
  function redrawGraph1() {\n\
    for (i=0;i<60;i++) {\n\    
      ctx1.beginPath();\n\
      ctx1.lineWidth=2;\n\
      ctx1.strokeStyle=\"black\";\n\
      ctx1.moveTo(i*4,1);\n\
      ctx1.lineTo(i*4,-1*canvas1.height);\n\
      ctx1.stroke();\n\
      ctx1.beginPath();\n\
      ctx1.strokeStyle=\"blue\";\n\
      ctx1.moveTo(i*4,1);\n\
      ctx1.lineTo(i*4,(-1*(gcCPS[i] / (y1Scale/10))));\n\
      ctx1.stroke();\n\
    }\n\
  }\n\
  function leftShiftGraph1() {\n\
    var lastMax = 0;\n\
    maxCPS = 0;\n\
    for (i=0;i<60;i++) {\n\
      gcCPS[i] = gcCPS[i+1];\n\
      if (gcCPS[i] > lastMax) {\n\
        lastMax = gcCPS[i];\n\
      }\n\
    }\n\
    redrawGraph1();\n\
    maxCPS = lastMax;\n\ 
  }\n\
  setInterval(function(){\n\
    loadJSON();\n\
    drawGraph();\n\
  }, 1000);\n\
  </script>\n\
  </body>\n\
  </html>\n", gc.gcdata.geigerCPS, gc.gcdata.geigerCPM, gc.gcdata.geigerDose, gc.gcdata.inverterVoltage, _ip);

  server.send(200, "text/html", _pagebuff);
}


void handleConfig() {

  char _ip[17];
  memset(timeStr, 0, sizeof(timeStr));
  getTime();
  getConfig();
  delay(300);
  ipStr.toCharArray(_ip,17); 
  snprintf(_pagebuff, 8192, "<html> \
   <head> \
   <style> \
   body{background-color:black;border:4px double white;padding:8px;} \
   table{color:white;font-family:monospace;font-size:120%%;} \
   h1{text-align:center;color:blue;font-family:verdana;font-size:140%%;} \
   nav{color:blue;font-family:verdana;font-size:100%%;} \
   p{color:red;font-family:courier;text-align:right;margin-right:60%%;font-size:100%%;} \
   a{color:blue;font-family:verdana;font-size:100%%;} \
   th,td{border:1px solid white;width:120px;font-family:monospace;padding:4px;} \
   </style> \
   </head> \
   <body onload=\"loadFunct()\"> \
   <link rel=icon href=data:;base64,=> \
   <nav><a href=/monitor>Monitor</a>   <a href=/config>Config</a></nav> \
   <h1>Gelidus Research Inc.</h1> \
   <h1>GC1 Monitor</h1> \
   <form> \
   <button type=submit formmethod=post>Submit</button> \
   <table><tr><th>Setting Name</th><th>Value</th></tr> \
   <tr><td>unitID</td> \
   <td>%8X</td></tr> \
   <tr><td>IP Address</td> \
   <td><input type=text name=ip value=%s size=16 ></td></tr> \
   <tr><td>SSID</td> \
   <td><input type=text name=ssid value=\"%s\" size=16 ></td></tr> \
   <tr><td>Wifi Passphrase</td><td><input type=password value=\"%s\" name=pass size=16></td></tr> \
   <tr><td>Wifi Mode</td> \
   <td><select id=mode name=mode> \
   <option value=1>WifiClient</option> \
   <option value=2>WifiAP</option> \
   </select> \
   </td></tr> \   
   <tr><td>Tube</td> \
   <td><select id=tube name=tube> \
   <option value=0>Generic</option> \
   <option value=1>SBM20</option> \
   <option value=2>SI29BG</option> \
   <option value=3>SBM19</option> \
   <option value=4>LND712</option> \
   <option value=5>SBM20M</option> \
   <option value=6>SI22G</option> \
   <option value=7>STS5</option> \
   <option value=8>SI3BG</option> \
   <option value=9>SBM21</option> \
   <option value=10>SBT9</option> \
   <option value=11>SI1G</option> \
   </select> \
   </td></tr> \
   <tr><td>Sound</td><td><select id=sound name=sound> \
   <option value=0>Off</option> \
   <option value=1>On</option> \
   </select></td></tr>  \
   <tr><td>Logging</td><td> \
   <select id=log name=log> \
   <option value=0>Off</option> \
   <option value=1>On</option> \
   </select></td></tr> \
   <tr><td>Log Frequency</td> \
   <td><select id=freq name=freq> \
   <option value=0>1m</option> \
   <option value=1>5m</option> \
   <option value=2>15m</option> \
   <option value=3>30m</option> \
   <option value=4>1h</option> \
   <option value=5>2h</option> \
   <option value=6>4h</option> \
   <option value=7>8h</option> \
   <option value=8>12h</option> \
   <option value=9>24h</option> \
   </select> \
   </td></tr> \
   <tr><td>Time</td><td><input type=time value=%s name=time></td></tr> \
   <tr><td>Date</td><td><input type=date value=%s name=date></td></tr> \ 
   </form> \
   <script> \
     function loadFunct() { \
       document.getElementById(\"tube\").selectedIndex = \"%u\";  \
       document.getElementById(\"sound\").selectedIndex = \"%u\";  \
       document.getElementById(\"mode\").selectedIndex = \"%u\";  \
       document.getElementById(\"freq\").selectedIndex = \"%u\";  \
       document.getElementById(\"log\").selectedIndex = \"%u\"; } \      
   </script> \
   </table> \
   </body> \
   </html>", gc.gcdata.unitID, _ip, gc.gcdata.ssid, gc.gcdata.password, timeStr, dateStr, gc.gcdata.geigerTube, gc.gcdata.speakerState, gc.gcdata.wifiMode, gc.gcdata.logInterval, gc.gcdata.loggingState);

  server.send(200, "text/html", _pagebuff);
  if (server.args()) {
    Serial.print("{");
    for (int i = 0; i < parms; i++) {
      _argName = server.argName(i);
      if ((_argName == "mac") or (_argName == "ip") or (_argName == "ssid") or (_argName == "pass") or (_argName == "time") or (_argName == "date")){ 
        Serial.print("\"" + server.argName(i) + "\":\"");   //Get the name of the parameter
        Serial.print(server.arg(i) + "\"");              //Get the value of the parameter
      } else {
        Serial.print("\"" + server.argName(i) + "\":");   //Get the name of the parameter
        Serial.print(server.arg(i));              //Get the value of the parameter
      }  
      if ((i < parms - 1) ? Serial.print(",") : Serial.print(""));
    }
    Serial.print("}\n");
    Serial.println();
  }
}

//..........................................................................

void handleRoot() {
  server.send(200, "text/html", root);
}

void handleNotFound() {

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void loop(void) {
  int i; //loops

  current_milli_stamp = millis();
  if ((current_milli_stamp - last_milli_stamp) > 5000 ) {
    if (timeClient.update()) {
      getTime();
    }
    last_milli_stamp = millis();
  }

  
  if (Serial.available()) {
    
    Serial.readBytesUntil(10,jsonBuff,sizeof(jsonBuff));
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(jdoc, jsonBuff);
    JsonObject gccfg = jdoc.as<JsonObject>();
    // Test if parsing succeeds.
    if (error) {
      Serial.println("JSON Data Error");
    } else {
    if (gccfg.containsKey("date")) {  
       gccfg["time"].as<String>().toCharArray(gctimeStr,9);
       gccfg["date"].as<String>().toCharArray(gcdateStr,11);
    }
    if (gccfg.containsKey("cpm")) {
		  gc.gcdata.geigerCPM = gccfg["cpm"].as<unsigned long>();
		  gc.gcdata.inverterVoltage = gccfg["voltage"].as<unsigned int>();
		  gc.gcdata.geigerDose = gccfg["dose"].as<double>();
		  gc.gcdata.geigerCPS = gccfg["cps"].as<unsigned int>();
  		gcdoc["cps"]=gc.gcdata.geigerCPS;
  		gcdoc["cpm"]=gc.gcdata.geigerCPM;
  		gcdoc["voltage"]=gc.gcdata.inverterVoltage;
  		gcdoc["dose"]=gc.gcdata.geigerDose;
      cps[gcCPSIndex] = gc.gcdata.geigerCPS;
      gcCPSIndex++;
      for (i=0;i<59;i++) {
        geigerCPM +=cps[i];
      }      
      if (gcCPSIndex > 59) {
        gcCPSIndex = 0;
        gcCPMIndex++;
        cpm[i] = geigerCPM;
        for (i=0;i<59;i++) {
          geigerCPH +=cpm[i];
        }
      }
      if (gcCPMIndex > 59) gcCPMIndex = 0;
    }

    if (gccfg.containsKey("sound")) {
        gc.gcdata.speakerState = gccfg["sound"].as<unsigned char>();
        gc.gcdata.loggingState = gccfg["log"].as<unsigned char>();
        gc.gcdata.geigerTube = gccfg["tube"].as<unsigned char>();
        gc.gcdata.logInterval = gccfg["freq"].as<unsigned char>();
        gc.gcdata.unitID = gccfg["mac"].as<unsigned long long>();                
//        gc.gcdata.wifiMode = gccfg["mode"].as<unsigned char>();
        gccfg["ssid"].as<String>().toCharArray(gc.gcdata.ssid,17);       //.toCharArray(gc.gcdata.ssid,17)        
//        gccfg["pass"].as<String>().toCharArray(gc.gcdata.password,17);     //.toCharArray(gc.gcdata.password,17)

    }

    if (gccfg.containsKey("get")) {
        sendjcmd = gccfg["get"].as<String>();
    }
        
    if (gccfg.containsKey("ack")) {    
        ackjcmd = gccfg["ack"].as<String>();  
    }
    
  }
  memset(jsonBuff,0,sizeof(jsonBuff));
  }
  
  if (ackjcmd == "date") {
      jcmdq = 0;
      ackjcmd = "";
	  jdoc["ack"] = "";
  }
    
  if (ackjcmd == "time") {
      jcmdq = 0;
      ackjcmd = "";
	    jdoc["ack"] = "";
  }
  
  if (ackjcmd == "ip") {
      jcmdq = 0;
      ackjcmd = "";
	    jdoc["ack"] = "";
  }

  if ((sendjcmd == "ip") && (jcmdq == 0)) {
      sendIP();
      sendjcmd = "";
	    jdoc["get"] = "";      
  }

  if ((sendjcmd == "time") && (jcmdq == 0)) {
      sendTime();
      sendjcmd = "";
  }
       
  if ((sendjcmd == "date") && (jcmdq == 0)) {
      sendDate();
      sendjcmd = "";
	    jdoc["get"] = "";
  }

  if ((sendjcmd == "ssid") && (jcmdq == 0)) {
      sendSSID();
      sendjcmd = "";
	    jdoc["get"] = "";
  }

  
  server.handleClient();
  
}
