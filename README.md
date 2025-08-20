
# ESP32 Automatic Bell Scheduler with RTC, SPIFFS, and Web Dashboard

##  Overview
This project is an **automatic bell system** built using **ESP32**, **DS3231 RTC module**, and a **relay** to control a bell.  
It features a **web dashboard** (HTML, CSS, JavaScript) for setting schedules for all days of the week, stored persistently in **SPIFFS** (JSON format).  

Even after power loss, the ESP32 automatically reloads schedules from memory, and the RTC keeps accurate time using its onboard battery.

---

##  Features
- **Weekly schedule**: Set up to 16 ring times for each day of the week.
- **Persistent storage**: Schedules are saved in SPIFFS as a JSON file.
- **Accurate timekeeping**: DS3231 RTC keeps the correct date and time.
- **NTP sync**: Sync RTC with internet time when needed.
- **Manual trigger**: Button on the dashboard to ring the bell instantly.
- **Automatic operation**: Bell rings for 20 seconds at scheduled times, then turns off.
- **Web interface**: Modern UI with HTML, CSS, and JavaScript (served directly from ESP32).
- **Serial Monitor logs**: Every action is logged for debugging.

---

##  Hardware Requirements
- **ESP32 board**
- **DS3231 RTC module**
- **Relay module** (to control the bell)
- **Electric bell** (or buzzer for testing)
- **Jumper wires**
- **RTC backup battery** (CR2032)

---

##  Wiring Diagram
| ESP32 Pin | Component      | Pin on Component |
|-----------|---------------|------------------|
| 3V3       | DS3231 VCC    | VCC              |
| GND       | DS3231 GND    | GND              |
| GPIO 21   | DS3231 SDA    | SDA              |
| GPIO 22   | DS3231 SCL    | SCL              |
| GPIO 4    | Relay Signal  | IN               |
| 3V3       | Relay VCC     | VCC              |
| GND       | Relay GND     | GND              |

---

##  Software Requirements
- **Arduino IDE** (or PlatformIO)
- ESP32 board support installed in Arduino IDE
- Libraries:
  - `WiFi.h`
  - `WebServer.h`
  - `Wire.h`
  - `RTClib.h`
  - `FS.h`
  - `SPIFFS.h`
  - `time.h`

---

##  How It Works
1. On boot:
   - ESP32 connects to Wi-Fi.
   - Loads schedule from SPIFFS JSON file.
   - Reads current time from RTC (or syncs from NTP if needed).
2. Every second:
   - Compares current time with the schedule for the day.
   - If a match is found:
     - Relay is activated for 20 seconds to ring the bell.
     - Relay turns off automatically.
3. Web dashboard:
   - Displays current time/date.
   - Lets you edit schedules for all days.
   - Sync RTC with NTP.
   - Trigger bell manually.
4. Any changes to the schedule are saved immediately to SPIFFS.

---

##  Web Dashboard
The dashboard is a **single-page application** served from ESP32, with:
- **Time display** (real-time updates)
- **Day-by-day schedule editing**
- **Buttons for NTP sync and manual bell trigger**
- **Responsive design** for phones and computers

---

##  Setup & Installation
1. Clone or download this project.
2. Open in Arduino IDE.
3. Update these lines with your Wi-Fi credentials:
   ```cpp
   const char* ssid = "Your_SSID";
   const char* password = "Your_PASSWORD";
   ```
4. Upload the sketch to your ESP32.
5. Open **Serial Monitor** at `115200` baud to see logs and the ESP32 IP address.
6. Open a browser and visit `http://<ESP32-IP>` to access the dashboard.

---

##  Testing
- **Manual Trigger** → click “Manual Bell Trigger” in the dashboard.
- **NTP Sync** → click “Sync NTP Time” to adjust RTC.
- **Schedule Test**:
  1. Set a time a few minutes ahead in today’s schedule.
  2. Wait until the scheduled time.
  3. Bell rings for 20 seconds.
  4. Serial Monitor logs:
     ```
     Bell ON (Day X, Time HH:MM)
     Bell OFF
     Schedule saved to SPIFFS
     ```

---

## SPIFFS JSON File
The schedule is saved in `/schedule.json` like this:
```json
{
  "0": ["08:00", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""],
  "1": ["09:00", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""],
  "2": ["", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""],
  "...": []
}
```
- Keys `0` to `6` → Sunday to Saturday.
- Each array has **16 slots** (maximum bells per day).
- Empty strings mean no bell at that slot.

---

## License
This project is open-source and free to use for educational or commercial purposes.
