/*
 * system_task.h
 *
 *  Created on: Apr 17, 2019
 *      Author: Alex Frank Thompson
 */

#ifndef SYSTEM_TASK_H_
#define SYSTEM_TASK_H_

#define Fan_Pin         Board_GPIO_00
#define Peltier_Pin     Board_GPIO_08
#define Lights_Pin      Board_GPIO_22


/* The FF_USE_LFN switches the support for LFN (long file name).*/

typedef enum{
    Device_Off,
    Device_On,
    Device_Toggle,
}Actuator_State;

typedef enum{
    File_System=0,
    File_Data,
    File_Warnings,
    File_End,
}Current_File;

//possibly make these const instead of defines.

#define systemFilename "fat:"STR(DRIVE_NUM)":system.txt"
#define dataFilename "fat:"STR(DRIVE_NUM)":dataLog.csv"
#define warningFilename "fat:"STR(DRIVE_NUM)":warnings.csv"

#define systemHeader      "check in time(hh:mm),check in date(mm/dd/yyyy),goal temp(°C),"\
                          "log Freq (min),light on time(hh:mm), light off time(hh:mm)\r\n"

#define dataHeader        "Time(HH:MM),Date(dd/mm/yyyy),inside Temp(°C),inside press(Pa),"\
                          "inside humid(%),outside Temp(°C), O2(ppm),eCO2(ppm),lights,fans,cooler,connection\r\n"

#define warningHeader     "Errors and warnings will be listed below, including date and time of occurrence\r\n"\
                          "Time,Date(dd/mm/yyyy),inside Temp(°C),inside press(Pa),inside humid(%),outside Temp(°C),"\
                          "O2(ppm),eCO2(ppm),lights,fans,cooler,connection, error\r\n"


#define dataLineBuffer    "00:00,00/00/0000,00.0,000000,00.0,00.0,000000,000000,off,off,off,off\r\n"
#define systemLineBuffer  "00:00,00/00/0000,23.0,001,20:00,08:00"
#define warningLineBuffer "00:00,00/00/0000,00.0,000000,00.0,00.0,000000,000000,off,off,off,off," \
                          "Example Error message: this is NOT an error, just an example of the structure of errors"

typedef struct{
    char *filename;
    char *header;
    char *lineBuf;

}FILE_INFO;



typedef struct{
    _u32 hour;
    _u32 minute;
}ScheduleTime;

/*    error codes       */

#define SD_OPENFAIL  0x01
#define BME280FAIL   0x02
#define CCS811FAIL   0x04
#define SD_WRITEFAIL 0x08



#define  StatePrint(val)       ((val==0)? "off":"on")





void * System_Task(void *pvParameters);


#endif /* SYSTEM_TASK_H_ */
