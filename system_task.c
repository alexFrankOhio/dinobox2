/*
 * system_task.c
 *
 *  Created on: Apr 17, 2019
 *      Author: Alex Frank Thompson
 */

//*****************************************************************************
//
//! \addtogroup out_of_box
//! @{
//
//*****************************************************************************

/* standard includes */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <file.h>
#include <stdbool.h>
#include <stddef.h>



/* TI-DRIVERS Header files */
#include <Board.h>
#include <uart_term.h>
#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SDFatFS.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/ADC.h>

#include <pthread.h>


/* Example/Board Header files */
#include "link_local_task.h"
#include "provisioning_task.h"
#include "out_of_box.h"
#include "ota_archive.h"
#include "system_task.h"

/* driverlib Header files */
#include <ti/devices/cc32xx/inc/hw_memmap.h>
#include <ti/devices/cc32xx/inc/hw_gprcm.h>

#include <third_party/fatfs/ffcio.h>


/* External sensor Drivers*/
#include "ccs811.h"
#include "bme280.h"

#ifndef CPY_BUFF_SIZE
#define CPY_BUFF_SIZE       2048
#endif

/* String conversion macro */
#define STR_(n)             #n
#define STR(n)              STR_(n)

/* Drive number used for FatFs */
#define DRIVE_NUM           0

//const char systemSetupFile[] = "fat:"STR(DRIVE_NUM)":system.csv";
//const char logFile[] = "fat:"STR(DRIVE_NUM)":dataLog.csv";
//const char warningfile[] = "fat:"STR(DRIVE_NUM)":warnings.csv";
const char checkFileName[] = "fat:"STR(DRIVE_NUM)":check.txt";


const struct timespec ts = {
    .tv_sec = 1556586141,
    .tv_nsec = 0
};

char fatfsPrefix[] = "fat";

unsigned char cpy_buff[CPY_BUFF_SIZE + 1];


/*global Variables */
Actuator_State Lights_State = Device_Off;
Actuator_State Fan_State= Device_Off;
Actuator_State Peltier_State= Device_Off;
ScheduleTime Sched_Lights_ON ={8,20};
ScheduleTime Sched_Lights_OFF={20,20};
int32_t goalTemp = 2000;//20 degrees C
int32_t tempMargin =100;// margin of 1 degree C

int16_t dataFreq;
uint32_t secondCount=0;
SlDateTime_t lastDump;
SlDateTime_t lastCheckin;
extern SlDateTime_t dateTime;


/*external environmental Variables for Control System*/
extern int32_t tempIn;
extern uint16_t airQuality;
extern uint32_t presIn;
extern uint32_t humidIn;
extern uint16_t oxygen;
extern float temperatureVal;



const char message[] = "pleas";





const char dataFileHeader[] = "Time,Date(dd/mm/yyyy),inside Temp,inside press,inside humid,outside Temp, O2(ppm),eCO2,lights,fans,cooler,connection\r\n";
char dataBuffer[] =          "00:00,00/00/0000,00.0°C,000000Pa,00.0%,00.0°C,000000ppmm,000000ppm,off,off,off,off\r\n";

const char sysFileHeader[] = "check in time,check in date,goal temp,log Freq (min),light on time, light off time\r\n";
char sysFileBuffer[] =      "00:00,00/00/0000,23.0°C,005,20:00,08:00";

unsigned char cpy_buff[CPY_BUFF_SIZE + 1];
SDFatFS_Handle sdfatfsHandle;


FILE_INFO FileList[File_End] ={
     { systemFilename,systemHeader, systemLineBuffer},
     { dataFilename, dataHeader, dataLineBuffer},
     {warningFilename,warningHeader,warningLineBuffer},
};



FILE *sys, *data, *error;
FILE *sdCard[File_End];
unsigned int bytesRead = 0;
unsigned int bytesWritten = 0;
unsigned int filesize;
unsigned int totalBytesCopied = 0;



