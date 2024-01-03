#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "mimpi.h"
#include "examples/test.h"
#include "examples/mimpi_err.h"

// TODO czy duzo wiadomosci zakleszcza sprawdzanie zakleszczen
// lista odebranych wiadomosci jest za duza

int main() {
    MIMPI_Init(0);

    if (MIMPI_World_rank() != 0) {
        int a[10];
        for (int i = 0; i < 1000000; i++) {
            MIMPI_Send(a, 10 * sizeof(int), 0, MIMPI_ANY_TAG);
        }
    }

    MIMPI_Barrier();
    printf("Done!\n");
    MIMPI_Finalize();

    return test_success();
}

// ./run_test 10s 6 examples_build/extended_pipe_closed
// ./run_test 30s 3 ./examples_build/send_any_size 2147483647 0 1

// extended pipe closed
// send any size

// printf("ok\n");

// ./mimpirun 2 valgrind --track-origins=yes --leak-check=full --max-stackframe=4000032 ./main

// TODO czy MIMPI_ERROR_REMOTE_FINISHED trzeba sprawdzać tylko w MIMPI_BARRIER

// TODO wiadomości mniejsze niz 512B
// TODO zrobic zeby world rank i size czytalo z pipea tylko raz
// TODO assert_sys_ok
// TODO co jak na starcie będzie zajęty deskryptor 19
// TODO szukanie wiadomosci nie od poczatku
// TODO chsend zwraca EPIPE jeśli koniec do odczytu łącza jest zamknięty więc możesz wiedzieć od razu że nie da się wysłać

// TODO nie wiemy chyba czy do odczytu bedzie parzysty czy nieparzysty deskryptor wiec moze trzeba to uwzglednic (teraz do read jest parzysty)

// TODO co miało być wysłane przed barierą będzie wysłane bo send musi się skończyć
// TODO nie przeszkadza nam że nie wszystko będzie odebrane podczas beriery (gdzie indziej też nam nie przeszkadza)
// TODO bo to co odebraliśmy/będziemy odbierali interesuje nas tylko jak robimy receive


// TODO cos zmienial w jakims tescie any size czy cos

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
 chmod -R 777 *

for i in {1..100}
do
   echo $i
   ./mimpirun 2 ./main
done

 */
