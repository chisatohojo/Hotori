#include <Arduino.h>

uint32_t touch1_count = 0;
uint32_t touch2_count = 0;
bool touch1_flag = false;
bool touch2_flag = false;

void IRAM_ATTR gotTouch1() {
  touch1_count++;
  touch1_flag = true;
}

void IRAM_ATTR gotTouch2() {
  touch2_count++;
  touch2_flag = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  touchAttachInterrupt(14, gotTouch1, 40);
  touchAttachInterrupt(4, gotTouch2, 40);
}

void loop() {
  if (touch1_flag) {
    Serial.printf("ピン 14 (T6) がタッチされました！ 回数: %u\n", touch1_count);
    touch1_flag = false;
  }

  if (touch2_flag) {
    Serial.printf("ピン 4 (T0) がタッチされました！ 回数: %u\n", touch2_count);
    touch2_flag = false;
  }
}
