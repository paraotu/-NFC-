
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>

#define Relay_out     8     //继电器控制脚
#define CHARGE_TIME (8 * 3600000) //充电8小时后关闭
//#define CHARGE_TIME   6000  //测试6s

#define Buzzer_PIN    6     // 蜂鸣器脚
#define Buzzer_Feq    2100  // 频率

#define CARD_NUM      5
/* MFRC522 管脚初始化配置*/
#define RST_PIN         9           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.


/* NFC卡信息结构体 */
struct Card_Info{
  byte _info[5];
};
/* 扩展定时器结构体 */
struct TickTimer{
  unsigned long start;
  unsigned long interval;
  char is_on;
};

/* ============================================================================================ */
/* ========================== 全局变量 ========================================================== */
// 申请的定时器，用于定时
TickTimer tick_off;

// 管理员Card UID
const Card_Info Admin_Card = {0xF0,0x54,0x37,0x8E, 0x20};/*居住证*/
//const Card_Info Admin_Card = {0x64,0x6D,0xA8,0x64, 0x08};/*空白卡*/
//const Card_Info Admin_Card = {0x03,0xC4,0xF4,0xB9, 0x08};/*钥匙卡*/
const Card_Info Empty_Card = {0,0,0,0,0};
// 充电卡信息
Card_Info ChargeCard[CARD_NUM] = {
  0
//  {0x64,0x6D,0xA8,0x64,0x08},
};
// 主程序使用变量
byte Admin_status = 0;    /* 0为正常刷卡充电；1为增删卡模式 */
byte m = 0;
byte relay_status = 0;
/* =======================定时器相关函数=============================================================== */
/* 扩展定时器开启 返回0开启成功，1已存在*/
char Timer_Start(TickTimer* _timer, unsigned long _inv)
{
  char t_status;
  if(_timer->is_on){
    t_status = 1;
  }else
  {
    _timer->start = millis();
    _timer->interval = _inv;
    _timer->is_on = 1;

    t_status = 0;
  }
  return t_status;
}
/* 扩展定时器检查，返回0定时时间到 */
unsigned long Timer_Check(TickTimer* _timer)
{
  unsigned long t_status = 0xFFFFFFFF;
  if(_timer->is_on){
    t_status = millis();
    // 计算时间间隔
    if(t_status < _timer->start){
      t_status += 0xFFFFFFFF - _timer->start;
    }else{
      t_status -= _timer->start;
    }
    if(t_status >= _timer->interval){
      // 定时时间到
      t_status = 0;
      _timer->start += _timer->interval;
    }
  }
  return t_status;
}
/* 扩展定时器停止 */
void Timer_Stop(TickTimer* _timer)
{
  _timer->is_on = 0;
}

/* =====================MFRC522相关函数================================================================= */
/* 16进制打印字节数值 */
void dump_byte_array(byte *buffer, byte bufferSize)
{
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}
/*
 * 比较NFC卡信息
 * 返回值为1：信息相同
 * 返回值为0：信息不同
 */
byte Compare_NFC_Card_Info(Card_Info card)
{
  byte j;
  for(j = 0; j < 4; j++){
    if(mfrc522.uid.uidByte[j] != card._info[j]){
      break;
    }
  }
  if(card._info[4] == mfrc522.uid.sak){
    j++;
  }
  if(5 == j){
    return 1;
  }else{
    return 0;
  }
}
/*
 * 寻找存储空余位置
 * 返回值为1：此位置为空
 * 返回值为0：此位置有信息
 */
byte Find_Empty_Pos(byte pos)
{
  byte i;
  for(i = 0; i < 5; i++){
    if(Empty_Card._info[i] != ChargeCard[pos]._info[i]){
      break;
    }
  }
//  Serial.print("i=");
//  Serial.println(i);
  if(5 == i){
    return 1;
  }else{
    return 0;
  }
}
/* 清除卡信息 */
void Clear_CardInfo(Card_Info* card)
{
  for(byte i = 0; i < 5; i++){
    card->_info[i] = 0;
  }
}
/* 将新获取的卡信息填入数组 */
void Fill_New_CardInfo(Card_Info* card)
{
  for(byte i = 0; i < 4; i++){
    card->_info[i] = mfrc522.uid.uidByte[i];
  }
  card->_info[4] = mfrc522.uid.sak;
}

