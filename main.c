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


#define NS_PER_1_MS 1 ## 000 ## 000

// based on: https://stackoverflow.com/questions/1157209/is-there-an-alternative-sleep-function-in-c-to-milliseconds
/* msleep(): Sleep for the requested number of milliseconds. */
static int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}



int main(int argc, char **argv)
{
    MIMPI_Init(true);

    int const world_rank = MIMPI_World_rank();

    int const tag = 17;

    char number = 42;
    if (world_rank == 0) {
        msleep(500);
    } else if (world_rank == 1) {
        assert(MIMPI_Recv(&number, 1, 0, tag) == MIMPI_ERROR_REMOTE_FINISHED);
        assert(MIMPI_Send(&number, 1, 0, tag) == MIMPI_ERROR_REMOTE_FINISHED);
        assert(MIMPI_Recv(&number, 1, 0, tag) == MIMPI_ERROR_REMOTE_FINISHED);
        assert(number == 42);
    }else if(world_rank == 2){
        msleep(1000);
        assert(MIMPI_Recv(&number, 1, 0, tag) == MIMPI_ERROR_REMOTE_FINISHED);
        assert(number == 42);
        assert(MIMPI_Recv(&number, 1, 0, tag) == MIMPI_ERROR_REMOTE_FINISHED);
        assert(number == 42);
    }else if(world_rank == 3){
        // heurystyczne założenie że wszytko zdąży się wykonać w odpowiednim czasie żeby ten proces już wiedział że odbiorca jest zamknięty
        msleep(1000);
        assert(MIMPI_Send(&number, 1, 0, tag) == MIMPI_ERROR_REMOTE_FINISHED);
    }else if(world_rank == 4){
        // heurystyczne założenie że chwrite zakończy się z błędem "Zamknięty odpbiorca".
        setenv("CHANNELS_WRITE_DELAY", "1000", true);
        assert(MIMPI_Send(&number, 1, 0, tag) == MIMPI_ERROR_REMOTE_FINISHED);
        setenv("CHANNELS_WRITE_DELAY", "1", true);
    }

    MIMPI_Finalize();
    printf("Done\n");
    return test_success();
}



// ./run_test 10s 6 examples_build/extended_pipe_closed

// extended pipe closed


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


// TODO count moze byc duzy (MAX_INT) wiec zadbac o to zeby nie było overflowa
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



  set -e
for i in {1..100000}; do
    echo "test $i"
    if ! ./run_test 1000s 2 examples_build/deadlock3; then
        echo "Test deadlock3 failed"
        break
    fi
done

 set -e
for i in {1..100000}; do
    echo "test $i"
    if ! ./run_test 1s 2 examples_build/deadlock1; then
        echo "Test deadlock1 failed"
        break
    elif ! ./run_test 1s 2 examples_build/deadlock2; then
        echo "Test deadlock2 failed"
        break
    elif ! ./run_test 1s 2 examples_build/deadlock3; then
        echo "Test deadlock3 failed"
        break
    elif ! ./run_test 1s 3 examples_build/deadlock4; then
        echo "Test deadlock4 failed"
        break
    elif ! ./run_test 1s 2 examples_build/deadlock5; then
        echo "Test deadlock5 failed"
        break
    elif ! ./run_test 1s 2 examples_build/deadlock6; then
        echo "Test deadlock6 failed"
        break
    fi
done





  set -e
for i in {1..100000}; do
    echo "test $i"
    if ! ./run_test 1s 2 examples_build/deadlock3; then
        echo "Test deadlock3 failed"
        break
    fi
done

 */
