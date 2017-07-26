#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

void usage(int ac, char **av)
{
	printf("usage: %s cached   uncached_mem_dev\n", av[0]);
	printf("       %s uncached uncached_mem_dev\n", av[0]);
	exit(1);
}

void die(char *msg)
{
	printf("%s\n", msg);
	exit(1);
}

/* Returns the current time. */
double time_now(void)
{
#if _POSIX_TIMERS > 0
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        return now.tv_sec + now.tv_nsec/1000000000.0;
#else
        struct timeval now;
        gettimeofday(&now, NULL);
        return now.tv_sec + now.tv_usec/1000000.0;
#endif
}

#define PAGE_SIZE (sysconf(_SC_PAGESIZE))
#define PAGE_MASK (~(PAGE_SIZE - 1))

void *get_uncached_mem(char *dev, int size)
{	
	assert(PAGE_SIZE != -1);
	
	int fd = open(dev, O_RDWR, 0);
	if (fd == -1) die("couldn't open device");
	
	printf("mmap()'ing %s\n", dev);

	if (size & ~PAGE_MASK)
		size = (size & PAGE_MASK) + PAGE_SIZE;

	void *map = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED)
		die("mmap failed.");
	return map;
}

int main(int ac, char **av)
{
	int size = 1024 * 4096;  // FIXME
	
	if (ac != 3)
		usage(ac, av);

	int uncached_mem_test;
	if      (!strcmp(av[1], "uncached"))  uncached_mem_test = 1;
	else if (!strcmp(av[1], "cached"))    uncached_mem_test = 0;
	else    usage(ac, av);

	void *map;
	if (uncached_mem_test)
		map = get_uncached_mem(av[2], size);
	else    
		map = malloc(size);	/* test normal memory */


	/*********************************************************************/
	/* Read benchmarks */

	double time_start = time_now();

	unsigned int *pt = ((unsigned int*)map);
	int tsize = size / sizeof(int);
	#define STEP 10
	unsigned int sum = 0;
	unsigned int reads = 0;
	for (int i = 0; i < tsize - STEP; i++)
		for (int k = 0; k < 10; k++)
		for (int j = 0; j < STEP; j++) {
			sum += pt[i+j];
			reads++;
		}

	printf("%s mem test: %u reads in %.2fs                                     (sum: %i)\n", 
	       (uncached_mem_test ? "uncached" : "cached"),
	       reads, time_now() - time_start, sum);
}
