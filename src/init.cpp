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
 * Name:				init.cpp                                                                     *
 * Created:			    2019-09-27                                                                   *
 * Originator:			Jonas Bjurel                                                                 *
 * Short description:	Provides a FreeRtos shim-layer for nearly non-stop real time systems with    *
 *                      the prime use-case for model air-craft on-board systems.                     *
 * Resources:           github, WIKI .....                                                           *
 * ************************************************************************************************* */

#include "init.h"
#include "../ci/myTestTask.h"

tasks initd;

//Todo, define macros to register tasks, both in build time as well as in run time;
#define _NO_OF_TASKS_ 4
// TODO: Bélow is just temporary
const uint8_t task1num = 1;
const uint8_t* task1num_p = &task1num;
const uint8_t task2num = 2;
const uint8_t* task2num_p = &task2num;
const uint8_t task3num = 3;
const uint8_t* task3num_p = &task3num;
const uint8_t task4num = 4;
const uint8_t* task4num_p = &task4num;

const static task_desc_t taskTable[] = //TODO: make it distributable to the task definitions, as a macro? Only task definitions (not run time vars should be exposed), provide default values.
{ 
{ (TaskFunction_t)myTestTask, "myTestTask1", 2048, (void*)task1num_p, 2, NULL, tskNO_AFFINITY, 7000, 60, 60, 1, 10, 0, NULL, false, 0, 0, 0, 0, 0, false, 0, 0, 0, NULL},
{ (TaskFunction_t)myTestTask, "myTestTask2", 2048, (void*)task2num_p, 2, NULL, tskNO_AFFINITY, 5, 60, 60, 0, 0, 1, NULL, false, 0, 0, 0, 0, 0, false, 0, 0, 0, NULL },
{ (TaskFunction_t)myTestTask, "myTestTask3", 2048, (void*)task3num_p, 2, NULL, tskNO_AFFINITY, 5, 60, 60, 3, 0, 0, NULL, false, 0, 0, 0, 0, 0, false, 0, 0, 0, NULL },
{ (TaskFunction_t)myTestTask, "myTestTask4", 2048, (void*)task4num_p, 2, NULL, tskNO_AFFINITY, 50000, 60, 60, 0, 0, 0, NULL, false, 0, 0, 0, 0, 0, false, 0, 0, 0, NULL }
};

bool tasks::tasksObjExist;
task_desc_t* tasks::classTasktable[_NO_OF_TASKS_]; //TODO: move to tasks class ?
SemaphoreHandle_t tasks::globalInitMutex;  //TODO: move to tasks class ?
TimerHandle_t tasks::watchDogTimer; //TODO: move to tasks class ?
int tasks::watchdogOverruns; 
int tasks::watchdogTaskCnt; //TODO: move to tasks class ?
int tasks::hunderedMsCnt; //TODO: needs to be initialized - move to tasks class?
int tasks::secCnt; //TODO: needs to be initialized - move to tasks class?
int tasks::tenSecCnt; //TODO: needs to be initialized - move to tasks class?
int tasks::minCnt; //TODO: needs to be initialized - move to tasks class?
int tasks::hourCnt; //TODO: needs to be initialized - move to tasks class?
SemaphoreHandle_t tasks::watchdogMutex;
char logstr[320]; //TODO: move to log.c and log.h

tasks::tasks() {}

void tasks::startInit(void) {
	logdAssert(_INFO_, "Initializing initd...");
	sleep(1);
	vTaskStartScheduler();
	globalInitMutex = xSemaphoreCreateRecursiveMutex();
	for (uint8_t i = 0; i < _NO_OF_TASKS_; i++) {
		classTasktable[i] = (task_desc_t*)malloc(sizeof(task_desc_t));
		memcpy(classTasktable[i], &taskTable[i], sizeof(task_desc_t));
		classTasktable[i]->taskWatchDogMutex = xSemaphoreCreateRecursiveMutex(); //taskInitMutex.. not yet implemented
		classTasktable[i]->heapCurr = 0;
		classTasktable[i]->restartCnt = 0;
		//logdAssert(_DEBUG_, "classTasktable [%u](%p) pointer assigned: %p", i, &classTasktable[i], classTasktable[i]); 
		//logdAssert(_INFO_, "classTasktable[%u] initiated, TaskName: %s, Stack: %u, Prio: %u", i, classTasktable[i]->pcName, classTasktable[i]->usStackDepth, classTasktable[i]->uxPriority);
	}
	initd.startAllTasks(false, true);
	watchdogMutex = xSemaphoreCreateMutex();
	watchDogTimer =  xTimerCreate("watchDogTimer", pdMS_TO_TICKS(_TASK_WATCHDOG_MS_), pdTRUE, (void*)0, runTaskWatchdogs); //Change back to pdTRUE
	if (watchDogTimer == NULL) {
		logdAssert(_PANIC_, "Could not get a initd watchdog timer");
		ESP.restart();
	}
	if (xTimerStart(watchDogTimer, 0) != pdPASS) {
		logdAssert(_PANIC_, "Could not start the initd watchdog timer");
		ESP.restart();
	}
	logdAssert(_INFO_, "initd scheduler has started");
	//logdAssert(_DEBUG_, "classTasktable[%u](%p) Task Pointer: %p", 0, &classTasktable[0], classTasktable[0]);
	//logdAssert(_DEBUG_, "classTasktable[%u](%p) Task Pointer: %p", 1, &classTasktable[1], classTasktable[1]);
}

tasks::~tasks() {
	logdAssert(_PANIC_, "Someone is trying to destruct initd - rebooting...");
	ESP.restart();
}

uint8_t tasks::getTidByTaskDesc(uint8_t* tid, task_desc_t* task) {
	
	if (task == NULL) {
		return(_TASKS_API_PARAMETER_ERROR_);
	}

	for (uint8_t i = 0; i < _NO_OF_TASKS_; i++) {
		if (classTasktable[i] == task) {
			*tid = i;
			return(_TASKS_OK_);
			break;
		}
	}
	return(_TASKS_NOT_FOUND_);
}

uint8_t tasks::startAllTasks(bool incrementRestartCnt, bool initiateStat) {
	globalInitMutexTake();
	logdAssert(_INFO_, "All tasks will be started/restarted");
	for (uint8_t i = _NO_OF_TASKS_-1; i < _NO_OF_TASKS_; i--) {
		logdAssert(_INFO_, "Tasks name: %s, tid: %u is being initiated", classTasktable[i]->pcName, i);
		if (uint8_t res = startTaskByTaskDesc(classTasktable[i], incrementRestartCnt, initiateStat)) {
			globalInitMutexGive();
			logdAssert(_ERR_, "Tasks name: %s, tid: %d didn't start, cause: %u", classTasktable[i]->pcName, i, res);
			return(_TASKS_GENERAL_ERROR_);
			break;
		}
		//logdAssert(_DEBUG_, "Tasks name: %s, tid: %u was successfully started/restarted", classTasktable[i]->pcName, i);
	}
	globalInitMutexGive();
	return(_TASKS_OK_);
}