void * System_Task(void *pvParameters){
    int32_t status;


    SDFatFS_init();

    add_device(fatfsPrefix, _MSA, ffcio_open, ffcio_close, ffcio_read,
            ffcio_write, ffcio_lseek, ffcio_unlink, ffcio_rename);

    sdfatfsHandle = SDFatFS_open(Board_SDFatFS0, DRIVE_NUM);

    if (sdfatfsHandle == NULL) {
        UART_PRINT( "Error starting the SD card\r\n");
    }
    else {
        UART_PRINT( "Drive %u is mounted\r\n", DRIVE_NUM);
    }

    status = Establish_Connection();
    if(status){
        UART_PRINT( "Failed to establish connection to sd Card\r\n");
        //error function call
    }
    else{
        UART_PRINT( "established Connection\r\n");
    }



    status = initializeFiles(NULL);
    if(status !=-1){
        UART_PRINT( "File Initialization failed at %s\r\n", FileList[status].filename);
        //error function call
    }


    TickType_t systemStartTime;

     const TickType_t systemTick = 1000;//1 second
     systemStartTime = xTaskGetTickCount();

    _u16 configLen = sizeof(SlDateTime_t);
    _u8 configOpt = SL_DEVICE_GENERAL_DATE_TIME;


   // while(provisioningStop());
    UART_PRINT(FileList[File_System].lineBuf);
    status = loadSystem();
    if(status){
        if(status ==-1){
            UART_PRINT(
                             "failed to initialize system file\n\r");
        }
        if(status ==-2){
            UART_PRINT(
                             "End of File Error\n\r");
        }
        if(status ==-3){
            UART_PRINT(
                             "Copy Error\n\r");
        }
        if(status ==-4){
            UART_PRINT(
                             "created new system file\n\r");
        }
    }
    else{
        UART_PRINT("read system File\r\n");
        UART_PRINT(FileList[File_System].lineBuf);
        UART_PRINT("\r\n");
        UART_PRINT("goalTemp:%d,logFreq%3.3d,LightOn: %2.2d:%2.2d,LightOff: %2.2d:%2.2d",
                   goalTemp,dataFreq,Sched_Lights_ON.hour,Sched_Lights_ON.minute,
                   Sched_Lights_OFF.hour, Sched_Lights_OFF.minute);
        UART_PRINT("\r\n");
    }


    UART_PRINT("made it to loop\r\n");
    sl_DeviceGet(SL_DEVICE_GENERAL,&configOpt,&configLen,(_u8 *)(&dateTime));
     while(1){

         vTaskDelayUntil(&systemStartTime,systemTick);


         sl_DeviceGet(SL_DEVICE_GENERAL,&configOpt,&configLen,(_u8 *)(&dateTime));



         /* Read BME, CCS811 and 02 sensor values */
         status = BME280Reading();

         if(status != 0)
         {
             UART_PRINT(
                 "[Link local task] Failed to read data from BME280\n\r");
         }


         status = oxySensorReading();
         if(status != 0){
             UART_PRINT(
                 "[Link local task] Failed to read oxygenSensor\n\r");
         }
         status = ccs811Reading();
         if(status != 0)
         {
             UART_PRINT(
                 "[Link local task] Failed to read or calibrate CCS811\n\r");
         }
         status = temperatureReading();
         if(status != 0)
         {
             UART_PRINT(
                 "[Link local task] Failed to"
                 " read data from temperature sensor\n\r");
         }


         if(sched_GreaterThan(Sched_Lights_ON,Sched_Lights_OFF) == 1){

             if((sched_GreaterThan_Time(Sched_Lights_ON,dateTime)==-1)||(sched_GreaterThan_Time(Sched_Lights_OFF,dateTime)==1)){
                 Lights_State= Device_On ;
             }
             else Lights_State= Device_Off;


         }else if(sched_GreaterThan(Sched_Lights_ON,Sched_Lights_OFF) == -1) {
             if((sched_GreaterThan_Time(Sched_Lights_OFF,dateTime)==-1)||(sched_GreaterThan_Time(Sched_Lights_ON,dateTime)==1)){
                 Lights_State= Device_Off  ;
             }
             else Lights_State= Device_On;

         }else{
             //create error
         }

         if(tempIn>=((goalTemp+tempMargin))){
             if(Peltier_State == Device_Off){
                    UART_PRINT("turning Cooling ON Goal Temp: %d actual temp: %d \r\n",goalTemp,tempIn);
                    Peltier_State = Device_On;
             }
             else{
                    UART_PRINT("Cooling ON Goal Temp: %d actual temp: %d \r\n",goalTemp,tempIn);
             }

         }

         else if(tempIn<=((goalTemp-tempMargin))){

             if(Peltier_State ==Device_On){
                   UART_PRINT("turning cooling OFF Goal Temp: %d actual temp: %d \r\n",goalTemp,tempIn);
                   Peltier_State = Device_Off;
             }
             else{
                   UART_PRINT("cooling OFF Goal Temp: %d actual temp: %d \r\n",goalTemp,tempIn);
             }

         }
         else{
             UART_PRINT("At Temp. cooling %s Goal Temp: %d actual temp: %d temp Margin: %d\r\n",
                         ((Peltier_State==1) ? "ON" : "OFF"  ),goalTemp,tempIn, tempMargin);
         }




         switch (Lights_State){
         case Device_On:
             GPIO_write(Board_GPIO_Lights,Board_GPIO_LED_ON);
             break;
          case Device_Toggle://need to add variable for whether the pin is ACTUALLY high or low, vs what the device has been set
             GPIO_toggle(Board_GPIO_Lights);
             break;
          case Device_Off:
             GPIO_write(Board_GPIO_Lights,Board_GPIO_LED_OFF);
             break;


         }


         switch (Peltier_State){
         case Device_On:
             GPIO_write(Board_GPIO_LeftPelt,Board_GPIO_LED_ON);
             GPIO_write(Board_GPIO_RightPelt,Board_GPIO_LED_ON);
             break;
          case Device_Toggle://need to add variable for whether the pin is ACTUALLY high or low, vs what the device has been set
             GPIO_toggle(Board_GPIO_RightPelt);
             GPIO_toggle(Board_GPIO_LeftPelt);

             break;
          case Device_Off:
             GPIO_write(Board_GPIO_RightPelt,Board_GPIO_LED_OFF);
             GPIO_write(Board_GPIO_LeftPelt,Board_GPIO_LED_OFF);
             break;


         }


         UART_PRINT("Year %d,Hour %d,Min %d,Sec %d leds: %d Peltier: %d  Temp: %d\r\n",dateTime.tm_year,
                             dateTime.tm_hour,dateTime.tm_min,dateTime.tm_sec, Lights_State, Peltier_State,tempIn);
         if(secondCount>=60*dataFreq){
             status = updateData();
             if(status){
                 UART_PRINT("could not update data. make sure sdcard is inserted\n\r");
                 UART_PRINT("all data is also streamed over USB if a logger is available\n\r");
             }
             else secondCount = 0;
         }

         secondCount++;
     }



}
/**************8*sched_GreaterThan***********************/
/*
 * a, first time
 * b, second time
 * ret: 1 if a is later than b
 * ret 0 if a = b
 * ret -1 if a is earlier that b

*/
int8_t sched_GreaterThan(ScheduleTime a, ScheduleTime b){
    if(a.hour>b.hour){
        //UART_PRINT("time a = %d:%d time b = %d:%d sched type %d \r\n",a.hour,a.minute,b.hour,b.minute,1);
        return  1;
    }
    if(a.hour<b.hour){
      //  UART_PRINT("time a = %d:%d time b = %d:%d sched type %d \r\n",a.hour,a.minute,b.hour,b.minute,-1);
        return -1;
    }
    if(a.minute > b.minute){
    //    UART_PRINT("time a = %d:%d time b = %d:%d sched type %d \r\n",a.hour,a.minute,b.hour,b.minute,1);
        return  1;
    }
    if(a.minute < b.minute){
  //      UART_PRINT("time a = %d:%d time b = %d:%d sched type %d \r\n",a.hour,a.minute,b.hour,b.minute,-1);
        return -1;
    }
//    UART_PRINT("time a = %d:%d time b = %d:%d sched type %d \r\n",a.hour,a.minute,b.hour,b.minute,0);
    return 0;

}
int8_t sched_GreaterThan_Time(ScheduleTime a, SlDateTime_t b){
    if(a.hour>b.tm_hour){
     //   UART_PRINT("time a = %d:%d time b = %d:%d  time dif %d \r\n",a.hour,a.minute,b.tm_hour,b.tm_min,1);
        return  1;
    }
    if(a.hour<b.tm_hour){
    //    UART_PRINT("time a = %d:%d time b = %d:%d time dif %d \r\n",a.hour,a.minute,b.tm_hour,b.tm_min,-1);
        return -1;
    }
    if(a.minute > b.tm_min) {
     //   UART_PRINT("time a = %d:%d time b = %d:%d time dife %d \r\n",a.hour,a.minute,b.tm_hour,b.tm_min,1);
        return  1;
    }
    if(a.minute < b.tm_min){
    //    UART_PRINT("time a = %d:%d time b = %d:%d time dife %d \r\n",a.hour,a.minute,b.tm_hour,b.tm_min,-1);
        return -1;
    }
 //   UART_PRINT("time a = %d:%d time b = %d:%d time dif %d \r\n",a.hour,a.minute,b.tm_hour,b.tm_min,1);
    return 0;
}
int32_t fatfs_getFatTime(void)
{

    time_t seconds;
    uint32_t fatTime;
    struct tm *pTime;

    /*
     *  TI time() returns seconds elapsed since 1900, while other tools
     *  return seconds from 1970.  However, both TI and GNU localtime()
     *  sets tm tm_year to number of years since 1900.
     */
    seconds = time(NULL);

    pTime = localtime(&seconds);

    /*
     *  localtime() sets pTime->tm_year to number of years
     *  since 1900, so subtract 80 from tm_year to get FAT time
     *  offset from 1980.
     */
    fatTime = ((uint32_t)(pTime->tm_year - 80) << 25) |
        ((uint32_t)(pTime->tm_mon) << 21) |
        ((uint32_t)(pTime->tm_mday) << 16) |
        ((uint32_t)(pTime->tm_hour) << 11) |
        ((uint32_t)(pTime->tm_min) << 5) |
        ((uint32_t)(pTime->tm_sec) >> 1);


    UART_PRINT("System Time Info: %u\r\n",fatTime);
    return ((int32_t)fatTime);
}

