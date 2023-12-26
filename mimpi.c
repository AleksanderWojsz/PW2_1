/**
 * This file is for implementation of MIMPI library.
 * */
#include "channel.h"
#include "mimpi.h"
#include "mimpi_common.h"

#include <stdio.h>
#include <stdlib.h>

int MIMPI_World_size();

int MIMPI_World_rank();

struct Message {
    void const *data;
    int count;
    int tag;
};
typedef struct Message Message;

Message message;

void notify_iam_in() {
    int is_active[MIMPI_World_size() + 1];
    chrecv(20, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    is_active[MIMPI_World_rank()] = 1;
    is_active[MIMPI_World_size()]++;
    chsend(21, is_active, sizeof(int) * (MIMPI_World_size() + 1));
}

void notify_iam_out() {
    int is_active[MIMPI_World_size() + 1];
    chrecv(20, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    is_active[MIMPI_World_rank()] = 0;
    is_active[MIMPI_World_size()]--;
    chsend(21, is_active, sizeof(int) * (MIMPI_World_size() + 1));
}

void print_active() {
    int is_active[MIMPI_World_size() + 1];
    chrecv(20, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    for (int i = 0; i < MIMPI_World_size() + 1; i++) {
        printf("%d ", is_active[i]);
    }
    printf("\n");

    chsend(21, is_active, sizeof(int) * (MIMPI_World_size() + 1));
}

bool is_in_MPI_block(int nr) {

    if (nr >= MIMPI_World_size()) {
        return false;
    }

    int is_active[MIMPI_World_size() + 1];
    chrecv(20, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    bool result = (is_active[nr] == 1);
    chsend(21, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    return result;
}

int MIMPI_get_read_desc(int src, int dest) {
    return  src * MIMPI_World_size() * 2 + 30 + dest * 2;
}

int MIMPI_get_write_desc(int src, int dest) {
    return  MIMPI_get_read_desc(src, dest) + 1;
}



void MIMPI_Init(bool enable_deadlock_detection) {
    channels_init();

    notify_iam_in();

//    print_active();
}

void MIMPI_Finalize() {

    notify_iam_out(); // TODO

    // zamykanie wszystkich deskryptorow
    for (int i = 20; i < 30; i++) {
        close(i);
    }

    for (int i = 0; i < MIMPI_World_size(); i++) {
        for (int j = 0; j < MIMPI_World_size(); j++) {
            close(MIMPI_get_read_desc(i, j));
            close(MIMPI_get_write_desc(i, j));
        }
    }

    channels_finalize();
}

int MIMPI_World_size() {
    return atoi(getenv("MIMPI_WORLD_SIZE")); // Konwertuje string na int
}

int MIMPI_World_rank() {
    return atoi(getenv("MIMPI_RANK")); // getenv zwraca string
}

MIMPI_Retcode MIMPI_Send(
    void const *data,
    int count,
    int destination,
    int tag
) {
    if (!is_in_MPI_block(destination)) {
        printf("cel nie jest w bloku MPI\n");
    }

    // dzielimy wiadomosc na części rozmiaru 500B + ile + z_ilu + tag
    // ile oznacza która to częśc wiadomości np 2 z 4
    // z_ilu oznacza ile części ma wiadomość np 4 z 4
    // tag oznacza tag wiadomości
    // size to dlugosc wiadomosci

    int max_message_size = 512 - sizeof(int) * 4;

    int z_ilu = (count + (max_message_size - 1)) / max_message_size; // ceil(A/B) use (A + (B-1)) / B

    int ile = 1;
    // tworzenie wiadomosci
    for (int i = 0; i < z_ilu; i++) {

        int current_message_size = max_message_size;
        if (i == z_ilu - 1) { // ostatnia część wiadomości
            current_message_size = count - (z_ilu - 1) * max_message_size;
        }

        void *message = malloc(current_message_size + sizeof(int) * 3);
        memcpy(message, &ile, sizeof(int));
        memcpy(message + sizeof(int), &z_ilu, sizeof(int));
        memcpy(message + sizeof(int) * 2, &tag, sizeof(int));
        memcpy(message + sizeof(int) * 3, &current_message_size, sizeof(int));
        memcpy(message + sizeof(int) * 4, data + i * max_message_size, current_message_size);

        printf("Ala4 %d %d %d\n", ile, z_ilu, current_message_size);
        chsend(MIMPI_get_write_desc(MIMPI_World_rank(), destination), message, 512);
        ile++;
        free(message);
    }

    return 0;
}

MIMPI_Retcode MIMPI_Recv(
        void *data,
        int count,
        int source,
        int tag
) {

    if (!is_in_MPI_block(source)) {
        printf("zrodlo nie jest w bloku MPI\n");
    }

    // Bufor do przechowywania fragmentów i metadanych
    char *buffer = malloc(512);  // maksymalny rozmiar fragmentu + metadane

    int received = 0;  // ile bajtów danych otrzymaliśmy
    int ile, z_ilu, fragment_tag, current_message_size;

    while (received < count) {

        // Odbieranie fragmentu
        chrecv(MIMPI_get_read_desc(source, MIMPI_World_rank()), buffer, 512);

        // Rozpakowywanie metadanych z bufora
        memcpy(&ile, buffer, sizeof(int));
        memcpy(&z_ilu, buffer + sizeof(int), sizeof(int));
        memcpy(&fragment_tag, buffer + sizeof(int) * 2, sizeof(int));
        memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));


        // Zapisywanie danych z fragmentu do głównego bufora danych
        printf("Kota5 %d %d %d\n", ile, z_ilu, current_message_size);
        memcpy(data + received, buffer + sizeof(int) * 4, current_message_size);
        received += current_message_size;  // aktualizacja liczby otrzymanych danych

    }

    free(buffer);  // Zwalnianie tymczasowego bufora


    return 0;
}


MIMPI_Retcode MIMPI_Barrier() {
    TODO
}

MIMPI_Retcode MIMPI_Bcast(
    void *data,
    int count,
    int root
) {
    TODO
}

MIMPI_Retcode MIMPI_Reduce(
    void const *send_data,
    void *recv_data,
    int count,
    MIMPI_Op op,
    int root
) {
    TODO
}