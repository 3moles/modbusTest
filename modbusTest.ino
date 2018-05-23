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

//#define MODDEBUG

#ifdef MODDEBUG
#define MOD_PRT(x) ({x;})
#else
#define MOD_PRT(x)
#endif

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
  MOD_PRT(Serial.begin(115200));
  Serial1.begin(9600);

  //init ModBus node
  node.begin(2, Serial1);

  // init flash
  Drv.begin();  
  delay(2000);
  // get board id from board.txt
  getBoardId();
  //get MQTT server ip address from ip.txt
  getServerAddress();

  //MQTT client reset server ip address
  client.setServer(serverIp,serverPort);
  
  //Attach gprs
  while (!LGPRS.attachGPRS("cmiot","",""))
  {
    delay(500);
  }
  MOD_PRT(Serial.println("connecting"));

  //connect to MQTT server and pulish
  if (client.connect(boardId)) {
    MOD_PRT(Serial.println("hello"));
    client.publish("datatest","hello world");
  }

  if (client.connected()) {
    MOD_PRT(Serial.println("subscribe ip"));
    client.subscribe("ip");
  }
}



void loop() {
  //MOD_PRT(changeCount ++);
  result = node.readHoldingRegisters(m_startAddress, m_length);//调用相关函数
  MOD_PRT(Serial.println("start connect 485"));
  result = node.ku8MBSuccess;
  if (result == node.ku8MBSuccess) {
    MOD_PRT(Serial.println("connect 485 success"));
    char datas[60];
    char topics[8];
    String tmp = String("3d33|bb6b|3cb3|bb6b|3cb3|bb6b|3f4c|cccd|3f07|ae14|3f4c|cccd");
    /*for (uint8_t j = 0; j < m_length; j++){
      tmp = tmp + String(node.getResponseBuffer(j), HEX);
      if (j != m_length - 1){
        tmp = tmp + "|";
      }
    }*/
    tmp.toCharArray(datas, 60);
    if (client.connected()){
      MOD_PRT(Serial.print("the data is: "));
      MOD_PRT(Serial.println(datas));
      client.publish("data", datas);
    }
  }
  if (client.connected()){

    if (changeCount == 50) {
      Serial.println("publish ipr");
      client.publish("ipr","test");
      changeCount = 0;
    }
  
    MOD_PRT(Serial.println(gclient.available()));
    MOD_PRT(Serial.println("loop"));

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
    MOD_PRT(Serial.println("id.txt is exist"));
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
  MOD_PRT(Serial.print("[getBoardId]The board id is: "));
  MOD_PRT(Serial.println(boardId));
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
    MOD_PRT(Serial.print("[getServerAddress] ip: "));
    MOD_PRT(Serial.print(serverIp[0]));
    MOD_PRT(Serial.print("."));
    MOD_PRT(Serial.print(serverIp[1]));
    MOD_PRT(Serial.print("."));
    MOD_PRT(Serial.print(serverIp[2]));
    MOD_PRT(Serial.print("."));
    MOD_PRT(Serial.print(serverIp[3]));
    MOD_PRT(Serial.print(":"));
    MOD_PRT(Serial.print(serverPort));
    MOD_PRT(Serial.println(""));
  }
  else {
    MOD_PRT(Serial.println("can't find ip.txt"));
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
  MOD_PRT(Serial.print("[callback]The topic is: "));
  MOD_PRT(Serial.print(topic));
  MOD_PRT(Serial.print(" length: "));
  MOD_PRT(Serial.print(length));
  MOD_PRT(Serial.print(" payload: "));
  String rst;
  for (int i = 0; i < length; i++) {
    rst += String(char(payload[i]));
  }
  MOD_PRT(Serial.println(rst));

  wirteIpAddress(payload, length);
  
  getServerAddressFromPublish(payload, length);
  client.setServer(serverIp,serverPort);
  client.disconnect();
  delay(2000);
  MOD_PRT(Serial.println("[callback] reconfig the server"));
  while (!client.connect(boardId)) {
    delay(2000);
  }
  
  if (client.connected()) {
    MOD_PRT(Serial.println("hello"));
    client.publish("datatest","hello world");
    MOD_PRT(Serial.println("subscribe ip"));
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
  MOD_PRT(Serial.println("reconnect"));
  while (!client.connected()) {

    if (client.connect(boardId)) {
      client.publish("datatest","hello world");
      MOD_PRT(Serial.println(gclient.available()));
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
  MOD_PRT(Serial.print("[writeIpAddress] "));
  MOD_PRT(Serial.println(rst));
  
  LFile ipFile;
  char* ipPath = (char*)"ip.txt";
  if (Drv.exists(ipPath)) {
    MOD_PRT(Serial.println("[writeIpAddress] remove ip.txt..."));
    Drv.remove(ipPath);
    delay(1000);
    MOD_PRT(Serial.println("[writeIpAddress] remove success"));
  }
  MOD_PRT(Serial.println("Start write ip address to ip.txt"));
  ipFile = Drv.open(ipPath,FILE_WRITE);
  ipFile.print(rst);
  ipFile.close();
}