int32_t initializeFiles(Current_File file){

    Current_File i = File_System;

    if(file ==NULL){
        while(i != File_End){


            sdCard[i] = fopen(FileList[i].filename, "r");

              if (!sdCard[i]) {
                      UART_PRINT( "Creating  File \"%s\"...",
                                  FileList[i].filename);

                      /* Open file for both reading and writing */
                      sdCard[i] = fopen(FileList[i].filename, "w+");
                      if (!sdCard[i]) {
                          UART_PRINT(
                              "Error: \"%s\" could not be created.\r\nPlease check the "
                              "Board.html if additional jumpers are necessary.\r\n",
                              FileList[i].filename);
                          UART_PRINT( "Aborting...\r\n");
                          return i;
                      }

                      fwrite(FileList[i].header, 1, strlen(FileList[i].header), sdCard[i]);
                      fwrite(FileList[i].lineBuf, 1, strlen(FileList[i].lineBuf), sdCard[i]);
                      fflush(sdCard[i]);

                      /* Reset the internal file pointer */
                      rewind(sdCard[i]);

                      UART_PRINT( "generated new file\r\n");
              }
              else {
              if(i== File_System){

              }
              //no time for warning checking
        //read sys file and load system parameters
              }
              fclose(sdCard[i]);
              i++;

        }
    }
    else{
        sdCard[file] = fopen(FileList[file].filename, "r");

          if (!sdCard[file]) {
                  UART_PRINT( "Creating  File \"%s\"...",
                              FileList[file].filename);

                  /* Open file for both reading and writing */
                  sdCard[file] = fopen(FileList[file].filename, "w+");
                  if (!sdCard[file]) {
                      UART_PRINT(
                          "Error: \"%s\" could not be created.\r\nPlease check the "
                          "Board.html if additional jumpers are necessary.\r\n",
                          FileList[file].filename);
                      UART_PRINT( "Aborting...\r\n");
                      return file;
                  }

                  fwrite(FileList[file].header, 1, strlen(FileList[file].header), sdCard[file]);
                  fwrite(FileList[file].lineBuf, 1, strlen(FileList[file].lineBuf), sdCard[file]);
                  fflush(sdCard[file]);

                  /* Reset the internal file pointer */
                  rewind(sdCard[file]);

                  UART_PRINT( "generated new file\r\n");
          }
          else {
          //no time for warning checking
    //read sys file and load system parameters
          }
          fclose(sdCard[file]);

    }
    return -1;
}

