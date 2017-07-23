#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>


void usage(int ac, char **av)
{
	printf("usage: %s cached   addr\n", av[0]);
	printf("       %s uncached addr\n", av[0]);
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

void *get_uncached_mem(void *base_addr, int size)
{
	int fd = open("/dev/mem", O_RDWR | O_SYNC, 0);
	if (fd == -1) die("couldn't open /dev/mem, are you root ?");
	
	printf("mmap()'ing %p\n", base_addr);
	
	void *map = mmap(0, size, PROT_READ /*| PROT_WRITE*/, MAP_FILE | MAP_SHARED, fd, (off_t)base_addr);
	if (map == MAP_FAILED)
		die("mmap failed. Note: we need a kernel without CONFIG_STRICT_DEVMEM.");	
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

	void *base_addr = (void*)strtoul(av[2], NULL, 16);

	void *map;
	if (uncached_mem_test)
		map = get_uncached_mem(base_addr, size);
	else    
		map = malloc(size);	/* test normal memory */

	unsigned int *pt = ((unsigned int*)map);
	/* uncached mem should start with 0xdeadbeaf */
	printf("read %#0x\n", *pt);
	if (uncached_mem_test) assert(*pt == 0xdeadbeaf);
	//printf("sizeof(off_t): %i\n", sizeof(off_t));

	/*********************************************************************/
	/* benchmark reads */

	double time_start = time_now();

	int tsize = size / sizeof(int);
	//int t[tsize];
	#define STEP 10
	unsigned int sum = 0;
	int reads = 0;
	for (int i = 0; i < tsize - STEP; i++)
		for (int j = 0; j < STEP; j++) {
			sum += pt[i+j];
			reads++;
		}

	printf("%s mem test: %i reads in %.2fs                                     (sum: %i)\n", 
	       (uncached_mem_test ? "uncached" : "cached"),
	       reads, time_now() - time_start, sum);
}
