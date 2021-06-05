#if !DEVICE_CAN
#error [NOT_SUPPORTED] CAN not supported for this target
#endif

/*
INCLUDES
*/
#include "mbed.h"
#include "rtos.h"
#include "LM75B.h"
#include "C12832.h"


/*
DEFINES
*/
//define pins used
#define MBED_CONF_APP_CAN_RD p30
#define MBED_CONF_APP_CAN_TD p29
#define MQ2_ANALOG_PIN p17

// defines msg
#define SOS 0xff
#define INIT 0xfe
#define ERR 0xfd
#define TMP 0xfc
#define AIR 0xfb
#define LUM 0xfa

//defines sizes e times
#define SENSOR_INTERVAL 3s
#define XBEE_MSG_SIZE 10


/*
ESTRUTURAS
*/
/* Mail */
typedef struct {
  char identifier; /* CAN message identifier */
  float data;    /* CAN message data */
} mail_t;

/*
FUNÇÔES
*/
void air_measure(void);
void temperature_measure(void);
char determinePPM(AnalogIn, float, float, float);
float calculateR0(AnalogIn, float);

void send(void);
void process_msg(void);

/*
VARIAVEIS GLOBAIS
*/

/** The constructor takes in RX, and TX pin respectively.
  * These pins, for this example, are defined in mbed_app.json
  */
//CAN can(MBED_CONF_APP_CAN_RD, MBED_CONF_APP_CAN_TD);
BufferedSerial xbee(p9, p10);
DigitalOut xbee_rst(p30); //Digital reset for the XBee, 200ns for reset
DigitalIn xbee_status(p29); //Digital status for the XBee

C12832 lcd(p5, p7, p6, p8, p11);
LM75B sensor(p28,p27);

Mutex stdio_mutex, lcd_mutex, can_mutex;
Thread thread, thread_air, thread_temprature, thread_luminosity, thread_msg;
Mail<mail_t, 16> mail_box;


AnalogIn pot1 (p19);
//AnalogIn pot2 (p20);

AnalogIn sensorMQ2(MQ2_ANALOG_PIN);
//constants
const int numReadings = 500;
//MQ2
const float airRatioMQ2 = 10.0;
const float slopeMQ2 = -0.4687;
const float interceptMQ2 = 1.3969;
//globals for the sensor readings
float mq2sensorPPM = 0;
//globals for the R0 values
float r0MQ2 = 0.83142;
//gloabals for alarm values
float alarmMQ2 = 1000;
