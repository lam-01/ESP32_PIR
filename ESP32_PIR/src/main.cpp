#include <WiFi.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <ESP_Mail_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Khai báo nguyên mẫu hàm
void connectToWiFi();
void sendNotifications();
bool sendTelegramMessage(String message);
bool sendEmail();
void smtpCallback(SMTP_Status status);
void handleNewMessages(int numNewMessages);

// Thông tin Wi-Fi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Thông tin Telegram
#define BOT_TOKEN "7984313333:AAEfM0Yox23-5fuM31lZmYBHe7NY2HsjXxA"
#define CHAT_ID "-4798680956"

// Thông tin email
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "lamho.010103@gmail.com"
#define AUTHOR_PASSWORD "rfao vshb twbg qlkh" 
#define RECIPIENT_EMAIL "lamho.010103@gmail.com"

// Cấu hình GPIO
const int PIR_PIN = 15;
const int LED_PIN = 13;
const int BUZZER_PIN = 12;

// Biến trạng thái
int pirState = LOW;
int lastPirState = LOW;
unsigned long lastTelegramSent = 0;
unsigned long lastEmailSent = 0;
const unsigned long telegramInterval = 5000; // 5 giây
const unsigned long emailInterval = 10000;   // 10 giây
bool systemEnabled = true; // Biến để bật/tắt hệ thống
unsigned long lastTimeBotRan; // Thời gian cuối cùng kiểm tra tin nhắn bot

// Đối tượng kết nối
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
SMTPSession smtp;

// Đối tượng NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7*3600); // GMT+7

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Hệ thống khởi động...");

  // Cấu hình chân
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Kết nối WiFi
  connectToWiFi();

  // Cấu hình client Telegram
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  // Khởi động và cập nhật thời gian NTP
  timeClient.begin();
  timeClient.update();
  Serial.print("Thời gian hiện tại: ");
  Serial.println(timeClient.getFormattedTime());
}

void loop() {
  // Kiểm tra tin nhắn Telegram mới
  if (millis() - lastTimeBotRan > 1000) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (numNewMessages) {
      handleNewMessages(numNewMessages);
    }
    lastTimeBotRan = millis();
  }

  if (!systemEnabled) {
    delay(100);
    return;
  }

  // Đọc tín hiệu từ cảm biến PIR
  pirState = digitalRead(PIR_PIN);

  // Chỉ in thông điệp khi trạng thái thay đổi
  if (pirState != lastPirState) {
    if (pirState == HIGH) {
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH); // Bật còi báo động
      Serial.println("CẢNH BÁO: Phát hiện chuyển động!");

      // Gửi thông báo khi tín hiệu chuyển từ LOW sang HIGH
      sendNotifications();
    } else {
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW); // Tắt còi báo động
      Serial.println("Không có chuyển động");
    }
  }

  // Cập nhật trạng thái trước đó
  lastPirState = pirState;

  delay(100);
}

void connectToWiFi() {
  WiFi.begin(ssid, password);

  Serial.print("Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nKết nối WiFi thành công!");
  Serial.print("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());
}

bool sendTelegramMessage(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Lỗi: Không có kết nối WiFi để gửi Telegram");
    return false;
  }

  bool result = bot.sendMessage(CHAT_ID, message, "");
  if (result) {
    Serial.println("Đã gửi tin nhắn Telegram thành công");
  } else {
    Serial.println("Lỗi: Không thể gửi tin nhắn Telegram");
  }
  return result;
}

bool sendEmail() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Lỗi: Không có kết nối WiFi để gửi Email");
    return false;
  }

  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = F("");

  SMTP_Message message;
  message.sender.name = F("ESP32 Security");
  message.sender.email = AUTHOR_EMAIL;
  message.subject = F("Cảnh Báo An Ninh");
  message.addRecipient(F("Người Nhận"), RECIPIENT_EMAIL);

  String textMsg = "Cảnh báo: Phát hiện chuyển động tại khu vực giám sát!";
  message.text.content = textMsg.c_str();

  smtp.callback(smtpCallback);

  if (!smtp.connect(&session)) {
    Serial.println("Lỗi: Không thể kết nối đến máy chủ SMTP");
    return false;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Lỗi: Không thể gửi email");
    Serial.println(smtp.errorReason());
    return false;
  }

  Serial.println("Đã gửi email thành công");
  return true;
}

void sendNotifications() {
  unsigned long currentTime = millis();

  // Gửi Telegram (mỗi 5 giây)
  if (currentTime - lastTelegramSent >= telegramInterval) {
    if (sendTelegramMessage("CẢNH BÁO: Phát hiện chuyển động!")) {
      lastTelegramSent = currentTime;
    }
  }

  // Gửi Email (mỗi 10 giây)
  if (currentTime - lastEmailSent >= emailInterval) {
    if (sendEmail()) {
      lastEmailSent = currentTime;
    }
  }
}

void smtpCallback(SMTP_Status status) {
  // In thông tin trạng thái email
  Serial.println("Email Status: " + String(status.info()));
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    if (text == "/status") {
      String status = "Trạng thái hệ thống:\n";
      status += systemEnabled ? "- Hệ thống đang BẬT\n" : "- Hệ thống đang TẮT\n";
      status += "- Trạng thái cảm biến: " + String(pirState == HIGH ? "Có chuyển động" : "Không có chuyển động") + "\n";
      status += "- LED: " + String(digitalRead(LED_PIN) == HIGH ? "Sáng" : "Tắt") + "\n";
      status += "- Còi báo động: " + String(digitalRead(BUZZER_PIN) == HIGH ? "Đang kêu" : "Tắt");
      bot.sendMessage(chat_id, status, "");
    }
    else if (text == "/on") {
      systemEnabled = true;
      bot.sendMessage(chat_id, "Đã BẬT hệ thống cảnh báo", "");
      Serial.println("Hệ thống đã được BẬT qua Telegram");
    }
    else if (text == "/off") {
      systemEnabled = false;
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      bot.sendMessage(chat_id, "Đã TẮT hệ thống cảnh báo", "");
      Serial.println("Hệ thống đã được TẮT qua Telegram");
    }
    else if (text == "/help") {
      String welcome = "Các lệnh có sẵn:\n";
      welcome += "/status - Kiểm tra trạng thái hệ thống\n";
      welcome += "/on - Bật hệ thống\n";
      welcome += "/off - Tắt hệ thống\n";
      welcome += "/help - Hiển thị trợ giúp";
      bot.sendMessage(chat_id, welcome, "");
    }
  }
}