#include "main.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);

Thread thread_msg;

char counter = 0;


float calculateR0(AnalogIn s, float ratio) {
        float sensor_volt;
        float rs;
        float sensorValue = 0.0;
        float r0;

        //take 500 sensor readings and add them together
        for(int i = 0; i < numReadings; i++) {
            sensorValue = sensorValue + s.read();
        }

        sensorValue = sensorValue/numReadings;//average sensor value
        sensor_volt = sensorValue * 3.3;
        rs = ((3.3-sensor_volt)/sensor_volt);
        r0 = rs/ratio;
        printf("RO VALUE: %f \n\n", r0);
        return r0;
}

char determinePPM(AnalogIn sensor, float R0, float m, float b) {
        //Slope and y-intercept of ppm graph line, and R0 from previous calculations
        float voltage = sensor.read() * 3.3;
        float RS_gas = ((3.3-voltage)/voltage);
        float ppmRatio = RS_gas/R0;
        float ppm_log = (log10(ppmRatio)-b)/m;
        float ppm = pow(10, ppm_log);
        if(ppm<0){
            ppm = 0.0;
        }
        if(ppm>10000){
            ppm = 10000;
        }
        return (char)floor(ppm);
}

void air_measure(void){
  unsigned char buffer[8];
  float *f_buf = (float*)(buffer+1);

  while(1){
    buffer[0] = AIR;
    *f_buf = determinePPM(sensorMQ2, r0MQ2, slopeMQ2, interceptMQ2); //mq2sensorPPM;
    can_mutex.lock();
    can1.write(CANMessage(1337, buffer, 5));
    can_mutex.unlock();
    stdio_mutex.lock();
    printf("Sent air: %d\n", (int)*f_buf);
    stdio_mutex.unlock();
    ThisThread::sleep_for(1s);
  }

}

void temperature_measure(void){
  unsigned char buffer[8];
  float *f_buf = (float*)(buffer+1);

  while(1){
    buffer[0] = TMP;
    //Try to open the LM75B
    if (sensor.open()) {
        stdio_mutex.lock();
        printf("Device detected!\n");
        stdio_mutex.unlock();
        *f_buf=sensor.temp();
        stdio_mutex.lock();
        printf("Temperature calculated: %d\n", (int)*f_buf);
        stdio_mutex.unlock();
        can_mutex.lock();
        can1.write(CANMessage(1337, buffer, sizeof(buffer)));
        can_mutex.unlock();

    } else {
        stdio_mutex.lock();
        error("Device not detected!\n");
        stdio_mutex.unlock();
        buffer[1] = ERR;
        can_mutex.lock();
        can1.write(CANMessage(1337, buffer, 2));
        can_mutex.unlock();
    }
    ThisThread::sleep_for(5s);
  }
}

void send(void){
  while(1){
    stdio_mutex.lock();
    printf("send()\n");
    if (can1.write(CANMessage(1337, &counter, 1))) {
      printf("wloop()\n");
      counter++;
      printf("Message sent: %d\n", counter);
    }
    stdio_mutex.unlock();
    ThisThread::sleep_for(1s);
  }
}

void process_msg(void){
  char msg_aux;
  float f_msg_aux;

  while(1){
    mail_t *mail = mail_box.try_get_for(100ms);
    if(mail!=NULL){
      msg_aux =  mail->identifier;
      stdio_mutex.lock();
      printf("Message received: %d\n", (int)msg_aux);
      stdio_mutex.unlock();
      if(msg_aux==AIR){
        f_msg_aux = mail->data;
        lcd_mutex.lock();
        lcd.cls();
        lcd.locate(0,0);
        lcd.printf("Air quality: %d\n", (int)f_msg_aux);
        lcd_mutex.unlock();
      } else if(msg_aux==TMP){
        f_msg_aux = mail->data;
        lcd_mutex.lock();
        lcd.cls();
        lcd.locate(0,12);
        lcd.printf("Temperature: %d\n", (int)f_msg_aux);
        lcd_mutex.unlock();
      }
      mail_box.free(mail);
    }
  }
}

int main(){
  //Uncomment if we want to reset R0 from default to our environment
  //r0MQ2 = calculateR0(sensorMQ2, airRatioMQ2);

  printf("main()\n");

  /*thread.start(send);
  thread.set_priority(osPriorityHigh);
  thread_air.start(air_measure);
  thread_air.set_priority(osPriorityNormal);*/
  thread_temprature.start(temperature_measure);
  thread_temprature.set_priority(osPriorityNormal);
  thread_msg.start(callback(process_msg));
  thread_msg.set_priority(osPriorityNormal1);

  CANMessage msg;

  while (1) {
    if (can2.read(msg)) {
      mail_t *mail = mail_box.try_alloc();
      mail->identifier = msg.data[0];
      float *f_buf = (float*)(msg.data+1);
      mail->data = *f_buf;
      mail_box.put(mail);
    }
  }
}
