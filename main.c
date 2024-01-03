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

#define NUMBER 1000
int main(int argc, char **argv)
{
//    MIMPI_Init(false);
    MIMPI_Init(true);

    int const world_rank = MIMPI_World_rank();
    int partner_rank = (world_rank / 2 * 2) + 1 - world_rank % 2;


    char number = '2';

    for (int i = 0; i < NUMBER; i++) {
        if (world_rank % 2 == 0) {
            ASSERT_MIMPI_OK(MIMPI_Recv(&number, 1, partner_rank, 17));
        }
        else if (world_rank % 2 == 1) {
            ASSERT_MIMPI_OK(MIMPI_Send(&number, 1, partner_rank, 17));
        }
    }



    MIMPI_Finalize();
    printf("ok %d\n", world_rank);

    return 0;
}


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
