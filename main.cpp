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
  //float mq2sensorPPM;
  char msg[8];

  while(1){
    msg[0] = AIR;
    *(msg+1) = determinePPM(sensorMQ2, r0MQ2, slopeMQ2, interceptMQ2); //mq2sensorPPM;
    can_mutex.lock();
    can1.write(CANMessage(1337, msg, 5));
    can_mutex.unlock();
    ThisThread::sleep_for(500ms);
  }

}

void temperature_measure(void){
  char msg[8];

  while(1){
    msg[0] = TMP;
    //Try to open the LM75B
    if (sensor.open()) {
        printf("Device detected!\n");
        *(msg+1)=(float)sensor.temp();
        can_mutex.lock();
        can1.write(CANMessage(1337, msg, 5));
        can_mutex.unlock();

    } else {
        error("Device not detected!\n");
        msg[1] = ERR;
        can_mutex.lock();
        can1.write(CANMessage(1337, msg, 2));
        can_mutex.unlock();
    }
    ThisThread::sleep_for(5s);
  }
}

void send(void){
  while(1){
    stdio_mutex.lock();
    printf("send()\n");
    can_mutex.lock();
    if (can1.write(CANMessage(1337, &counter, 1))) {
      printf("wloop()\n");
      counter++;
      printf("Message sent: %d\n", counter);
    }
    can_mutex.unlock();
    stdio_mutex.unlock();
    ThisThread::sleep_for(1s);
  }
}

void process_msg(void){
  char msg_aux;

  while(1){
    mail_t *mail = mail_box.try_get();
    if(mail!=NULL){
      msg_aux =  mail->msg[0];
      stdio_mutex.lock();
      printf("Message received: %d\n", (int)msg_aux);
      stdio_mutex.unlock();
      if(mail->msg[0]==AIR){
        lcd_mutex.lock();
        lcd.cls();
        lcd.locate(0,3);
        lcd.printf("Temp = %.3f\n", (float)*(mail->msg+1));
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
  thread.set_priority(osPriorityHigh);*/
  thread_air.start(air_measure);
  thread_air.set_priority(osPriorityLow1);
  thread_temprature.start(temperature_measure);
  thread_temprature.set_priority(osPriorityLow2);
  thread_msg.start(callback(process_msg));
  thread_msg.set_priority(osPriorityLow);

  CANMessage msg;
  bool flag;
  while (1) {
    stdio_mutex.lock();
    printf("Loop...\n");
    stdio_mutex.unlock();
    flag = can2.read(msg, 0);
      stdio_mutex.lock();
      printf("Bug...\n");
      stdio_mutex.unlock();
    if (flag) {
      mail_t *mail = mail_box.try_alloc();
      *(mail->msg) = (long long)*(msg.data);
      mail_box.put(mail);
    }
  }
}
