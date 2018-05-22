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


uint16_t changeCount = 0;

uint16_t m_startAddress = 1000;
uint8_t m_length = 12;
uint8_t result;

byte serverIp[4];
uint16_t serverPort;
byte server[] = { 122, 112, 196, 119 }; // MQTT服务地址
uint16_t port = 4002;
char* boardId;
ModbusMaster node;
LGPRSClient gclient;

void getBoardId();
void getServerAddress();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void wirteIpAddress(byte* ip, unsigned int length);


PubSubClient client(server, port, callback, gclient);

void setup() {
  //Set serial baut rate, Serial is used for PC and Serial1 is used for 485
//  IF_SDEBUG(Serial.begin(115200))
  Serial1.begin(9600);
  node.begin(2, Serial1);

  // init flash
  Drv.begin();  
  delay(2000);
  
  // get board id from board.txt
  getBoardId();

  //get MQTT server ip address from ip.txt
  getServerAddress();

  //client reset server ip address
  client.setServer(serverIp,serverPort);
  
  //Attach gprs
  while (!LGPRS.attachGPRS("cmiot","",""))
  {
    delay(500);
  }
//  IF_SDEBUG(Serial.println("connecting");)

  //connect to MQTT server and pulish
  if (client.connect(boardId)) {
//    IF_SDEBUG(Serial.println("hello");)
    client.publish("datatest","hello world");
  }

  if (client.connected()) {
//    IF_SDEBUG(Serial.println("subscribe ip"));
    client.subscribe("ip");
  }
}



void loop() {
  //changeCount ++;
  result = node.readHoldingRegisters(m_startAddress, m_length);//调用相关函数
//  IF_SDEBUG(Serial.println("start connect 485"));
  if (result == node.ku8MBSuccess) {
//    IF_SDEBUG(Serial.println("connect 485 success"));
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
//      IF_SDEBUG(Serial.print("the data is: "));
//      IF_SDEBUG(Serial.println(datas));
      client.publish("data", datas);
    }
  }
  if (client.connected()){

    if (changeCount == 50) {
      Serial.println("publish ipr");
      client.publish("ipr","test");
      changeCount = 0;
    }
//  
//    IF_SDEBUG(Serial.println(gclient.available()));
//    IF_SDEBUG(Serial.println("loop"));

    client.loop();
  }
  delay(500);
  
  if (!client.connected()){
    reconnect();
  }
}

void getBoardId() {
  LFile idFile;
  String id;
  char* idPath = (char *)"id.txt";
  if (Drv.exists(idPath)) {
//    IF_SDEBUG(Serial.println("id.txt is exist"));
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
//  IF_SDEBUG(Serial.print("[getBoardId]The board id is: "));
//  IF_SDEBUG(Serial.println(boardId));
}

void getServerAddress() {
  LFile ipFile;
  char* ipPath = (char*)"ip.txt";
  if (Drv.exists(ipPath)) {
    Serial.println("[getServerAddress]found ip.txt");
    ipFile = Drv.open(ipPath);
    ipFile.seek(0);
    int num = 0;
    int pos = 0;
    while(ipFile.available()) {
      int tmp = ipFile.read() - 48;
      if ((tmp >= 0) && (tmp <= 9)) {
        num = num * 10 + tmp;
      }
      else {
        if (pos < 4) {
          serverIp[pos] = num;
          num = 0;
          pos++;
        }
      }  
    }
    serverPort = num;
//    IF_SDEBUG(Serial.print("[getServerAddress] ip: "));
//    IF_SDEBUG(Serial.print(serverIp[0]));
//    IF_SDEBUG(Serial.print("."));
//    IF_SDEBUG(Serial.print(serverIp[1]));
//    IF_SDEBUG(Serial.print("."));
//    IF_SDEBUG(Serial.print(serverIp[2]));
//    IF_SDEBUG(Serial.print("."));
//    IF_SDEBUG(Serial.print(serverIp[3]));
//    IF_SDEBUG(Serial.print(":"));
//    IF_SDEBUG(Serial.print(serverPort));
//    IF_SDEBUG(Serial.println(""));
  }
  else {
//    IF_SDEBUG(Serial.println("can't find ip.txt"));
    ipFile = Drv.open(ipPath,FILE_WRITE);
    ipFile.print("122.112.196.119:4001");
    serverIp[0] = 122;
    serverIp[1] = 112;
    serverIp[2] = 196;
    serverIp[3] = 119;
    serverPort = 4001;
  }
  ipFile.close();
}

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
//  IF_SDEBUG(Serial.print("[callback]The topic is: "));
//  IF_SDEBUG(Serial.print(topic));
//  IF_SDEBUG(Serial.print(" length: "));
//  IF_SDEBUG(Serial.print(length));
//  IF_SDEBUG(Serial.print(" payload: "));
  String rst;
  for (int i = 0; i < length; i++) {
    rst += String(char(payload[i]));
  }
//  IF_SDEBUG(Serial.println(rst));

  wirteIpAddress(payload, length);
  
  getServerAddressFromPublish(payload, length);
  client.setServer(serverIp,serverPort);
  client.disconnect();
  delay(2000);
//  IF_SDEBUG(Serial.println("[callback] reconfig the server"));
  while (!client.connect(boardId)) {
    delay(2000);
  }
  
  if (client.connected()) {
//    IF_SDEBUG(Serial.println("hello"));
    client.publish("datatest","hello world");
//    IF_SDEBUG(Serial.println("subscribe ip"));
    client.subscribe("ip");
  }

}

void getServerAddressFromPublish(byte* ip, unsigned int length) {
  int num = 0;
  int pos = 0;
  int i = 0;

  if (ip == NULL) return;
  if (length != 20) return ;
  while(i < length) {
    int tmp = ip[i] - 48;
    if ((tmp >= 0) && (tmp <= 9)) {
      num = num * 10 + tmp;
    }
    else {
      if (pos < 4) {
        serverIp[pos] = num;
        num = 0;
        pos++;
      }
    }
    i++;  
  }
  serverPort = num;
}

void reconnect() {
  // Loop until we're reconnected
//  IF_SDEBUG(Serial.println("reconnect"));
  while (!client.connected()) {

    if (client.connect(boardId)) {
      client.publish("datatest","hello world");
//      IF_SDEBUG(Serial.println(gclient.available()));
    } else {
      delay(5000);
    }
  }
}

void wirteIpAddress(byte* ip, unsigned int length) 
{
  String rst;
  if ((ip == NULL) || (length == 0)) {
    return;
  }

  for (int i = 0; i < length; i++) {
    rst += String(char(ip[i]));
  }
//  IF_SDEBUG(Serial.print("[writeIpAddress] "));
//  IF_SDEBUG(Serial.println(rst));
  
  LFile ipFile;
  char* ipPath = (char*)"ip.txt";
  if (Drv.exists(ipPath)) {
//    IF_SDEBUG(Serial.println("[writeIpAddress] remove ip.txt..."));
    Drv.remove(ipPath);
    delay(1000);
//    IF_SDEBUG(Serial.println("[writeIpAddress] remove success"));
  }
//  IF_SDEBUG(Serial.println("Start write ip address to ip.txt"));
  ipFile = Drv.open(ipPath,FILE_WRITE);
  ipFile.print(rst);
  ipFile.close();
}

