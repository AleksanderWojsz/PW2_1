/**
 * This file is for implementation of MIMPI library.
 * */
#include "channel.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include "moja.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define POM_PIPES 5
#define OUT_OF_MPI_BLOCK (-1)
#define META_INFO_SIZE 4

Message* received_list = NULL;
bool finished[16] = {false};

// Wszystkie wiadomości mają rozmiar 512B, jak wiadomość jest za mała to jest dopełniana zerami

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




pthread_mutex_t mutex_pipes;
pthread_mutex_t parent_sleeping;

int waiting_count = -10;
int waiting_source = -10;
int waiting_tag = -10;

void *read_messages_from_source(void *args) {
    int source = *((int *)args);

    // Czytamy wiadomosci od konkretnego procesu i zapisujemy je na liscie
    while (true) {
        int read_buffer_size = 496;
        void* read_message = malloc(496); // Ten bufor będzie się powiększał w razie potrzeby

        void* buffer = malloc(512);  // Bufor do przechowywania fragmentów i metadanych
        int received = 0;  // Ile bajtów danych już otrzymaliśmy
        int ile = 0, z_ilu = 1, fragment_tag, current_message_size;

//        printf("Jestem %d czekam na wiadomosc od %d z pipe'a %d\n", MIMPI_World_rank(), source, MIMPI_get_read_desc(source, MIMPI_World_rank()));
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

        // W tym miejscu mamy już całą jedną wiadomość

//        printf("Jestem %d dostalem wiadomosc od %d, tag %d \n", MIMPI_World_rank(), source, fragment_tag);

        pthread_mutex_lock(&mutex_pipes);
        list_add(&received_list, read_message, received, source, fragment_tag);

        // Zapisujemy wszystkie wiadomości, a jak rodzic czeka na daną wiadomość
        // lub wiadomość to dowolna specjalna wiadomość od tego nadawcy to budzimy rodzica
        if ((waiting_count == received && waiting_source == source && (waiting_tag == fragment_tag || fragment_tag == 0)) ||
                (waiting_source == source && fragment_tag < 0)) {
            waiting_count = -10;
            waiting_source = -10;
            waiting_tag = -10;
            pthread_mutex_unlock(&parent_sleeping);
        }
        pthread_mutex_unlock(&mutex_pipes);

        free(buffer);
        free(read_message);

        if (fragment_tag == -1) { // Jak nie będzie już więcej wiadomości to kończymy
//            printf("Koncze watek %d\n", MIMPI_World_rank());
            break;
        }
    }

    free(args);

    return NULL;
}

pthread_t threads[16];

void MIMPI_Init(bool enable_deadlock_detection) {
    channels_init();

    pthread_mutex_init(&mutex_pipes, NULL);
    pthread_mutex_init(&parent_sleeping, NULL);
    pthread_mutex_lock(&parent_sleeping); // zmniejszanie licznika do 0

    // Tworzenie po jednym wątku na każdy pipe
    for (int i = 0; i < MIMPI_World_size(); i++) {
        if (i != MIMPI_World_rank()) {
            pthread_t thread;
            int *source = malloc(sizeof(int));
            *source = i;
            pthread_create(&thread, NULL, read_messages_from_source, source);
            threads[i] = thread;
        }
    }
}

void MIMPI_Finalize() {

    notify_iam_out(); // Zabraniamy wysłać do nas wiadomości poprzez MIMPI_SEND

    // Wysyłamy wiadomości z tagiem -1 do samych siebie przez wszystkie pipe'y, żeby nasze wątki wiedziały, że mają się zakończyć
    for (int i = 0; i < MIMPI_World_size(); i++) {
        if (i != MIMPI_World_rank()) {
            int foo = 1;
            int tag = -1;
            int message_size = sizeof(int) * META_INFO_SIZE + sizeof(int);
            void *message = malloc(message_size);
            memcpy(message + sizeof(int) * 0, &foo, sizeof(int));
            memcpy(message + sizeof(int) * 1, &foo, sizeof(int));
            memcpy(message + sizeof(int) * 2, &tag, sizeof(int));
            memcpy(message + sizeof(int) * 3, &message_size, sizeof(int));
            memcpy(message + sizeof(int) * 4, &foo, sizeof(int));

//            printf("Wyslalem -1 do %d (deskryptor zapisu %d)\n", MIMPI_World_rank(), MIMPI_get_write_desc(i, MIMPI_World_rank()));
            chsend(MIMPI_get_write_desc(i, MIMPI_World_rank()), message, message_size);

            free(message);
        }
    }

//    printf("Czekam na koniec wątku, jestem %d \n", MIMPI_World_rank());
    // Czekanie na zakończenie wszystkich wątków
    for (int i = 0; i < MIMPI_World_size() - 1; i++) {
        pthread_join(threads[i], NULL);
    }

    // TODO czyszczenie listy otrzymanych wiadomości

    // wysyłamy wszystkim innym wiadomość z tagiem OUT_OF_MPI_BLOCK (-1) oznaczającą, że skończyliśmy
    for (int i = 0; i < MIMPI_World_size(); i++) {
        if (i != MIMPI_World_rank() && is_in_MPI_block(i) == true) {
            int message = 0; // TODO to nie powinno miec rozmiaru 512? Bo ktoś czeka na dowolnie długą wiadomość
            MIMPI_Send(&message, 1, i, OUT_OF_MPI_BLOCK);
        }
    }

    pthread_mutex_destroy(&mutex_pipes);
    pthread_mutex_destroy(&parent_sleeping);

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

//        printf("Wysylam czesc %d z %d\n", i + 1, liczba_czesci);

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

    // Potrzebna jakby ktoś kilka razy zrobił recv od zakończonego nadawcy
    pthread_mutex_lock(&mutex_pipes);
    if (finished[source] == true) {
        pthread_mutex_unlock(&mutex_pipes);
        return MIMPI_ERROR_REMOTE_FINISHED;
    }


    // Sprawdzamy, czy taka wiadomość była odebrana już wcześniej
    // list_find znajdzie też wiadomości z tagiem 0 lub dowolną specjalną od tego nadawcy
    Message *message = list_find(received_list, count, source, tag);
    if (message != NULL) {

        if (message->tag < 0) { // Wiadomość specjalna
            if (message->tag == -1) {
                finished[source] = true;
                return MIMPI_ERROR_REMOTE_FINISHED;
            } else {
                assert(0 == 1); // Nieobsłużona wiadomość specjalna
            }
        }

        memcpy(data, message->data, message->count); // Przepisanie danych do bufora użytkownika
        list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
        pthread_mutex_unlock(&mutex_pipes);
        return MIMPI_SUCCESS;
    }

    // Nie znaleźliśmy wiadomości, więc czekamy na nią
    waiting_count = count;
    waiting_source = source;
    waiting_tag = tag;

    pthread_mutex_unlock(&mutex_pipes);
    pthread_mutex_lock(&parent_sleeping); // idziemy spać

    // W tym miejscu na liście jest już nasza wiadomość, tylko musimy ją znaleźć
    pthread_mutex_lock(&mutex_pipes);

    message = list_find(received_list, count, source, tag);
    if (message->tag < 0) { // Wiadomość specjalna
        if (message->tag == -1) {
            finished[source] = true;
            return MIMPI_ERROR_REMOTE_FINISHED;
        } else {
            assert(0 == 1); // Nieobsłużona wiadomość specjalna
        }
    }
    memcpy(data, message->data, message->count); // Przepisanie danych do bufora użytkownika
    list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy

    pthread_mutex_unlock(&mutex_pipes);
    return MIMPI_SUCCESS;
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
