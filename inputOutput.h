// servo
#include <ESP32Servo.h>

Servo myservo;
int servoPin = 33;

bool isFeeding = false;

// indikator led
const int ledPin = 25;

// button
const int buttonPin = 34;

// temperature
#include <OneWire.h>
const int dsPin = 32;
float currentTemp = 0;

// ph sensor
const int phPin = 35;

const int numPoints = 20;
float dataPoints[numPoints];
float smaValues[numPoints];

float finalVoltage;
float phFinal;

void inputOutputSetup()
{
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT);

    for (int i = 0; i < 8; i++) {
        digitalWrite(ledPin, !digitalRead(ledPin));
        delay(100);
    }

    // enable ds
    dallas(dsPin, 1);

    // Allow allocation of all timers
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    myservo.setPeriodHertz(50); // standard 50 hz servo
    myservo.attach(servoPin, 500, 2400); // attaches the servo on pin to the servo object
    // using default min/max of 1000us and 2000us

    myservo.write(0);

    for (int i = 0; i < numPoints; i++) {
        dataPoints[i] = 0.0;
        smaValues[i] = 0.0;
    }

    Serial.println("Input Output Setup Completed");
}

// get and update data point
void updateDataPoints(float newValue)
{
    for (int i = 1; i < numPoints; i++) {
        dataPoints[i - 1] = dataPoints[i];
    }
    dataPoints[numPoints - 1] = newValue;
}

// simple moving average
void calculateSMA()
{
    float sum = 0.0;
    for (int i = 0; i < numPoints; i++) {
        sum += dataPoints[i];
        if (i >= numPoints - 1) {
            smaValues[i] = sum / numPoints;
            sum -= dataPoints[i - numPoints + 1];
        }
    }
}

// exponential moving average
float calculateEMA(float previousEMA, float alpha)
{
    // Read new data point
    float newDataPoint = dataPoints[numPoints - 1];

    // Calculate EMA based on the new data point and the previous EMA
    float ema = alpha * newDataPoint + (1 - alpha) * previousEMA;

    return ema;
}

float calculatePH(float voltage)
{
    // Calculate the slope (m) and intercept (C) based on the calibration points

    float X1 = setVph7; // Voltage at pH 6.86
    float X2 = setVph4; // Voltage at pH 4.01
    float Y1 = 7.00; // batas pH Netral
    float Y2 = 4.01; // batas pH Asam

    //  slope = pH(Asam - Netral) / (vpH Asam - vpH Netral);
    float slope = (Y2 - Y1) / (X2 - X1);
    // intercept = pH Netral - slope * vpH Netral
    float intercept = Y1 - slope * X1;

    // Convert voltage to pH using the calculated slope and intercept: pH = slope * voltage + intercept
    float pH = slope * voltage + intercept;

    pH = pH - setOffsetPH;

    if (pH > 14)
        pH = 14;
    if (pH < 0)
        pH = 0;

    return pH;
}

float readingPHsensor()
{
    // Read new data point and update dataPoints array
    float newDataPoint = analogRead(phPin) * (3.3 / 4095.0);
    updateDataPoints(newDataPoint);
    calculateSMA();
    // Choose an appropriate alpha for the EMA (e.g., 0.2)
    float alpha = 0.1;
    float emaValue = calculateEMA(smaValues[numPoints - 1], alpha);
    // Combine SMA and EMA to get a combined smoothed value
    finalVoltage = (smaValues[numPoints - 1] + emaValue) / 2.0;
    phFinal = calculatePH(finalVoltage);

    return phFinal;
}

// function to read the ds18b20
float dallas(int x, byte start)
{
    OneWire ds(x);
    byte i;
    byte data[2];
    int16_t result;
    float temperature;

    do {
        ds.reset();
        ds.write(0xCC);
        ds.write(0xBE);
        for (int i = 0; i < 2; i++)
            data[i] = ds.read();
        result = (data[1] << 8) | data[0];
        // Here you could print out the received bytes as binary, as requested in my comment:
        // Serial.println(result, BIN);
        int16_t whole_degree = (result & 0x07FF) >> 4; // cut out sign bits and shift
        temperature = whole_degree + 0.5 * ((data[0] & 0x8) >> 3) + 0.25 * ((data[0] & 0x4) >> 2)
            + 0.125 * ((data[0] & 0x2) >> 1) + 0.625 * (data[0] & 0x1);
        if (data[1] & 128)
            temperature *= -1;
        ds.reset();
        ds.write(0xCC);
        ds.write(0x44, 1);
        if (start)
            delay(1000);
    } while (start--);
    return temperature;
}

unsigned long lastButtonPressTime = 0;
bool button()
{
    unsigned long currentMillis = millis();
    if (currentMillis - lastButtonPressTime >= 200) {
        if (!digitalRead(buttonPin)) {
            lastButtonPressTime = currentMillis;
            return true;
        }
    }
    return false;
}

const unsigned long longPressDuration = 1000; // Define the long press duration in milliseconds
// Variables will change:
int longLastState = LOW; // the previous state from the input pin
int longCurrentState; // the current reading from the input pin
unsigned long pressedTime = 0;
bool isPressing = false;
bool isLongDetected = false;

bool longPress(int btnPin)
{
    isLongDetected = false;
    // read the state of the switch/button:
    longCurrentState = digitalRead(btnPin);
    if (longLastState == HIGH && longCurrentState == LOW) { // button is pressed
        pressedTime = millis();
        isPressing = true;
        isLongDetected = false;
    } else if (longLastState == LOW && longCurrentState == HIGH) { // button is released
        isPressing = false;
    }

    if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;

        if (pressDuration > longPressDuration) {
            //Serial.println("A long press is detected");
            // beeping(4);
            isLongDetected = true;
            pressedTime = millis();
        }
    }

    // save the the last state
    longLastState = longCurrentState;

    return isLongDetected;
}

int pos = 0;

void feeding()
{
    for (pos = 0; pos <= 180; pos += 1) {
        myservo.write(pos);
        delay(10);
    }
    for (pos = 180; pos >= 0; pos -= 1) {
        myservo.write(pos);
        delay(10);
    }

    pos = 0;

    isFeeding = false;
}