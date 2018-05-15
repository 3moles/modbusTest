#include <LGPRSServer.h>
#include <LGPRS.h>
#include <LGPRSUdp.h>
#include <LGPRSClient.h>
#include <LFlash.h>
#include <LSD.h>
#include <LStorage.h>

#include <ModbusMaster.h>
#include "PubSubClient.h"

#define Drv LFlash

uint16_t m_startAddress = 1000;
uint8_t m_length = 12;
uint8_t result;

ModbusMaster node;

byte server[] = { 122, 112, 196, 119 }; // MQTT服务地址

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

LGPRSClient gclient;
char* boardId;

PubSubClient client(server, 4001, callback, gclient);

void setup() {
  //Set serial baut rate, Serial is used for PC and Serial1 is used for 485
  Serial.begin(115200);
  Serial1.begin(9600);
  node.begin(2, Serial1);

  // get board id from flash
  Drv.begin();  // init flash
  delay(2000);
  
  LFile idFile;
  String id;
  char* idPath = (char *)"id.txt";
  if (Drv.exists(idPath)) {
    //Serial.println("id.txt is exist");
    idFile = Drv.open(idPath);
    idFile.seek(0);
    while(idFile.available()) {
      id += String(char(idFile.read()));
    }

    //cast String to char*
    int len = id.length() + 1;
    boardId = new char [len];
    strcat(boardId,id.c_str());

  }
  else {
    boardId = (char*)"defaultId";
  }
  Serial.print("The board id is: ");
  Serial.println(boardId);

  //Attach gprs
  while (!LGPRS.attachGPRS("cmiot","",""))
  {
    delay(500);
  }
  Serial.println("connecting");

  //connect to MQTT server and pulish
  if (client.connect(boardId)) {
    Serial.println("hello");
    client.publish("datatest","hello world");
  }
}

void reconnect() {
  // Loop until we're reconnected
  Serial.println("reconnect");
  while (!client.connected()) {

    if (client.connect(boardId)) {
      client.publish("datatest","hello world");
      Serial.println(gclient.available());
    } else {
      delay(5000);
    }
  }
}

void loop() {
  
  result = node.readHoldingRegisters(m_startAddress, m_length);//调用相关函数
  if (result == node.ku8MBSuccess) {
    char datas[60];
    char topics[8];
    String tmp = "";
    for (uint8_t j = 0; j < m_length; j++){
      tmp = tmp + String(node.getResponseBuffer(j), HEX);
      if (j != m_length - 1){
        tmp = tmp + "|";
      }
    }
    tmp.toCharArray(datas, 60);
    if (client.connected()){
     client.publish("data", datas);
    }
  }
  if (client.connected()){
  
    Serial.println(gclient.available());
    Serial.println("loop");

    client.loop();
  }
  delay(500);
  
  if (!client.connected()){
    reconnect();
  }
  // put your main code here, to run repeatedly:

}
