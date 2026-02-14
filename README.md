

# IoT Smart Ticketing 

This project is a full-stack transit solution that combines a physical **ESP32 Kiosk** with a **Web-Based Admin & Passenger Portal**. It provides an end-to-end workflow for modern transport management, from on-site ticket issuance to centralized cloud administration.

## 🌟 System Architecture

The project is divided into two main environments that stay synchronized via a Google Sheets cloud database:

1. **The Physical Kiosk (Hardware):** An ESP32-powered station for RFID scanning, PIN verification, and thermal ticket printing.
2. **The Admin & Passenger Portal (Web):** A responsive web interface for managing user accounts, configuring machine prices, and monitoring live transaction logs.

## ✨ Key Features

### 🖥️ Admin & Passenger Web Portals

The web interface provides a high-level management layer for operators and users:

* **Passenger Management:** Search, view, and update passenger records. Admins can manually adjust balances, update security PINs, and toggle account statuses between "Active" and "Blocked."
* **Machine Configuration:** A dedicated module to remotely configure kiosk settings. Admins can assign stations to specific machines and manage complex pricing tables for 1st, 2nd, and 3rd-class tickets.
* **Live Transaction Monitoring:** A searchable log that displays every ticket issued across the network, including passenger names, timestamps, RFID UIDs, and total prices.
* **Responsive Dashboard:** A clean, modern UI built with HTML5, CSS3, and JavaScript, featuring a navigation grid for easy access to different management modules.

### 🤖 The Smart Kiosk (Hardware)

* **Secure Authentication:** Multi-factor verification using RFID cards and private PIN entry.
* **Tiered Ticketing:** Support for multiple ticket categories (Adult/Child) and travel classes.
* **Thermal Issuance:** Real-time generation of physical transit receipts with custom headers and timestamps.
* **Automatic Lockout:** Hardware-level security that triggers an account block on the cloud database after multiple failed PIN attempts.

## 🛠️ Components & Technologies

### **Hardware Components:**

* **ESP32 Microcontroller:** Core processor with WiFi connectivity.
* **MFRC522 RFID Reader:** For passenger card identification.
* **Adafruit Thermal Printer:** For physical ticket printing.
* **20x4 I2C LCD:** For user-facing menus and feedback.
* **3x4 Matrix Keypad:** For PIN and quantity input.

### **Software Stack:**

* **Frontend:** HTML5, CSS3 (Flexbox/Grid), and Vanilla JavaScript.
* **Firmware:** AVR Assembly (for Door Lock module) and C++/Arduino (for ESP32 Kiosk).
* **Backend:** Google Apps Script (REST API).
* **Database:** Google Sheets (Cloud storage).

## 📋 System Workflow

1. **Registration:** Admin uses the **Web Portal** to register a new passenger RFID and set an initial balance.
2. **Configuration:** Admin sets the station pricing for "Model001" via the **Machine Config** page.
3. **Interaction:** A passenger taps their card at the **Physical Kiosk**.
4. **Verification:** The Kiosk fetches data from the cloud; the passenger enters their PIN on the keypad.
5. **Issuance:** The Kiosk deducts the balance in the cloud and prints a receipt.
6. **Audit:** The transaction immediately appears in the **Transaction History** log on the Admin Portal.

