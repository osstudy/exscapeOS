#ifndef _MUTEX_H
#define _MUTEX_H

#include <kernel/task.h> /* task_t */

typedef struct {
	task_t *owner;
	uint32 mutex;
} mutex_t;

mutex_t *mutex_create(void);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);


#endif