#ifndef arduino_HEADER_INCLUDED
	#define arduino_HEADER_INCLUDED
	#include <Arduino.h>
#endif

#ifndef log_HEADER_INCLUDED
	#define log_HEADER_INCLUDED

	#define __SHORT_FILE__ \
			(strrchr(__FILE__,'\') \
			? strrchr(__FILE__,'\')+1 \
			: __FILE__ \
)

/*	#define logdAssert(severity, ...) {sprintf (logstr, "%u %u in function %s, file %s, line %u: ", xTaskGetTickCount(), severity,__FUNCTION__,  __SHORT_FILE__, __LINE__);\
                                       Serial.print(logstr);\
                                       sprintf (logstr,__VA_ARGS__);\
                                       Serial.print(logstr);\
									   Serial.print("\n");}*/

#define logdAssert(severity, ...) {sprintf (logstr, "%u %u in function %s, line %u: ", xTaskGetTickCount(), severity,__FUNCTION__, __LINE__-3);\
                                       Serial.print(logstr);\
                                       sprintf (logstr,__VA_ARGS__);\
                                       Serial.print(logstr);\
									   Serial.print("\n");}

	#define _DEBUG_ 255
    #define _PANIC_ 4
	#define _CRITICAL_ 3
	#define _ERR_ 2
	#define _WARN_ 1
	#define _INFO_ 0

	

//	void logAssertFunc(TickType_t TickCount, const char* functionName, const char* file, const int line, uint8_t severity, ...);
#endif