/* 删除卡信息
 * 删除对应位置的卡信息
 */
void Delete_CardInfo(byte pos)
{
  byte i = pos;
  // 后方信息向前覆盖
  for(; i < (CARD_NUM-1); i++){
    ChargeCard[i] = ChargeCard[i+1];
  }
  // 最后一条信息清空
  Clear_CardInfo(&ChargeCard[CARD_NUM-1]);
}
/* 增加卡信息 */
void Add_CardInfo(void)
{
  // 先查找空余位
  byte i;
  for(i = 0; i < CARD_NUM; i++){
    if(Find_Empty_Pos(i)){
      break;
    }
  }
  // 判断是否有空位
  if(i < CARD_NUM){
    // 有空位，直接录入
    Fill_New_CardInfo(&ChargeCard[i]);
  }else{
    // 无空位，删除第一组，后方信息向前覆盖
    Delete_CardInfo(0);
    // 将新卡信息放在最后一个
    Fill_New_CardInfo(&ChargeCard[CARD_NUM-1]);
  }
}
/* =====================EEPROM相关函数================================================================= */
/* 上电加载所有卡信息 */
void Load_Card_Info(void)
{
  if(CARD_NUM == EEPROM.read(0)){
    // 存在有效信息，才加载信息
    for(byte i = 0; i < CARD_NUM; i++){
      EEPROM.get(1+5*i, ChargeCard[i]);
    }
  }
  // 打印出卡信息
  Serial.println(F("ALL Card UID:"));
  for(byte j = 0; j < CARD_NUM; j++){
    dump_byte_array((byte*)&ChargeCard[j], 5);
    Serial.println("");
  }
}
/* 存储所有卡信息 */
void Store_Card_Info(void)
{
  EEPROM.write(0, CARD_NUM);
  for(byte i = 0; i < CARD_NUM; i++){
    EEPROM.put(1+5*i, ChargeCard[i]);
  }
}

/* =====================蜂鸣器相关函数================================================================= */
/* 管理员卡刷卡蜂鸣函数   三声长响 */
void Admin_Buzzer(void)
{
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(200);
  noTone(Buzzer_PIN);
  delay(50);
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(200);
  noTone(Buzzer_PIN);
  delay(50);
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(200);
  noTone(Buzzer_PIN);
}
/* 非法刷卡蜂鸣函数   三声短响 */
void Illegal_Buzzer(void)
{
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(50);
  noTone(Buzzer_PIN);
  delay(50);
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(50);
  noTone(Buzzer_PIN);
  delay(50);
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(50);
  noTone(Buzzer_PIN);
}
/* 增加卡蜂鸣函数 两声响 */
void Add_Buzzer(void)
{
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(200);
  noTone(Buzzer_PIN);
  delay(50);
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(200);
  noTone(Buzzer_PIN);
}
/* 删除卡蜂鸣函数 两声响 */
void Del_Buzzer(void)
{
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(50);
  noTone(Buzzer_PIN);
  delay(50);
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(50);
  noTone(Buzzer_PIN);
}
/* 刷卡开始充电蜂鸣函数 一声长响*/
void Start_Buzzer(void)
{
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(500);
  noTone(Buzzer_PIN);
}
/* 刷卡停止充电蜂鸣函数 一声短响*/
void Stop_Buzzer(void)
{
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(50);
  noTone(Buzzer_PIN);
}

/* ==========================初始化函数========================================================== */
void setup()
{
  // put your setup code here, to run once:
  pinMode(Relay_out, OUTPUT);   // 继电器初始化
  pinMode(Buzzer_PIN, OUTPUT);   // 蜂鸣器初始化
  Serial.begin(9600);
  while (!Serial);
  SPI.begin();            // Init SPI bus
  mfrc522.PCD_Init();     // Init MFRC522
  // 加载卡信息
  Load_Card_Info();
}

