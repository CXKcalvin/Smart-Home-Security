#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <MFRC522.h>
#include <PubSubClient.h>

// =====================
// FUNCTION DECLARATION
// =====================
void connectWiFi();
void showLCD(String line1, String line2);
void beep(int duration);
void openDoor(bool accessGranted);
void closeDoor();
void forceLockPosition();     // <-- BARU: paksa servo ke 0 derajat
bool checkRFID();
void checkRFIDCard();
void callback(char *topic, byte *payload, unsigned int length);
void reconnect();
void publishDoorStatus();

// =====================
// WIFI
// =====================
const char *ssid = "ANITA";
const char *password = "mogucocoboymeong";

// =====================
// MQTT
// =====================
const char *mqtt_server = "broker.hivemq.com";

const char *topicControl =
    "cxkcalvin/smarthome/door/control";

const char *topicStatus =
    "cxkcalvin/smarthome/door/status";

// =====================
// PIN
// =====================
#define SERVO_PIN 33
#define BUZZER_PIN 32

// =====================
// LCD
// =====================
#define LCD_SDA 13
#define LCD_SCL 25
#define LCD_ADDRESS 0x27

LiquidCrystal_I2C lcd(
    LCD_ADDRESS,
    16,
    2);

// =====================
// RFID
// =====================
#define RFID_SS_PIN 27
#define RFID_SCK_PIN 26
#define RFID_MOSI_PIN 12
#define RFID_MISO_PIN 35
#define RFID_RST_PIN 14

MFRC522 rfid(
    RFID_SS_PIN,
    RFID_RST_PIN);

// =====================
// UID TERDAFTAR
// =====================
byte allowedUID[] = {
    0x54,
    0x4E,
    0xCB,
    0x22};

// =====================
// SERVO
// =====================
Servo doorServo;

bool doorOpen = false;

// =====================
// MQTT
// =====================
WiFiClient espClient;
PubSubClient client(espClient);

// =====================
// LCD
// =====================
void showLCD(String line1, String line2)
{
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print(line1);

    lcd.setCursor(0, 1);
    lcd.print(line2);
}

// =====================
// BUZZER
// =====================
void beep(int duration)
{
    // Sengaja TIDAK pakai tone()/noTone().
    // Di ESP32, tone() memakai hardware LEDC yang juga dipakai oleh
    // ESP32Servo. Kalau LEDC servo terganggu, sinyal PWM ke servo bisa
    // hilang/rusak, dan kalau kita "perbaiki" dengan detach()+attach(),
    // servo kehilangan sinyal sesaat -> mekanik pintu bisa kebuka
    // sendiri sebelum servo narik balik ke 0. Solusinya: buzzer di-toggle
    // manual pakai digitalWrite, jadi LEDC servo tidak pernah tersentuh
    // dan servo TIDAK PERNAH perlu di-detach.
    unsigned long startTime = millis();

    while (millis() - startTime < (unsigned long)duration)
    {
        digitalWrite(BUZZER_PIN, HIGH);
        delayMicroseconds(600);

        digitalWrite(BUZZER_PIN, LOW);
        delayMicroseconds(600);
    }
}

// =====================
// STATUS PINTU
// =====================
void publishDoorStatus()
{
    if (doorOpen)
    {
        client.publish(
            topicStatus,
            "OPEN",
            true);

        Serial.println(
            "STATUS MQTT: OPEN");
    }
    else
    {
        client.publish(
            topicStatus,
            "CLOSED",
            true);

        Serial.println(
            "STATUS MQTT: CLOSED");
    }
}

// =====================
// PAKSA SERVO KE 0 DERAJAT
// Dipanggil setiap kali akses DITOLAK.
// Tidak bergantung pada flag doorOpen, jadi servo dijamin
// balik/tetap ke posisi 0 walau status software sempat tidak sinkron
// (misalnya setelah ESP32 restart saat pintu sedang terbuka).
// =====================
void forceLockPosition()
{
    Serial.println("=== FORCE LOCK: PAKSA SERVO KE 0 DERAJAT ===");

    // TIDAK detach()/attach() di sini dengan sengaja.
    // beep() sekarang tidak lagi pakai tone()/LEDC, jadi channel PWM
    // servo tidak pernah terganggu -> write(0) langsung cukup, dan
    // servo tidak pernah kehilangan sinyal (yang sebelumnya bikin
    // mekanik pintu sempat kebuka sendiri saat detach()).
    doorServo.write(0);
    delay(1000);

    doorOpen = false;

    showLCD(
        "SMART HOME",
        "PINTU TERKUNCI");

    publishDoorStatus();
}