uint8_t tasks::stopAllTasks() {
	globalInitMutexTake();
	//logdAssert(_DEBUG_, "Stoping all tasks");
	for (uint8_t i = 0; i < _NO_OF_TASKS_; i++) {
		//logdAssert(_DEBUG_, "Trying to stop taskid: %d, with task name: %c", i, classTasktable[i]->pcName);
		if (uint8_t res = stopTaskByTid(i)) {
			logdAssert(_WARN_, "Stoping taskid: %d, with task name: %c failed with cause: %d", i, classTasktable[i]->pcName, res);
			globalInitMutexGive();
			return(res);
			break;
		}
		//logdAssert(_DEBUG_, "Taskid: %d, with task name: %c stopped", i, classTasktable[i]->pcName);
	}
	globalInitMutexGive();
	return(_TASKS_OK_);
}

uint8_t tasks::startTaskByName(char* pcName, bool incrementRestartCnt, bool initiateStat) {
	task_desc_t* task;
	globalInitMutexTake();
	//logdAssert(_DEBUG_, "Trying to start/restart taskname: %c", pcName);
	if (uint8_t res = getTaskDescByName(pcName, &task)) {
		logdAssert(_ERR_, "Failed to get task descriptor by name: %s, Cause: %d", pcName, res);
		globalInitMutexGive();
		return(res);
	}
	
	if(uint8_t res = startTaskByTaskDesc(task, incrementRestartCnt, initiateStat)) {
		logdAssert(_ERR_, "Failed to start task by name: %s, Cause: %d", pcName, res);
		globalInitMutexGive();
		return(res);
	}
	globalInitMutexGive();
	//logdAssert(_INFO_, "Task: %s started/restarted", pcName);
	return(_TASKS_OK_);
}

uint8_t tasks::startTaskByTid(uint8_t tid, bool incrementRestartCnt, bool initiateStat) {
	//logdAssert(_INFO_, "Trying to start/restart task tid: %u", tid);
	if (tid >= _NO_OF_TASKS_) {
		logdAssert(_ERR_, "Parameter error, tid: %u", tid);
		return(_TASKS_API_PARAMETER_ERROR_);
	}
	taskInitMutexTake(classTasktable[tid]);
	if (uint8_t res = startTaskByTaskDesc(classTasktable[tid], incrementRestartCnt, initiateStat)) {
		logdAssert(_ERR_, "Could not start task by descriptor, tid: %u, cause: %u", tid, res);
		return (res);
	}
	taskInitMutexGive(classTasktable[tid]);
	//logdAssert(_INFO_, "Task tid: %u started/restarted", tid);
	return(_TASKS_OK_);
}

uint8_t tasks::stopTaskByName(char* pcName) {
	task_desc_t* task;
	//logdAssert(_INFO_, "Trying to stop task name: %s", task->pcName);
	globalInitMutexTake();
	if (uint8_t res = getTaskDescByName(pcName, &task)) {
		globalInitMutexGive();
		logdAssert(_ERR_, "Could not find a taske with name: %s", task->pcName);
		return(res);
	}
	if (uint8_t res = stopTaskByTaskDesc(task)) {
		globalInitMutexGive();
		logdAssert(_ERR_, "Could not stop task: %s", task->pcName);
		return(res);
	}
	else {
		globalInitMutexGive();
		//logdAssert(_INFO_, "Task name: %s stopped", task->pcName);
		return(_TASKS_OK_);
	}
}

uint8_t tasks::stopTaskByTid(uint8_t tid) {
	//logdAssert(_INFO_, "Trying to stop task tid: %u", tid);
	if (tid >= _NO_OF_TASKS_) {
		logdAssert(_ERR_, "Parameter error, tid: %u", tid);
		return(_TASKS_API_PARAMETER_ERROR_);
	}
	taskInitMutexTake(classTasktable[tid]);
	if (uint8_t res = stopTaskByTaskDesc(classTasktable[tid])) {
		taskInitMutexGive(classTasktable[tid]);
		logdAssert(_ERR_, "Could not stop task tid: %u", tid);
		return(res);
	}
	else {
		taskInitMutexGive(classTasktable[tid]);
		//logdAssert(_INFO_, "Task tid %u stopped", tid);
		return(_TASKS_OK_);
	}
}

uint8_t tasks::getTaskDescByName(char* pcName, task_desc_t** task) {
	//logdAssert(_DEBUG_, "Trying to get task by name: %c", pcName);
	globalInitMutexTake();
	for (uint8_t i = 0; i < _NO_OF_TASKS_; i++) {
		if (uint8_t res = getTaskDescByTID(i, task)) {
			globalInitMutexGive();
			logdAssert(_ERR_, "Could not get task tid: %u, Cause: %u",i ,res);
			return(res);
		}
		if (!strcmp((*task)->pcName, pcName)) {
			globalInitMutexGive();
			//logdAssert(_INFO_, "Got the task by name: %s, tid: %u" , pcName, i);
			return(_TASKS_OK_);
			break;
		}
	}
	globalInitMutexGive();
	//logdAssert(_INFO_, "Didn't find any task by name: %s", pcName);
	return(_TASKS_NOT_FOUND_);
}

uint8_t tasks::getTaskDescByTID(uint8_t tid, task_desc_t **task) {
	//logdAssert(_DEBUG_, "Trying to get task by tid: %i", tid);
	if (tid >= _NO_OF_TASKS_) {
		logdAssert(_ERR_, "Parameter error, tid: %u", tid);
		return(_TASKS_API_PARAMETER_ERROR_);
	}
	
	if ((*task = classTasktable[tid]) == NULL) { //Assumed atomic not needing any lock
		logdAssert(_ERR_, "General error in getting descriptor by tid: %u", tid);
		return(_TASKS_GENERAL_ERROR_);
	}
	//logdAssert(_DEBUG_, "Task(%p) Task Pointer: %p", task, *task);
	//logdAssert(_DEBUG_, "Got task descriptor by tid: %i, taskName %s", tid, (*task)->pcName);
	//logdAssert(_DEBUG_, "**task: %p *task: %p", task, *task);
	return(_TASKS_OK_);
}

uint8_t tasks::getStalledTasks(uint8_t* tid, task_desc_t* task) {
	TaskStatus_t* pxTaskStatus;
	globalInitMutexTake();
	//logdAssert(_INFO_, "Scanning for stalled tasks starting from tid: %u", *tid);
	for (uint8_t i = *tid; i < _NO_OF_TASKS_; i++) {
		task = classTasktable[i];
		if (task != NULL) {
			if (pxTaskStatus == NULL || eTaskGetState(task->pvCreatedTask) == 1 /*eDeleted*/) { //eDeleted doesn't work
				*tid = i;
				globalInitMutexGive();
				logdAssert(_INFO_, "Found a stalled task tid: %u", i);
				return(_TASKS_OK_);
				break;
			}
		}
	}
	task = NULL;
	globalInitMutexGive();
	//logdAssert(_INFO_, "Found no more stalled tasks");
	return(_TASKS_NOT_FOUND_);
}

uint8_t tasks::clearStats(task_desc_t* task) {
	//logdAssert(_INFO_, "Clearing task stats");
	if (task == NULL) {
		logdAssert(_ERR_, "Parameter error");
		return(_TASKS_API_PARAMETER_ERROR_);
	}
	
	taskInitMutexTake(task);
	task->watchdogHighWatermarkWarnAbs = (task->watchdogHighWatermarkWarnPerc) * (task->WatchdogTimeout) / 100;
	task->watchdogHighWatermarkAbs = 0;
	task->heapHighWatermarkAbs = 0;
	task->heapHighWatermarkWarnAbs = (task->heapHighWatermarkKBWarn) * 1024;
	task->stackHighWatermarkWarnAbs = (task->stackHighWatermarkWarnPerc) * (task->usStackDepth) / 100;
	task->stackHighWatermarkAbs = 0;
	taskInitMutexGive(task);
	return(_TASKS_OK_);
}

