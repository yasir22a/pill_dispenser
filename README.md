💊 Smart Pill Dispenser (IoT Based)

An IoT-enabled Smart Pill Dispenser designed to automate medication dispensing, ensure timely intake, and reduce human error. The system uses an ESP32 microcontroller, motorized dispensing, real-time scheduling, voice alerts, and GSM notifications to improve medication adherence, especially for elderly and chronic patients.

📌 Problem Statement

Many patients forget to take medicines on time or consume incorrect doses, leading to health risks. Manual medication management is unreliable for elderly and chronically ill individuals.

✅ Solution

The Smart Pill Dispenser automatically dispenses pills at scheduled times, provides audio alerts, and sends notifications to caregivers. It ensures correct dosage, accurate timing, and improved patient safety.


///////////////////////////////////////////////

✨ Features

Automatic pill dispensing based on schedule

Web-based medicine scheduling

Voice alerts using speaker

Buzzer and LED notifications

SMS alerts via GSM module

Real-Time Clock (RTC) for accurate timing

Pill drop detection using sensor

Low-cost and user-friendly design

///////////////////////////////////

🛠 Hardware Used

ESP32 microcontroller

DC / Servo motors

Motor driver (DRV8833 / L298N)

RTC module (DS1307 / DS3231)

MAX98357A + speaker

SIM800L GSM module

Buck converter (LM2596)


////////////////////////////////////////////


🔌 Pin Mapping (ESP32 – Key Pins)

I2S Audio: GPIO 25, 26, 27

GSM UART: GPIO 16, 17

RTC I2C: GPIO 21 (SDA), GPIO 22 (SCL)

Motors: GPIO in1 18, in3 19


//////////////////////////////////////////

🎥 Demo

🔗 Demo video link


🧩 Software Used

Arduino IDE

ESP32 Arduino Core

HTML / CSS / JavaScript (Web Interface)

KiCad (Schematic & PCB)

///////////////////////////////////////////

🚀 How It Works

User sets medicine schedule via web interface

ESP32 checks time using RTC

At scheduled time:

Motor dispenses pill

Voice alert is played

Buzzer & LED notify user

Sensor confirms pill drop

GSM sends SMS notification

//////////////////////////////////////////

⚠️ Important Notes

SIM800L must be powered at 3.8–4.2V only

All grounds must be common

Do not power GSM from ESP32 3.3V

Capacitors near GSM are mandatory

1M ohm resistor connected parallel to piezo disc sensor for proper feedback connection as per circuit diagram

///////////////////////////////////////////

🔮 Future Improvements

Mobile app integration

Cloud database (Firebase)

Refill alert system

Camera-based pill verification

////////////////////////////////////////////////

👨‍💻 Author

Mohd Yasir
Mechatronics / Embedded Systems Enthusiast

