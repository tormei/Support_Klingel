#include <sd_diskio.h>
#include <sd_defines.h>

#include "Audio.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"

#define SLEEP_TIME 30
#define ADC_PIN   34
#define BTN       16
#define SD_CS      5
#define SD_SCK    18
#define SD_MOSI   23
#define SD_MISO   19
#define AMP_DOUT   4
#define AMP_BCLK   2
#define AMP_LRC   15

Audio audio;
RTC_DATA_ATTR bool runOnAkku = false;
RTC_DATA_ATTR bool audioIsPlaying = false;
RTC_DATA_ATTR int numberOfSolutions = 0;
RTC_DATA_ATTR char * solutionsDir = "/solutions";
RTC_DATA_ATTR int boot_count = 0;
int last_sup = 0;
int last_sup2 = 0;
int last_sup3 = 0;
int last_sup4 = 0;
int last_sup5 = 0;
int rndSeed = 27;

void setup() {
  _initVoltsArray();
  randomSeed(analogRead(rndSeed));
  pinMode(BTN, INPUT_PULLUP);
  pinMode(SD_CS, OUTPUT);      
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  SPI.setFrequency(1000000);
  Serial.begin(115200);
  while(!SD.begin(SD_CS))
  {
    Serial.println("Error talking to SD card!");
    delay(2000);
  }
  audio.setPinout(AMP_BCLK, AMP_LRC, AMP_DOUT);
  audio.setVolume(10); // 0...21
  audio.forceMono(true);
  int wake_reason = print_wakeup_reason();
  if (wake_reason == 0)
  {
    Serial.println("Prepare welcome audio...");
    audio.connecttoFS(SD, "welcome.mp3");
    numberOfSolutions = countSupportAnswers(SD, solutionsDir); //Dir of 0_index_based_number.mp3 Files
    last_sup = random(0, numberOfSolutions);
    last_sup2 = random(0, numberOfSolutions);
    last_sup3 = random(0, numberOfSolutions);
    last_sup4 = random(0, numberOfSolutions);
    last_sup5 = random(0, numberOfSolutions);
      if(getBatteryVolts() > 2.8) {
       esp_sleep_enable_timer_wakeup(SLEEP_TIME * 1000000);
       runOnAkku = true;
       Serial.println("Runnin with Akku");
    }
  }
  else if (wake_reason ==3) {
    check_akku();
    Serial.println("going back to sleep-mode");
    esp_deep_sleep_start();
  }
}


void loop() {
  btn_loop(); //Wait for some User to click the button.
}


void btn_loop() {
  Serial.println("Start of btn_loop");
  int solution_index = 0; 
  int counter = 0;
  while (digitalRead(BTN) == HIGH) {
	  audio.loop();
    counter++;
    if(counter == 1000) {
      if (runOnAkku)
      {
        check_akku();
      }
      else
      {
        solution_index = random(0, numberOfSolutions);
        counter = 0;
      }
    }
    if(counter > 6000 && runOnAkku) {
      counter = 0;
      Serial.println("Entering sleep-mode");
      esp_deep_sleep_start();
    }
    delay(5);
    
  }
  //take care that the solutions do not repead to quickly
  while (solution_index == last_sup || solution_index == last_sup2 || solution_index == last_sup3 || solution_index == last_sup4 || solution_index == last_sup5) {
    solution_index = random(0, numberOfSolutions);
  }
  last_sup5 = last_sup4;
  last_sup4 = last_sup3;
  last_sup3 = last_sup2;
  last_sup2 = last_sup;
  last_sup = solution_index;
  //Present solution to user
  //Serial.println(solution_index);
  giveSupport(solution_index);
  Serial.println("Reached end of btn_loop");
}


void giveSupport(int solution_index) {
  char file[40]; //prepare the matching solution_file for playing
  sprintf(file, "/solutions/%i.mp3", solution_index);
  Serial.print("Play Supportevent "); Serial.println(String(solution_index));
  //Welcome the User
  talkToUser("start.mp3");
  //Give some tech advice
  talkToUser(file);
  //Let the satisfied user go
  talkToUser("end.mp3");
}


void talkToUser(char * file) {
  audio.connecttoFS(SD, file);
  audioIsPlaying = true;
  while(audioIsPlaying)
  {
    audio.loop();
  }
  Serial.println("Reached end of Talkfunction");
}

//System stuff - you realy do not need to read this
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
    audioIsPlaying = false;
}

void check_akku() {
  int akkulvl = getBatteryChargeLevel();
  Serial.print("Akku: "); Serial.println(akkulvl);
  if(akkulvl<15 && runOnAkku) {
    talkToUser("/solutions/0.mp3");
    talkToUser("/solutions/0.mp3");
  }
  else if (akkulvl <25 && runOnAkku) {
    talkToUser("/solutions/0.mp3");
  }
}

int print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); return 1; break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); return 2;  break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); return 3; break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); return 4; break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); return 5; break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); return 0; break;
  }
}

int countSupportAnswers(fs::FS &fs, String dirname){
	int files = 0;
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return -1;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return -1;
    }
    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
			files++;
        }
        file = root.openNextFile();
    }
    Serial.print(files);
    Serial.println(" Solutions found.");
	return files;
}
