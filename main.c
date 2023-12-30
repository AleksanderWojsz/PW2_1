#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "mimpi.h"
#include "examples/mimpi_err.h"
#define WRITE_VAR "CHANNELS_WRITE_DELAY"


int main(int argc, char **argv)
{
    size_t data_size = 1;
    if (argc > 1)
    {
        data_size = atoi(argv[1]);
    }

    MIMPI_Init(false);
    int const world_rank = MIMPI_World_rank();

    const char *delay = getenv("DELAY");
    if (delay)
    {
        int res = setenv(WRITE_VAR, delay, true);
        assert(res == 0);
    }

    uint8_t *data = malloc(data_size);
    assert(data);
    memset(data, 1, data_size);

    uint8_t *recv_data = malloc(data_size);

    if (world_rank == 0)
    {
        uint8_t *recv_data = malloc(data_size);
        assert(recv_data);


        ASSERT_MIMPI_OK(MIMPI_Reduce(data, recv_data, data_size, MIMPI_SUM, 0));



        for (int i = 1; i < data_size; ++i)
            assert(recv_data[i] == recv_data[0]);
        printf("Number: %d\n", recv_data[0]);
        fflush(stdout);
        free(recv_data);
    }
    else
    {

        if (world_rank != 1) {
            ASSERT_MIMPI_OK(MIMPI_Reduce(data, NULL, data_size, MIMPI_SUM, 0));
        }
//        ASSERT_MIMPI_OK(MIMPI_Reduce(data, NULL, data_size, MIMPI_SUM, 0));



    }
    assert(data[0] == 1);
    for (int i = 1; i < data_size; ++i)
        assert(data[i] == data[0]);
    free(data);

    int res = unsetenv(WRITE_VAR);
    assert(res == 0);

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
