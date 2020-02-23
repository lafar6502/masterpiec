#include <Ethernet.h>
#include "global_variables.h"
#include "boiler_control.h"
#include "burn_control.h"




byte mac[] = { 0xE1, 0xC9, 0x3E, 0xEC, 0x91, 0x22 };   //physical mac address
byte ip[] = { 192, 168, 88, 8 };                      // ip in lan (that's what you need to use in your browser. ("192.168.1.178")
byte gateway[] = { 192, 168, 88, 1 };                   // internet access via router
byte subnet[] = { 255, 255, 255, 0 };                  //subnet mask
EthernetServer server(80);                             //server port     
String readString;


void setupWebServer() {
  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();
}


void webHandlingTask() {
  EthernetClient client = server.available();
  if (client) {
    while (client.connected()) {   
      if (client.available()) {
        char c = client.read();
     
        //read char by char HTTP request
        if (readString.length() < 100) {
          //store characters to string
          readString += c;
          //Serial.print(c);
         }

         if (c == '\n') 
         {          
           Serial.println(readString); //print to serial monitor for debuging
           client.println("HTTP/1.1 200 OK"); //send new page
           client.println("Content-Type: application/json");
           client.println();
           client.print("{\"TempCO\":");
           client.print(g_TempCO);
           client.print("}");
           
           delay(1);
           //stopping client
           client.stop();
         }
      }
    }
  };     
}
