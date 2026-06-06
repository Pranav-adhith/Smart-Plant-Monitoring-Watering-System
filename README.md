# 🌱 Smart Plant Monitoring & Watering System

## Overview

The Smart Plant Monitoring & Watering System is an ESP32-based IoT solution designed to automate plant care by continuously monitoring environmental conditions and soil moisture levels. The system automatically activates a water pump when the soil becomes dry while ensuring sufficient water is available in the reservoir.

A custom PCB and enclosure were designed to create a compact and deployable embedded system.

---

## Features

* Automatic irrigation control
* Soil moisture monitoring
* Temperature and humidity measurement
* Water tank level monitoring
* OLED display interface
* Touch-based interaction
* IR sensor-based interaction
* Low-water alerts using buzzer
* Blynk cloud dashboard integration
* Custom PCB design
* Custom enclosure design

---

## Hardware Components

| Component            | Purpose                |
| -------------------- | ---------------------- |
| ESP32 DevKit V1      | Main Controller        |
| Soil Moisture Sensor | Soil Monitoring        |
| DHT11                | Temperature & Humidity |
| Ultrasonic Sensor    | Water Level Detection  |
| Relay Module         | Pump Control           |
| Water Pump           | Irrigation             |
| OLED Display         | User Interface         |
| Touch Sensor         | User Interaction       |
| IR Sensor            | Presence Detection     |
| Buzzer               | Alerts                 |

---

## System Architecture

Sensors → ESP32 → Decision Logic → Relay → Water Pump

Additional Interfaces:

* OLED Display
* Touch Sensor
* IR Sensor
* Blynk Dashboard

---

## PCB Design

Custom PCB designed using EasyEDA for compact integration of all sensors and actuators.

---

## Enclosure Design

Custom enclosure designed in CAD to house the electronics, water reservoir, and plant container in a single compact unit.

---

## Working Principle

1. Soil moisture is continuously monitored.
2. Temperature and humidity are measured.
3. Water reservoir level is monitored using an ultrasonic sensor.
4. If soil moisture falls below the threshold:

   * Pump is activated.
5. Pump stops once adequate moisture is detected.
6. Low-water conditions trigger an alert.
7. System status is displayed on the OLED screen and Blynk dashboard.

---

## Future Improvements

* Mobile application
* Cloud data logging
* Predictive irrigation using AI
* Solar-powered operation
* Multi-plant monitoring

---

## Author

R. Pranav Adhith

Electronics and Communication Engineering