int32_t updateData(){

    _i16 Status;
    _u8 NumConnectedStations;
    _u16 ValueLen = sizeof(_u8);

    Status = sl_NetCfgGet(SL_NETCFG_AP_STATIONS_NUM_CONNECTED, NULL, &ValueLen,
    &NumConnectedStations);
    if( Status )
    {
    NumConnectedStations = 0;
    }

    sdCard[File_Data] = fopen(FileList[File_Data].filename, "a");
    if (!sdCard[File_Data]) {
        Status = initializeFiles(File_Data);
        if(Status !=-1){
            return -1;
        }
    }
    else{

    }

    sprintf(FileList[File_Data].lineBuf,
            "%.2u:%.2u,%.2u/%.2u/%.4u,%.2u.%.1u,%.6u,%.2u,%4.1f,%.6u,%.6u,%3.3s,%3.3s,%3.3s,%3.3s\r\n",
            dateTime.tm_hour,dateTime.tm_min,dateTime.tm_mon,dateTime.tm_day,dateTime.tm_year,
            (tempIn/100),((tempIn%100)/10),presIn,humidIn,temperatureVal,oxygen,airQuality,
            StatePrint(Lights_State),StatePrint(Fan_State),StatePrint(Peltier_State),StatePrint(NumConnectedStations));
    fwrite(FileList[File_Data].lineBuf, 1, strlen(FileList[File_Data].lineBuf), sdCard[File_Data]);
    fflush(sdCard[File_Data]);

    fclose(sdCard[File_Data]);
    UART_PRINT(FileList[File_Data].lineBuf);
    return 0;
}

