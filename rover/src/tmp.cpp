const int US_PIN        = A0;
const int THRESHOLD_ADC = 300;   // 1500mV @ 3.3V ref: (1500/3300)*1023
const int NUM_AVERAGES  = 3;
const int DELTA         = 30;    // ADC counts needed to trigger closer/further message

int prevReading = 0;
int averagedRead() {
  long sum = 0;
  for (int i = 0; i < NUM_AVERAGES; i++) {
    sum += analogRead(US_PIN);
    delay(10);                   // 10ms gap matches RC time constant of circuit
  }
  return (int)(sum / NUM_AVERAGES);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  analogReadResolution(10);
  Serial.println("Ready");
}

void checkProximity(int current) {
  int delta = current - prevReading;

  if (delta > DELTA) {
    Serial.println(">>> GETTING CLOSER");
  } else if (delta < -DELTA) {
    Serial.println(">>> GETTING FURTHER");
  }
  // if neither, print nothing at all

  prevReading = current;
}

void loop() {
  int   raw     = averagedRead();
  float voltage = raw * (3300.0 / 1023.0);
  bool  present = raw > THRESHOLD_ADC;

  Serial.print(voltage, 1);
  Serial.print(" mV  |  ");
  Serial.println(present ? "PRESENT" : "ABSENT");

  checkProximity(raw);

  delay(200);
}

