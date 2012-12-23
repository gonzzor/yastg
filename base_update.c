#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "base.h"
#include "item.h"
#include "log.h"
#include "universe.h"

#define BASE_UPDATE_INTERVAL 10		/* in seconds, no larger than once a day */

#define SECONDS_PER_DAY (24 * 60 * 60)
#define BASE_UPDATE_FRACTION (SECONDS_PER_DAY / BASE_UPDATE_INTERVAL)

pthread_t thread;
volatile int terminate;

pthread_condattr_t termination_attr;
pthread_cond_t termination_cond;
pthread_mutex_t termination_lock;

static void update_base(struct base *base, uint32_t iteration)
{
	struct base_item *item;
	long change, mod, fraction_iteration;

	pthread_rwlock_wrlock(&base->items_lock);

	list_for_each_entry(item, &base->items, list) {
		if (!item->daily_change)
			continue;

		change = item->daily_change / BASE_UPDATE_FRACTION;
		mod = item->daily_change % BASE_UPDATE_FRACTION;
		fraction_iteration = BASE_UPDATE_FRACTION / item->daily_change;

		if (mod && item->daily_change && fraction_iteration
				&& iteration % fraction_iteration == 0) {
			if (item->daily_change > 0)
				change++;
			else
				change--;
		}

		if (change < 0 && item->amount < -change)
			change = -item->amount;
		else if (change > 0 && item->max - item->amount < change)
			change = item->max - item->amount;

		item->amount += change;
	}

	pthread_rwlock_unlock(&base->items_lock);
}

static void update_all_bases(uint32_t iteration)
{
	struct base *base;

	list_for_each_entry(base, &univ.bases, list)
		update_base(base, iteration);
}

static void* base_update_worker(void *ptr)
{
	struct timespec next, now;
	uint32_t iteration = 0;

	if (clock_gettime(CLOCK_MONOTONIC, &next))
		return NULL;

	do {
		pthread_rwlock_rdlock(&univ.bases_lock);
		update_all_bases(iteration);
		pthread_rwlock_unlock(&univ.bases_lock);

		iteration++;
		if (iteration >= BASE_UPDATE_FRACTION)
			iteration = 0;

		next.tv_sec += BASE_UPDATE_INTERVAL;

		do {
			pthread_mutex_lock(&termination_lock);
			pthread_cond_timedwait(&termination_cond, &termination_lock, &next);
			pthread_mutex_unlock(&termination_lock);

			if (clock_gettime(CLOCK_MONOTONIC, &now))
				break;

		} while (!terminate && now.tv_sec < next.tv_sec);

	} while (!terminate);

	return NULL;
}

int start_updating_bases(void)
{
	int r;

	if (pthread_condattr_init(&termination_attr))
		return -1;

	if (pthread_condattr_setclock(&termination_attr, CLOCK_MONOTONIC))
		return -1;

	if (pthread_mutex_init(&termination_lock, NULL))
		return -1;

	if (pthread_cond_init(&termination_cond, &termination_attr))
		return -1;

	if (pthread_create(&thread, NULL, base_update_worker, NULL))
		return -1;

	return 0;
}

void stop_updating_bases(void)
{
	terminate = 1;
	pthread_mutex_lock(&termination_lock);
	pthread_cond_signal(&termination_cond);
	pthread_mutex_unlock(&termination_lock);

	pthread_join(thread, NULL);

	pthread_cond_destroy(&termination_cond);
	pthread_condattr_destroy(&termination_attr);
	pthread_mutex_destroy(&termination_lock);
}
