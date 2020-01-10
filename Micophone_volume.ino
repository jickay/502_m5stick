
#include <M5StickC.h>
#include <driver/i2s.h>

#define PIN_CLK  0
#define PIN_DATA 34
#define READ_LEN (10 * 256)
#define GAIN_FACTOR 3
uint8_t BUFFER[READ_LEN] = {0};

uint16_t oldy[160];
int16_t *adcBuffer = NULL;

int volume = 0;
int oldvol = 0;
float minratio = 1.0;
float maxratio = 1.0;
int diff = 0;
int xmin = 0;
int xmax = 0;

int sensitivityMode = 0;
String modeText = "Low";
float margin = 0.02;
float margin2 = 0.04;
float margin3 = 0.06;

uint16_t color = BLUE;

void i2sInit()
{
   i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate =  44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
    .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 2,
    .dma_buf_len = 128,
   };

   i2s_pin_config_t pin_config;
   pin_config.bck_io_num   = I2S_PIN_NO_CHANGE;
   pin_config.ws_io_num    = PIN_CLK;
   pin_config.data_out_num = I2S_PIN_NO_CHANGE;
   pin_config.data_in_num  = PIN_DATA;
	
   
   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
   i2s_set_pin(I2S_NUM_0, &pin_config);
   i2s_set_clk(I2S_NUM_0, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
}

int setVolume(float ratio, float margin) {
  if (ratio < (1.00 - margin)) { return 1; }
  else if (ratio > (1.00 + margin)) { return 1; }
  return 0;
}

int decayVolume(int val, int oldVal, int decay) {
  if (val == oldVal) { 
    val -= decay;
    if (val < 0) {
      val = 0;
    }
  }
  return val;
}

void changeModes() {
  sensitivityMode++;
  if (sensitivityMode > 2) { sensitivityMode = 0; }
  if (sensitivityMode == 0) {
    modeText = "Low";
    margin = 0.02;
    margin2 = 0.03;
    margin3 = 0.04;
  } else if (sensitivityMode == 1) {
    modeText = "Med";
    margin = 0.03;
    margin2 = 0.04;
    margin3 = 0.05;
  } else {
    modeText = "High";
    margin = 0.04;
    margin2 = 0.05;
    margin3 = 0.06;
  }
}

void setBackgroundColor(int val) {
  int limit = 100;
  float multiplier = 2;
  uint16_t oldColor = color;
  if (0 <= val && val <= limit) { color = BLUE; }
  if (limit < val && val <= limit*2) { color = GREEN; }
  if (limit*2 < val && val <= limit*3) { color = 0xff80; }
  if (limit*3 < val && val <= limit*4) { color = 0xfbe4; }
  if (limit*4 < val && val <= limit*5) { color = 0xe8e4; }

  if (color != oldColor) {
    M5.Lcd.fillScreen(color);
  }
}