// =====================
// OPEN DOOR
// =====================
void openDoor(bool accessGranted)
{
    if (!accessGranted)
    {
        Serial.println(
            "AKSES DITOLAK: SERVO TIDAK AKAN BERGERAK");

        forceLockPosition();
        return;
    }

    Serial.println(
        "PERINTAH: BUKA PINTU");

    // =============================
    // BUZZER AKSES BERHASIL
    // =============================
    beep(100);

    // =============================
    // BUKA SERVO
    // =============================
    doorServo.write(90);

    delay(1000);

    doorOpen = true;

    showLCD(
        "SMART HOME",
        "PINTU TERBUKA");

    publishDoorStatus();

    Serial.println(
        "PINTU TERBUKA");
}

// =====================
// CLOSE DOOR
// =====================
void closeDoor()
{
    Serial.println("=== CLOSE DOOR ===");

    Serial.println("Servo → 0 derajat");

    doorServo.write(0);

    delay(1000);

    doorOpen = false;

    showLCD(
        "SMART HOME",
        "PINTU TERKUNCI");

    publishDoorStatus();

    Serial.println(
        "PINTU TERKUNCI");
}

// =====================
// CHECK RFID UID
// =====================
bool checkRFID()
{
    // UID harus tepat 4 byte
    if (rfid.uid.size != 4)
    {
        Serial.println(
            "RFID DITOLAK: PANJANG UID SALAH");

        return false;
    }

    // Bandingkan SETIAP byte
    for (byte i = 0; i < 4; i++)
    {
        Serial.print(
            "UID[");

        Serial.print(i);

        Serial.print("] = ");

        Serial.print(
            rfid.uid.uidByte[i],
            HEX);

        Serial.print(
            " | ALLOWED = ");

        Serial.println(
            allowedUID[i],
            HEX);

        if (
            rfid.uid.uidByte[i] != allowedUID[i])
        {
            Serial.println(
                "UID TIDAK COCOK");

            return false;
        }
    }

    Serial.println(
        "SEMUA UID COCOK");

    return true;
}

// =====================
// RFID
// =====================
void checkRFIDCard()
{
    if (!rfid.PICC_IsNewCardPresent())
    {
        return;
    }

    if (!rfid.PICC_ReadCardSerial())
    {
        return;
    }

    // =====================================
    // TAMPILKAN UID
    // =====================================

    Serial.print(
        "RFID UID: ");

    for (
        byte i = 0;
        i < rfid.uid.size;
        i++)
    {
        if (
            rfid.uid.uidByte[i] < 0x10)
        {
            Serial.print("0");
        }

        Serial.print(
            rfid.uid.uidByte[i],
            HEX);

        if (
            i < rfid.uid.size - 1)
        {
            Serial.print(":");
        }
    }

    Serial.println();

    // =====================================
    // VALIDASI RFID
    // =====================================

    bool accessGranted =
        checkRFID();

    // =====================================
    // KARTU BENAR
    // =====================================

    if (accessGranted)
    {
        Serial.println(
            "RFID DITERIMA");

        showLCD(
            "RFID",
            "KARTU DITERIMA");

        client.publish(
            topicStatus,
            "RFID_ACCESS_GRANTED");

        // HANYA DI SINI OPEN DOOR
        openDoor(true);
    }

    // =====================================
    // KARTU SALAH
    // =====================================

    else
    {
        Serial.println(
            "RFID DITOLAK");

        Serial.println(
            "AKSES DITOLAK");

        showLCD(
            "SMART HOME",
            "RFID DITOLAK");

        beep(3000);

        client.publish(
            topicStatus,
            "RFID_ACCESS_DENIED");

        // =================================
        // FAIL-SAFE: SELALU PAKSA SERVO KE 0
        // Tidak lagi bergantung pada flag doorOpen,
        // jadi servo dijamin tetap/kembali ke 0 derajat.
        // =================================
        forceLockPosition();
    }

    // =====================================
    // SELESAI
    // =====================================

    rfid.PICC_HaltA();

    rfid.PCD_StopCrypto1();

    delay(1000);
}