uint8_t tasks::startTaskByTaskDesc(task_desc_t* task, bool incrementRestartCnt, bool initiateStat) {
	//logdAssert(_INFO_, "Start task: %s by descriptor", task->pcName);
	if (task == NULL) {
		logdAssert(_ERR_, "Parameter error - task=NULL");
		return(_TASKS_API_PARAMETER_ERROR_);
	}
	taskInitMutexTake(task);
	if (!checkIfTaskAlive(task)) {
		if (uint8_t res = stopTaskByTaskDesc(task)) {
			logdAssert(_ERR_, "Could not stop task: %s", task->pcName);
			return(_TASKS_GENERAL_ERROR_);
		}
	}

	//logdAssert(_DEBUG_, "Check if incrementing restart cnt");
	if (incrementRestartCnt) {
		if (task->escalationRestartCnt) {
			//logdAssert(_DEBUG_, "Incrementing restart cnt");
			++(task->restartCnt);
			if (task->escalationRestartCnt) {
				if (task->restartCnt >= task->escalationRestartCnt) {
					logdAssert(_PANIC_, "Task: %s has frequently restarted, escalating to system restart...", task->pcName);
					ESP.restart();
				}
			}
			else {
				//logdAssert(_DEBUG_, "Reset restart cnt");
				task->restartCnt = 0;
			}
			//logdAssert(_DEBUG_, "Incremented restart cnt, now %u", task->restartCnt);
		}
	}
	if (initiateStat) {
		clearStats(task);
	}
	//logdAssert(_DEBUG_, "*task %p taskhandle %p", task, task->pvCreatedTask);
	//(_DEBUG_, "Resetting watchdog timer for Task: %s", task->pcName);
	task->watchDogTimer = 0;

	//logdAssert(_DEBUG_, "Watchdog cleared");

	//logdAssert(_DEBUG_, "Starting task");
	if (xTaskCreatePinnedToCore(
		task->pvTaskCode,
		task->pcName,
		task->usStackDepth,
		task,
		task->uxPriority,
		&(task->pvCreatedTask),
		task->xCoreID) != pdPASS) {
		taskInitMutexGive(task);
		logdAssert(_ERR_, "General error, the task was probably not started");
		return(_TASKS_GENERAL_ERROR_);
	}
	//logdAssert(_DEBUG_, "Giving back the Mutex");
	taskInitMutexGive(task);
	//logdAssert(_DEBUG_, "The Mutex is given back");
	logdAssert(_INFO_, "Task: %s was started/restarted", task->pcName);
	return(_TASKS_OK_);
}

uint8_t tasks::stopTaskByTaskDesc(task_desc_t* task) {
	//logdAssert(_INFO_, "Trying to stop task: %s", task->pcName);
	taskInitMutexTake(task);
	if (!checkIfTaskAlive(task)) {
		//logdAssert(_INFO_, "Task %s is running and will be stopped", task->pcName);
		vTaskDelete(task->pvCreatedTask);
		initd.taskMfreeAll(task);
		taskInitMutexGive(task);
		//logdAssert(_INFO_, "Task: %s stopped", task->pcName);
		return(_TASKS_OK_);
	}
	else {
		taskInitMutexGive(task);
		logdAssert(_ERR_, "Task: %s is not running and cannot be stoped, trying to perform task garbage collection", task->pcName);
		//initd.taskMfreeAll(task);
		return(_TASKS_GENERAL_ERROR_);
	}
}

uint8_t tasks::checkIfTaskAlive(task_desc_t* task) {
	taskInitMutexTake(task);
	//logdAssert(_DEBUG_, "Mutex taken");
	//logdAssert(_DEBUG_, "*task %p taskhandle %p", task, task->pvCreatedTask);
	if (task->pvCreatedTask == NULL) {
		logdAssert(_INFO_, "Task does not exist, handle = NULL");
		taskInitMutexGive(task);
		return (_TASK_DELETED_);
	}
	if (eTaskGetState(task->pvCreatedTask) == 1 /*eDeleted*/) { //eDeleted doesn't work
		logdAssert(_INFO_, "Task deleted");
		taskInitMutexGive(task);
		return (_TASK_DELETED_);
	}
	else {
		//logdAssert(_INFO_, "Task %s exists with state %u, edeleted %u", task->pcName, eTaskGetState(task->pvCreatedTask), eDeleted);
		taskInitMutexGive(task);
		return (_TASK_EXIST_);
	}
}

/* ************************** Task mutex/critical section handling  ******************************** */
/* These methods are providing intends to provide secure pass through critical sections avoiding     */
/* interfearence from parallel tasks/threads manipulating the datastructures being processed.        */
/* Two access methods are defined: tasks:globalInitMutexTake/(Give) locking the entire tasks data-   */
/* structure and:                                                                                    */
/* tasks:taskInitMutexTake/(Give) [not implemented - translated to tasks:globalInitMutexTake/(Give)] */
/* intended to implement a atomic method where global mutexes blocks any task data structure         */
/* modifications, ...                                                                                */
/* while task    */
/* blocks global-, this task   */
/* ------------------------------------------------------------------------------------------------- */
/* Private methods:                                                                                  */
/* tasks::globalInitMutexTake(TickType_t timeout)*/
uint8_t tasks::globalInitMutexTake(TickType_t timeout) {
	if (timeout != portMAX_DELAY) {
		xSemaphoreTakeRecursive(globalInitMutex, pdMS_TO_TICKS(timeout));
		if (xSemaphoreGetMutexHolder(globalInitMutex) != xTaskGetCurrentTaskHandle()) {
			return(_TASKS_MUTEX_TIMEOUT_);
		}
		else {
			return(_TASKS_OK_);
		}
	}
	else {
		return(_TASKS_OK_);
	}
}

uint8_t tasks::globalInitMutexGive() {
	xSemaphoreGiveRecursive(globalInitMutex);
	return(_TASKS_OK_);
}

uint8_t tasks::taskInitMutexTake(task_desc_t* task, TickType_t timeout) { //Fine grained task locks not yet implemente
	return(globalInitMutexTake(timeout));
}

uint8_t tasks::taskInitMutexGive(task_desc_t* task) { //Fine grained task locks not yet implemente
	return(globalInitMutexGive());
}

/* *************************** Task Management/watchdog core tasks  ******************************** */
/* These tasks are periodic core init OS, monitoring spawned tasks, restarting those ig crashed      */
/* or if watchdog expired, collecting and logging system and task statistics, and restarting the     */
/* system if there is no other way out, or if these services are not responding - monitored by the   */
/* Arduino main loop                                                                                 */
/* ------------------------------------------------------------------------------------------------- */
/* Public methods:                                                                                   */
/* void tasks::kickTaskWatchdogs(...);                                                               */
/*      tasks method to kick the watchdog in case task_dec_t.WatchdogTimeout != 0 [??? 10 ms ???]    */
/* void tasks::taskPanic(...);                                                                       */
/*      Initiates a graceful task panic exception handling, task will be terminated and it's         */
/*      resources will be collected and cleaned.  The task might be restarted depending on           */
/*      task_dec_t.WatchdogTimeout (if 0 the task will not be restarted, otherwise it will....       */
/*                                                                                                   */
/* Private methods:                                                                                  */
/* void tasks:: runTaskWatchdogs(...)                                                                */
/*         A core tasks timer method to run periodic maintenance tasks, checking watchdogs, task-    */
/*         health, collecting and reporting statistics, etc.                                         */
/*                                                                                                   */
/* --------------------------------------------------------------------------------------------------*/

