#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>
#include <UniversalTelegramBot.h>
#include <Preferences.h>

Preferences pref;

Servo doorMotor;

String admin = "1355536430";
int count;
// kkjjdf jhdfj
#define WIFI_SSID "Ground_M"
#define WIFI_PASSWORD "800800000"
#define BOT_TOKEN "5305557231:AAGoACycqIlP5_P7vWps0tW9vMaCWgrvjnI"

const int securityPin = 15;
const int securityDevicePin = 16;
const int lightsPin = 17;
const int doorPin = 18;

//<defining stats for our devices in case of need to check the state from other core>
String doorState = "und";
String lightsState = "und";
String securityDevice_State = "und";
String logsState = "und";
//</defining stats for our devices in case of need to check the state from other core>

String emergency_State = "OFF";

const unsigned long BOT_MTBS = 500;
bool latch = false;

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;

//<functions to be executed on the the core 0>
void securityDevice(void *parameters)
{
  // disableCore0WDT();
  while (true)
  {
    if (digitalRead(securityDevicePin) == 1)
    {
      int state = digitalRead(securityPin);

      if (state == HIGH && emergency_State == "OFF")
      {
        latch = true;
        doorMotor.write(90);
        digitalWrite(lightsPin, 0);
      }
    }
    delay(100);
  }
}
//</functions to be executed on the the core 0>

void emergencyMode()
{
  digitalWrite(lightsPin, 0);
  digitalWrite(securityDevicePin, 0);
  latch = false;
  doorMotor.write(90);
}

void handleRequest(String name, String id, String opr)
{
  if (opr == "add")
  {
    pref.putString(name.c_str(), id.c_str());
  }

  else if (opr == "remove")
  {
    pref.remove(name.c_str());
  }
}

void informUsers(String info, int n)
{
  if (logsState == "Enabled" || n == 0)
    for (int user = 1; user <= count; user++)
    {
      if (user == n)
        continue;
      if (pref.getString(String(user).c_str(), "null").indexOf("@") != -1)
      {
        Serial.println(String(user));
        String id = pref.getString(String(user).c_str(), "null").substring(0, pref.getString(String(user).c_str(), "null").indexOf("@"));
        bot.sendMessage(id, info);
      }
    }
}

