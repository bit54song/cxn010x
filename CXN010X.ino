#include <IRremote.h>
//#include <avr/sleep.h>
#include <Wire.h>
#include <time.h>
#include <EEPROM.h>
#include "cxn010x.h"
#include "HexDump.h"

/*
 * 主板: Arduino Nano
 * 引导: Atmega328P(Old bootloader)
 *  引脚定义表:
      IIC1 PA4 SDA    PA5 SCL
*/


#define LED               13  // LED 引脚 PD13
#define CMD_REQ_PIN       2  // PD2 INT0
#define VOLTAGE_PIN       14  // PA0 (电压测量引脚)
#define RECV_PIN          PD6 // 红外遥控引脚

// 注意: 光机控制引脚改用模拟引脚控制. 引脚序号 17  对应主板上的 A3 引脚

IRrecv irrecv(RECV_PIN);
decode_results results;
CXNProjector projector;

unsigned long g_ms = 0;



void setup() {

  pinMode(LED, OUTPUT);
  pinMode(CXNProjector_POWER_PIN, OUTPUT);
  pinMode(CMD_REQ_PIN, INPUT);
  pinMode(RECV_PIN, INPUT);
  irrecv.blink13(false);
  irrecv.enableIRIn();
  
  Serial.begin(115200);
  Wire.begin(); //初始化I2C
  analogWrite(CXNProjector_POWER_PIN, 0);
  delay(200);
}


// 通知引脚变化, 调用到实例中.
void OnCXNProjectorSingle(void) {
  noInterrupts(); //关闭中断源
  // 循环读取,直到CMD_REQ 电平拉低了
  do {
    projector.OnNotify();
  } while(HIGH == digitalRead(CMD_REQ_PIN));
  interrupts(); //开启中断源
}


void loop() {
  //这里暂时不使用中断控制
  if(projector.GetState() != STATE_POWER_OFF) {
    if(digitalRead(2)==HIGH) {
      delay(10);  //等待 10ms; 如果电平还是高电平,读取通知.
      // 循环读取,直到CMD_REQ 电平拉低了
      while(HIGH == digitalRead(CMD_REQ_PIN)) {
        Serial.println("CMD_REQ");
        projector.OnNotify();
      }
    }
  }

  if (irrecv.decode(&results)) {
    if (results.decode_type == NEC) {
      if(results.value != -1) {
        Serial.println(results.value,HEX);
        switch(results.value) {
          case 0xDC2300FF:
            Serial.println(projector.GetState(),HEX);
            if(STATE_POWER_OFF == projector.GetState()) {
              float v = analogRead(VOLTAGE_PIN) *  (5.0 / 1023.0);
              if(v >= 4.8){
                projector.PowerOn();
                delay(50);
              }else{
                  Serial.println("ERR: LOW POWER!!");
              }
            }else{
              projector.Shutdown(false);
              Serial.println("INF: POWER OFF!!");
            }
            break;
        }
        //关机状态 下列遥控指令不响应!
        if(STATE_POWER_OFF != projector.GetState()) {
          switch(results.value) {
            case 0xDC2348B7: // 右
              projector.SetPan(+1);
              break;
            case 0xDC2308F7: // 左
              projector.SetPan(-1);
              break;
            case 0xDC23B04F: // 上
              projector.SetTilt(1);
              break;
            case 0xDC23A857: // 下
              projector.SetTilt(-1);
              break;
            case 0xDC238877: // OK
              projector.SetFlip();
              break;
            case 0xDC2330CF: // VOL+ 亮度+
              projector.SetLight(+1);
              break;
            case 0xDC23708F: // VOL- 亮度-
              projector.SetLight(-1);
              break;
            case 0xDC236897://Mute 静音
              projector.m_Brightness = 0;
              projector.SetLight(0);
              break;
            case 0xDC238A75: // 上下文(保存)
              projector.SaveConfig();
              break;
            case 0xDC230AF5: //返回 恢复所有位置信息
              projector.m_Pan = projector.m_Tilt = projector.m_Flip = 0;
              projector.SetVideoPosition();
              break;
          }
        } // if
      }
    }
    irrecv.resume(); // Receive the next value
  }
}
