#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

Adafruit_8x8matrix matrix = Adafruit_8x8matrix();
RF24 radio(7, 8);  // CE, CSN pins

const byte address[][6] = { "000001", "000002" };
// boolean buttonState = 0;
// boolean buttonStateR = 0;
// boolean buttonStateT = 0;
int buttonState = LOW;
int lastButtonState = LOW;
char Data, ch;

int tonePin = 3;
int toneFreq = 1000;
int ledPin_Send = 2;
int ledPin_Receive = 5;
int buttonPin = 4;
int SendPin = 6;
int debounceDelay = 30;

int dotLength = 350;
// dotLength = basic unit of speed in milliseconds
// 240 gives 5 words per minute (WPM) speed.
// WPM = 1200/dotLength.
// For other speeds, use dotLength = 1200/(WPM)
//
// Other lengths are computed from dot length
int dotSpace = dotLength;
int dashLength = dotLength * 3;
int letterSpace = dotLength * 3;
int wordSpace = dotLength * 7;
float wpm = 1200. / dotLength;

int t1, t2, onTime, gap;
bool newLetter, newWord, letterFound, keyboardText;
int lineLength = 0;
int maxLineLength = 20;

char* letters[] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..",    // A-I
  ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.",  // J-R
  "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."          // S-Z
};

char* numbers[] = {
  "-----", ".----", "..---", "...--", "....-",  //0-4
  ".....", "-....", "--...", "---..", "----."   //5-9
};

String dashSeq = "";
char keyLetter;
int i, index;

enum State {
  TASK_A,
  TASK_B
};

State currentState = TASK_A;

void setup() {
  delay(500);
  matrix.begin(0x70);  // Address 0x70 for the 8x8 matrix
  pinMode(ledPin_Send, OUTPUT);
  pinMode(ledPin_Receive, OUTPUT);
  pinMode(tonePin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(SendPin, INPUT);
  radio.begin();
  radio.openWritingPipe(address[1]);
  radio.openReadingPipe(1, address[0]);
  radio.stopListening();
  radio.setPALevel(RF24_PA_MIN);


  // Test the LED and tone
  tone(tonePin, toneFreq);
  digitalWrite(ledPin_Send, HIGH);
  delay(2000);
  digitalWrite(ledPin_Send, LOW);
  noTone(tonePin);
  delay(600);

  newLetter = false;  //if false, do NOT check for end of letter gap
  newWord = false;    //if false, do NOT check for end of word gap
}




void loop() {

  while (digitalRead(SendPin) == HIGH) {
    digitalWrite(ledPin_Receive, HIGH);
    sendSignal();
  }

  while (digitalRead(SendPin) == LOW) {
    digitalWrite(ledPin_Receive, LOW);
    radio.startListening();
    if (radio.available()) {
      receiveSignal();
    } else {
      break;
    }
  };
}

void sendSignal() {

  if (digitalRead(buttonPin) == HIGH) {
    newLetter = true;
    t1 = millis();                    // time at button press
    digitalWrite(ledPin_Send, HIGH);  // turn on LED and tone
    tone(tonePin, toneFreq);
    delay(debounceDelay);

    while (digitalRead(buttonPin) == HIGH) {
      delay(debounceDelay);
    }
    delay(debounceDelay);

    t2 = millis();                   // time at button release
    onTime = t2 - t1;                // length of dot or dash keyed in
    digitalWrite(ledPin_Send, LOW);  // turn off LED and tone
    noTone(tonePin);

    // check if dot or dash
    if (onTime <= dotLength * 1.5)  // allow for 50% longer
    {
      dashSeq = dashSeq + ".";
    }  // build dot/dash sequence
    else {
      dashSeq = dashSeq + "-";
    }
  }

  // look for a gap >= letterSpace to signal end letter
  gap = millis() - t2;
  if (newLetter == true && gap >= letterSpace) {
    // check through letter sequences to find matching dash sequence
    letterFound = false;
    keyLetter = 63;  // char 63 is "?"
    for (i = 0; i <= 25; i++) {
      if (dashSeq == letters[i]) {
        keyLetter = i + 65;
        letterFound = true;
        break;  // don't keep checking if letter found
      }
    }
    if (!letterFound)  // now check for numbers
    {
      for (i = 0; i <= 10; i++) {
        if (dashSeq == numbers[i]) {
          keyLetter = i + 48;
          letterFound = true;
          break;  // don't keep checking if number found
        }
      }
    }
    if (!letterFound)  // buzz for an unknown key sequence
    {
      tone(tonePin, 100, 500);
    }
    newLetter = false;  // reset
    dashSeq = "";

    matrix.setTextSize(1);
    matrix.setTextColor(LED_ON);
    matrix.clear();
    matrix.setCursor(1, 0);
    matrix.print(keyLetter);
    matrix.writeDisplay();
    radio.stopListening();
    Data = keyLetter;
    radio.write(&Data, sizeof(Data));
  }
}


void receiveSignal() {
  radio.startListening();
  while (!radio.available())
    ;
  radio.read(&ch, sizeof(ch));
  if (ch >= 'a' && ch <= 'z') { ch = ch - 32; }

  if (ch >= 'A' && ch <= 'Z') {
    // Serial.print(ch);
    // Serial.print(" ");
    // Serial.println(letters[ch-'A']);
    flashSequence(letters[ch - 'A'], ch);
    delay(letterSpace);
  }
  if (ch >= '0' && ch <= '9') {
    // Serial.print(ch);
    // Serial.print(" ");
    // Serial.println(numbers[ch-'0']);
    flashSequence(numbers[ch - '0'], ch);
    delay(letterSpace);
  }
  if (ch == ' ') {
    // Serial.println("_");
    delay(wordSpace);
  }
}


void flashSequence(char* sequence, char letter) {
  int i = 0;
  while (sequence[i] == '.' || sequence[i] == '-') {
    flashDotOrDash(sequence[i]);
    i++;
  }

  matrix.setTextSize(1);
  matrix.setTextColor(LED_ON);
  matrix.clear();
  matrix.setCursor(1, 0);
  matrix.print(letter);
  matrix.writeDisplay();
}

void flashDotOrDash(char dotOrDash) {
  digitalWrite(ledPin_Receive, HIGH);
  tone(tonePin, toneFreq);
  if (dotOrDash == '.') {
    delay(dotLength);
  } else {
    delay(dashLength);
  }

  digitalWrite(ledPin_Receive, LOW);
  noTone(tonePin);
  delay(dotLength);
}
//--- end of sketch ---