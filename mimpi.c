/**
 * This file is for implementation of MIMPI library.
 * */
#include "channel.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include "moja.h"

#include <stdio.h>
#include <stdlib.h>

#define POM_PIPES 5
#define OUT_OF_MPI_BLOCK (-1)
#define META_INFO_SIZE 4

Message* received_list = NULL;
bool finished[16] = {false};

int check_arguments_correctness(int other_process_rank) {
    if (MIMPI_World_rank() == other_process_rank) {
        return MIMPI_ERROR_ATTEMPTED_SELF_OP;
    }
    else if (other_process_rank >= MIMPI_World_size() || other_process_rank < 0) {
        return MIMPI_ERROR_NO_SUCH_RANK;
    }
    else {
        return 0;
    }
}

void notify_iam_out() {
    int is_active[MIMPI_World_size() + 1];
    chrecv(20, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    is_active[MIMPI_World_rank()] = 0;
    is_active[MIMPI_World_size()]--;
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
    return  src * MIMPI_World_size() * 2 + 20 + 2 * POM_PIPES + dest * 2;
}

int MIMPI_get_write_desc(int src, int dest) {
    return  MIMPI_get_read_desc(src, dest) + 1;
}

void MIMPI_Init(bool enable_deadlock_detection) {
    channels_init();
}

void MIMPI_Finalize() {

    notify_iam_out();

    // wysyłamy wszystkim wiadomość z tagiem OUT_OF_MPI_BLOCK (-1) oznaczającą, że skończyliśmy
    for (int i = 0; i < MIMPI_World_size(); i++) {
        if (i != MIMPI_World_rank() && is_in_MPI_block(i) == true) {
            int message = 0; // TODO to nie powinno miec rozmiaru 512? Bo ktoś czeka na dowolnie długą wiadomość
            MIMPI_Send(&message, 1, i, OUT_OF_MPI_BLOCK);
        }
    }

    // zamykanie wszystkich deskryptorów
    for (int i = 20; i < 20 + 2 * POM_PIPES; i++) {
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

    if (check_arguments_correctness(destination) != 0) {
        return check_arguments_correctness(destination);
    }
    else if (is_in_MPI_block(destination) == false) {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }

    // Dzielimy wiadomość na części rozmiaru ile + z_ilu + tag + current_message_size + wiadomość = 512B
    int max_message_size = 512 - sizeof(int) * META_INFO_SIZE;
    int liczba_czesci = (count + (max_message_size - 1)) / max_message_size; // ceil(A/B) use (A + (B-1)) / B
    int nr_czesci = 1;

    // tworzenie wiadomosci
    for (int i = 0; i < liczba_czesci; i++) {

        printf("Wysylam czesc %d z %d\n", i + 1, liczba_czesci);

        int current_message_size = max_message_size;
        if (i == liczba_czesci - 1) { // ostatnia część wiadomości
            current_message_size = count - (liczba_czesci - 1) * max_message_size;
        }

        void *message = malloc(current_message_size + sizeof(int) * META_INFO_SIZE);
        memcpy(message + sizeof(int) * 0, &nr_czesci, sizeof(int));
        memcpy(message + sizeof(int) * 1, &liczba_czesci, sizeof(int));
        memcpy(message + sizeof(int) * 2, &tag, sizeof(int));
        memcpy(message + sizeof(int) * 3, &current_message_size, sizeof(int));
        memcpy(message + sizeof(int) * 4, data + i * max_message_size, current_message_size);

        // Wysyłanie wiadomości
        chsend(MIMPI_get_write_desc(MIMPI_World_rank(), destination), message, current_message_size + sizeof(int) * META_INFO_SIZE);
        nr_czesci++;
        free(message);
    }

    return MIMPI_SUCCESS;
}


MIMPI_Retcode MIMPI_Recv(
        void* data,
        int count,
        int source,
        int tag
) {

    if (check_arguments_correctness(source) != 0) {
        return check_arguments_correctness(source);
    }
    else if (finished[source] == true) { // Potrzebne jakby ktoś kilka razy z rzędu wywołał recv
        return MIMPI_ERROR_REMOTE_FINISHED;
    }

    // Sprawdzamy, czy już wcześniej nie odebraliśmy takiej wiadomości
    // list_find znajdzie też wiadomości z tagiem 0
    Message *message = list_find(received_list, count, source, tag);
    if (message != NULL) {
        memcpy(data, message->data, message->count); // Przepisanie danych do bufora użytkownika
        list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
        return MIMPI_SUCCESS;
    }

    // Tak długo, jak nie trafimy na wiadomość na którą czekamy od konkretnego nadawcy
    // to czytamy dalej i inne wiadomości zapisujemy na później
    while (true) {
        int read_buffer_size = 496;
        void* read_message = malloc(496); // Ten bufor będzie się powiększał w razie potrzeby

        void* buffer = malloc(512);  // Bufor do przechowywania fragmentów i metadanych
        int received = 0;  // Ile bajtów danych już otrzymaliśmy
        int ile = 0, z_ilu = 1, fragment_tag, current_message_size;

        while (ile < z_ilu) {
            // Odbieranie fragmentu
            chrecv(MIMPI_get_read_desc(source, MIMPI_World_rank()), buffer, 512);

            // Rozpakowywanie metadanych z bufora
            memcpy(&ile, buffer + sizeof(int) * 0, sizeof(int));
            memcpy(&z_ilu, buffer + sizeof(int) * 1, sizeof(int));
            memcpy(&fragment_tag, buffer + sizeof(int) * 2, sizeof(int));
            memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));

            // Zapisywanie danych z fragmentu do głównego bufora danych
            memcpy(read_message + received, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
            received += current_message_size;

            if (ile < z_ilu) { // Jak to nie koniec czytania to powiększamy bufor
                read_buffer_size += 496;
                read_message = realloc(read_message, read_buffer_size);
            }
        }

        // W ty miejscu odebraliśmy całą jedną wiadomość
        if ((tag == fragment_tag || tag == 0) && received == count) { // Jak wiadomość pasuje to kończymy czytanie
            memcpy(data, read_message, received); // Przepisanie danych do bufora użytkownika
            free(buffer);
            free(read_message);
            return MIMPI_SUCCESS;

        } else { // zapisujemy dane na później i czytamy dalej

            if (fragment_tag < 0) { // Wiadomości specjalna
                // Czekaliśmy na wiadomość od konkretnego procesu, ale jedynce co dostaliśmy to, że się skończy
                if (fragment_tag == -1) {
                    finished[source] = true;
                    free(buffer);
                    free(read_message);
                    return MIMPI_ERROR_REMOTE_FINISHED;
                }
            } else {
                list_add(&received_list, read_message, received, source, fragment_tag);
            }

            free(buffer);
            free(read_message);
        }
    }

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



void print_active() {
    int is_active[MIMPI_World_size() + 1];
    chrecv(20, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    for (int i = 0; i < MIMPI_World_size() + 1; i++) {
        printf("%d ", is_active[i]);
    }
    printf("\n");

    chsend(21, is_active, sizeof(int) * (MIMPI_World_size() + 1));
}