int32_t loadSystem(){
    _i16 Status;
    _u16 bufferSize = strlen(FileList[File_System].lineBuf);
    sdCard[File_System] = fopen(FileList[File_System].filename, "r");
    if (!sdCard[File_System]) {
        Status =initializeFiles(File_System);
        if(Status !=-1){
            return -1;
        }
        return -4;
    }
    bytesRead = fread(cpy_buff, 1, strlen(FileList[File_System].header),sdCard[File_System]);
    if (bytesRead == 0) {
        return -2; /* Error or EOF */
    }
    if (bytesRead != strlen(FileList[File_System].header)) {
        return -3; /* Error reading file*/
    }
    UART_PRINT("fileHeader\r\n");
    UART_PRINT(cpy_buff);
    bytesRead = fread(cpy_buff, 1, strlen(FileList[File_System].lineBuf),sdCard[File_System]);
    if (bytesRead == 0) {
        return -2; /* Error or EOF */
    }
    if (bytesRead < strlen(FileList[File_System].lineBuf)) {
        return -3; /* Error reading file*/
    }
    UART_PRINT("lineRead\r\n");
    UART_PRINT(cpy_buff);
    snprintf(FileList[File_System].lineBuf,strlen(FileList[File_System].lineBuf),"%s",cpy_buff);
    parseSysData(cpy_buff);
    return 0;

}

int32_t updateWarning(){
    return 0;
}

int32_t parseSysData(char *line){

char *pEnd = (line + 17);

long int GT = 0, LonThr, LonTmin, LoffThr, LoffTmin;

GT += (strtol ((line + 17),&pEnd,10))*100;
GT += (strtol ((line + 20),&pEnd,10))*10;
goalTemp = GT;

dataFreq = strtol ((line + 22),&pEnd,10);
LonThr = strtol ((line + 26),&pEnd,10);
LonTmin = strtol ((line + 29),&pEnd,10);
LoffThr = strtol ((line + 32),&pEnd,10);
LoffTmin = strtol ((line + 35),&pEnd,10);


Sched_Lights_ON.hour =LonThr;
Sched_Lights_ON.minute =LonTmin;

Sched_Lights_OFF.hour =LoffThr;
Sched_Lights_OFF.minute =LoffTmin;

return 0;//with more time there would be error correction.
}

int32_t Establish_Connection(){
    int32_t Status=0;
    FILE *checkFile;
    while(Status<4){
        checkFile = fopen(checkFileName, "w");
            if (!checkFile) {
                Status++;
            }
            else{
                fclose(checkFile);
                remove(checkFileName);
                return 0;
            }
        vTaskDelay(50);
    }
    return -1;
}