/* --------------------------------- - Public:kickTaskWatchdogs()- --------------------------------- */
/* void tasks::kickTaskWatchdogs(task_desc_t* task_p)                                                */
/*	Description:  This method kicks the watchdog for task: task_p and needs to be called frequently  */
/*                with a periodicy less than defined in task_desc_t.WatchdogTimeout                  */
/*  Properties:   Public method. This method is thread safe.                                         */
/*  Return codes: void                                                                               */
void tasks::kickTaskWatchdogs(task_desc_t* task_p) {
	task_p->watchDogTimer = 0; //atomic
	return;
}
/* ---------------------------------- - END:kickTaskWatchdogs()- ----------------------------------- */

/* ------------------------------------- - Public:taskPanic()- ------------------------------------- */
/* void tasks::taskPanic(task_desc_t* task_p)                                                        */
/*	Description:  Initiates a graceful task panic exception handling, task will be terminated and    */
/*                it's resources will be collected and cleaned.  The task might be restarted         */
/*                depending on task_dec_t.WatchdogTimeout (if 0 the task will not be restarted,      */
/*                otherwise it will....
/*  Properties:   Public method. This method is thread safe.                                         */
/*  Return codes: void                                                                               */
void tasks::taskPanic(task_desc_t* task_p) {
	initd.taskMfreeAll(task_p);
	//release any other resources
	vTaskDelete(NULL);
}
/* -------------------------------------- - END:taskPanic() - -------------------------------------- */

/* ---------------------------------- - Public:taskExceptions()- ----------------------------------- */

// Stack-overflow management void vApplicationStackOverflowHook( TaskHandle_t xTask, signed char* pcTaskName );
// Out of space handling void vApplicationMallocFailedHook( void );
/* ------------------------------------ - END:taskExceptions()- ------------------------------------ */


