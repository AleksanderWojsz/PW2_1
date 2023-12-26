#include "mimpi_common.h"
#include "mimpi.h"
#include "channel.h"
#include "moja.h"

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

#define OVERWRITE 1


int main(int argc, char *argv[]) {

    int n = atoi(argv[1]);   // Liczba procesow
    char* program = argv[2]; // Sciezka do programu

    int foo[20][2];
    int pom_desc[5][2];

    int k = 0;
    do {
        k++;
        channel(foo[k - 1]);
    } while (foo[k - 1][0] <= 18 && foo[k - 1][1] <= 18);
    if (foo[k - 1][1] == 20) {
        close(foo[k - 1][1]);
        foo[k - 1][1] = -1;
    }

    for (int i = 0; i < 5; i++) {
        channel(pom_desc[i]);
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

    // W pipe 20-21 będą współdzielone dane pomiędzy wszystkimi procesami
    // Na razie tablica 0-1 ktory proces jest w srodku a ktory nie, i licznik ile procesow jest w srodku



    int active[n + 1];
    for (int i = 0; i < n; i++) {
        active[i] = 1;
    }
    active[n] = n;

    write(21, active, sizeof(int) * (n + 1));


    pid_t pids[n]; // pid'y dzieci
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




    // zamykanie deskryptorow rodzica
    for (int i = 0; i < 5; i++) {
        close(pom_desc[i][0]);
        close(pom_desc[i][1]);
    }


    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            close(desc[i][j][0]);
            close(desc[i][j][1]);
        }
    }

    for (int i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0); // Czekanie na zakonczenie wszystkich dzieci
    }

//    for (int i = 0; i < n; i++) {
//        for (int j = 0; j < n; j++) {
//            printf("%d -> %d %d %d \n", i, j, desc[i][j][0], desc[i][j][1]);
//        }
//    }
//    print_open_descriptors();


    return 0;
}
