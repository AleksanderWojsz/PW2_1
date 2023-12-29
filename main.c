#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "mimpi.h"
#include "examples/mimpi_err.h"

#define WRITE_VAR "CHANNELS_WRITE_DELAY"

int main(int argc, char **argv)
{
    MIMPI_Init(false);
    int const world_rank = MIMPI_World_rank();

    const char *delay = getenv("DELAY");
    if (delay)
    {
        int res = setenv(WRITE_VAR, delay, true);
        assert(res == 0);
    }

    char number = 0;
    if (world_rank == 0)
        number = 42;
    ASSERT_MIMPI_OK(MIMPI_Bcast(&number, 1, 0));
    printf("Number: %d\n", number);
    fflush(stdout);

    int res = unsetenv(WRITE_VAR);
    assert(res == 0);

    MIMPI_Finalize();
    return 0;
}

//#include <stdbool.h>
//#include <stdio.h>
//#include <stdint.h>
//#include <string.h>
//#include <assert.h>
//#include "mimpi.h"
//#include "examples/mimpi_err.h"
//#define WRITE_VAR "CHANNELS_WRITE_DELAY"
//
//static unsigned factorial(unsigned n) {
//    if (n == 0 || n == 1) {
//        return 1;
//    } else {
//        unsigned result = 1;
//        for (unsigned i = 2; i <= n; i++) {
//            result *= i;
//        }
//        return result;
//    }
//}
//
//#define DATA_LEN 213721
//
//int main(int argc, char **argv)
//{
//    MIMPI_Init(false);
//
//    int const world_rank = MIMPI_World_rank();
//    int const world_size = MIMPI_World_size();
//
//    MIMPI_Op const ops[] = {MIMPI_PROD, MIMPI_SUM, MIMPI_MIN, MIMPI_MAX};
//    uint8_t const results[] = {factorial(world_size), (world_size + 1) * world_size / 2, 1, world_size};
//
//    uint8_t send_data[DATA_LEN];
//    memset(send_data, world_rank + 1, DATA_LEN);
//    uint8_t recv_data[DATA_LEN];
//
//    for (int i = 0; i < sizeof(ops) / sizeof(MIMPI_Op); ++i) {
//        MIMPI_Op const op = ops[i];
//        int const root = 1;
//        ASSERT_MIMPI_OK(MIMPI_Reduce(send_data, recv_data, DATA_LEN, op, root));
//        if (world_rank == root) {
//            uint8_t const result = results[i];
//            for (int k = 0; k < DATA_LEN; ++k) {
//                // if (recv_data[k] != result) {
//                //     fprintf(stderr, "MISMATCH: op_idx %i, root %i, k=%i: expected %i, got %i.\n", i, root, k, result, recv_data[k]);
//                // }
//                assert(recv_data[k] == result);
//            }
//        }
//    }
//
//    printf("ok\n");
//
//    MIMPI_Finalize();
//    return 0;
//}



//#include <assert.h>
//#include <stdbool.h>
//#include <string.h>
//#include "mimpi.h"
//#include "examples/mimpi_err.h"
//
//
//#define WRITE_VAR "CHANNELS_WRITE_DELAY"
//
//#define LARGE_MESSAGE_SIZE 1000000
//char data[LARGE_MESSAGE_SIZE];
//
//int main(int argc, char **argv)
//{
//    MIMPI_Init(false);
//    int const world_rank = MIMPI_World_rank();
//    memset(data, world_rank == 0 ? 42 : 7, LARGE_MESSAGE_SIZE);
//
//
//    const char *delay = getenv("DELAY");
//    if (delay) {
//        int res = setenv(WRITE_VAR, delay, 1);
//        assert(res == 0);
//    }
//    ASSERT_MIMPI_OK(MIMPI_Bcast(data, LARGE_MESSAGE_SIZE, 0));
//    int res = unsetenv(WRITE_VAR);
//    assert(res == 0);
//
//
//    for (int i = 200; i < LARGE_MESSAGE_SIZE; i += 200) {
////        printf("Sample Data: %d\n", data[i]);
//        assert(data[i] == 42);
//    }
//    printf("Sample Data: %d\n", data[LARGE_MESSAGE_SIZE - 10]);
//    fflush(stdout);
//
//    MIMPI_Finalize();
//    return 0;
//}


//
//#include <stdbool.h>
//#include <stdio.h>
//#include <string.h>
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


// TODO czy MIMPI_ERROR_REMOTE_FINISHED trzeba sprawdzać tylko w MIMPI_BARRIER

// TODO wiadomości mniejsze niz 512B
// TODO zrobic zeby world rank i size czytalo z pipea tylko raz
// TODO assert_sys_ok
// TODO co jak na starcie będzie zajęty deskryptor 19
// TODO szukanie wiadomosci nie od poczatku
// TODO chsend zwraca EPIPE jeśli koniec do odczytu łącza jest zamknięty więc możesz wiedzieć od razu że nie da się wysłać

// TODO co miało być wysłane przed barierą będzie wysłane bo send musi się skończyć
// TODO nie przeszkadza nam że nie wszystko będzie odebrane podczas beriery (gdzie indziej też nam nie przeszkadza)
// TODO bo to co odebraliśmy/będziemy odbierali interesuje nas tylko jak robimy receive


// https://pubs.opengroup.org/onlinepubs/9699919799/functions/pipe.html     - pipe zwraca zawsze dwa najmniejsze wolne deskryptory
// https://pubs.opengroup.org/onlinepubs/009604599/functions/pipe.html
// https://stackoverflow.com/questions/29852077/will-a-process-writing-to-a-pipe-block-if-the-pipe-is-full#comment47830197_29852077 - write do pełnego pipe'a jest blokujący, read z pustego też

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