/* --------------------------------- - Private:runTaskWatchdogs() - -------------------------------- */
/* void tasks::runTaskWatchdogs(TimerHandle_t xTimer)                                                */
/*         A core tasks timer method to run periodic maintenance tasks, checking watchdogs, task-    */
/*         health, collecting and reporting statistics, etc.                                         */
/*         The periodicy of this task is set by: */
/*                                                                                                   */
void tasks::runTaskWatchdogs(TimerHandle_t xTimer) {
	task_desc_t* task;
	static int nexthunderedMSWatchdogTaskCnt = (100 / _TASK_WATCHDOG_MS_) + (100 % _TASK_WATCHDOG_MS_);
	static int nextSecWatchdogTaskCnt = (1000 / _TASK_WATCHDOG_MS_) + (1000 % _TASK_WATCHDOG_MS_);
	static int nextTenSecWatchdogTaskCnt = ((10*1000) / _TASK_WATCHDOG_MS_) + ((10*1000) % _TASK_WATCHDOG_MS_);
	static int nextMinWatchdogTaskCnt = ((60 * 1000) / _TASK_WATCHDOG_MS_) + ((60 * 1000) % _TASK_WATCHDOG_MS_);
	static int nextHourWatchdogTaskCnt = ((3600 * 1000) / _TASK_WATCHDOG_MS_) + ((3600 * 1000) % _TASK_WATCHDOG_MS_);
	bool decreaseEscalationCnt = false;
	static uint8_t minStatTaskWalk = 255;

	if (xSemaphoreTake(watchdogMutex, 0) == pdFALSE) {
		watchdogOverruns++;
		//logdAssert(_DEBUG_, "Watchdog overrun, skipping this watchdog run");
		return;
	}
//	logdAssert(_DEBUG_, "Running watchdogs nexthunderedMSWatchdogTaskCnt %u", nexthunderedMSWatchdogTaskCnt); 
//	logdAssert(_DEBUG_, "Running watchdogs initd.watchdogTaskCnt %u initd.hunderedMsCnt %u", initd.watchdogTaskCnt, initd.hunderedMsCnt);
	initd.watchdogTaskCnt++; //make sure it is initiated somewhere

	//Every 100 ms tasks
	if (initd.watchdogTaskCnt >= nexthunderedMSWatchdogTaskCnt) {
		initd.hunderedMsCnt++;
		//logdAssert(_DEBUG_, "Running recurring watchdog maintenance tasks [100 ms]");
		nexthunderedMSWatchdogTaskCnt = (initd.hunderedMsCnt + 1) * ((100 / _TASK_WATCHDOG_MS_) + (100 % _TASK_WATCHDOG_MS_));
		// do short 1 second stuff here, long run tasks must be deferred evenly across the watchdog runs - 
		//  any mutexes must be local in this "if second" context handled in here.

		//Distributing minute statistics reporting work to 100 ms chunks.
		if (minStatTaskWalk < _NO_OF_TASKS_) {
			initd.getTaskDescByTID(minStatTaskWalk, &task);
			logdAssert(_INFO_, "Heap size for Task %s is currently %u, with a high watermark of %u", task->pcName, task->heapCurr, task->heapHighWatermarkAbs);
			logdAssert(_INFO_, "Stack use for Task %s is currently [currently unsupported], with a high watermark of %u", task->pcName, uxTaskGetStackHighWaterMark(task->pvCreatedTask));
			logdAssert(_INFO_, "CPU use for Task %s is currently [currently unsupported]", task->pcName);
			logdAssert(_INFO_, "Watchdog statistics for Task %s, High water mark %u, Timout set to %u", task->pcName, task->watchdogHighWatermarkAbs, task->WatchdogTimeout);
			minStatTaskWalk++;
		}
	}

	//*** Every 1 second tasks
	if (initd.watchdogTaskCnt >= nextSecWatchdogTaskCnt) {
		initd.secCnt++;
		//logdAssert(_DEBUG_, "Running recurring watchdog maintenance tasks [second] - %u hours %u minutes %u seconds", initd.hourCnt, initd.minCnt%60, initd.secCnt%60);
		nextSecWatchdogTaskCnt = (initd.secCnt + 1) * ((1000 / _TASK_WATCHDOG_MS_) + (1000 % _TASK_WATCHDOG_MS_));
		// do short 1 second stuff here, long run tasks must be deferred evenly across the watchdog runs - 
		//  any mutexes must be local in this "if second" context handled in here.
	}

	//*** Every 10 second tasks
	if (initd.watchdogTaskCnt >= nextTenSecWatchdogTaskCnt) {
		initd.tenSecCnt++;
		logdAssert(_DEBUG_, "Running recurring watchdog maintenance tasks [10 seconds] - %u hours %u minutes %u seconds", initd.hourCnt, initd.minCnt%60, initd.secCnt%60);
		nextTenSecWatchdogTaskCnt = (initd.tenSecCnt + 1) * (((10 * 1000) / _TASK_WATCHDOG_MS_) + ((10 * 1000) % _TASK_WATCHDOG_MS_));
		//  do short 10 second stuff here, long run tasks must be deferred evenly across the watchdog runs - 
		//  any mutexes must be local in this "if tensecond" context handled in here.
	}

	//*** Every minute tasks
	if (initd.watchdogTaskCnt >= nextMinWatchdogTaskCnt) {
		initd.minCnt++;
		logdAssert(_DEBUG_, "Running recurring watchdog maintenance tasks [minute]- %u hours %u minutes %u seconds", initd.hourCnt, initd.minCnt%60, initd.secCnt%60);
		logdAssert(_INFO_, "Watchdog overruns: %u", watchdogOverruns);
		nextMinWatchdogTaskCnt = (initd.minCnt + 1) * (((60 * 1000) / _TASK_WATCHDOG_MS_) + ((60 * 1000) % _TASK_WATCHDOG_MS_));
		//  do short minute stuff here, long run tasks must be deferred evenly across the watchdog runs - 
		//  any mutexes must be local in this "if minute" context handled in here.
		decreaseEscalationCnt = true;
		minStatTaskWalk = 0;
	}

	//*** Houerly tasks
	if (initd.watchdogTaskCnt >= nextHourWatchdogTaskCnt) {
		logdAssert(_DEBUG_, "Running hourely recurring maintenance tasks");
		initd.hourCnt++;
		logdAssert(_DEBUG_, "Running recurring watchdog maintenance tasks [hour]- %u hours %u minutes %u seconds", initd.hourCnt, initd.minCnt % 60, initd.secCnt % 60);
		nextHourWatchdogTaskCnt = (initd.hourCnt + 1) * (((3600 * 1000) / _TASK_WATCHDOG_MS_) + ((3600 * 1000) % _TASK_WATCHDOG_MS_));
		//  do short hour stuff here, long run tasks must be deferred evenly across the watchdog runs - 
		//  any mutexes must be local in this "if hour" context handled in here.

	}

	initd.globalInitMutexTake();

	for (uint8_t i = 0; i < _NO_OF_TASKS_; i++) {
		//	logdAssert(_DEBUG_, "**classTasktable[%u] %p classTasktable[%u] %p", i, &classTasktable[i], i, classTasktable[i]);
		//	logdAssert(_DEBUG_, "**task %p *task %p", &task, task);
		initd.getTaskDescByTID(i, &task);
		//	logdAssert(_DEBUG_, "**task %p *task %p", &task, task);
		if (task == NULL) {
			//		logdAssert(_PANIC_, "Could not find task by id: %u", i);
			ESP.restart();
		}
		//	logdAssert(_DEBUG_, "Breakpoint 1");
		if (decreaseEscalationCnt && task->restartCnt) {
			//	logdAssert(_DEBUG_, "Breakpoint 2");
			task->restartCnt--;
		}

		//logdAssert(_DEBUG_, "Breakpoint 3, watchdog timer %u", task->watchDogTimer);
		if (task->WatchdogTimeout && task->watchDogTimer != _WATCHDOG_DISABLE_) {
			//	logdAssert(_DEBUG_, "Breakpoint 4");
			if (initd.checkIfTaskAlive(task)) {
				logdAssert(_ERR_, "Task: %s  has stalled", task->pcName);
				initd.taskMfreeAll(task);
				initd.startTaskByTid(i, true, false);
			}
			//logdAssert(_DEBUG_, "Breakpoint 5");
			if (++(task->watchDogTimer) >= task->WatchdogTimeout) {
				logdAssert(_ERR_, "Watchdog timeout for Task: %s, watchdog counter: %u", task->pcName, task->watchDogTimer);
				initd.startTaskByTid(i, true, false);
			}
			else {
				//logdAssert(_DEBUG_, "Breakpoint 6");
				if (task->watchDogTimer > task->watchdogHighWatermarkAbs) {
					//logdAssert(_DEBUG_, "Breakpoint 7");
					task->watchdogHighWatermarkAbs = task->watchDogTimer;
					if (task->watchdogHighWatermarkWarnPerc && task->watchdogHighWatermarkWarnAbs <= task->watchdogHighWatermarkAbs) {
						//The logging below is likely causing stack overflow - need to increase configMINIMAL_STACK_SIZE
						//logdAssert(_WARN_, "Watchdog count for Task: %s reached Warn level of: %u  percent, watchdog counter: %u" ,task->pcName, task->watchdogHighWatermarkWarnPerc, task->watchDogTimer);
					}
				}
			}
		}
		//logdAssert(_DEBUG_, "Breakpoint 8 Task: %s, restartcnt %u", task->pcName, task->restartCnt);
		if (task->escalationRestartCnt && task->restartCnt >= task->escalationRestartCnt) {
			logdAssert(_PANIC_, "Task: %s has restarted exessively, escalating to system restart", task->pcName);
			ESP.restart();
		}
		if (task->watchdogHighWatermarkAbs >= task->watchdogHighWatermarkWarnAbs && task->WatchdogTimeout && !task->watchdogWarn) {
			task->watchdogWarn = true;
			logdAssert(_WARN_, "Watchdog for Task %s, has reached warning level %u, High water mark is %u, Timout set to %u", task->pcName, task->watchdogHighWatermarkWarnAbs, task->watchdogHighWatermarkAbs, task->WatchdogTimeout);
		}
	}
	initd.globalInitMutexGive();
	xSemaphoreGive(watchdogMutex);
	return;
}

/* *********************************** Task resources management *********************************** */
/* Task resource management internal task class functions meant to provide general mechanisms for    */
/* various resource management, such as memory-, message passing-, queues- management or any other   */
/* arbitrary resources which the task class need to be aware of in order to be able to destruct      */
/* in case there are premature task terminations.                                                    */
/* Using these class primitives enables destruction of resources if a task is terminated             */
/* ------------------------------------------------------------------------------------------------- */
/* No pubblic methods are exposed, but-                                                              */
/* Following private primitives are provided:                                                        */
/* taskResourceLink(...)                                                                             */
/* taskResourceUnLink(...)                                                                           */
/* findResourceDescByResourceObj(...)                                                                */
/*                                                                                                   */
/* --------------------------------------------------------------------------------------------------*/

/* ----------------------------------- PRIVATE:taskResourceLink() -----------------------------------*/
/* uint8_t taskResourceLink(task_resource_t** taskResourceRoot_pp, void* taskResourceObj_p)          */
/*	Description:  This method links a new resource descriptor with an object pointer to a list       */
/*                of descriptors starting at ask_resource_t** taskResourceRoot_pp                    */
/*  Properties:   Private method.                                                                    */
/*                This method is thread unsafe, it is unsafe to use this method while the root or    */
/*                task object is being modified by any other parallel thread.                        */
/*  Return codes: [_TASKS_OK_= 0 | _TASKS_RESOURCES_EXHAUSTED | _TASKS_API_PARAMETER_ERROR_ |        */
/*                 | _TASKS_GENERAL_ERROR_, _TASKS_NOT_FOUND_ | _TASKS_MUTEX_TIMEOUT_]               */
/*  Parameters:   task_resource_t** taskResourceRoot_pp: Address of the root pointer for the-        */
/*                resource list                                                                      */
/*                void* taskResourceObj_p: An arbitrary resource object pointer                      */
/*                                                                                                   */