void processRequest(int numNewMessages)
{

  pref.begin("settings", false);
  for (int i = 0; i < numNewMessages; i++)
  {
    if (emergency_State != "ON" || bot.messages[i].chat_id == admin)
    {
      if (bot.messages[i].text == "/request-permission")
      {
        for (int user = 1; user <= count; user++)
          if (pref.getString(String(user).c_str(), "null").substring(0, bot.messages[i].chat_id.length()) == bot.messages[i].chat_id)
            return;

        count += 1;
        pref.putInt("count", count);
        handleRequest(String(count), bot.messages[i].chat_id + "$" + bot.messages[i].from_name, "add");
        String keyboard = "[[{ \"text\" : \"Allow\", \"callback_data\" : \"" + String("#") + bot.messages[i].chat_id + "@" + bot.messages[i].from_name + "\"},{ \"text\" : \"Deny\", \"callback_data\" : \"" + "$" + bot.messages[i].chat_id + "$" + bot.messages[i].from_name + "\" }]]";
        bot.sendMessageWithInlineKeyboard(admin, bot.messages[i].from_name + " requesting access to your home system", "", keyboard);
        return;
      }

      for (int j = 1; j <= count; j++)
      {
        Serial.println(j);
        Serial.println(pref.getString(String(j).c_str(), "null")); //.substring(0, pref.getString(String(j).c_str(), "null").indexOf("@")));
        if (pref.getString(String(j).c_str(), "null").indexOf("@") != -1 && bot.messages[i].text.substring(0, 1) != "#" && bot.messages[i].text.substring(0, 1) != "$")
        {

          if (pref.getString(String(j).c_str(), "null").substring(0, pref.getString(String(j).c_str(), "null").indexOf("@")) == bot.messages[i].chat_id)
          {
            if (bot.messages[i].text == "open door" && doorMotor.read() == 90)
            {
              syncMotor();
              doorMotor.write(180);
              bot.sendMessage(bot.messages[i].chat_id, "done", "");
              pref.putString("DState", "open");
              informUsers("door opened by : " + bot.messages[i].from_name, j);
            }
            else if (bot.messages[i].text == "open door" && doorMotor.read() == 180)
            {
              bot.sendMessage(bot.messages[i].chat_id, "door already open", "");
            }
            else if (bot.messages[i].text == "close door" && doorMotor.read() == 180)
            {
              syncMotor();
              doorMotor.write(90);
              bot.sendMessage(bot.messages[i].chat_id, "done", "");
              pref.putString("DState", "close");
              informUsers("door closed by : " + bot.messages[i].from_name, j);
            }
            else if (bot.messages[i].text == "close door" && doorMotor.read() == 90)
            {
              bot.sendMessage(bot.messages[i].chat_id, "door already closed", "");
            }
            else if (bot.messages[i].text == "Lights : ON" && digitalRead(lightsPin) == 0)
            {
              digitalWrite(lightsPin, 1);
              bot.sendMessage(bot.messages[i].chat_id, "done", "");
              pref.putString("LState", "ON");
              informUsers("lights switched on by : " + bot.messages[i].from_name, j);
            }
            else if (bot.messages[i].text == "Lights : ON" && digitalRead(lightsPin) == 1)
            {
              bot.sendMessage(bot.messages[i].chat_id, "lights already on", "");
            }
            else if (bot.messages[i].text == "Lights : OFF" && digitalRead(lightsPin) == 1)
            {
              digitalWrite(lightsPin, 0);
              bot.sendMessage(bot.messages[i].chat_id, "done", "");
              pref.putString("LState", "OFF");
              informUsers("lights switched off  by : " + bot.messages[i].from_name, j);
            }
            else if (bot.messages[i].text == "Lights : OFF" && digitalRead(lightsPin) == 0)
            {
              bot.sendMessage(bot.messages[i].chat_id, "lights already off", "");
            }
            else if (bot.messages[i].text == "Security Mode : ON" && digitalRead(securityDevicePin) == 0)
            {
              securityDevice_State = "ON";
              digitalWrite(securityDevicePin, 1);
              bot.sendMessage(bot.messages[i].chat_id, "done", "");
              pref.putString("SDState", "ON");
              informUsers("security device turned on by : " + bot.messages[i].from_name, j);
            }
            else if (bot.messages[i].text == "Security Mode : ON" && digitalRead(securityDevicePin) == 1)
            {
              bot.sendMessage(bot.messages[i].chat_id, "Security Device already on", "");
            }
            else if (bot.messages[i].text == "Security Mode : OFF" && digitalRead(securityDevicePin) == 1)
            {
              securityDevice_State = "OFF";
              digitalWrite(securityDevicePin, 0);
              latch = false;
              bot.sendMessage(bot.messages[i].chat_id, "done", "");
              pref.putString("SDState", "OFF");
              informUsers("security device turned off by : " + bot.messages[i].from_name, j);
            }
            else if (bot.messages[i].text == "Security Mode : OFF" && digitalRead(securityDevicePin) == 0)
            {
              bot.sendMessage(bot.messages[i].chat_id, "Security Device already off", "");
            }
            else if (bot.messages[i].text == "Emergency Mode : ON" && emergency_State == "OFF")
            {
              emergency_State = "ON";
              emergencyMode();
              bot.sendMessage(bot.messages[i].chat_id, "done", "");
              pref.putString("EMState", "ON");
              informUsers("emergency mode turned on by : " + bot.messages[i].from_name, j);
            }
            else if (bot.messages[i].text == "Emergency Mode : ON" && emergency_State == "ON")
            {
              bot.sendMessage(bot.messages[i].chat_id, "emergency mode already on", "");
            }
            else if (bot.messages[i].text == "Emergency Mode : OFF" && emergency_State == "ON")
            {
              bot.sendMessage(bot.messages[i].chat_id, "done", "");
              pref.putString("EMState", "OFF");
              informUsers("emergency mode turned off by : " + bot.messages[i].from_name, j);
              emergency_State = "OFF";
            }
            else if (bot.messages[i].text == "Emergency Mode : OFF" && emergency_State == "OFF")
            {
              bot.sendMessage(bot.messages[i].chat_id, "emergency mode already off", "");
            }
            else if (bot.messages[i].text == "show options")
            {
              String keyboard = "";
              if (bot.messages[i].chat_id == admin)
                keyboard = "[[{\"text\" :\"open door\"},{\"text\" :\"close door\"}],[{\"text\" :\"Lights : ON\"},{\"text\" :\"Lights : OFF\"}],[{\"text\" :\"Security Mode : ON\"},{\"text\" :\"Security Mode : OFF\"}],[{\"text\" :\"Emergency Mode : ON\"},{\"text\" :\"Emergency Mode : OFF\"}],[{\"text\" :\"Logs : Enable\"},{\"text\" :\"Logs : Disable\"}],[{\"text\" :\"Get Users\"}]]";
              else
                keyboard = "[[{\"text\" :\"open door\"},{\"text\" :\"close door\"}],[{\"text\" :\"Lights : ON\"},{\"text\" :\"Lights : OFF\"}],[{\"text\" :\"Security Mode : ON\"},{\"text\" :\"Security Mode : OFF\"}],[{\"text\" :\"Emergency Mode : ON\"},{\"text\" :\"Emergency Mode : OFF\"}]]";

              bot.sendMessageWithReplyKeyboard(bot.messages[i].chat_id, "options updated, check them below", "", keyboard, true);
            }
            else
            {
              if (bot.messages[i].text != "Get Users" && bot.messages[i].text != "Logs : Enable" && bot.messages[i].text != "Logs : Disable" && bot.messages[i].text != "/start" && bot.messages[i].text.substring(0, 1) != "$" && bot.messages[i].text.substring(0, 1) != "#")
                bot.sendMessage(bot.messages[i].chat_id, "stop missing around", "");
            }
            if (bot.messages[i].chat_id != admin)
            {
              pref.end();
              return;
            }
          }
        }
        else if (pref.getString(String(j).c_str(), "null").substring(0, pref.getString(String(j).c_str(), "null").indexOf("$")) == bot.messages[i].chat_id)
        {
          String warning = String("unauthorized account trying to execute commands over the network. \n\n") + String("Account No  : ") + String(j) + String(" , ") + String(" Lid : ") + String(pref.getString(String(j).c_str(), "not found"));
          Serial.println(warning);
          bot.sendMessage(admin, warning);
          return;
        }
      }

      if (bot.messages[i].chat_id != admin)
      {
        String warning = String("unknown account trying to execute commands over the network. \n\n") + String("Name  : ") + String(bot.messages[i].from_name) + String(" , ") + String(" ID : ") + String(bot.messages[i].chat_id);
        Serial.println(warning);
        bot.sendMessage(admin, warning);
        return;
      }

      if (bot.messages[i].text.substring(0, 1) == "#")
      {
        for (int user = 2; user <= count; user++)
        {
          if (pref.getString(String(user).c_str(), "null").substring(0, pref.getString(String(user).c_str(), "null").indexOf("$")) == bot.messages[i].text.substring(1, bot.messages[i].text.indexOf("@")))
          {
            handleRequest(String(user), bot.messages[i].text.substring(1), "add");
            String name = bot.messages[i].text.substring(bot.messages[i].text.indexOf("@") + 1);
            bot.sendMessage(admin, name + " allowed");
            String keyboard = "[[{\"text\" :\"open door\"},{\"text\" :\"close door\"}],[{\"text\" :\"Lights : ON\"},{\"text\" :\"Lights : OFF\"}],[{\"text\" :\"Security Mode : ON\"},{\"text\" :\"Security Mode : OFF\"}],[{\"text\" :\"Emergency Mode : ON\"},{\"text\" :\"Emergency Mode : OFF\"}]]";
            bot.sendMessageWithReplyKeyboard(pref.getString(String(user).c_str(), "null"), "your account connected now, you can control devices that is connected to the network via options provided below", "", keyboard, false);
          }
        }
      }

      if (bot.messages[i].text.substring(0, 1) == "$")
      {
        for (int user = 2; user <= count; user++)
        {
          if (pref.getString(String(user).c_str(), "null") == bot.messages[i].text.substring(1))
          {
            count -= 1;
            pref.putInt("count", count);
            handleRequest(String(user), "non", "remove");
            String name = bot.messages[i].text.substring(bot.messages[i].text.lastIndexOf("$") + 1);
            bot.sendMessage(admin, name + " denied");
            return;
          }
        }
      }

      if (bot.messages[i].text == "Get Users" && bot.messages[i].chat_id == admin)
      {
        String users = "connected users : \n\n";
        String keyboard = "[[";
        for (int user = 2; user <= count; user++)
        {
          if (pref.getString(String(user).c_str(), "null").indexOf("@") != -1)
          {
            String name = pref.getString(String(user).c_str(), "null").substring(pref.getString(String(user).c_str(), "null").indexOf("@") + 1);
            String lID = pref.getString(String(user).c_str(), "null");
            users += pref.getString(String(user).c_str(), "null").substring(pref.getString(String(user).c_str(), "null").indexOf("@") + 1) + "\n";
            if (keyboard != "[[")
              keyboard += ",";
            keyboard += "{\"text\":\"Remove " + name + "\",\"callback_data\":\"$rem-" + lID + "\"}";
          }
        }

        keyboard += "]]";

        if (users.length() > 20)
          bot.sendMessageWithInlineKeyboard(bot.messages[i].chat_id, users, "", keyboard);

        return;
      }

      if (bot.messages[i].text == "Logs : Enable" && bot.messages[i].chat_id == admin)
      {
        if (logsState != "Enabled")
        {
          pref.putString("logsState", "Enabled");
          logsState = "Enabled";
          bot.sendMessage(admin, "logs enabled");
        }
        else
        {
          bot.sendMessage(admin, "logs already enabled");
        }
        return;
      }

      if (bot.messages[i].text == "Logs : Disable" && bot.messages[i].chat_id == admin)
      {
        if (logsState != "Disabled")
        {
          pref.putString("logsState", "Disabled");
          logsState = "Disabled";
          bot.sendMessage(admin, "logs disabled");
        }
        else
        {
          bot.sendMessage(admin, "logs already Disabled");
        }
        return;
      }

      if (bot.messages[i].text.indexOf("$rem-") != -1)
      {

        String name = bot.messages[i].text.substring(bot.messages[i].text.indexOf("-") + 1);

        for (int user = 2; user <= count; user++)
        {
          if (pref.getString(String(user).c_str(), "null") == bot.messages[i].text.substring(5)) //.substring(pref.getString(String(user).c_str(), "null").indexOf("@") + 1) == name)
          {
            int k = count - user;
            handleRequest(String(user), "non", "remove");
            count -= 1;
            pref.putInt("count", count);
            for (int n = 1; n <= k; n++)
            {
              String temp = pref.getString(String(user + n).c_str(), "null");
              handleRequest(String(user + n), "non", "remove");
              handleRequest(String(user + n - 1), temp, "add");
            }
          }
        }
        bot.sendMessage(admin, name + String(" removed"));
      }
    }
  }
  pref.end();
}

