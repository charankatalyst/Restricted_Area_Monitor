# Restricted Area Monitoring System Using ESP32

## Project Overview

The **Restricted Area Monitoring System** is a hardware-based security system designed to protect restricted areas such as laboratories, server rooms, equipment stores, and other controlled spaces.

The system combines **motion detection** with **RFID-based authentication**. When movement is detected, the system immediately requires an authorized RFID card to be presented within a strict **10-second authentication window**. This prevents a person from simply entering or remaining near the monitored area without being challenged.

## Objectives

- Prevent unauthorized entry into restricted areas.
- Require RFID authentication immediately after motion is detected.
- Enforce a strict 10-second authentication window.
- Trigger an alarm for invalid authentication or timeout.
- Provide clear visual and audio feedback for system status.

## Hardware and Technologies

- ESP32 / microcontroller-based control system
- PIR Motion Sensor
- MFRC522 RFID Reader
- RFID access card/keycard
- SSD1306 OLED Display
- Buzzer
- Jumper wires and breadboard
- Arduino-compatible firmware

## Methodology

1. The system remains in monitoring mode and waits for movement.
2. The PIR sensor detects physical movement in the restricted area.
3. Detection starts a 10-second RFID authentication window.
4. The RFID reader scans for a card during this period.
5. If the card is valid, access is granted and a welcome message is displayed with a short confirmation beep.
6. If an invalid card is presented, or no valid card is detected before the timer expires, the system enters breach mode.
7. Breach mode activates a distinct **6-pulse alarm** to indicate unauthorized access.

## Key Features

### Motion-Triggered Authentication

Unlike a passive RFID reader that only reacts when someone voluntarily presents a card, this system first detects physical movement and then demands authentication.

### 10-Second Authentication Window

The authentication period is intentionally limited to 10 seconds, reducing the opportunity for unauthorized users to delay or bypass verification.

### Non-Blocking Timing

The system uses non-blocking timing logic so that the countdown, RFID scanning, display updates, and security logic can operate without freezing the system.

### State-Based Logic

The control flow switches between monitoring, authentication, and security-breach states based on sensor input, RFID validation, and timer status.

### Real-Time Feedback

The OLED display provides instructions/status information, while the buzzer provides different audio indications for successful access and security breaches.

## Testing and Results

Testing covered the main operating scenarios:

- **Valid RFID card:** Access granted immediately, with a welcome message and short beep.
- **Invalid RFID card:** Security alarm triggered.
- **No card within 10 seconds:** Security alarm triggered after timeout.
- **Concurrent operation:** The system continues monitoring and processing RFID input without noticeable lag because of the non-blocking timing approach.

According to the project poster, the system achieved a **100% success rate** across the tested authorization and breach scenarios.

## Demo Video

[Watch the Prototype / Design Simulation Demo](https://drive.google.com/file/d/1-PjBMq172qBOaEeAs68yqM1teROfdV-A/view?usp=sharing)

## Future Scope

A Wi-Fi-enabled version can be developed to send real-time mobile alerts and record breach timestamps in a cloud database. The project poster identifies this as a future upgrade to extend the system beyond a localized alarm.