uint8_t tasks::taskResourceLink(task_resource_t** taskResourceRoot_pp, void* taskResourceObj_p) {
	void* taskResourceDesc_p;
	
	if (taskResourceRoot_pp == NULL || taskResourceObj_p == NULL) {
		logdAssert(_INFO_, "API error, one of task_resource_t** taskResourceRoot_pp, void* taskResourceObj_p is NULL");
		return(_TASKS_API_PARAMETER_ERROR_);
	}
	//logdAssert(_DEBUG_, "Trying to link in taskResourceObj_p %p to taskResourceRoot_pp %p, which is pointing to %p", taskResourceObj_p, taskResourceRoot_pp, *taskResourceRoot_pp);
	if ((taskResourceDesc_p = malloc(sizeof(task_resource_t))) == NULL) {
		logdAssert(_INFO_, "Could not allocate memory for resource descriptor");
		return(_TASKS_RESOURCES_EXHAUSTED_);
	}
	
	//logdAssert(_DEBUG_, "taskResourceDesc_p %p allocated, linking it to beginning of root_pp %p", taskResourceDesc_p, taskResourceRoot_pp);
	((task_resource_t*)taskResourceDesc_p)->prevResource_p = NULL;
	((task_resource_t*)taskResourceDesc_p)->taskResource_p = taskResourceObj_p;

	if (*taskResourceRoot_pp == NULL) {
		//logdAssert(_DEBUG_, "This is the first object");
		*taskResourceRoot_pp = (task_resource_t*)taskResourceDesc_p;
		((task_resource_t*)taskResourceDesc_p)->nextResource_p = NULL;
	}
	else {
		//logdAssert(_DEBUG_, "There exists more resource objects, linking it in as the first object");
		(*taskResourceRoot_pp)->prevResource_p = (task_resource_t*)taskResourceDesc_p;
		((task_resource_t*)taskResourceDesc_p)->nextResource_p = *taskResourceRoot_pp;
		*taskResourceRoot_pp = (task_resource_t*)taskResourceDesc_p;
		//logdAssert(_DEBUG_, "root pointer is now (*taskResourceRoot_pp) is now %p", *taskResourceRoot_pp);
		//logdAssert(_DEBUG_, "the created object descriptor address (taskResourceDesc_p) is %p", taskResourceDesc_p);
		//logdAssert(_DEBUG_, "and its content is: prev %p, object %p, next %p", ((task_resource_t*)taskResourceDesc_p)->prevResource_p, ((task_resource_t*)taskResourceDesc_p)->taskResource_p, ((task_resource_t*)taskResourceDesc_p)->nextResource_p);
		//ogdAssert(_DEBUG_, "and its successor content is: prev %p, object %p, next %p", ((task_resource_t*)taskResourceDesc_p)->nextResource_p->prevResource_p, ((task_resource_t*)taskResourceDesc_p)->nextResource_p->taskResource_p, ((task_resource_t*)taskResourceDesc_p)->nextResource_p->nextResource_p);

	}
	//logdAssert(_DEBUG_, "Resource descriptor successfully linked-in");
	return(_TASKS_OK_);
}
/* --------------------------------- END PRIVATE:taskResourceLink() ---------------------------------*/

/* ---------------------------------- PRIVATE:taskResourceUnLink() ----------------------------------*/
/* uint8_t tasks::taskResourceUnLink(task_resource_t** taskResourceRoot_pp, void* taskResourceObj_p) */
/* Description: This method un-links and destroys a resource descriptor, but does destroy the        */
/*               actual resource object.                                                             */
/*               The execution time is almost linear with the amount of linked task resource         */
/*               desctiptors                                                                         */
/* Properties:   Private method.                                                                     */
/*               This method is thread unsafe, it is unsafe to use this method while the root- or    */
/*               task object is being modified by any other parallel thread.                         */
/*               The execution time is almost linear with the amount of linked task resource         */
/*               desctiptors                                                                         */
/* Return codes: [_TASKS_OK_= 0 | _TASKS_RESOURCES_EXHAUSTED | _TASKS_API_PARAMETER_ERROR_ |         */
/*               | _TASKS_GENERAL_ERROR_, _TASKS_NOT_FOUND_ | _TASKS_MUTEX_TIMEOUT_]                 */
/* Parameters:   task_resource_t** taskResourceRoot_pp: Address of the root pointer for the-         */
/*               resource list                                                                       */
/*               void* taskResourceObj_p: An arbitrary resource object pointer for which it's        */
/*               resource descriptor should be removed                                               */
/*                                                                                                   */


uint8_t tasks::taskResourceUnLink(task_resource_t** taskResourceRoot_pp, void* taskResourceObj_p) {
	task_resource_t* taskResourceDesc_p;
	task_resource_t** taskResourceDesc_pp = &taskResourceDesc_p;

	if (taskResourceRoot_pp == NULL || taskResourceObj_p == NULL) {
		logdAssert(_ERR_, "API error, one of task_resource_t** taskResourceRoot_pp, void* taskResourceObj_p are NULL");
		return(_TASKS_API_PARAMETER_ERROR_);
	}
	//logdAssert(_DEBUG_, "Trying to unlink  taskResourceObj_p %p from taskResourceRoot_pp %p pointing at %p", taskResourceObj_p, taskResourceRoot_pp, *taskResourceRoot_pp);
	//logdAssert(_DEBUG_, "Trying to find taskResourceDesc_p by taskResourceObj_p %p from the list pointed to by taskResourceRoot_pp %p pointing at *taskResourceRoot_pp", taskResourceObj_p, taskResourceRoot_pp, *taskResourceRoot_pp);
	if (uint8_t res = initd.findResourceDescByResourceObj(taskResourceObj_p, *taskResourceRoot_pp, taskResourceDesc_pp)) {
		logdAssert(_WARN_, "resourceObj_p %p could not be found in the list pointed to by *taskResourceRoot_pp %p", taskResourceObj_p, *taskResourceDesc_pp);
		return(_TASKS_NOT_FOUND_);
	}
	//logdAssert(_DEBUG_, "Found taskResourceDesc_p by taskResourceObj_p %p", taskResourceObj_p);

	if ((*taskResourceDesc_pp)->nextResource_p == NULL) {
		//logdAssert(_DEBUG_, "This *taskResourceDesc_pp %p is the last in the list", *taskResourceDesc_pp);
		if ((*taskResourceDesc_pp)->prevResource_p == NULL) {
			*taskResourceRoot_pp = NULL;
			//logdAssert(_DEBUG_, "This *taskResourceDesc_pp %p was the only one and has been un-linked, *taskResourceRoot_pp in now %p", *taskResourceDesc_pp, *taskResourceRoot_pp);
		}
		else {
			((*taskResourceDesc_pp)->prevResource_p)->nextResource_p = NULL;
			//logdAssert(_DEBUG_, "This taskResourceDesc_p %p were at the end of many, and has been un-linked, *taskResourceRoot_pp is now %p", *taskResourceDesc_pp, *taskResourceRoot_pp);
		}
	}
	else {
		if ((*taskResourceDesc_pp)->prevResource_p == NULL) {
			*taskResourceRoot_pp = (*taskResourceDesc_pp)->nextResource_p;
			(*taskResourceDesc_pp)->nextResource_p->prevResource_p = NULL;
			//logdAssert(_DEBUG_, "There were several taskResourceDesc in the list, but this *taskResourceDesc_pp %p was the first one and has been unlinked, *taskResourceRoot_pp is now %p", *taskResourceDesc_pp, *taskResourceRoot_pp);
		}
		else {
			(*taskResourceDesc_pp)->nextResource_p->prevResource_p = (*taskResourceDesc_pp)->prevResource_p;
			(*taskResourceDesc_pp)->prevResource_p->nextResource_p = (*taskResourceDesc_pp)->nextResource_p;
			//logdAssert(_DEBUG_, "This *taskResourceDesc_pp %p  was in the mid of the list and has been un-linked, *taskResourceRoot_pp is now %p", *taskResourceDesc_pp, *taskResourceRoot_pp);
			//logdAssert(_DEBUG_, "The previous *taskResourceDesc_pp was %p and it's next is %p", (*taskResourceDesc_pp)->prevResource_p, (*taskResourceDesc_pp)->nextResource_p);
			//logdAssert(_DEBUG_, "The Next *taskResourceDesc_pp was %p and it's next is %p", (*taskResourceDesc_pp)->prevResource_p, (*taskResourceDesc_pp)->nextResource_p);
		}
	}
	free(*taskResourceDesc_pp);
	return(_TASKS_OK_);
}
/* -------------------------------- END PRIVATE:taskResourceUnLink() --------------------------------*/

