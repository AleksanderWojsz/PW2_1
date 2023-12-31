
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "mimpi.h"
#include "examples/mimpi_err.h"
#define WRITE_VAR "CHANNELS_WRITE_DELAY"


static unsigned factorial(unsigned n) {
    if (n == 0 || n == 1) {
        return 1;
    } else {
        unsigned result = 1;
        for (unsigned i = 2; i <= n; i++) {
            result *= i;
        }
        return result;
    }
}

#define DATA_LEN 100

int main(int argc, char **argv)
{
    MIMPI_Init(false);

    int const world_rank = MIMPI_World_rank();
    int const world_size = MIMPI_World_size();

    MIMPI_Op const ops[] = {MIMPI_MAX};
    uint8_t const results[] = {world_size};

    uint8_t send_data[DATA_LEN];
    memset(send_data, world_rank + 1, DATA_LEN);
    uint8_t recv_data[DATA_LEN];

    for (int i = 0; i < sizeof(ops) / sizeof(MIMPI_Op); ++i) {
        MIMPI_Op const op = ops[i];
        int const root = 0;
        ASSERT_MIMPI_OK(MIMPI_Reduce(send_data, recv_data, DATA_LEN, op, root));
        if (world_rank == root) {
            uint8_t const result = results[i];
            for (int k = 0; k < DATA_LEN; ++k) {
                // if (recv_data[k] != result) {
                //     fprintf(stderr, "MISMATCH: op_idx %i, root %i, k=%i: expected %i, got %i.\n", i, root, k, result, recv_data[k]);
                // }
                assert(recv_data[k] == result);
            }
        }
    }

    MIMPI_Finalize();
    return 0;
}






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

 */
