/*
  Streaming of sound data with Bluetooth to other Bluetooth device.
  We demonstrate how we can disconnect and reconnect again.
  
  Copyright (C) 2020 Phil Schatzmann
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "BluetoothA2DPSource.h"
#include <driver/i2s.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define I2S_CH_NUM   I2S_NUM_0
#define I2S_OUT_BCK  16
#define I2S_OUT_LCK  17
#define I2S_OUT_DIN  21
#define I2S_OUT_NC   -1
#define I2S_SAMPLE_RATE  192000
#define I2S_BUFFER_COUNT 4
#define I2S_BUFFER_SIZE (SOUND_BUFFER_SIZE*8)
#define SOUND_BUFFER_SIZE 64
#define SINE_TABLE_SIZE 4096
#define SINE_TABLE_CYCLE (SINE_TABLE_SIZE*4)

TaskHandle_t thp[1];

int32_t sound_buffer[SOUND_BUFFER_SIZE*2];
float pi = 3.141592;
int16_t sin_data[SINE_TABLE_SIZE];

void i2s_initilaize();

int c3_frequency = 100;
double A1 = 1.0;
double A2 = 0;

double epsilon = 0.00001; // 許容される誤差

BluetoothA2DPSource a2dp_source;
bool state = true;
bool state2 = true;

double angle = 0.0;

uint64_t t = 0;

unsigned long TimeA1 = 0.0;
unsigned long Timebefore = 0.0;
unsigned long TimeA2 = 0.0;

// The supported audio codec in ESP32 A2DP is SBC. SBC audio stream is encoded
// from PCM data normally formatted as 44.1kHz sampling rate, two-channel 16-bit sample data
int32_t get_data_channels(Frame *frame, int32_t channel_len) {
    static double m_time = 0.0;
    double m_amplitude = 10000.0;  // -32,768 to 32,767
    double m_deltaTime = 1.0 / 44100.0;
    double m_phase = 0.0;
    double double_Pi = PI * 2.0;
    // fill the channel data
    for (int sample = 0; sample < channel_len; ++sample) {
        angle = double_Pi * c3_frequency * m_time + m_phase;
        frame[sample].channel1 = m_amplitude * sin(angle);
        frame[sample].channel2 = frame[sample].channel1;
        m_time += m_deltaTime;
        if(channel_len == 2147483647) sample = 0;
    }


    return channel_len;
}

void i2s_initilaize(){
  i2s_driver_uninstall(I2S_CH_NUM);
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = I2S_SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT, //stereo
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = I2S_BUFFER_COUNT,
    .dma_buf_len          = I2S_BUFFER_SIZE,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
    .fixed_mclk           = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num           = I2S_OUT_BCK,
    .ws_io_num            = I2S_OUT_LCK,
    .data_out_num         = I2S_OUT_DIN,
    .data_in_num          = I2S_OUT_NC,
  };
 
  i2s_driver_install(I2S_CH_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_CH_NUM, &pin_config);
}

// タスク1の実行関数
void task1(void *param) {
    uint64_t t = 0;
    while (1) {
        size_t transBytes = SOUND_BUFFER_SIZE*8;
        uint16_t pos = t%SOUND_BUFFER_SIZE;
        sound_buffer[pos*2+0] = sin_fast(t*SINE_TABLE_CYCLE*100/I2S_SAMPLE_RATE)*65536*0.1;
        sound_buffer[pos*2+1] = sin_fast(t*SINE_TABLE_CYCLE*100/I2S_SAMPLE_RATE)*65536*0.1;
        if(pos==(SOUND_BUFFER_SIZE-1)){
          i2s_write(I2S_CH_NUM,(char*)sound_buffer, I2S_BUFFER_SIZE, &transBytes, portMAX_DELAY);
        }
         t++;
    }
}
// タスク2の実行関数
void task2(void *param) {
  TimeA1 = millis();
  TimeA2 = millis();
    while (1) {
          a2dp_source.set_volume(A1*255);
          if((millis()-TimeA1) >= 1)
          {
            if(fabs(abs(A1 - 0)) < epsilon) state = false;
            if(fabs(abs(A1 - 1)) < epsilon) state = true;
            if(state == false) A1 = A1 + 0.01;
            if(state == true) A1 = A1 - 0.01;
            Serial.print("A1:");
            Serial.println(A1);
            //Serial.println(state);
          if((millis()-TimeA2) >= 700){
            if(fabs(abs(A2 - 0)) < epsilon) state2 = false;
            if(fabs(abs(A2 - 1)) < epsilon) state2 = true;
            if(state2 == false) A2 = A2 + 0.01;
            if(state2 == true) A2 = A2 - 0.01;
            Serial.print("A2:");
            Serial.println(A2);
            //Serial.println(state2);
            if(fabs(abs(A2 - 0)) < epsilon && state2 == true) TimeA2 = millis();
          }
            TimeA1 = millis();
          }else{
            delay(1);
          }
    }
}

void setup() {
  Serial.begin(115200);
  //a2dp_source.set_auto_reconnect(false);
  a2dp_source.start("BSHSBE200", get_data_channels);  
  a2dp_source.set_volume(255);
  i2s_initilaize();

  for(int16_t i=0; i<SINE_TABLE_SIZE; i++){
    sin_data[i] = sin(2.0*pi*i/SINE_TABLE_CYCLE)*32767;
  }

   // タスク1をコア0に割り当て
   xTaskCreatePinnedToCore(task2, "Task2", 4096, NULL, 1, &thp[0], 0);
}


void loop() {

    while(true){
    
    
    size_t transBytes = SOUND_BUFFER_SIZE*8;
    uint16_t pos = t%SOUND_BUFFER_SIZE;

    if(c3_frequency == 300)
    {
     sound_buffer[pos*2+0] = 0;
     sound_buffer[pos*2+1] = 0;
    }else{
      sound_buffer[pos*2+0] = sin_fast(t*SINE_TABLE_CYCLE*(c3_frequency)/I2S_SAMPLE_RATE)*65536*A2;
      sound_buffer[pos*2+1] = sin_fast(t*SINE_TABLE_CYCLE*(c3_frequency)/I2S_SAMPLE_RATE)*65536*A2;
    }
    t++;

    if(pos==(SOUND_BUFFER_SIZE-1)){
      i2s_write(I2S_CH_NUM,(char*)sound_buffer, I2S_BUFFER_SIZE, &transBytes, portMAX_DELAY);
    }
    }
    
}

//高速サイン関数
int16_t sin_fast(uint64_t theta0){
  uint16_t theta = theta0 & 0x3FFF;

  if(theta < SINE_TABLE_SIZE){
    return(sin_data[theta]);
  }
  else if(theta < SINE_TABLE_SIZE*2){
    return(sin_data[SINE_TABLE_SIZE*2-theta-1]);
  }
  else if(theta < SINE_TABLE_SIZE*3){
    return(-sin_data[theta-SINE_TABLE_SIZE*2]);
  }
  else{
    return(-sin_data[SINE_TABLE_SIZE*4-theta-1]);
  }
}
