#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdint-gcc.h>
#include "mimpi.h"
#include "examples/test.h"
#include "examples/mimpi_err.h"

#define WRITE_VAR "CHANNELS_WRITE_DELAY"
#define NS_PER_1_MS 1 ## 000 ## 000

#define WRITE_VAR "CHANNELS_WRITE_DELAY"

int main(int argc, char **argv)
{
    MIMPI_Init(true);
    int rank = MIMPI_World_rank();
    char buf = '1';
    if (rank == 0) {
        assert(MIMPI_Recv(&buf, sizeof(buf), 1, 2137) == MIMPI_ERROR_DEADLOCK_DETECTED); // zakleszczenie
        assert(MIMPI_Send(&buf, sizeof(buf), 1, 2137) == MIMPI_SUCCESS); // sukces czy zakleszczenie?
    }
    if (rank == 1) {
        assert(MIMPI_Recv(&buf, sizeof(buf), 0, 2137) == MIMPI_ERROR_DEADLOCK_DETECTED); // zakleszczenie
        assert(MIMPI_Recv(&buf, sizeof(buf), 0, 2137) == MIMPI_SUCCESS); // sukces czy zakleszczenie?
    }
    MIMPI_Finalize();

}




// Wszystkie wiadomości mają rozmiar 512B, jak wiadomość jest za mała, to jest dopełniana zerami
// TODO assert_sys_ok
// TODO wiadomości mniejsze niz 512B
// TODO szukanie wiadomosci nie od poczatku
// TODO count moze byc duzy (MAX_INT) wiec zadbac o to zeby nie było overflowa
// TODO malloc


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



 */
