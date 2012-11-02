#include <string.h>
#include <kernel/timer.h>
#include <kernel/task.h>
#include <kernel/kernutil.h>
#include <kernel/kheap.h>
#include <kernel/kworker.h>

/*
 * This file implements a task that does miscellaneous kernel-mode tasks,
 * such as handle received network packets, which would otherwise be
 * handled in code called from ISRs, where interrupts would be disabled
 * and the OS "frozen". Since having the OS "stopped" like that for longer
 * periods of time is unacceptable, the ISR instead tells the kernel worker
 * to take care of it.
 */

kworker_t *kworker_arp = NULL;
kworker_t *kworker_icmp = NULL;

typedef struct {
	void (*function)(void *, uint32);
	void *data;
	uint32 length; // data size in bytes
	uint8 priority;
} kworker_task_t;

kworker_t *kworker_create(const char *name) {
	assert(strlen(name) + 1 <= KWORKER_NAME_SIZE);
	kworker_t *worker = kmalloc(sizeof(kworker_t));
	strlcpy(worker->name, name, KWORKER_NAME_SIZE);
	worker->tasks = list_create();
	worker->task = create_task(kworker_task, name, &kernel_console, worker, sizeof(kworker_t));

	return worker;
}

void kworker_add(kworker_t *worker, void (*func)(void *, uint32), void *data, uint32 length, uint8 priority) {
	assert(worker != NULL);
	assert(data != NULL);
	assert(length > 0);

	kworker_task_t *task = kmalloc(sizeof(kworker_task_t));
	task->function = func;

	if (length > 0) {
		task->data = kmalloc(length);
		memcpy(task->data, data, length);
	}
	else
		task->data = NULL;

	task->length = length;
	task->priority = priority;

	list_append(worker->tasks, task);
}

static void kworker_remove(kworker_t *worker, node_t *node) {
	assert(worker != NULL);
	assert(worker->tasks != NULL);
	assert(node != NULL);

	// It would be less ugly to pass the task as a parameter instead,
	// but that would mean we would have to use list_find_first() to
	// find the node again, and pass that to list_remove().
	// Efficiency wins over beauty here.
	kworker_task_t *task = node->data;

	if (task->data)
		kfree(task->data);
	list_remove(worker->tasks, node);
	kfree(task);
}

// The *process* that does all the work. I'll try to come up with better naming
// than to have "task" mean two different things...
void kworker_task(void *data, uint32 length) {
	kworker_t *worker = (kworker_t *)data;

	while (true) {
		while (list_size(worker->tasks) == 0) {
			// Nothing to do; switch to some other task, that can actually do something
			asm volatile("int $0x7e");
		}

		if (list_size(worker->tasks) == 1) {
			// No point in checking priority if there's only one task! Take care of this one.
			node_t *node = worker->tasks->head;
			kworker_task_t *task = (kworker_task_t *)node->data;

			assert(task->function != NULL);
			task->function(task->data, task->length);
			kworker_remove(worker, node);
		}
		else {
			// Run the task with the highest priority first. Priority is a simple integer
			// level, so 255 is the maximum, while 0 is the minimum.
			node_t *max = worker->tasks->head;
			for (node_t *it = worker->tasks->head; it != NULL; it = it->next) {
				kworker_task_t *task = (kworker_task_t *)it->data;
				if (task->priority > ((kworker_task_t *)it->data)->priority) {
					max = it;
				}
			}
			kworker_task_t *max_task = (kworker_task_t *)max->data;
			assert(max_task != NULL);
			assert(max_task->function != NULL);

			max_task->function(max_task->data, max_task->length);
			kworker_remove(worker, max);
		}
	}
}
