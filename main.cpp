#include "main.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);

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

float determinePPM(AnalogIn sensor, float R0, float m, float b) {
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
        return ppm;
}

void sensor_read(){
  unsigned char buffer[8];
  float *f_buf = (float*)(buffer+1);

  while(1){
    buffer[0] = LUM;
    *f_buf = (float)pot1.read()*100; //mq2sensorPPM;
    can_mutex.lock();
    mail_t *mail = mail_box.try_alloc();
    mail->identifier = buffer[0];
    mail->data = *f_buf;
    mail_box.put(mail);
    can_mutex.unlock();
    stdio_mutex.lock();
    printf("Sent luminosity: %d\n", (int)*f_buf);
    stdio_mutex.unlock();
    ThisThread::sleep_for(SENSOR_INTERVAL);

    buffer[0] = TMP;
    //Try to open the LM75B
    if (sensor.open()) {
        stdio_mutex.lock();
        printf("Device detected!\n");
        stdio_mutex.unlock();
        *f_buf=sensor.temp();
        can_mutex.lock();
        if(can.write(CANMessage(1337, buffer, sizeof(buffer)))){
          stdio_mutex.lock();
          printf("Sent temperature: %d\n", (int)*f_buf);
          stdio_mutex.unlock();
        }
        can_mutex.unlock();

    } else {
        stdio_mutex.lock();
        error("Device not detected!\n");
        stdio_mutex.unlock();
        buffer[1] = ERR;
        can_mutex.lock();
        can.write(CANMessage(1337, buffer, 2));
        can_mutex.unlock();
    }
    ThisThread::sleep_for(SENSOR_INTERVAL);

    buffer[0] = AIR;
    *f_buf = determinePPM(sensorMQ2, r0MQ2, slopeMQ2, interceptMQ2); //mq2sensorPPM;
    can_mutex.lock();
    if(can.write(CANMessage(1337, buffer, 5))){
      stdio_mutex.lock();
      printf("Sent air: %d\n", (int)*f_buf);
      stdio_mutex.unlock();
    }
    can_mutex.unlock();
    ThisThread::sleep_for(SENSOR_INTERVAL);
  }
}

void process_msg(void){
  char msg_aux;
  float f_msg_aux;
  char buffer[8];

  while(1){
    mail_t *mail = mail_box.try_get_for(100ms);
    if(mail!=NULL){
      msg_aux =  mail->identifier;
      stdio_mutex.lock();
      printf("Message received: %d\n", (int)msg_aux);
      stdio_mutex.unlock();
      //lcd.cls();
      if(msg_aux==AIR){
        f_msg_aux = mail->data;
        lcd_mutex.lock();
        lcd.locate(0,0);
        lcd.printf("Air quality: %d    \n", (int)f_msg_aux);
        lcd_mutex.unlock();
        if(f_msg_aux>100){
          buffer[0]=INIT;
          buffer[1]=SOS;
          buffer[2]=END;
          node.write(buffer, sizeof(buffer));
        }
        ThisThread::sleep_for(10ms);
        buffer[0]=INIT;
        buffer[1]=AIR;
        buffer[2] = (char)f_msg_aux;
        buffer[3]=END;
        node.write(buffer, sizeof(buffer));
        ThisThread::sleep_for(10ms);
      } else if(msg_aux==TMP){
        f_msg_aux = mail->data;
        lcd_mutex.lock();
        //lcd.cls();
        lcd.locate(0,10);
        lcd.printf("Temperature: %d    \n", (int)f_msg_aux);
        lcd_mutex.unlock();
        if(f_msg_aux>50){
          buffer[0]=INIT;
          buffer[1]=SOS;
          buffer[2]=END;
          node.write(buffer, sizeof(buffer));
        }
        ThisThread::sleep_for(10ms);
        buffer[0]=INIT;
        buffer[1]=TMP;
        buffer[2] = (char)f_msg_aux;
        buffer[3]=END;
        node.write(buffer, sizeof(buffer));
        ThisThread::sleep_for(10ms);
      } else if(msg_aux==LUM){
        f_msg_aux = mail->data;
        lcd_mutex.lock();
        //lcd.cls();
        lcd.locate(0,20);
        lcd.printf("Luminosity: %d    \n", (int)f_msg_aux);
        lcd_mutex.unlock();
        if(f_msg_aux<50){
          buffer[0]=INIT;
          buffer[1]=SOS;
          buffer[2]=END;
          node.write(buffer, sizeof(buffer));
        }
        ThisThread::sleep_for(10ms);
        buffer[0]=INIT;
        buffer[1]=LUM;
        buffer[2] = (char)f_msg_aux;
        buffer[3]=END;
        node.write(buffer, sizeof(buffer));
        ThisThread::sleep_for(10ms);
      }
      mail_box.free(mail);
    }
  }
}

int main(){
  //Uncomment if we want to reset R0 from default to our environment
  //r0MQ2 = calculateR0(sensorMQ2, airRatioMQ2);

/*
  node.write("Hi Node!\n", 4);
  if (node.read(buf, sizeof(buf))) {
    printf("Node answered back :)\n");
  }*/
  printf("Entering main()\n");

  thread_sensor_read.start(callback(sensor_read));
  thread_sensor_read.set_priority(osPriorityNormal);
  thread_msg.start(callback(process_msg));
  thread_msg.set_priority(osPriorityNormal2);

  CANMessage msg;

  while (1) {
    if (can.read(msg)) {
      mail_t *mail = mail_box.try_alloc();
      mail->identifier = msg.data[0];
      float *f_buf = (float*)(msg.data+1);
      mail->data = *f_buf;
      mail_box.put(mail);
    }
  }
}
