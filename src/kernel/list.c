/*
 * An implementation of a doubly-linked list. Stores pointers only, or anything else
 * that can be cast to a void *.
 * The implementation isn't really optimal; it especially wastes lines of code by
 * implementing adding to a list in 4 rather than 2 functions, due to an oversight
 * while planning... */

#include <types.h>
#include <kernel/kernutil.h>
#include <kernel/kheap.h>
#include <kernel/list.h>

#define LIST_DEBUG 1

static void list_validate(list_t *list) {
	/* Runs some debug checks to make sure the list isn't corrupt. */
	assert(list != NULL);

	if (list->count == 0) {
		/* Exit early for empty lists */
		assert(list->head == NULL && list->tail == NULL);
		return;
	}

	assert(list->head != NULL);
	assert(list->tail != NULL);
	if (list->count > 1)
		assert(list->head != list->tail);

	assert(list->head->prev == NULL);
	assert(list->tail->next == NULL);

	uint32 actual_count = 0;
	/* Test the links */
	/* Also test whether the count is correct or not */
	for (node_t *it = list->head; it != NULL; it = it->next) {
		actual_count++;
		assert(it->list == list);
		if (it->next != NULL)
			assert(it->next->prev == it);
	}

	assert(actual_count == list->count);
}

node_t *list_prepend(list_t *list, void *data) {
	assert(list != NULL);

	if (list->head != NULL) {
		/* The list has at least one element */

		node_t *old_head = list->head;

		/* Set up a node */
		node_t *new = kmalloc(sizeof(node_t));
		new->data = data;
		new->prev = NULL;
		new->next = old_head;
		new->list = list;

		/* Set up the second-first node */
		assert(old_head->prev == NULL);
		old_head->prev = new;

		/* Set up the list itself */
		assert(list->head == old_head);
		list->head = new;
		list->count++;

#if LIST_DEBUG > 0
		list_validate(list);
#endif

		return new;
	}
	else {
		/* The list is empty. Make sure the entire state reflects this. */
		assert(list->tail == NULL);
		assert(list->count == 0);

		/* Set up a node */
		node_t *new = kmalloc(sizeof(node_t));
		new->data = data;
		new->next = NULL;
		new->prev = NULL;
		new->list = list;

		/* Insert it into the list */
		list->count++;
		list->head = new;
		list->tail = new;
		assert(list->count == 1);

#if LIST_DEBUG > 0
		list_validate(list);
#endif

		return new;
	}
}

list_t *list_create(void) {
	list_t *new = kmalloc(sizeof(list_t));

	new->head = NULL;
	new->tail = NULL;
	new->count = 0;

#if LIST_DEBUG > 0
	list_validate(new);
#endif

	return new;
}

node_t *list_append(list_t *list, void *data) {
	assert(list != NULL);

	if (list->tail != NULL) {
		node_t *new = kmalloc(sizeof(node_t));

		/* set up the new node */
		new->data = data;
		new->prev = list->tail;
		new->next = NULL;
		new->list = list;

		/* update the penultimate node */
		assert(list->tail->next == NULL);
		list->tail->next = new;

		/* update the list itself */
		list->tail = new;
		list->count++;

#if LIST_DEBUG > 0
		list_validate(list);
#endif

		return new;
	}
	else {
		/* The tail pointer is NULL, which should mean that this list is empty. Thus
		 * the head and count members should also indicate this. */
		assert(list->head == NULL);
		assert(list->count == 0);

		/* OK, so the list is empty. No problem, but we can't use the exact same procedure. */
		node_t *new = kmalloc(sizeof(node_t));

		/* set up the node */
		new->data = data;
		new->prev = NULL;
		new->next = NULL;
		new->list = list;

		/* update the list */
		list->head = new;
		list->tail = new;
		list->count++;
		assert(list->count == 1);

#if LIST_DEBUG > 0
		list_validate(list);
#endif
		return new;
	}
}

/* Creates a new node, and inserts it before the node specified */
node_t *list_node_insert_before(node_t *node, void *data) {
	assert(node != NULL);
	assert(node->list != NULL);
	list_t *list = node->list;

	if (node->prev == NULL) {
		/* This node in the head of the list, and we want to insert a new node *before* it -
		 * which means we want the new node to be the new head. Easy! */
		assert(list->head == node);
		return list_prepend(list, data);
	}
	else {
		/* We're inserting to somewhere in the middle of the list */
		assert(list->head != node);
		node_t *new = kmalloc(sizeof(node_t));

		/* set up the new node */
		new->data = data;
		new->prev = node->prev;
		new->next = node;
		new->list = list;

		/* update the preceding node */
		assert(new->prev != NULL);
		new->prev->next = new;

		/* update the node following (/node/) */
		node->prev = new;

		/* update the list */
		list->count++;
		/* head and tail are unchanged no matter what here */

#if LIST_DEBUG > 0
		list_validate(list);
#endif

		return new;
	}
}