// =====================
// MQTT CALLBACK
// =====================
void callback(
    char *topic,
    byte *payload,
    unsigned int length)
{
    if (strcmp(topic, topicControl) != 0)
    {
        return;
    }

    String message = "";

    for (
        unsigned int i = 0;
        i < length;
        i++)
    {
        message += (char)payload[i];
    }

    message.trim();

    Serial.println();
    Serial.println("==============================");
    Serial.println("MQTT MESSAGE");

    Serial.print("Topic: ");
    Serial.println(topic);

    Serial.print("Pesan: [");
    Serial.print(message);
    Serial.println("]");

    // =====================================
    // TUTUP PINTU
    // =====================================

    if (message == "CLOSE")
    {
        Serial.println(
            "AKSI: TUTUP PINTU");

        closeDoor();

        Serial.println(
            "SELESAI CLOSE");

        Serial.println(
            "==============================");

        return;
    }

    // =====================================
    // BUKA + PASSWORD
    // =====================================

    if (
        message.startsWith("OPEN:"))
    {
        String inputPassword =
            message.substring(5);

        inputPassword.trim();

        Serial.print(
            "Password yang diterima: [");

        Serial.print(
            inputPassword);

        Serial.println("]");

        // =================================
        // PASSWORD BENAR
        // =================================

        if (
            inputPassword == "1234")
        {
            Serial.println(
                "PASSWORD BENAR");

            Serial.println(
                "AKSES DITERIMA");

            openDoor(true);

            Serial.println(
                "OPEN DOOR DIPANGGIL");
        }

        // =================================
        // PASSWORD SALAH
        // =================================

        else
        {
            Serial.println(
                "PASSWORD SALAH");

            Serial.println(
                "AKSES DITOLAK");

            beep(3000);

            showLCD(
                "SMART HOME",
                "PIN SALAH");

            client.publish(
                topicStatus,
                "PASSWORD_SALAH");

            // =================================
            // FAIL-SAFE: SELALU PAKSA SERVO KE 0
            // Tidak lagi bergantung pada flag doorOpen,
            // jadi servo dijamin tetap/kembali ke 0 derajat.
            // =================================
            forceLockPosition();
        }

        Serial.println(
            "==============================");

        return;
    }

    // =====================================
    // SEMUA PESAN LAIN DITOLAK
    // =====================================

    Serial.println(
        "COMMAND TIDAK DIKENAL");

    Serial.println(
        "AKSES DITOLAK");

    Serial.println(
        "SERVO TIDAK AKAN BERGERAK");

    Serial.println(
        "==============================");
}

// =====================
// MQTT RECONNECT
// =====================
void reconnect()
{
    while (!client.connected())
    {
        Serial.println(
            "Menghubungkan MQTT...");

        String clientID =
            "ESP32SmartHome-" +
            String(
                (uint32_t)ESP.getEfuseMac(),
                HEX);

        if (
            client.connect(
                clientID.c_str()))
        {
            Serial.println(
                "MQTT CONNECTED");

            client.subscribe(
                topicControl);

            client.publish(
                topicStatus,
                "ONLINE",
                true);

            // Kirim kondisi pintu saat reconnect
            publishDoorStatus();
        }
        else
        {
            Serial.print(
                "MQTT gagal, state=");

            Serial.println(
                client.state());

            delay(2000);
        }
    }
}

// =====================
// SETUP
// =====================
void setup()
{
    Serial.begin(115200);

    // =====================
    // SERVO
    // =====================

    doorServo.attach(
        SERVO_PIN);

    doorServo.write(0);

    doorOpen = false;

    // =====================
    // BUZZER
    // =====================

    pinMode(
        BUZZER_PIN,
        OUTPUT);

    // =====================
    // LCD
    // =====================

    Wire.begin(
        LCD_SDA,
        LCD_SCL);

    lcd.init();
    lcd.backlight();

    showLCD(
        "SMART HOME",
        "PINTU TERKUNCI");

    // =====================
    // RFID
    // =====================

    SPI.begin(
        RFID_SCK_PIN,
        RFID_MISO_PIN,
        RFID_MOSI_PIN,
        RFID_SS_PIN);

    rfid.PCD_Init();

    Serial.println(
        "RFID SIAP");

    // =====================
    // WIFI
    // =====================

    connectWiFi();

    // =====================
    // MQTT
    // =====================

    client.setServer(
        mqtt_server,
        1883);

    client.setCallback(
        callback);
}

// =====================
// WIFI
// =====================
void connectWiFi()
{
    Serial.println(
        "Menghubungkan WiFi...");

    WiFi.begin(
        ssid,
        password);

    while (
        WiFi.status() != WL_CONNECTED)
    {
        delay(500);

        Serial.print(".");
    }

    Serial.println();

    Serial.println(
        "WiFi Connected");

    Serial.print(
        "IP ESP32: ");

    Serial.println(
        WiFi.localIP());
}

// =====================
// LOOP
// =====================
void loop()
{
    if (!client.connected())
    {
        reconnect();
    }

    client.loop();

    checkRFIDCard();

    // TIDAK ADA AUTO CLOSE
}