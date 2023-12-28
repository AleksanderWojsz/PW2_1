#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "mimpi.h"
#include "examples/mimpi_err.h"

char data[21372137];

int main(int argc, char **argv)
{
    MIMPI_Init(false);

    int const world_rank = MIMPI_World_rank();

    memset(data, world_rank == 0 ? 42 : 7, sizeof(data));

    int const tag = 17;

    if (world_rank == 0) {

        ASSERT_MIMPI_OK(MIMPI_Send(data, sizeof(data), 1, tag));
        for (int i = 0; i < sizeof(data); i += 789) {
            assert(data[789] == 42);
        }
    }
    else if (world_rank == 1)
    {
        ASSERT_MIMPI_OK(MIMPI_Recv(data, sizeof(data), 0, tag));
        for (int i = 0; i < sizeof(data); i += 789) {
            assert(data[789] == 42);
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

// TODO grupowe
// TODO wiadomości mniejsze niz 512B
// TODO assert_sys_ok
// TODO co jak na starcie będzie zajęty deskryptor 19
// TODO szukanie wiadomosci nie od poczatku
// TODO chsend zwraca EPIPE jeśli koniec do odczytu łącza jest zamknięty więc możesz wiedzieć od razu że nie da się wysłać



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
