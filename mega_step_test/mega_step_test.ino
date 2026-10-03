const int EN_PIN   = 8;
const int STEP_PIN = 9;
const int DIR_PIN  = 10;

void setup()
{
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  digitalWrite(EN_PIN, LOW);   // LOW = 有効
  digitalWrite(DIR_PIN, HIGH);
}

void loop()
{
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(1000);

  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(1000);
}
