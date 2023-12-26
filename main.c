#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "mimpi.h"

#define NS_PER_1_MS 1 ## 000 ## 000

int main(int argc, char **argv)
{
    MIMPI_Init(false);

    int const world_rank = MIMPI_World_rank();

    int const tag = 17;

    char number = 42;
    if (world_rank == 0) {
        struct timespec ts = (struct timespec){.tv_sec = 0, .tv_nsec = NS_PER_1_MS};
        int res;
        do {
            res = nanosleep(&ts, &ts);
        } while (res && errno == EINTR);
    } else if (world_rank == 1) {
        assert(MIMPI_Recv(&number, 1, 0, tag) == MIMPI_ERROR_REMOTE_FINISHED);
        assert(MIMPI_Send(&number, 1, 0, tag) == MIMPI_ERROR_REMOTE_FINISHED);
        printf("Process 1 received number %d from process 0\n", number);
    }

    // Process with rank 0 finishes before rank 1 gets its message.
    MIMPI_Finalize();
    return 0;
}



/*
 make
 ./mimirun

 cd examples_build
 ./send_recv

 bash ./test

 valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./mimpirun 2 ./main



Procedury pomocnicze:
  void MIMPI_Init(bool enable_deadlock_detection)
  void MIMPI_Finalize()
  int MIMPI_World_size()
  void MIMPI_World_rank()

Procedury do komunikacji punkt-punkt
 MIMPI_Retcode MIMPI_Send(void const *data, int count, int destination, int tag)
 MIMPI_Retcode MIMPI_Recv(void *data, int count, int source, int tag)

 Procedury do komunikacji grupowej
  MIMPI_Retcode MIMPI_Barrier()
  MIMPI_Retcode MIMPI_Bcast(void *data, int count, int root)
  MIMPI_Retcode MIMPI_Reduce(const void *send_data, void *recv_data, int count, MPI_Op op, int root)

 */
