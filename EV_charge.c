
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>

#define Relay_out     8     //�̵������ƽ�
#define CHARGE_TIME (8 * 3600000) //���8Сʱ��ر�
//#define CHARGE_TIME   6000  //����6s

#define Buzzer_PIN    6     // ��������
#define Buzzer_Feq    2100  // Ƶ��

#define CARD_NUM      5
/* MFRC522 �ܽų�ʼ������*/
#define RST_PIN         9           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.


/* NFC����Ϣ�ṹ�� */
struct Card_Info{
  byte _info[5];
};
/* ��չ��ʱ���ṹ�� */
struct TickTimer{
  unsigned long start;
  unsigned long interval;
  char is_on;
};

/* ============================================================================================ */
/* ========================== ȫ�ֱ��� ========================================================== */
// ����Ķ�ʱ�������ڶ�ʱ
TickTimer tick_off;

// ����ԱCard UID
const Card_Info Admin_Card = {0xF0,0x54,0x37,0x8E, 0x20};/*��ס֤*/
//const Card_Info Admin_Card = {0x64,0x6D,0xA8,0x64, 0x08};/*�հ׿�*/
//const Card_Info Admin_Card = {0x03,0xC4,0xF4,0xB9, 0x08};/*Կ�׿�*/
const Card_Info Empty_Card = {0,0,0,0,0};
// ��翨��Ϣ
Card_Info ChargeCard[CARD_NUM] = {
  0
//  {0x64,0x6D,0xA8,0x64,0x08},
};
// ������ʹ�ñ���
byte Admin_status = 0;    /* 0Ϊ����ˢ����磻1Ϊ��ɾ��ģʽ */
byte m = 0;
byte relay_status = 0;
/* =======================��ʱ����غ���=============================================================== */
/* ��չ��ʱ������ ����0�����ɹ���1�Ѵ���*/
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
/* ��չ��ʱ����飬����0��ʱʱ�䵽 */
unsigned long Timer_Check(TickTimer* _timer)
{
  unsigned long t_status = 0xFFFFFFFF;
  if(_timer->is_on){
    t_status = millis();
    // ����ʱ����
    if(t_status < _timer->start){
      t_status += 0xFFFFFFFF - _timer->start;
    }else{
      t_status -= _timer->start;
    }
    if(t_status >= _timer->interval){
      // ��ʱʱ�䵽
      t_status = 0;
      _timer->start += _timer->interval;
    }
  }
  return t_status;
}
/* ��չ��ʱ��ֹͣ */
void Timer_Stop(TickTimer* _timer)
{
  _timer->is_on = 0;
}

/* =====================MFRC522��غ���================================================================= */
/* 16���ƴ�ӡ�ֽ���ֵ */
void dump_byte_array(byte *buffer, byte bufferSize)
{
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}
/*
 * �Ƚ�NFC����Ϣ
 * ����ֵΪ1����Ϣ��ͬ
 * ����ֵΪ0����Ϣ��ͬ
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
 * Ѱ�Ҵ洢����λ��
 * ����ֵΪ1����λ��Ϊ��
 * ����ֵΪ0����λ������Ϣ
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
/* �������Ϣ */
void Clear_CardInfo(Card_Info* card)
{
  for(byte i = 0; i < 5; i++){
    card->_info[i] = 0;
  }
}
/* ���»�ȡ�Ŀ���Ϣ�������� */
void Fill_New_CardInfo(Card_Info* card)
{
  for(byte i = 0; i < 4; i++){
    card->_info[i] = mfrc522.uid.uidByte[i];
  }
  card->_info[4] = mfrc522.uid.sak;
}

/* ɾ������Ϣ
 * ɾ����Ӧλ�õĿ���Ϣ
 */
void Delete_CardInfo(byte pos)
{
  byte i = pos;
  // ����Ϣ��ǰ����
  for(; i < (CARD_NUM-1); i++){
    ChargeCard[i] = ChargeCard[i+1];
  }
  // ���һ����Ϣ���
  Clear_CardInfo(&ChargeCard[CARD_NUM-1]);
}
/* ���ӿ���Ϣ */
void Add_CardInfo(void)
{
  // �Ȳ��ҿ���λ
  byte i;
  for(i = 0; i < CARD_NUM; i++){
    if(Find_Empty_Pos(i)){
      break;
    }
  }
  // �ж��Ƿ��п�λ
  if(i < CARD_NUM){
    // �п�λ��ֱ��¼��
    Fill_New_CardInfo(&ChargeCard[i]);
  }else{
    // �޿�λ��ɾ����һ�飬����Ϣ��ǰ����
    Delete_CardInfo(0);
    // ���¿���Ϣ�������һ��
    Fill_New_CardInfo(&ChargeCard[CARD_NUM-1]);
  }
}
/* =====================EEPROM��غ���================================================================= */
/* �ϵ�������п���Ϣ */
void Load_Card_Info(void)
{
  if(CARD_NUM == EEPROM.read(0)){
    // ������Ч��Ϣ���ż�����Ϣ
    for(byte i = 0; i < CARD_NUM; i++){
      EEPROM.get(1+5*i, ChargeCard[i]);
    }
  }
  // ��ӡ������Ϣ
  Serial.println(F("ALL Card UID:"));
  for(byte j = 0; j < CARD_NUM; j++){
    dump_byte_array((byte*)&ChargeCard[j], 5);
    Serial.println("");
  }
}
/* �洢���п���Ϣ */
void Store_Card_Info(void)
{
  EEPROM.write(0, CARD_NUM);
  for(byte i = 0; i < CARD_NUM; i++){
    EEPROM.put(1+5*i, ChargeCard[i]);
  }
}

