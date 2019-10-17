/* ************************************************************************************************* *
 * (c) Copyright 2019 Jonas Bjurel (jonasbjurel@hotmail.com)                                         *
 *                                                                                                   *
 * Licensed under the Apache License, Version 2.0 (the "License");                                   *
 * you may not use this file except in compliance with the License.                                  *
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0                *
 *                                                                                                   *
 * Unless required by applicable law or agreed to in writing, software distributed under the License *
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express- *
 * or implied.                                                                                       *
 * See the License for the specific language governing permissions and limitations under the         *
 * licence.                                                                                          *
 *                                                                                                   *
 * ************************************************************************************************* */

/* ************************************************************************************************* *
 * Project:		        coreNoStopRTOS/www.......                                                    *
 * Name:				init.h                                                                       *
 * Created:			    2019-09-27                                                                   *
 * Originator:			Jonas Bjurel                                                                 *
 * Short description:	Provides a FreeRtos shim-layer for nearly non-stop real time systems with    *
 *					    the prime use-case for model air-craft on-board systems.                     *
 * Resources:           github, WIKI .....                                                           *
 * ************************************************************************************************* */

#ifndef arduino_HEADER_INCLUDED
	#define arduino_HEADER_INCLUDED
	#include <Arduino.h>
#endif

#ifndef init_HEADER_INCLUDED
#define init_HEADER_INCLUDED
	#include "log.h"
	struct task_resource_t {
		task_resource_t* prevResource_p;
		void* taskResource_p;
		task_resource_t* nextResource_p;
	};

	struct task_desc_t {
		TaskFunction_t pvTaskCode;
		char pcName[20]; // A descriptive name for the task
		uint32_t usStackDepth; //The size of the task stack specified as the number of bytes [1024]
		void* pvParameters; //Pointer that will be used as the parameter for the task being created
		UBaseType_t uxPriority; //The priority at which the task should run. 0-3 where 3 is the highest
		TaskHandle_t pvCreatedTask; //Created task handle
		BaseType_t xCoreID; //Afinity to core (0|1|tskNO_AFFINITY)
		int WatchdogTimeout; //Watchdog timeout in ms*10, 0 means no watch dog
		uint8_t watchdogHighWatermarkWarnPerc; // Warn watchdog high watermark in % [60]
		uint8_t stackHighWatermarkWarnPerc; // Warn watchdog high watermark in % [60]
		int heapHighWatermarkKBWarn; // Warn level for the heap in KB.
		uint8_t escalationRestartCnt; // Number of task restarts escalating to a system restart, 0 means never [3]

		// Below: dynamic task data, do not edit
		int watchDogTimer;
		SemaphoreHandle_t taskWatchDogMutex;
		bool watchdogWarn;
		int watchdogHighWatermarkAbs;
		int watchdogHighWatermarkWarnAbs;
		int heapCurr;
		int heapHighWatermarkAbs;
		int heapHighWatermarkWarnAbs;
		bool heapWarn;
		int stackHighWatermarkAbs;
		int stackHighWatermarkWarnAbs;
		uint8_t restartCnt;
		task_resource_t* dynMemObjects_p;
	};

	#define _WATCHDOG_DISABLE_ INT_MAX //Applied to task_desc_t.watchDogTimer to temporallily disable the watchdog (eq. to 2.55 seconds) t

/* ********************************************************************************************* */
/* tasks class definitions                                                                       */
/* Type: Platform Helpers                                                                        */
/* Description: TODO	                                                                         */

/* tasks methods Return codes                                                                    */
	#define _TASKS_OK_ 0
	#define _TASKS_RESOURCES_EXHAUSTED_ 1
	#define _TASKS_API_PARAMETER_ERROR_ 2
	#define _TASKS_GENERAL_ERROR_ 3
	#define _TASKS_NOT_FOUND_ 4
	#define _TASKS_MUTEX_TIMEOUT_ 5

	#define _TASK_EXIST_ 0
	#define _TASK_DELETED_ 1

	#define _TASK_WATCHDOG_MS_ 10

	class tasks {
	public:
		tasks();
		~tasks();
		static void startInit(void);
		void kickTaskWatchdogs(task_desc_t* task_p);
		void taskPanic(task_desc_t* task);
		void* taskMalloc(task_desc_t* task_p, int size);
		uint8_t taskMfree(task_desc_t* task_p, void* mem_p);
		uint8_t getTidByTaskDesc(uint8_t* tid, task_desc_t* task);

	private:
		//private variables:
		static bool tasksObjExist; //Only one tasks object may be instantiated
		static task_desc_t* classTasktable[];
		static SemaphoreHandle_t globalInitMutex;
		static TimerHandle_t watchDogTimer;
		static int watchdogOverruns; //TODO: needs to be initialized
		static int watchdogTaskCnt; //TODO: needs to be initialized
		static int hunderedMsCnt; //TODO: needs to be initialized
		static int secCnt; //TODO: needs to be initialized
		static int tenSecCnt; //TODO: needs to be initialized
		static int minCnt; //TODO: needs to be initialized
		static int hourCnt; //TODO: needs to be initialized
		static SemaphoreHandle_t watchdogMutex;

		//private methods:
		
		uint8_t startTaskByName(char* pcName, bool incrementRestartCnt, bool initiateStat); //pcName: The human reading task name, initiateStat:[_TASKS_INITIATE_STATS_ | _TASKS_DONOT_INITIATE_STATS_]
		uint8_t startTaskByTid(uint8_t tid, bool incrementRestartCnt, bool initiateStat); //tid: task identifier/tasks index, initiateStat:[_TASKS_INITIATE_STATS_ | _TASKS_DONOT_INITIATE_STATS_]
		uint8_t stopTaskByName(char* pcName); //pcName: The human reading task name
		uint8_t stopTaskByTid(uint8_t tid); //tid: task identifier/tasks index,
		uint8_t getTaskDescByName(char* pcName, task_desc_t** task); //pcName: The human reading task name , task* pointer to resulting task descriptor
		uint8_t getTaskDescByTID(uint8_t tid, task_desc_t** task); //
		uint8_t getStalledTasks(uint8_t* tid, task_desc_t* task);
		uint8_t startAllTasks(bool incrementRestartCnt, bool initiateStat);
		uint8_t stopAllTasks();
		uint8_t clearStats(task_desc_t* task);
		uint8_t startTaskByTaskDesc(task_desc_t* task, bool incrementRestartCnt, bool initiateStat);
		uint8_t stopTaskByTaskDesc(task_desc_t* task);
		uint8_t checkIfTaskAlive(task_desc_t* task);
		uint8_t globalInitMutexTake(TickType_t timeout = portMAX_DELAY);
		uint8_t globalInitMutexGive();
		uint8_t taskInitMutexTake(task_desc_t* task, TickType_t timeout = portMAX_DELAY);
		uint8_t taskInitMutexGive(task_desc_t* task);
		static void runTaskWatchdogs(TimerHandle_t xTimer);
		uint8_t taskResourceLink(task_resource_t** taskResourceRoot_pp, void* taskResourceObj_p);
		uint8_t taskResourceUnLink(task_resource_t** taskResourceRoot_pp, void* taskResourceObj_p);
		uint8_t findResourceDescByResourceObj(void* taskResourceObj_p, task_resource_t* taskResourceRoot_p, task_resource_t** taskResourceDesc_pp);
		uint8_t taskMfreeAll(task_desc_t* task_p);
		void incHeapStats(task_desc_t* task_p, int size);
		void decHeapStats(task_desc_t* task_p, int size);
	};

	extern tasks initd;
	extern char logstr[320]; //TODO: Move to log.c and log.h

#endif