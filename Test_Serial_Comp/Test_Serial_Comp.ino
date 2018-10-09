#include <MPU6050.h>
#include <I2Cdev.h>
#include <ADXL345.h>
#include <Wire.h>
#include<Arduino_FreeRTOS.h>
#include<task.h>
#define STACK_SIZE 500
#include<stdio.h>
#include<stdlib.h>

#define DEVICE_A_ACCEL (0x53)    //first ADXL345 device address
#define DEVICE_B_ACCEL (0x1D)    //second ADXL345 device address
#define DEVICE_C_GYRO (0x68) // MPU6050 address
#define TO_READ (6)        //num of bytes we are going to read each time
#define voltageDividerPin 0
#define currentSensorPin 1
#define RS 0.1
#define RL 10000
#define PKT_SIZE 20

ADXL345 sensorA = ADXL345(DEVICE_A_ACCEL);
ADXL345 sensorB = ADXL345(DEVICE_B_ACCEL);
MPU6050 sensorC = MPU6050(DEVICE_C_GYRO);

//determining scale factor based on range set
//Since range in +-2g, range is 4mg/LSB.
//value of 1023 is used because ADC is 10 bit
int rangeAccel = (2-(-2));
const float scaleFactorAccel = rangeAccel/1023.0;

/*
 * +-250degrees/second already set initialize() function
 * value of 65535 is used due to 16 bit ADC
 */ 
//determining the scale factor for gyroscope
int rangeGyro = 250-(-250);
const float scaleFactorGyro = rangeGyro/65535.0; 
 
//declaring variable to store value of volt and amps
float vOut , voltageReading, currentReading;

//16 bit integer values for raw data of accelerometers
int16_t xa_raw, ya_raw, za_raw, xb_raw, yb_raw, zb_raw;

//16 bit integer values for offset data of accelerometers
int16_t xa_offset, ya_offset, za_offset, xb_offset, yb_offset, zb_offset;

//16 bit integer values for gyroscope readings
int16_t xg_raw, yg_raw, zg_raw;

//16 bit integer values for offset data of gyroscope
int16_t xg_offset, yg_offset, zg_offset;

//Structure of data packet
typedef struct Packet {
  float num;
  float gyro[3];
  float acc1[3];
  float acc2[3];
  float power;
  float current;
  float voltage;
  float energy;
} Packet;   

Packet packet;

char* acc1_x;
char* acc1_y;
char* acc1_z;
char* acc2_x;
char* acc2_y;
char* acc2_z;
char* gyro_x;
char* gyro_y;
char* gyro_z;
char* voltage_c;
char* current_c;
char* power_c;
char* energy_c;

char databuf[1000];

/**
 * Main Task
 */
void mainTask(void *p) {
TickType_t xLastWakeTime = xTaskGetTickCount();
for (int i=0;i <PKT_SIZE; i++) {
  getData(); 
  changeFormat(i);
  strcat(databuf, ",");
  vTaskDelayUntil(&xLastWakeTime, (20U/ portTICK_PERIOD_MS));
}
sendToPi();
}



/** 
 *To send to Pi once Pi is ready for communication
 *Packet is of Type float (4 bytes) 
 * A sample of 20 packets will be sent to Rpi every 200ms
 */
 void sendToPi() {
      strcat(databuf, "\r");
      Serial.println();
      Serial.println(sizeof(databuf));
      strcpy(databuf, "");
}

 /**
 *  To collect readings from all the sensor and package into one packet every 20ms
 */
void getData(){

      // Read values from different sensors
      getScaledReadings();
    
      //Measure and display voltage measured from voltage divider
      voltageReading = analogRead(voltageDividerPin);
      packet.voltage = remapVoltage(voltageReading) * 2;


      //Measure voltage out from current sensor to calculate current
      vOut = analogRead(currentSensorPin);
      vOut = remapVoltage(vOut);
      packet.current = (vOut * 1000) / (RS * RL) * 1000;

      //Power is in mW due to current being in mA
      packet.power = packet.current * packet.voltage;

      static long prevTime = 0;
      float secondsPassed = (millis()-prevTime) / (1000.0);

      static float energy = 0;
      //Power / 1000.0 because converting mW to W
      //This allows joules to  be in W per seconds
      energy += secondsPassed * (packet.power /1000.0);

      prevTime = millis();

      packet.energy = energy;
}

 /*
 * Data processing
 */
float remapVoltage(int volt) {
  float analogToDigital;
  analogToDigital = (5.0/1023) * volt;  
  return analogToDigital;
}


/*
 * To process the raw data obtained from sensor reading
 */