/* ------------------------------ PRIVATE:findResourceDescByResourceObj -----------------------------*/
/* uint8_t findResourceDescByResourceObj(void* resourceObj_p, task_resource_t* taskResourceRoot_p,   */
/*                                       task_resource_t** taskResourceDesc_pp)                      */
/*	Description: This method tries to find the a task_resource_t** taskResourceDesc_pp task resource */
/*               descriptor holding an void* resourceObj_p resource object.                          */
/*  Properties:  Private method.                                                                     */
/*               This method is thread unsafe, it is unsafe to use this method while the root- or    */
/*               task object is being modified by any other parallel thread.                         */
/*               The execution time is almost linear with the amount of linked task resource         */
 /*              desctiptors                                                                         */
/*  Return codes: [_TASKS_OK_= 0 | _TASKS_RESOURCES_EXHAUSTED | _TASKS_API_PARAMETER_ERROR_ |        */
/*                | _TASKS_GENERAL_ERROR_, _TASKS_NOT_FOUND_ | _TASKS_MUTEX_TIMEOUT_]                */
/*  Parameters:   void* resourceObj_p: The arbitrary object being searche for.                       */
/*                task_resource_t** taskResourceRoot_pp: Address of the root pointer for the -       */
/*                                                       resource list                               */
/*                task_resource_t** taskResourceDesc_pp: The address of the pointer which this       */
/*														 this function return - the pointer to the   */
/*                                                       descriptor holding the resource object      */

uint8_t tasks::findResourceDescByResourceObj(void* taskResourceObj_p, task_resource_t* taskResourceRoot_p, task_resource_t** taskResourceDesc_pp) {

	if (taskResourceObj_p == NULL || taskResourceRoot_p == NULL) {
		logdAssert(_ERR_, "API error, one or both of: void* taskResourceObj_p, task_resource_t* taskResourceRoot_p is NULL");
		return(_TASKS_API_PARAMETER_ERROR_);
	}
	//logdAssert(_DEBUG_, "Finding *taskResourceDesc_pp from &taskResourceRoot_p %p, taskResourceRoot_p %p", &taskResourceRoot_p, taskResourceRoot_p);

	*taskResourceDesc_pp = taskResourceRoot_p;
	while ((*taskResourceDesc_pp)->nextResource_p != NULL && (*taskResourceDesc_pp)->taskResource_p != taskResourceObj_p) { 
		(*taskResourceDesc_pp) = (*taskResourceDesc_pp)->nextResource_p;
		//logdAssert(_DEBUG_, "Itterating to next *taskResourceDesc_pp %p", *taskResourceDesc_pp);
	}
	//logdAssert(_DEBUG_, "Itterating END");
	if ((*taskResourceDesc_pp)->taskResource_p == taskResourceObj_p) {
		//logdAssert(_DEBUG_, "Found *taskResourceDesc_pp %p by taskResourceObj_p %p", *taskResourceDesc_pp, taskResourceObj_p);
		return(_TASKS_OK_);
	}
	else {
		*taskResourceDesc_pp = NULL;
		logdAssert(_INFO_, "Could not find taskResourceObj_p by resource object: %p", taskResourceObj_p);
		return(_TASKS_NOT_FOUND_);
	}
}
/* --------------------------- END PRIVATE:findResourceDescByResourceObj ----------------------------*/

/* ********************************* END Task resources management ********************************* */


/* ********************************** Memory resources management ********************************** */
/* Task memory resource management makes sure that memory allocated by Tasks can be released         */
/* after a task crash or whenever the task manager kills a tak due to watchdog timeouts, etc         */
/* ------------------------------------------------------------------------------------------------- */
/* Pubblic methods:                                                                                  */
/* void* taskMalloc(task_desc_t* task_p, int size);                                                  */
/* uint8_t taskMfree(task_desc_t* task_p, void* mem_p)                                               */
/*                                                                                                   */
/* Private methods:                                                                                  */
/* uint8_t tasks::taskMfreeAll(task_desc_t* task_p)                                                  */
/* --------------------------------------------------------------------------------------------------*/

/* --------------------------------------- PUBLIC:taskMalloc ----------------------------------------*/
/* void* tasks::taskMalloc(task_desc_t* task_p, int size)                                            */
/* Description:   This method allocates memory of size "size" and register it with the task manager  */
/*                such that it will beleted, should the task crash or otherwise terminated.          */
/* Properties:    Public method.                                                                     */
/*                This method is thread SAFE, it is safe to use this method while the root- or       */
/*                task object is trying to be modified.                                              */
/*                The execution time is almost linear with the amount of linked task memory resource */
/*                desctiptors.                                                                       */
/* Return codes:  pointer to/handle to the beginning of the memory chunk allocated                   */
/* Parameters:    vtask_desc_t* task_p: The task decriptor pointer.                                  */
/*                int size: size of the memory chunk to be allocated.                                */
/*                                                                                                   */
void* tasks::taskMalloc(task_desc_t* task_p, int size) {
	void* mem_p;

	if (size == 0 || task_p == NULL) {
		logdAssert(_ERR_, "API error, size=0 or task_p=NULL");
		return(NULL);
	}
	//logdAssert(_DEBUG_, "Allocating memory of size: %u: requested from task: %s", size, task_p->pcName)
	taskInitMutexTake(task_p);
	if ((mem_p = malloc(size + sizeof(int))) == NULL) {
		logdAssert(_ERR_, "Could not allocate memory");
		taskInitMutexGive(task_p);
		return(NULL);
	}
	//logdAssert(_DEBUG_, "base mem_p is %p size is %u", mem_p, size + sizeof(int));
	*((int*)mem_p) = size + sizeof(int);

	if (uint8_t res = taskResourceLink(&(task_p->dynMemObjects_p) , (char*)mem_p + sizeof(int))) {
		free(mem_p);
		logdAssert(_ERR_, "Register/link dynamic memory resource with return code: %u", res);
		taskInitMutexGive(task_p);
		return(NULL);
	}
	//logdAssert(_DEBUG_, "Memory allocated at base adrress: %p, Size: %u", mem_p, size);
	incHeapStats(task_p, size + sizeof(int));
	taskInitMutexGive(task_p);
	return((char*)mem_p + sizeof(int));
}
/* -------------------------------------- END PUBLIC:taskMalloc -------------------------------------*/