void loop()
{
  // put your main code here, to run repeatedly:
  
  // 寻找新卡
  if ( mfrc522.PICC_IsNewCardPresent()) {
    // 选择其中一个卡
    if ( mfrc522.PICC_ReadCardSerial()) {
      // 打印输出NFC卡的UID值
      Serial.print(F("Card UID:"));
      dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
      Serial.println();
      // 打印输出NFC卡的SAK值
      Serial.print(F("Card SAK: "));
      if(mfrc522.uid.sak < 0x10)
        Serial.print(F("0"));
      Serial.println(mfrc522.uid.sak, HEX);
      
      // 判断是否进入增删卡模式
      if(Admin_status)
      {
        if(0 == Compare_NFC_Card_Info(Admin_Card)){
          // 非管理卡
          // 增删卡模式
          for(m = 0; m < CARD_NUM; m++){
            // 判断此卡是否已存在
            if(Compare_NFC_Card_Info(ChargeCard[m])){
              break;
            }
          }
          // m小于CARD_NUM则说明存在充电卡
          if(m < CARD_NUM){
            // 此卡已存在，删除此卡
            Delete_CardInfo(m);
            Serial.println("删除卡！");
            Del_Buzzer(); /* 蜂鸣器 */
          }else{
            // 此卡不存在，增加此卡
            Add_CardInfo();
            Serial.println("增加卡！");
            Add_Buzzer(); /* 蜂鸣器 */
          }
          // 卡信息变更，存储卡信息
          Store_Card_Info();
        }else{
          // 管理员卡
          Admin_Buzzer();
        }
        
        // 恢复正常刷卡模式
        Admin_status = 0;
      }
      else
      {
        // 正常刷卡模式
        // 判断是否为管理员NFC卡权限
        if(Compare_NFC_Card_Info(Admin_Card)){
          // 是管理员卡
          Serial.println("管理员卡！");
          Admin_Buzzer(); /* 蜂鸣器 */
          Admin_status = 1;/* 进入增删卡模式 */
        }else{
          // 不是管理员卡
          // 继续循环判断
          for(m = 0; m < CARD_NUM; m++){
            // 判断是否为充电卡
            if(Compare_NFC_Card_Info(ChargeCard[m])){
              // 合法卡
              break;
            }
          }
          // m不等于CARD_NUM则说明存在充电卡
          if(m < CARD_NUM){
            // 存在有效卡
            // 卡号正确
            if(0 == relay_status){
              digitalWrite(Relay_out, HIGH);
              relay_status = 1;
              Serial.println("HIGH!");
              // 开启定时
              Timer_Start(&tick_off, CHARGE_TIME);
              Start_Buzzer(); /* 蜂鸣器 */
            }else{
              digitalWrite(Relay_out, LOW);
              relay_status = 0;
              Serial.println("LOW!");
              // 关闭定时
              Timer_Stop(&tick_off);
              Stop_Buzzer(); /* 蜂鸣器 */
            }
          }else{
            // 无有效卡
            Illegal_Buzzer();
            Serial.println("非法卡！");
          }
        }
      }
      // 等待下一次发现新卡
      mfrc522.PICC_HaltA();       // Halt PICC
      //mfrc522.PCD_StopCrypto1();  // Stop encryption on PCD
      
      Serial.print(millis());
      Serial.println("ms");
    }
  }

  // 定时关闭充电
  if(0 == Timer_Check(&tick_off)){
    // 定时时间到，关闭继电器
    digitalWrite(Relay_out, LOW);
    relay_status = 0;
    // 关闭定时器
    Timer_Stop(&tick_off);
    Stop_Buzzer(); /* 蜂鸣器 */
    
    Serial.println("关闭充电!");
    Serial.print(millis());
    Serial.println("ms");
  }
}