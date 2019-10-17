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

 /* ************************************************************************************************ *
  * Project:			 coreNoStopRTOS/www.......                                                   * 
  * Name:				 myTestTask.cpp                                                              *
  * Created:			 2019-09-27                                                                  *
  * Originator:		     Jonas Bjurel                                                                *
  * Short description:   Provides CI (Continous Integration) test stimuli for for coreNoStopRTOS     *
  *                      functions...                                                                *
  * Resources:           github, WIKI .....                                                          *
  * ************************************************************************************************ */
#include "myTestTask.h"

void myTestTask(task_desc_t* myTask) {
	uint8_t myTid;
	if (initd.getTidByTaskDesc(&myTid, myTask)){
		//initd.taskPanic();
	}

	switch (*((uint8_t*)(myTask->pvParameters))) {
	case 1:
		logdAssert(_INFO_, "Task: %s, Watchdog enabled, but not kicking it, memory leaking task", myTask->pcName);
		break;

	case 2:
		logdAssert(_INFO_, "Task: %s, Watchdog enabled, and kicking it", myTask->pcName);
		break;

	case 3:
		logdAssert(_INFO_, "Task: %s, Watchdog is enabled, and kicking it, heavy tight loop memory allocation/deallocation", myTask->pcName);
		break;

	case 4:
		logdAssert(_INFO_, "Task: %s, Watchdog is enabled, but task is self-terminated", myTask->pcName);
		break;

	default:
		break;
	}

	while (true) {
		while (true) {
			switch (*((uint8_t*)(myTask->pvParameters))) {
			case 1:
				{void* memory_forget_p = initd.taskMalloc(myTask, sizeof("allocating memory"));
				}
				vTaskDelay(1000);
				break;

			case 2:
				initd.kickTaskWatchdogs(myTask);
				vTaskDelay(10);
				break;

			case 3: {
					void* mem_p = initd.taskMalloc(myTask, 2048);
					for (int i = 0; i < 2048; i++) {
						*((char*)mem_p + i) = 255;
					}
					initd.taskMfree(myTask, mem_p);
					initd.kickTaskWatchdogs(myTask);
					vTaskDelay(1);
				}
				break;

			case 4:
				vTaskDelay(30000);
				logdAssert(_INFO_, "Task: %s, Killing my self", myTask->pcName);
				vTaskDelete(NULL);
				break;

			default:
				vTaskDelete(NULL);
				break;
			}
		}
	}
}