void mic_record_task (void* arg)
{   
  size_t bytesread;
  
  int timer = 0;
  int seconds = 10;
  int cycle = 15;
  int decay = 10;

  int graphBase = 60;
  
  int oldy1[cycle];
  int oldy2[cycle];
  int oldy3[cycle];
  int volarr1[cycle];
  int volarr2[cycle];
  int volarr3[cycle];
  int vol1 = 0;
  int vol2 = 0;
  int vol3 = 0;
  int vol1_total = 0;
  int vol2_total = 0;
  int vol3_total = 0;

  while(1){
    // Detect button press
    if (digitalRead(M5_BUTTON_HOME) == LOW) {
      changeModes();
    }
    
    // Read in sound data
    float delayRate = 100 / portTICK_RATE_MS;
    i2s_read(I2S_NUM_0,(char*) BUFFER, READ_LEN, &bytesread, delayRate);
    adcBuffer = (int16_t *)BUFFER;

    // Set values
    showSignal();
    float ratio = (float)volume/oldvol;

    // Print values
    M5.Lcd.setCursor(0, 60);

//    M5.Lcd.println(timer/10);
    M5.Lcd.println(modeText);
//    M5.Lcd.println(ratio);
    if (vol1_total < 100 && vol1_total > -100) {
      M5.Lcd.printf("1: %d, 2: %d, 3: %d\n", vol1_total, vol2_total, vol3_total);
    }

    // Set volume points for different sensitivities
    vol1 += setVolume(ratio, margin);
    vol2 += setVolume(ratio, margin2);
    vol3 += setVolume(ratio, margin3);

    timer += 1;

    // For every second draw graph
    if (timer % 10 == 0) {
      vol1_total = 0;
      vol2_total = 0;
      vol3_total = 0;

      // Get last values, and decay if no change
      int endIndex = cycle - 1;
      int lastVal1 = volarr1[endIndex];
      int lastVal2 = volarr2[endIndex];
      int lastVal3 = volarr3[endIndex];

      vol1 = decayVolume(vol1, lastVal1, decay);
      vol2 = decayVolume(vol2, lastVal2, decay);
      vol3 = decayVolume(vol3, lastVal3, decay);

      // 'Move' values over to the 'left'
      for (int i=0; i<cycle; i++) {

        if (i == endIndex) {
          volarr1[endIndex] = vol1;
          volarr2[endIndex] = vol2;
          volarr3[endIndex] = vol3;
        } else {
          volarr1[i] = volarr1[i+1];
          volarr2[i] = volarr2[i+1];
          volarr3[i] = volarr3[i+1];
        }
        
      // Graph START
        // 'Erase' old values, and draw new values
        int nLength = 160 / cycle;
        int nStart = i*nLength;
        int nEnd = (i+1)*nLength;
        for (int n=nStart; n<nEnd; n++) {
          M5.Lcd.drawPixel(n,graphBase-oldy1[i],BLACK);
          M5.Lcd.drawPixel(n,graphBase-oldy2[i],BLACK);
          M5.Lcd.drawPixel(n,graphBase-oldy3[i],BLACK);
          M5.Lcd.drawPixel(n,graphBase-volarr1[i],GREEN);
          M5.Lcd.drawPixel(n,graphBase-volarr2[i],BLUE);
          M5.Lcd.drawPixel(n,graphBase-volarr3[i],RED);
        }
      // Graph END

        // Set current totals for each volume level
        vol1_total += volarr1[i];
        vol2_total += volarr2[i];
        vol3_total += volarr3[i];
        
        // Set old y values to 'erase' on graph next cycle
        oldy1[i] = volarr1[i];
        oldy2[i] = volarr2[i];
        oldy3[i] = volarr3[i];
      }
    }

    // Set background color
//    if (vol1 == 0) { 
//      vol1_total -= 1; 
//      if (vol1_total < 0) { vol1_total = 0;}
//    } 
    setBackgroundColor(vol1_total);

      // Set timer to zero after 10 seconds
//    if (timer >= seconds * 10) { 
//      timer = 0;
//    }
    
    vTaskDelay(delayRate);
  }
}

void setup() {
  M5.begin();
  pinMode(M5_BUTTON_HOME, INPUT);
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE, BLACK);
  i2sInit();
  xTaskCreate(mic_record_task, "mic_record_task", 2048, NULL, 1, NULL);
}

int setColor(int x) {
  if (x > 255/2) {
    return RED;
  } else {
    return BLUE;
  }
}

void showSignal(){
  int x;
  int y;
  xmin = 0;
  xmax = 0;
  int total = 0;

  // Set all sound values and sum any differences from baseline
  // Sound values are both positive and negative, so absolute value used
  for (int n = 0; n < 100; n++){
    x = map(adcBuffer[n], INT16_MIN, INT16_MAX, -100, 100);
    total += abs(x);
  }

  // Store val from last cycle, set new val for current cycle
  oldvol = volume;
  volume = total;
}

void loop() {
  printf("loop cycling\n");
  vTaskDelay(1000 / portTICK_RATE_MS); // otherwise the main task wastes half of the cpu cycles
}