/* =====================��������غ���================================================================= */
/* ����Ա��ˢ����������   �������� */
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
/* �Ƿ�ˢ����������   �������� */
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
/* ���ӿ��������� ������ */
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
/* ɾ������������ ������ */
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
/* ˢ����ʼ���������� һ������*/
void Start_Buzzer(void)
{
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(500);
  noTone(Buzzer_PIN);
}
/* ˢ��ֹͣ���������� һ������*/
void Stop_Buzzer(void)
{
  tone(Buzzer_PIN,Buzzer_Feq);
  delay(50);
  noTone(Buzzer_PIN);
}

/* ==========================��ʼ������========================================================== */
void setup()
{
  // put your setup code here, to run once:
  pinMode(Relay_out, OUTPUT);   // �̵�����ʼ��
  pinMode(Buzzer_PIN, OUTPUT);   // ��������ʼ��
  Serial.begin(9600);
  while (!Serial);
  SPI.begin();            // Init SPI bus
  mfrc522.PCD_Init();     // Init MFRC522
  // ���ؿ���Ϣ
  Load_Card_Info();
}

void loop()
{
  // put your main code here, to run repeatedly:
  
  // Ѱ���¿�
  if ( mfrc522.PICC_IsNewCardPresent()) {
    // ѡ������һ����
    if ( mfrc522.PICC_ReadCardSerial()) {
      // ��ӡ���NFC����UIDֵ
      Serial.print(F("Card UID:"));
      dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
      Serial.println();
      // ��ӡ���NFC����SAKֵ
      Serial.print(F("Card SAK: "));
      if(mfrc522.uid.sak < 0x10)
        Serial.print(F("0"));
      Serial.println(mfrc522.uid.sak, HEX);
      
      // �ж��Ƿ������ɾ��ģʽ
      if(Admin_status)
      {
        if(0 == Compare_NFC_Card_Info(Admin_Card)){
          // �ǹ���
          // ��ɾ��ģʽ
          for(m = 0; m < CARD_NUM; m++){
            // �жϴ˿��Ƿ��Ѵ���
            if(Compare_NFC_Card_Info(ChargeCard[m])){
              break;
            }
          }
          // mС��CARD_NUM��˵�����ڳ�翨
          if(m < CARD_NUM){
            // �˿��Ѵ��ڣ�ɾ���˿�
            Delete_CardInfo(m);
            Serial.println("ɾ������");
            Del_Buzzer(); /* ������ */
          }else{
            // �˿������ڣ����Ӵ˿�
            Add_CardInfo();
            Serial.println("���ӿ���");
            Add_Buzzer(); /* ������ */
          }
          // ����Ϣ������洢����Ϣ
          Store_Card_Info();
        }else{
          // ����Ա��
          Admin_Buzzer();
        }
        
        // �ָ�����ˢ��ģʽ
        Admin_status = 0;
      }
      else
      {
        // ����ˢ��ģʽ
        // �ж��Ƿ�Ϊ����ԱNFC��Ȩ��
        if(Compare_NFC_Card_Info(Admin_Card)){
          // �ǹ���Ա��
          Serial.println("����Ա����");
          Admin_Buzzer(); /* ������ */
          Admin_status = 1;/* ������ɾ��ģʽ */
        }else{
          // ���ǹ���Ա��
          // ����ѭ���ж�
          for(m = 0; m < CARD_NUM; m++){
            // �ж��Ƿ�Ϊ��翨
            if(Compare_NFC_Card_Info(ChargeCard[m])){
              // �Ϸ���
              break;
            }
          }
          // m������CARD_NUM��˵�����ڳ�翨
          if(m < CARD_NUM){
            // ������Ч��
            // ������ȷ
            if(0 == relay_status){
              digitalWrite(Relay_out, HIGH);
              relay_status = 1;
              Serial.println("HIGH!");
              // ������ʱ
              Timer_Start(&tick_off, CHARGE_TIME);
              Start_Buzzer(); /* ������ */
            }else{
              digitalWrite(Relay_out, LOW);
              relay_status = 0;
              Serial.println("LOW!");
              // �رն�ʱ
              Timer_Stop(&tick_off);
              Stop_Buzzer(); /* ������ */
            }
          }else{
            // ����Ч��
            Illegal_Buzzer();
            Serial.println("�Ƿ�����");
          }
        }
      }
      // �ȴ���һ�η����¿�
      mfrc522.PICC_HaltA();       // Halt PICC
      //mfrc522.PCD_StopCrypto1();  // Stop encryption on PCD
      
      Serial.print(millis());
      Serial.println("ms");
    }
  }

  // ��ʱ�رճ��
  if(0 == Timer_Check(&tick_off)){
    // ��ʱʱ�䵽���رռ̵���
    digitalWrite(Relay_out, LOW);
    relay_status = 0;
    // �رն�ʱ��
    Timer_Stop(&tick_off);
    Stop_Buzzer(); /* ������ */
    
    Serial.println("�رճ��!");
    Serial.print(millis());
    Serial.println("ms");
  }
}