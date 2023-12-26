#include "mimpi.h"
#include "examples/mimpi_err.h"

int main(int argc, char **argv)
{
    MIMPI_Init(false);

    int const world_rank = MIMPI_World_rank();
//    int const tag = 17;

    char number;
    if (world_rank == 0)
    {
        number = 42;
        ASSERT_MIMPI_OK(MIMPI_Send(&number, 1, 1, 2));

    }
    else if (world_rank == 1)
    {
        ASSERT_MIMPI_OK(MIMPI_Recv(&number, 1, 0, 0));
        printf("Process 1 received number %d with tag %d\n", number, 0);
    }

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