/* ---------------------------------------- PUBLIC:taskMfree ----------------------------------------*/
/* uint8_t tasks::taskMfree(task_desc_t* task_p, void* mem_p)                                        */
/* Description:   This method frees memory previously allocated with taskMalloc and unregisters it   */
/*                with the task manager memory allocation tracking                                   */
/* Properties:    Public method.                                                                     */
/*                This method is thread SAFE, it is safe to use this method while the root- or       */
/*                task object is trying to be modified.                                              */
/*                The execution time is almost linear with the amount of linked task memory resource */
/*                desctiptors.                                                                       */
/* Return codes:  [_TASKS_OK_= 0 | _TASKS_RESOURCES_EXHAUSTED | _TASKS_API_PARAMETER_ERROR_ |        */
/*                | _TASKS_GENERAL_ERROR_, _TASKS_NOT_FOUND_ | _TASKS_MUTEX_TIMEOUT_]                */
/* Parameters:    vtask_desc_t* task_p: The task decriptor pointer.                                  */
/*                void* mem_p the memory chunk handle to be freed/starting pointer to the memory     */
/*                chunk.                                                                             */
/*                                                                                                   */
uint8_t tasks::taskMfree(task_desc_t* task_p, void* mem_p) {
	task_resource_t** taskResourceDesc_pp = &(task_p->dynMemObjects_p);
	int size;

	//logdAssert(_DEBUG_, "taskResourceDesc_pp %p", taskResourceDesc_pp);
	if (task_p == NULL || mem_p == NULL) {
		logdAssert(_ERR_, "API error, one of ask_desc_t* task_p, void* mem_p is NULL");
		return(_TASKS_API_PARAMETER_ERROR_);
	}
	taskInitMutexTake(task_p);
	//logdAssert(_DEBUG_, "Trying to free heap memory %p allocated by task %s", mem_p, task_p->pcName);
	size = *((int*)((char*)mem_p - sizeof(int)));
	//logdAssert(_DEBUG_, "base mem_p is %p size is %u", (char*)mem_p - sizeof(int), size);
	free((char*)mem_p - sizeof(int));
	decHeapStats(task_p, size);
	//logdAssert(_DEBUG_, "Task heap memory freed: %p", mem_p);
	
	//logdAssert(_DEBUG_, "Trying to un-link memory resource %p from root (&(task_p->dynMemObjects_p) %p", mem_p, &(task_p->dynMemObjects_p));
	if (uint8_t res = taskResourceUnLink(&(task_p->dynMemObjects_p), mem_p)) {
		logdAssert(_ERR_, "un-linking memory resource failed - resason: %u", res);
		taskInitMutexGive(task_p);
		return(res);
	}
	//logdAssert(_DEBUG_, "un-link memory resource %p from root (&(task_p->dynMemObjects_p) %p successfull", mem_p, &(task_p->dynMemObjects_p));
	taskInitMutexGive(task_p);
	return(_TASKS_OK_);
}
/* -------------------------------------- END PUBLIC:taskMfree --------------------------------------*/

/* -------------------------------------- PRIVATE:taskMfreeAll --------------------------------------*/
/* uint8_t tasks::taskMfreeAll(task_desc_t* task_p)                                                  */
/* Description:   This is a private method which frees all memory previously allocated by a task     */
/*                with taskMalloc method and unregisters it                                          */
/* Properties:    Private method.                                                                    */
/*                This method is thread SAFE, it is safe to use this method while the root- or       */
/*                task object is trying to be modified.                                              */
/*                The execution time is almost linear with the amount of linked task memory resource */
/*                desctiptors.                                                                       */
/* Return codes:  [_TASKS_OK_= 0 | _TASKS_RESOURCES_EXHAUSTED | _TASKS_API_PARAMETER_ERROR_ |        */
/*                | _TASKS_GENERAL_ERROR_, _TASKS_NOT_FOUND_ | _TASKS_MUTEX_TIMEOUT_]                */
/* Parameters:    vtask_desc_t* task_p: The task decriptor pointer.                                  */
/*                                                                                                   */
uint8_t tasks::taskMfreeAll(task_desc_t* task_p) {
	
	if (task_p == NULL) {
		logdAssert(_ERR_, "API ERROR, task_p = NULL");
		return(_TASKS_API_PARAMETER_ERROR_);
	}
	//logdAssert(_DEBUG_, "Freing all heap memory allocated by task: %s", task_p->pcName);
	taskInitMutexTake(task_p);
	//#ifdef debug

	if (task_p->dynMemObjects_p == NULL) {
		//logdAssert(_DEBUG_, "No heap memory to free for task %s", task_p->pcName);
	}
	else {
		//logdAssert(_DEBUG_, "There are heap memory objects for task %s, freeng those", task_p->pcName);
	}
	while (task_p->dynMemObjects_p != NULL) {
		//logdAssert(_DEBUG_, "Freing Memory resource descriptor %p alocated by task %s", task_p->dynMemObjects_p, task_p->pcName);
		if (uint8_t res = taskMfree(task_p, (task_p->dynMemObjects_p->taskResource_p))) {
			logdAssert(_INFO_, "Could not free memory, result code: %u", res);
			taskInitMutexGive(task_p);
			return(res);
		}
	} 
	taskInitMutexTake(task_p);
	return(_TASKS_OK_);
}
/* ------------------------------------ END PRIVATE:taskMfreeAll ------------------------------------*/

/* -------------------------------------- PRIVATE:incHeapStats --------------------------------------*/
/* void tasks::incHeapStats(task_desc_t* task_p, int size)                                           */ 
void tasks::incHeapStats(task_desc_t* task_p, int size) {
	task_p->heapCurr += size;
	//logdAssert(_DEBUG_, "Task %s is currently using %u of the heap, the warn level is set to %u and heapwarn is %u", task_p->pcName, task_p->heapCurr, task_p->heapHighWatermarkWarnAbs, task_p->heapWarn);
	if (task_p->heapCurr >= task_p->heapHighWatermarkAbs) {
		task_p->heapHighWatermarkAbs = task_p->heapCurr;
	}
	if (task_p->heapCurr >= task_p->heapHighWatermarkWarnAbs && task_p->heapHighWatermarkKBWarn &&!task_p->heapWarn) {
		task_p->heapWarn = true;
		logdAssert(_WARN_, "Task %s has reached warning level %u, High water mark is %u", task_p->pcName, task_p->heapHighWatermarkWarnAbs, task_p->heapHighWatermarkAbs);
	}
}
/* ---------------------------------------- END:incHeapStats ----------------------------------------*/

/* -------------------------------------- PRIVATE:decHeapStats --------------------------------------*/
void tasks::decHeapStats(task_desc_t* task_p, int size) {
	task_p->heapCurr -= size;
	if (task_p->heapCurr < task_p->heapHighWatermarkWarnAbs && task_p->heapHighWatermarkKBWarn && task_p->heapWarn) {
		task_p->heapWarn = false;
		logdAssert(_INFO_, "Task %s has no longer exceeded the heap warning watermark", task_p->pcName);
	}
}
/* ---------------------------------------- END:decHeapStats ----------------------------------------*/

/* ******************************** END Memory resources management ******************************** */