void syncMotor()
{
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  doorMotor.setPeriodHertz(50);
  doorMotor.attach(doorPin, 500, 4000);
}

void setup()
{

  Serial.begin(115200);
  Serial.println();
  syncMotor();
  pinMode(securityPin, INPUT);
  pinMode(lightsPin, OUTPUT);
  pinMode(securityDevicePin, OUTPUT);

  pref.begin("settings", false);
  doorState = pref.getString("DState", "und");
  delay(1000);
  Serial.println("doorState : " + doorState);
  securityDevice_State = pref.getString("SDState", "und");
  delay(1000);
  Serial.println("securityDevice_State : " + securityDevice_State);
  lightsState = pref.getString("LState", "und");
  delay(1000);
  Serial.println("lightsState : " + lightsState);
  emergency_State = pref.getString("EMState", "und");
  delay(1000);
  Serial.println("emergency_State : " + emergency_State);
  count = pref.getInt("count", 1);
  delay(1000);
  Serial.println("Users : " + String(count));
  logsState = pref.getString("logsState", "und");
  Serial.println("logs_State : " + logsState);
  pref.end();

  if (doorState == "open")
    doorMotor.write(180);
  if (doorState == "close")
    doorMotor.write(90);

  if (securityDevice_State == "ON")
    digitalWrite(securityDevicePin, 1);
  if ((securityDevice_State == "OFF"))
    digitalWrite(securityDevicePin, 0);

  if (lightsState == "ON")
    digitalWrite(lightsPin, 1);
  if (lightsState == "OFF")
    digitalWrite(lightsPin, 0);

  Serial.print("\nConnecting to ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  xTaskCreate(
      securityDevice,   /* Task function. */
      "securityDevice", /* String with name of task. */
      10000,            /* Stack size in bytes. */
      NULL,             /* Parameter passed as input of the task */
      1,                /* Priority of the task. */
      NULL);            /* Task handle. */
  /*
    xTaskCreatePinnedToCore(securityDevice, "securityDeviceprocess", 10000, NULL, 1, &securityDeviceProcess, 0);
    xTaskCreatePinnedToCore(mainTask, "mainProcess", 10000, NULL, 1, &mainProcess, 1);
  */
}

void loop()
{

  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages > 0)
  {
    Serial.println("got response");
    processRequest(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }

  if (emergency_State == "ON")
  {
    emergencyMode();
  }

  bot_lasttime = millis();

  if (latch)
  {
    pref.begin("settings", false);
    informUsers("be aware someone tring to break into your house.", 0);
    pref.end();
  }
}
