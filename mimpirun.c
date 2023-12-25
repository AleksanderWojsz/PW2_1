#include "mimpi_common.h"
#include "mimpi.h"
#include "channel.h"
#include "moja.h"

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#define OVERWRITE 1





int main(int argc, char *argv[]) {

    int n = atoi(argv[1]);  // Liczba procesow
    char* program = argv[2];  // Sciezka do programu


    // dla bezpieczenstwa zajmujemy 20 pierwszych deskryptorow, które zwolnimy po stworzeniu docelowych pipeów
    int foo[20][2];


    int k = 0;
    do {
        k++;
        channel(foo[k - 1]);
    } while (foo[k - 1][0] <= 18 && foo[k - 1][1] <= 18);
    if (foo[k - 1][1] == 20) {
        close(foo[k - 1][1]);
        foo[k - 1][1] = -1;
    }

    // https://pubs.opengroup.org/onlinepubs/9699919799/functions/pipe.html     - pipe zwraca zawsze dwa najmniejsze wolne deskryptory
    // https://pubs.opengroup.org/onlinepubs/009604599/functions/pipe.html
    int desc[n][n][2]; // [skad][dokad][0 - write, 1 - read]
    for (int i = 0; i < n; i++) { // Tworzenie potokow
        for (int j = 0; j < n; j++) {
            channel(desc[i][j]);
        }
    }

    for (int i = 0; i < k; i++) { // Zwalnianie deskryptorow
        close(foo[i][0]);

        if (foo[i][1] != -1) {
            close(foo[i][1]);
        }
    }

    pid_t pids[n]; // pidy dzieci
    for (int i = 0; i < n; i++) {

        pids[i] = fork();
        if (pids[i] == 0) { // Proces dziecko

            // przekazujemy informacje o rozmiarze świata
            char world_size_str[256];
            sprintf(world_size_str, "%d", n);  // Konwertuje n do stringa
            setenv("MIMPI_WORLD_SIZE", world_size_str, OVERWRITE); // Ustawia zmienna srodowiskowa

            // przekazujemy informacje o randze
            char rank_str[256];
            sprintf(rank_str, "%d", i);
            setenv("MIMPI_RANK", rank_str, OVERWRITE);

            execvp(program, &argv[2]); // Uruchomienie programu
        }
    }

    for (int i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0); // Czekanie na zakonczenie wszystkich dzieci
    }



//    print_open_descriptors();
//    for (int i = 0; i < n; i++) {
//        for (int j = 0; j < n; j++) {
//            printf("%d -> %d %d %d \n", i, j, desc[i][j][0], desc[i][j][1]);
//        }
//    }


    return 0;
}