void getScaledReadings() {
  sensorA.getAcceleration(&xa_raw, &ya_raw, &za_raw);
  packet.acc1[0] = (xa_raw + xa_offset)*scaleFactorAccel;
  packet.acc1[1] = (ya_raw + ya_offset)*scaleFactorAccel;
  packet.acc1[2] = (za_raw + za_offset)*scaleFactorAccel;
  
  sensorB.getAcceleration(&xb_raw, &yb_raw, &zb_raw);
  packet.acc2[0] = (xb_raw + xb_offset)*scaleFactorAccel;
  packet.acc2[1] = (yb_raw + yb_offset)*scaleFactorAccel;
  packet.acc2[2] = (zb_raw + zb_offset)*scaleFactorAccel;
  
  sensorC.getRotation(&xg_raw, &yg_raw, &zg_raw);
  packet.gyro[0] = (xg_raw + xg_offset)*scaleFactorGyro;
  packet.gyro[1]= (yg_raw + yg_offset)*scaleFactorGyro;
  packet.gyro[2] = (zg_raw + zg_offset)*scaleFactorGyro;
 }


/*
 * Purpose of adding 255 is to account for downward default acceleration in Z axis to be 1g
 */
void calibrateSensors() {
  //Setting range of ADXL345
  sensorA.setRange(ADXL345_RANGE_2G);
  sensorB.setRange(ADXL345_RANGE_2G);
  
  sensorA.getAcceleration(&xa_raw, &ya_raw, &za_raw);
  sensorB.getAcceleration(&xb_raw, &yb_raw, &zb_raw);
  sensorC.getRotation(&xg_raw, &yg_raw, &zg_raw);
  
  xa_offset = -xa_raw;
  ya_offset = -ya_raw;
  za_offset = -za_raw+255;

  xb_offset = -xb_raw;
  yb_offset = -yb_raw;
  zb_offset = -zb_raw+255;

  xg_offset = -xg_raw;
  yg_offset = -yg_raw;
  zg_offset = -zg_raw;
}


/**
 *  To perform handshake to ensure that communication between Rpi and Aduino is ready
 */
void handshake() {
  int h_flag = 0;
  int n_flag = 0;


  while (h_flag == 0) {
    if (Serial1.available()) {
      if ((Serial1.read() == 'H')) {
        h_flag = 1;
      }
    }
  }

  while (n_flag == 0) {
    if (Serial1.available()) {
      Serial1.write('A');
      if (Serial1.read() == 'N') {
        Serial.println("Handshake done");
        n_flag = 1;
      }
    }
  }

}

/** 
 Change the format of the data from float to string
 */
void changeFormat(int num){
char charbuf[64] ;


  acc1_x = dtostrf( packet.acc1[0],3,2,charbuf);
  strcat(databuf, acc1_x); 
  strcat(databuf, ",");
  acc1_y = dtostrf(packet.acc1[1],3,2,charbuf);
  strcat(databuf, acc1_y); 
  strcat(databuf, ",");
  acc1_z = dtostrf(packet.acc1[2],3,2,charbuf);
  strcat(databuf, acc1_z); 
  strcat(databuf, ",");

  acc2_x = dtostrf(packet.acc2[0],3,2,charbuf);
  strcat(databuf, acc2_x); 
  strcat(databuf, ",");
  acc2_y = dtostrf(packet.acc2[1],3,2,charbuf);
  strcat(databuf, acc2_y); 
  strcat(databuf, ",");
  acc2_z = dtostrf(packet.acc2[2],3,2,charbuf);
  strcat(databuf, acc2_z); 
  strcat(databuf, ",");

  gyro_x = dtostrf(packet.gyro[0],3,2,charbuf);
  strcat(databuf, gyro_x); 
  strcat(databuf, ",");
  gyro_y = dtostrf( packet.gyro[1],3,2,charbuf);
  strcat(databuf, gyro_y); 
  strcat(databuf, ",");
  gyro_z = dtostrf(packet.gyro[2],3,2,charbuf);
  strcat(databuf, gyro_z); 
  strcat(databuf, ",");

  voltage_c = dtostrf(packet.voltage,3,2,charbuf);
  strcat(databuf,  voltage_c); 
  strcat(databuf, ",");
  current_c = dtostrf( packet.current,3,2,charbuf);
  strcat(databuf, current_c); 
  strcat(databuf, ",");
  power_c = dtostrf( packet.power,3,2,charbuf);
  strcat(databuf, power_c ); 
  strcat(databuf, ","); 
  energy_c = dtostrf( packet.energy,3,2,charbuf);
  strcat(databuf, energy_c  );



  int len = strlen(databuf);
        for (int i = 0; i < len; i++) 
    Serial.print(databuf[i]);
 
     Serial.println();
 
}
void setup()
{
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(115200);  // start serial for output
  Serial1.begin(115200); //serial for gpio connection between Mega and Rpi
  
  // Initializing sensors 
  sensorA.initialize();
  sensorB.initialize();
  sensorC.initialize();

  // Testing connection by reading device ID of each sensor
  // Returns false if deviceID not found, Returns true if deviceID is found
  Serial.println();
  Serial.println(sensorA.testConnection() ? "Sensor A connected successfully" : "Sensor A failed to connect");
  Serial.println(sensorB.testConnection() ? "Sensor B connected successfully" : "Sensor B failed to connect");
  Serial.println(sensorC.testConnection() ? "Sensor C connected successfully" : "Sensor C failed to connect");
  
  calibrateSensors();
  //handshake();
  xTaskCreate(mainTask, "Main Task", STACK_SIZE, (void *)NULL, 2, NULL);
} 


void loop()
{  
}
 

