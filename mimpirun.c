#include "mimpi_common.h"
#include "channel.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define OVERWRITE 1
#define POM_PIPES 90
#define MAX_N 16

// bariera od 20 do 59
// pomocnicze od 60 do 69
// zakleszczenia semafory od 70 do 109 + od 110 do 119 na zgłoszenia + od 120 do 159 na odebrane wiadomości + od 160 do 199 na liczniki(mutexy)
// n^2 pipeów zaczyna się od 200

int get_deadlock_counter_read_desc(int rank) {
    return 160 + rank * 2;
}

int get_deadlock_counter_write_desc(int rank) {
    return get_deadlock_counter_read_desc(rank) + 1;
}

void create_descriptors(int n, int desc[MAX_N][MAX_N][2], int pom_desc[POM_PIPES][2]) {

    int foo[20][2]; // Do zajęcia aż do 19 deskryptora włącznie

    int index = 0;
    ASSERT_SYS_OK(channel(foo[index]));
    while (foo[index][1] != 20 && foo[index][1] != 21) {
        index++;
        ASSERT_SYS_OK(channel(foo[index]));
    }
    ASSERT_SYS_OK(close(20));
    if (foo[index][1] == 21) {
        ASSERT_SYS_OK(close(21));
    }

    // Zajmowanie pomocniczych 'POM_PIPES' pipeów
    for (int i = 0; i < POM_PIPES; i++) {
        ASSERT_SYS_OK(channel(pom_desc[i]));
    }

    // Pipe'y pomiędzy każdymi dwoma procesami
    for (int i = 0; i < n; i++) { // Tworzenie potoków
        for (int j = 0; j < n; j++) {
            ASSERT_SYS_OK(channel(desc[i][j]));
        }
    }

    // Zamykanie deskryptorów, które otworzyliśmy do 19 włącznie
    for (int i = 0; i <= index; i++) {
        if (foo[i][0] <= 19) {
            ASSERT_SYS_OK(close(foo[i][0]));
        }
        if (foo[i][1] <= 19) {
            ASSERT_SYS_OK(close(foo[i][1]));
        }
    }
}

void close_descriptors(int n, int desc[MAX_N][MAX_N][2], int pom_desc[POM_PIPES][2]) {
    // zamykanie 'POM_PIPES' pipe'ów
    for (int i = 0; i < POM_PIPES; i++) {
        ASSERT_SYS_OK(close(pom_desc[i][0]));
        ASSERT_SYS_OK(close(pom_desc[i][1]));
    }

    // zamykanie pipe'ów między każdymi dwoma procesami
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            ASSERT_SYS_OK(close(desc[i][j][0]));
            ASSERT_SYS_OK(close(desc[i][j][1]));
        }
    }
}

int main(int argc, char *argv[]) {

    int n = atoi(argv[1]);   // Liczba procesów
    char* program = argv[2]; // Ścieżka do programu

    int pom_desc[POM_PIPES][2]; // 'POM_PIPES' pipeów do współdzielenie danych między wszystkimi procesami
    int desc[MAX_N][MAX_N][2]; // [skąd][dokąd][0 - write, 1 - read]

    create_descriptors(n, desc, pom_desc);

    // W pipe 60-61 będą współdzielone dane: tablica 0-1 który proces jest w środku(1) a który nie(0)
    int active[MAX_N];
    for (int i = 0; i < n; i++) {
        active[i] = 1;
    }
    ASSERT_SYS_OK(chsend(61, active, sizeof(int) * n));

    // Tablica zgłoszeń do deadlocków
    int waiting_data[MAX_N];
    memset(waiting_data, -1, sizeof(int) * n);
    ASSERT_SYS_OK(chsend(111, waiting_data, sizeof(int) * n)); // C
    ASSERT_SYS_OK(chsend(113, waiting_data, sizeof(int) * n)); // A
    ASSERT_SYS_OK(chsend(115, waiting_data, sizeof(int) * n)); // source
    ASSERT_SYS_OK(chsend(117, waiting_data, sizeof(int) * n)); // count
    ASSERT_SYS_OK(chsend(119, waiting_data, sizeof(int) * n)); // tag

    // Licznik wiadomości już odebranych, które będą usuwane z listy
    int no_of_messages_to_remove = 0;
    for (int i = 0; i < n; i++) {
        ASSERT_SYS_OK(chsend(get_deadlock_counter_write_desc(i), &no_of_messages_to_remove, sizeof(int)));
    }


    pid_t pids[MAX_N]; // pid'y dzieci
    for (int i = 0; i < n; i++) {

        ASSERT_SYS_OK(pids[i] = fork());
        if (pids[i] == 0) { // Proces dziecko

            // przekazujemy informacje o rozmiarze świata
            char world_size_str[256];
            sprintf(world_size_str, "%d", n);  // Konwertuje n do stringa
            ASSERT_SYS_OK(setenv("MIMPI_WORLD_SIZE", world_size_str, OVERWRITE)); // Ustawia zmienna srodowiskowa

            // przekazujemy informacje o randze
            char rank_str[256];
            sprintf(rank_str, "%d", i);
            ASSERT_SYS_OK(setenv("MIMPI_RANK", rank_str, OVERWRITE));

            ASSERT_SYS_OK(execvp(program, &argv[2])); // Uruchomienie programu
        }
    }

    close_descriptors(n, desc, pom_desc);

    // Czekanie na zakończenie wszystkich dzieci
    for (int i = 0; i < n; i++) {
        ASSERT_SYS_OK(waitpid(pids[i], NULL, 0));
    }

    return 0;
}