#include <power.h>
#include <MPU6050.h>
#include <I2Cdev.h>
#include <ADXL345.h>
#include <Wire.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>

#define STACK_SIZE 500
#define DEVICE_A_ACCEL (0x53)    //first ADXL345 device address
#define DEVICE_B_ACCEL (0x1D)    //second ADXL345 device address
#define DEVICE_C_GYRO (0x68) // MPU6050 address
#define TO_READ (6)        //num of bytes we are going to read each time
#define voltageDividerPin 0
#define currentSensorPin 1
#define RS 0.1
#define RL 10000
#define PKT_SIZE 2

ADXL345 sensorA = ADXL345(DEVICE_A_ACCEL);
ADXL345 sensorB = ADXL345(DEVICE_B_ACCEL);
MPU6050 sensorC = MPU6050(DEVICE_C_GYRO);
TickType_t xLastWakeTime;

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


//Structure of data packet
typedef struct Packet {
  float gyro[3];
  float acc1[3];
  float acc2[3];
  float power;
  float current;
  float voltage;
  float energy;
} Packet;   

//16 bit integer values for gyroscope readings
int16_t xg_raw, yg_raw, zg_raw;
Packet packet;


char databuf[2800];
char checksum_c[40];
int checkSum;
int F_checkSum;

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

int ledflag = HIGH;
int countLED = 0;

  int h_flag = 0;
  int n_flag = 0;

int i;
  
/**
 * Main Task
 */
void mainTask(void *p) {
  
  while(1){
    xLastWakeTime = xTaskGetTickCount();
    for (i=0;i <PKT_SIZE; i++) {
    getData(); 
    changeFormat();
    vTaskDelayUntil(&xLastWakeTime, (20/ portTICK_PERIOD_MS));
    }

   
    for (i=0; i < strlen(databuf) ; i++){
    checkSum += int(databuf[i]);
    }
     Serial.print(checkSum);
     Serial.print("---");
     Serial.println();

//     F_checkSum = (int)checkSum;
//     Serial.print(F_checkSum);
//     Serial.println();

  itoa(checkSum, checksum_c, 10); 
  strcat(databuf, checksum_c);
  Serial.print(checksum_c);
  strcat(databuf, "\r");

    Serial.print("Entering");
   for (i = 0; i < strlen(databuf); i++) {
   Serial1.write(databuf[i]);
   //Serial.print(databuf[i]);
   }
   Serial.println();

     strcpy(databuf, "");
     checkSum =0;
     
  }
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
      packet.current = ((vOut * 1000) / (RS * RL));

      //Power is in mW due to current being in mA
      packet.power = packet.current * packet.voltage;

      static long prevTime = 0;
      static float energy = 0;
      
      //Power / 1000.0 because converting mW to W
      //This allows joules to  be in W per seconds
      energy += ((millis()-prevTime) / (1000.0)) * packet.power;

      prevTime = millis();

      packet.energy = energy;
}

 /*
 * Data processing
 */
float remapVoltage(int volt) {
  return (5.0/1023) * volt;
}


/*
 * To process the raw data obtained from sensor reading
 */
void getScaledReadings() {
  sensorA.getAcceleration(&xa_raw, &ya_raw, &za_raw);
  packet.acc1[0] = (xa_raw)*scaleFactorAccel;
  packet.acc1[1] = (ya_raw)*scaleFactorAccel;
  packet.acc1[2] = (za_raw)*scaleFactorAccel;
  
  sensorB.getAcceleration(&xb_raw, &yb_raw, &zb_raw);
  packet.acc2[0] = (xb_raw)*scaleFactorAccel;
  packet.acc2[1] = (yb_raw)*scaleFactorAccel;
  packet.acc2[2] = (zb_raw)*scaleFactorAccel;
  
  sensorC.getRotation(&xg_raw, &yg_raw, &zg_raw);
  packet.gyro[0] = (xg_raw)*scaleFactorGyro;
  packet.gyro[1]= (yg_raw)*scaleFactorGyro;
  packet.gyro[2] = (zg_raw)*scaleFactorGyro;
 }

/**
 *  To perform handshake to ensure that communication between Rpi and Aduino is ready
 */
void handshake() {
   
  while (h_flag == 0) {
    if (Serial1.available()) {
      if ((Serial1.read() == 'H')) {
        h_flag = 1;
        Serial1.write('A');
      }
    }
  }

  while (n_flag == 0) {
    if (Serial1.available()) {
      if (Serial1.read() == 'N') {
        Serial.println("Handshake done");
        n_flag = 1;
      }
      else {
        Serial1.write('A');
      }
    }
  }

}

/** 
 Change the format of the data from float to string
 */
void changeFormat(){
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
 strcat(databuf, "\r");
}

void powerSavings() {
  // Initializing all analog pins that are not used to output pins
  // Turning the useless analog pins to digital pins and setting them to LOW
  // This should save a total of 5ma
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  pinMode(A4, OUTPUT);
  pinMode(A5, OUTPUT);
  pinMode(A6, OUTPUT);
  pinMode(A7, OUTPUT);
  pinMode(A8, OUTPUT);
  pinMode(A9, OUTPUT);
  pinMode(A10, OUTPUT);
  pinMode(A11, OUTPUT);
  pinMode(A12, OUTPUT);
  pinMode(A13, OUTPUT);
  pinMode(A14, OUTPUT);
  pinMode(A15, OUTPUT);

  digitalWrite(A2, LOW);
  digitalWrite(A3, LOW);
  digitalWrite(A4, LOW);
  digitalWrite(A5, LOW);
  digitalWrite(A6, LOW);
  digitalWrite(A7, LOW);
  digitalWrite(A8, LOW);
  digitalWrite(A9, LOW);
  digitalWrite(A10, LOW);
  digitalWrite(A11, LOW);
  digitalWrite(A12, LOW);
  digitalWrite(A13, LOW);
  digitalWrite(A14, LOW);
  digitalWrite(A15, LOW);

  // Changing all the rest of the digital pins that are not used to OUTPUT LOW
  // This should save ~9mA
  for(i = 0; i <=16; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
  for(i = 22; i <=53; i++) {  
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }

  // Turning off peripherals that are not in use
  // Timer1, 2 and 3 are for interrupts (attaching interrupts and detaching them, setting PWM etc)
  power_spi_disable(); 
  power_timer1_disable();
  power_timer2_disable();
  power_timer3_disable();
}

void setup()
{
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(115200);  // start serial for output
  Serial1.begin(115200); //serial for gpio connection between Mega and Rpi
  pinMode(LED_BUILTIN, OUTPUT);

  powerSavings();
  // Initializing sensors 
  sensorA.initialize();
  sensorB.initialize();
  sensorC.initialize();

  // Testing connection by reading device ID of each sensor
  // Returns false if deviceID not found, Returns true if deviceID is found
  //Serial.println();
  Serial.println(sensorA.testConnection() ? "Sensor A connected successfully" : "Sensor A failed to connect");
  Serial.println(sensorB.testConnection() ? "Sensor B connected successfully" : "Sensor B failed to connect");
  Serial.println(sensorC.testConnection() ? "Sensor C connected successfully" : "Sensor C failed to connect");
  
  // calibrateSensors();
  handshake();
  xTaskCreate(mainTask, "Main Task", STACK_SIZE, (void *)NULL, 2, NULL);
} 




void loop()
{  
}