/* Creates a new node, and inserts it after the node specified */
node_t *list_node_insert_after(node_t *node, void *data) {
	assert(node != NULL);
	assert(node->list != NULL);
	list_t *list = node->list;

	if (node->next == NULL) {
		/* This node in the tail of the list, and we want to insert a new node *after* it - that's easy! */
		assert(list->tail == node);
		return list_append(list, data);
	}
	else {
		/* We're inserting to somewhere in the middle of the list */
		assert(list->tail != node);
		assert(node->next != NULL);
		node_t *new = kmalloc(sizeof(node_t));

		/* set up the new node */
		new->data = data;
		new->prev = node;
		new->next = node->next;
		new->list = list;

		/* update the preceding node (/node/) */
		node->next = new;

		/* update the node following */
		new->next->prev = new;

		/* update the list */
		list->count++;
		/* head and tail are unchanged no matter what here */

#if LIST_DEBUG > 0
		list_validate(list);
#endif

		return new;
	}
}

void list_remove(list_t *list, node_t *elem) {
	/* Find /elem/ and remove it from the list */
	assert(list != NULL);
	assert(elem != NULL);
	assert(elem->list == list);

	/* Update the node before this one */
	if (elem->prev != NULL)
		elem->prev->next = elem->next;

	/* Update the node after this one */
	if (elem->next != NULL)
		elem->next->prev = elem->prev;

	/* Update the list itself */
	list->count--;
	if (elem == list->head)
		list->head = elem->next;
	if (elem == list->tail)
		list->tail = elem->prev;

#if LIST_DEBUG > 0
		list_validate(list);
#endif

	kfree(elem);
}

/* Destroys an entire list, and frees all elements */
void list_destroy(list_t *list) {
	assert(list != NULL);

#if LIST_DEBUG > 0
		list_validate(list);
#endif

	node_t *it = list->head;
	while (it != NULL) {
		node_t *next = it->next;
		kfree(it);
		it = next;
	}

	/* Just in case */
	list->head = NULL;
	list->tail = NULL;
	list->count = 0;

	kfree(list);
}

node_t *list_find_first(list_t *list, void *data) {
	/* Finds the first node where node->data == data, and returns it. */
	assert(list != NULL);
	assert(list->head != NULL);

#if LIST_DEBUG > 0
		list_validate(list);
#endif

	for (node_t *it = list->head; it != NULL; it = it->next) {
		if (it->data == data)
			return it;
	}

	return NULL;
}

node_t *list_find_last(list_t *list, void *data) {
	/* Finds the last node where node->data == data, and returns it. */
	assert(list != NULL);
	assert(list->tail != NULL);

#if LIST_DEBUG > 0
		list_validate(list);
#endif

	for (node_t *it = list->tail; it != NULL; it = it->prev) {
		if (it->data == data)
			return it;
	}

	return NULL;
}

node_t *list_node_find_next_predicate(node_t *node, bool (*predicate_func)(node_t *) ) {
	assert(node != NULL);
	assert(predicate_func != NULL);
	/* Look through the remainer of the list first */
	for (node_t *it = node->next; it != NULL; it = it->next) {
		if (predicate_func(it))
			return it;
	}

	/* Restart at the beginning of the list and try up to the point where we started */
	/* Note that /node/ itself is not tested, because the function should return the NEXT match,
	 * not the one the user passed in! */
	for (node_t *it = node->list->head; it != node; it = it->next) {
		if (predicate_func(it)) {
			assert(it != node); /* TODO: remove this check after debugging */
			return it;
		}
	}

	/* We didn't find anything! */
	return NULL;
}

#if 0
/* This function is meant for debugging only. */
node_t *RAND_ELEMENT(list_t *list) {
	assert(list != NULL);
	int num = RAND_RANGE(0, list->count - 1);

	int i=0;
	for (node_t *it = list->head; it != NULL; it = it->next) {
		if (i == num)
			return it;
		i++;
	}

	assert(1 == 0); /* shouldn't be reached */
	return NULL;
}
#endif
