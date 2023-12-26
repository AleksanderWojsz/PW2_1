/**
 * This file is for implementation of MIMPI library.
 * */
#include "channel.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include "moja.h"

#include <stdio.h>
#include <stdlib.h>

int MIMPI_World_size();

int MIMPI_World_rank();

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

}

void MIMPI_Finalize() {

    notify_iam_out(); // TODO

    // wysylamy wszystkim wiadomosc z tagiem -1 oznaczajaca ze skonczylismy
    for (int i = 0; i < MIMPI_World_size(); i++) {
        if (i != MIMPI_World_rank()) {
            int message = 0;
            MIMPI_Send(&message, 1, i, -1);
        }
    }

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

    if (is_in_MPI_block(destination) == false) {
        printf("Cel %d wyszedl juz z MPI\n", destination);
        return MIMPI_ERROR_REMOTE_FINISHED;
    }

    // dzielimy wiadomosc na części rozmiaru ile + z_ilu + tag + current_message_size + wiadomosc = 512B

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

//        printf("Ala4 %d %d %d\n", ile, z_ilu, current_message_size);
        chsend(MIMPI_get_write_desc(MIMPI_World_rank(), destination), message, 512);
        ile++;
        free(message);
    }

    return 0;
}

Message* received_list = NULL;
bool finished[16] = {false};

MIMPI_Retcode MIMPI_Recv(
        void* data,
        int count,
        int source,
        int tag
) {

    if (finished[source] == true) {
        printf("zrodlo %d skonczylo \n", source);
        return MIMPI_ERROR_REMOTE_FINISHED;
    }

    // sprawdzamy czy już wcześniej nie odebraliśmy takiej wiadomości
    Message *message = list_find(received_list, count, source, tag);
    if (message != NULL) {
        // Przepisanie danych do bufora uzytkownika
        memcpy(data, message->data, message->count);

        // Usunięcie wiadomości z listy
        list_remove(&received_list, count, source, tag);

        return 0;
    }


    while (true) {
        // tak dlugo jak nie trafimy na wiadomość na którą czekamy od konkretnego nadawcy
        // to czytamy dalej i inne wiadomości zapisujemy na później
        int actual_read_message_size = 0;
        int read_buffer_size = 496;
        void* read_message = malloc(496); // ten bufor będzie się powiększał w razie potrzeby


        // Bufor do przechowywania fragmentów i metadanych
        void* buffer = malloc(512);  // maksymalny rozmiar fragmentu + metadane
        int received = 0;  // ile bajtów danych otrzymaliśmy
        int ile = 0, z_ilu = 1, fragment_tag, current_message_size;

        while (ile < z_ilu) {
            // Odbieranie fragmentu
            chrecv(MIMPI_get_read_desc(source, MIMPI_World_rank()), buffer, 512);

            // Rozpakowywanie metadanych z bufora
            memcpy(&ile, buffer, sizeof(int));
            memcpy(&z_ilu, buffer + sizeof(int), sizeof(int));
            memcpy(&fragment_tag, buffer + sizeof(int) * 2, sizeof(int));
            memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));

            // Zapisywanie danych z fragmentu do głównego bufora danych
            // printf("Kota5 %d %d %d\n", ile, z_ilu, current_message_size);
            memcpy(read_message + received, buffer + sizeof(int) * 4, current_message_size);
            received += current_message_size;  // aktualizacja liczby otrzymanych danych

            actual_read_message_size += current_message_size;

            if (ile < z_ilu) { // Jak to nie koniec czytania to powiększamy bufor
                read_buffer_size += 496;
                read_message = realloc(read_message, read_buffer_size);
            }
        }

        // W ty mmiejscu odebraliśmy całą jedną wiadomość
        if (tag == fragment_tag && actual_read_message_size == count) { // Jak wiadomosc pasuje to konczymy czytanie
            // Przepisanie danych do bufora użytkownika
            memcpy(data, read_message, actual_read_message_size);

            free(buffer);  // Zwalnianie tymczasowego bufora
            free(read_message);
            break;
        } else { // zapisujemy dane na później i czytamy dalej

            if (fragment_tag < 0) { // wiadomosc specjalna
                if (fragment_tag == -1) { // source skonczyl
                    finished[source] = true;
                    printf("Zrodlo %d skonczylo\n", source);
                    free(buffer);
                    free(read_message);
                    return MIMPI_ERROR_REMOTE_FINISHED;
                }
            } else { // TODO wiadomosc z tagiem 0
                list_add(&received_list, read_message, actual_read_message_size, source, fragment_tag);
            }


            free(buffer);
            free(read_message);
        }
    }


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