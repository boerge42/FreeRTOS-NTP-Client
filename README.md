# FreeRTOS-NTP-Client

## Was haben wir hier?

Ich wollte schon immer mal mit [FreeRTOS](https://www.freertos.org/) (unter der Arduino-IDE ) rumspielen. Mit der Implementierung einer einfachen Uhr, welche:

* Datum/Uhrzeit zyklisch von einem NTP-Server im Netzwerk bezieht

* auf einem OLED asynchron folgende Informationen ausgibt:
  
  * aktuelle Datum/Uhrzeit
  
  * Netzwerkstatus
  
  * Zeitpunkt des letzten NTP-Sync
  
  * Sync-Status

wurde ein halbwegs sinnvolles Studienobjekt gefunden.

## Hardware

Für dieses kleine Projekt wurde folgende Hardware verwendet:

* MCU: [ESP32-P4-ETH](https://docs.waveshare.com/ESP32-P4-ETH) 
  Das dieses Ding eine Ethernet-Schnittstelle hat, ist einem anderen Projektvorhaben geschuldet. Wenn ein anderes ESP32-Board verwendet werden soll, muss u.U. im Quelltext das "ETH-Zeugs" durch entsprechendes "WLAN-Gedöns" ersetzt werden.

* Display: [SSD1306-OLED (128x64)]([ESP32 OLED Display with Arduino IDE | Random Nerd Tutorials](https://randomnerdtutorials.com/esp32-ssd1306-oled-display-arduino-ide/))
  Lag in einer meiner Bastelkisten ganz oben. Es sollte kein Problem sein, ein anderes Display zu verwenden und den Quelltext entsprechend anzupassen.  

![](/home/bergeruw/mnt/banane/home/bergeruw/work/esp32-p4-eth/simple_oled_clock/clock.jpg)

## 

## Firmware

Die Geschichten mit Datum/Uhrzeit, NTP-Client etc. wurden mit Hilfe der [Espressif-IDF System-API (System Time)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/system_time.html) implementiert. 

Zur Steuerung der (asynchronen) Ausgaben auf dem OLED-Displays, wurden vor allem folgende FreeRTOS-Mechanismen verwendet:

* Tasks (...logisch...)

* Mutexe

* Queues

(Im Bezug auf FreeRTOS-Tasks wurde in der hier vorliegenden Version noch keine [Optimierung des Stack-/Heap-Speicherverbrauchs](https://randomnerdtutorials.com/esp32-freertos-arduino-tasks/#memory-usage) im Quelltext vorgenommen...)

Letztendlich sehen dann ungefähr so die Ausgaben auf dem OLED aus:

<img title="" src="file:///home/bergeruw/mnt/banane/home/bergeruw/work/esp32-p4-eth/simple_oled_clock/oled.jpg" alt="" width="633">

## Interessante Links

**FreeRTOS (Arduino-IDE):**

* [FreeRTOS mit ESP32 und Arduino nutzen • Wolles Elektronikkiste](https://wolles-elektronikkiste.de/freertos-mit-esp32-und-arduino-nutzen)

* [ESP32 with FreeRTOS (Arduino IDE) - Create Tasks | Random Nerd Tutorials](https://randomnerdtutorials.com/esp32-freertos-arduino-tasks/)

* [ESP32 FreeRTOS Queues: Inter-Task Communication (Arduino) | Random Nerd Tutorials](https://randomnerdtutorials.com/esp32-freertos-queues-inter-task-arduino/)

* [ESP32 with FreeRTOS: Getting Started Semaphores (Arduino) | Random Nerd Tutorials](https://randomnerdtutorials.com/esp32-freertos-semaphores-arduino/)

* [ESP32 FreeRTOS: Software Timers/Timer Interrupts (Arduino) | Random Nerd Tutorials](https://randomnerdtutorials.com/esp32-freertos-software-timers-interrupts/)

**Date/Time, NTP, etc.:**

* https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/

* [Wie man die ESP32-Uhr mit einem SNTP-Server synchronisiert](https://www.makerguides.com/de/how-to-synchronize-esp32-clock-with-sntp-server-de/)

-----

Uwe Berger; 2026
