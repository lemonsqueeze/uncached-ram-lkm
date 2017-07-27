### uncached ram lkm

map uncached memory in userspace experiment.

**How to use**

User process requests uncached memory by calling `mmap()` on module's character device (see kern.log for major number).
type must be `MAP_SHARED` and size must be a multiple of page size.
See `test/test.c` for example doing read benchmarks on cached / uncached memory.

**Demo**

    # make
    make -C /lib/modules/4.4.0-45-generic/build/ M=/root/src/uncached_ram_lkm modules
    make[1]: Entering directory `/usr/src/linux-headers-4.4.0-45-generic'
      CC [M]  /root/src/uncached_ram_lkm/uncached_ram.o
      Building modules, stage 2.
      MODPOST 1 modules
      CC      /root/src/uncached_ram_lkm/uncached_ram.mod.o
      LD [M]  /root/src/uncached_ram_lkm/uncached_ram.ko
    make[1]: Leaving directory `/usr/src/linux-headers-4.4.0-45-generic'

    # insmod uncached_ram.ko

    # tail /var/log/kern.log
    Jul 27 20:42:44 localhost kernel: [115315.205744] Uncached ram module loaded
    Jul 27 20:42:44 localhost kernel: [115315.205764] Created char device, major: 249

    # cd test

    # mknod dev c 249 0

    # make
    gcc -Wall -ggdb3 -std=gnu99     test.c   -o test

    # ./test
    usage: ./test cached   uncached_mem_dev
           ./test uncached uncached_mem_dev

    # ./test cached dev
    cached mem test: 104856600 reads in 1.06s                                     (sum: 0)

    # ./test uncached dev
    mmap()'ing dev
    uncached mem test: 104856600 reads in 10.73s                                     (sum: -138595026)

    