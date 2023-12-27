#include <stdbool.h>
#include <stdio.h>
#include "mimpi.h"
#include "examples/mimpi_err.h"

int main(int argc, char **argv)
{
    MIMPI_Init(false);
    int const world_rank = MIMPI_World_rank();


    char number;
    if (world_rank == 0)
    {
        number = 42;
        MIMPI_Send(&number, 1, 1, 1);
    }
    else if (world_rank == 1)
    {
        if (MIMPI_Recv(&number, 1, 0, 2) != 3) {
            printf("blad\n");
        }
    }

    MIMPI_Finalize();
    return 0;
}


//#include <stdbool.h>
//#include <stdio.h>
//#include "mimpi.h"
//#include "examples/mimpi_err.h"
//
//char data[21372137];
////char data[10000];
////char data[1000000000];
//
//int main(int argc, char **argv)
//{
//    MIMPI_Init(false);
//
//    int const world_rank = MIMPI_World_rank();
//    memset(data, 42, sizeof(data));
//
//    if (world_rank == 0) {
//        MIMPI_Send(data, sizeof(data), 1, 999);
//    }
//    else if (world_rank == 1)
//    {
//        MIMPI_Send(data, sizeof(data), 0, 999);
//    }
//
////    sleep(1);
//
//    MIMPI_Finalize();
//    printf("Skonczylem, jestem %d\n", world_rank);
//
//    return 0;
//}



/*
 make
 ./mimirun

 cd examples_build
 ./send_recv

 bash ./test

 ./mimpirun 2 valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./main

 chmod +x test

 chmod -R 777 ścieżka/do/folderu

for i in {1..100}
do
   echo $i
   ./mimpirun 2 ./main
done


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
