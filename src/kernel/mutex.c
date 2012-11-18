#include <kernel/kheap.h>
#include <kernel/task.h>
#include <kernel/mutex.h>
#include <kernel/kernutil.h>

mutex_t *mutex_create(void) {
	mutex_t *mutex = kmalloc(sizeof(mutex_t));
	mutex->owner = NULL;
	mutex->mutex = 0;

	return mutex;
}

extern volatile bool in_isr;
extern volatile bool task_switching;

void mutex_destroy(mutex_t *mutex) {
	assert(mutex->mutex == 0); // Must not be locked
	assert(mutex->owner == NULL); // Set during unlock
	kfree(mutex);
}


#define DISABLE_MUTEXES 0 // for bochs, temporarily

void mutex_lock(mutex_t *mutex) {
	assert(mutex != NULL);
#if (!DISABLE_MUTEXES)
	uint8 success = 0;

	while (success == 0) {
		asm volatile("LOCK BTSL $0, %[mutex];"
					 "SETNCB %[success];"
					 :
					  [mutex]"=m"(mutex->mutex),
					  [success]"=m"(success)
					  : : "cc", "memory");


		if (success) {
			mutex->owner = (task_t *)current_task;
		}
		else {
			if (task_switching)
				asm volatile("int $0x7e");
		}
	}
#else
	disable_interrupts();
#endif
}

void mutex_unlock(mutex_t *mutex) {
	assert(mutex != NULL);
#if (!DISABLE_MUTEXES)
	assert(mutex->mutex != 0); // mutex is locked
	assert(mutex->owner == current_task);
	mutex->mutex = 0;
	mutex->owner = NULL;
#else
	enable_interrupts();
#endif
}

bool mutex_is_locked(mutex_t *mutex) {
#if DISABLE_MUTEXES
	return false;
#else
	return mutex->mutex != 0;
#endif
}
