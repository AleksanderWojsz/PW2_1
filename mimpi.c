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

#define POM_PIPES 25
#define OUT_OF_MPI_BLOCK (-1)
#define META_INFO_SIZE 4

Message* received_list = NULL;
bool finished[16] = {false};

// Wszystkie wiadomości mają rozmiar 512B, jak wiadomość jest za mała, to jest dopełniana zerami
// Zakładam, że jeśli podczas wysyłania bardzo długiej wiadomości odbiorca się skończy, to przestajemy przesyłać wiadomość,
// żeby nie zawiesić się na pipe



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
    chrecv(60, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    is_active[MIMPI_World_rank()] = 0;
    is_active[MIMPI_World_size()]--;
    chsend(61, is_active, sizeof(int) * (MIMPI_World_size() + 1));
}

bool is_in_MPI_block(int nr) {

    if (nr >= MIMPI_World_size()) {
        return false;
    }

    int is_active[MIMPI_World_size() + 1];
    chrecv(60, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    bool result = (is_active[nr] == 1);
    chsend(61, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    return result;
}

int MIMPI_get_read_desc(int src, int dest) {
    return src * MIMPI_World_size() * 2 + 20 + 2 * POM_PIPES + dest * 2;
}

int MIMPI_get_write_desc(int src, int dest) {
    return MIMPI_get_read_desc(src, dest) + 1;
}

// Przekazany bufor ma mieć rozmiar 496B
void read_message_from_pom_pipe(int descriptor, void** result_buffer, int* count, int* tag) {

    int result_buffer_size = 496;
    void* buffer = malloc(512);  // Bufor do przechowywania fragmentów i metadanych
    int received = 0;  // Ile bajtów danych już otrzymaliśmy
    int ile = 0, z_ilu = 1, fragment_tag, current_message_size;

    while (ile < z_ilu) {

        // Odbieranie fragmentu
        chrecv(descriptor, buffer, 512);

        // Rozpakowywanie metadanych z bufora
        memcpy(&ile, buffer + sizeof(int) * 0, sizeof(int));
        memcpy(&z_ilu, buffer + sizeof(int) * 1, sizeof(int));
        memcpy(&fragment_tag, buffer + sizeof(int) * 2, sizeof(int));
        memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));

        // Zapisywanie danych z fragmentu do głównego bufora danych
        memcpy(*result_buffer + received, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
        received += current_message_size;

        if (ile < z_ilu) { // Jak to nie koniec czytania to powiększamy bufor
            result_buffer_size += 496;
            *result_buffer = realloc(*result_buffer, result_buffer_size);
        }
    }
    *count = received;
    *tag = fragment_tag;

    free(buffer);
}


pthread_mutex_t mutex_pipes;
pthread_mutex_t parent_sleeping;

int waiting_count = -10;
int waiting_source = -10;
int waiting_tag = -10;

void *read_messages_from_source(void *src) {

    int source = *((int *)src);

    // Czytamy wiadomosci od konkretnego procesu i zapisujemy je na liscie
    while (true) {

        void* read_message = malloc(496);
        int fragment_tag = 0;
        int count = 0;
        read_message_from_pom_pipe(MIMPI_get_read_desc(source, MIMPI_World_rank()), &read_message, &count, &fragment_tag);

        // W tym miejscu mamy już całą jedną wiadomość

        pthread_mutex_lock(&mutex_pipes);
        list_add(&received_list, read_message, count, source, fragment_tag);

        // Zapisujemy wszystkie wiadomości, a jak rodzic czeka na daną wiadomość
        // lub wiadomość to dowolna specjalna wiadomość od tego nadawcy to budzimy rodzica
        if ((waiting_count == count && waiting_source == source && (waiting_tag == fragment_tag || waiting_tag == 0)) ||
            (waiting_source == source && fragment_tag < 0)) {
            waiting_count = -10;
            waiting_source = -10;
            waiting_tag = -10;
            pthread_mutex_unlock(&parent_sleeping);
        }
        pthread_mutex_unlock(&mutex_pipes);

        free(read_message);

        if (fragment_tag == -1) { // Jak nie będzie już więcej wiadomości to kończymy
            break;
        }
    }

    free(src);

    return NULL;
}

pthread_t threads[16];

void MIMPI_Init(bool enable_deadlock_detection) {

    channels_init();
    MIMPI_World_size();
    MIMPI_World_rank();

    pthread_mutex_init(&mutex_pipes, NULL);
    pthread_mutex_init(&parent_sleeping, NULL);
    pthread_mutex_lock(&parent_sleeping); // zmniejszanie licznika do 0

    // Tworzenie po jednym wątku na każdy pipe
    for (int i = 0; i < MIMPI_World_size(); i++) {
        if (i != MIMPI_World_rank()) {
            int *source = malloc(sizeof(int));
            *source = i;
            pthread_create(&threads[i], NULL, read_messages_from_source, source);
        }
    }
}

void MIMPI_Finalize() {

    notify_iam_out(); // Zabraniamy wysłać do nas wiadomości poprzez MIMPI_SEND

    // Wysyłamy wiadomości z tagiem -1 do samych siebie przez wszystkie pipe'y, żeby nasze wątki wiedziały, że mają się zakończyć
    for (int i = 0; i < MIMPI_World_size(); i++) {
        if (i != MIMPI_World_rank()) {
            int foo1 = 1;
            int tag = -1;
            int message_size = sizeof(int) * META_INFO_SIZE + sizeof(int);
            void *message = malloc(512);
            memset(message, 0, 512);
            memcpy(message + sizeof(int) * 0, &foo1, sizeof(int));
            memcpy(message + sizeof(int) * 1, &foo1, sizeof(int));
            memcpy(message + sizeof(int) * 2, &tag, sizeof(int));
            memcpy(message + sizeof(int) * 3, &message_size, sizeof(int));
            memcpy(message + sizeof(int) * 4, &foo1, sizeof(int));

            chsend(MIMPI_get_write_desc(i, MIMPI_World_rank()), message, 512);

            free(message);
        }
    }

    // Czekanie na zakończenie wszystkich wątków
    for (int i = 0; i < MIMPI_World_size(); i++) {
        if (i != MIMPI_World_rank()) {
            pthread_join(threads[i], NULL);
        }
    }

    list_clear(&received_list);

    // wysyłamy wszystkim innym wiadomość z tagiem OUT_OF_MPI_BLOCK (-1) oznaczającą, że skończyliśmy
    for (int i = 0; i < MIMPI_World_size(); i++) {
        if (i != MIMPI_World_rank() && is_in_MPI_block(i) == true) {
            int message = 0;
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

int world_size = -10;
int MIMPI_World_size() {
    if (world_size == -10) {
        world_size = atoi(getenv("MIMPI_WORLD_SIZE")); // Konwertuje string na int
    }
    return world_size;
}

int world_rank = -10;
int MIMPI_World_rank() {
    if (world_rank == -10) {
        world_rank = atoi(getenv("MIMPI_RANK")); // getenv zwraca string
    }
    return world_rank;
}

int send_message_to_pipe(int descriptor, void const *data, int count, int tag, bool check_if_dest_active, int destination) {
    // Dzielimy wiadomość na części rozmiaru ile + z_ilu + tag + current_message_size + wiadomość = 512B
    int max_message_size = 512 - sizeof(int) * META_INFO_SIZE;
    int liczba_czesci = (count + (max_message_size - 1)) / max_message_size; // ceil(A/B) use (A + (B-1)) / B
    int nr_czesci = 1;

    // tworzenie wiadomosci
    for (int i = 0; i < liczba_czesci; i++) {

        // Na wypadek, gdyby odbiorca się zakończył przy wysłaniu długiej wiadomości
        if (check_if_dest_active && is_in_MPI_block(destination) == false) {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
//        printf("Wysylam czesc %d z %d, jestem %d \n", i + 1, liczba_czesci, MIMPI_World_rank());

        int current_message_size = max_message_size;
        if (i == liczba_czesci - 1) { // ostatnia część wiadomości
            current_message_size = count - (liczba_czesci - 1) * max_message_size;
        }

        void *message = malloc(512);
        memset(message, 0, 512);

        memcpy(message + sizeof(int) * 0, &nr_czesci, sizeof(int));
        memcpy(message + sizeof(int) * 1, &liczba_czesci, sizeof(int));
        memcpy(message + sizeof(int) * 2, &tag, sizeof(int));
        memcpy(message + sizeof(int) * 3, &current_message_size, sizeof(int));
        memcpy(message + sizeof(int) * 4, data + i * max_message_size, current_message_size);

        // Wysyłanie wiadomości
        chsend(descriptor, message, 512);
        nr_czesci++;
        free(message);
    }

    return MIMPI_SUCCESS;
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

    return send_message_to_pipe(MIMPI_get_write_desc(MIMPI_World_rank(), destination), data, count, tag, true, destination);
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
                list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
                pthread_mutex_unlock(&mutex_pipes);
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
            list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
            pthread_mutex_unlock(&mutex_pipes);
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



int get_barrier_read_desc(int rank) {
    return 20 + rank * 2;
}

int get_barrier_write_desc(int rank) {
    return get_barrier_read_desc(rank) + 1;
}

int find_parent(int root) {
    int i = world_size - root;
    int n = world_size;
    int w = world_rank;

    return ((((w + i) % n) - 1) / 2 + n - i) % n;
}


void children_wake_up_and_send_data(int w, int i, int n, void* data, int count) {
    // budzimy dzieci
    if (2 * ((w + i) % n) + 1 < n) { // Jak mamy lewe dziecko
        int l = (2 * w + i + 1) % n;
        send_message_to_pipe(get_barrier_write_desc(l), data, count, 10, false, 10);
    }
    if (2 * ((w + i) % n) + 2 < n) {
        int p = (2 * w + i + 2) % n;
        send_message_to_pipe(get_barrier_write_desc(p), data, count, 10, false, 10);
    }
}




MIMPI_Retcode MIMPI_Bcast(
        void *data,
        int count,
        int root
) {
    void* buffer = malloc(512);
    void* foo_message = malloc(512);
    memset(foo_message, 0, 512);

    int foo_fragment_tag = 0;
    int foo_count = 0;

    int i = world_size - root;
    int n = world_size;
    int w = world_rank;

    // Czekanie aż wszyscy się zbiorą
    if (world_rank == root) { // jesteśmy korzeniem
        chrecv(get_barrier_read_desc(world_rank), buffer, 512); // Czekamy na dowolną wiadomość
        if (2 * ((w +i) % n) + 2 < world_size) { // Jak mamy prawe dziecko to czekamy na drugą wiadomość
            chrecv(get_barrier_read_desc(world_rank), buffer, 512);
        }
    }
    else if (2 * ((w +i) % n) + 1 < world_size) { // jesteśmy rodzicem niekorzeniem
        chrecv(get_barrier_read_desc(world_rank), buffer, 512);
        if (2 * ((w +i) % n) + 2 < world_size) { // Jak mamy prawe dziecko to czekamy na drugą wiadomość
            chrecv(get_barrier_read_desc(world_rank), buffer, 512);
        }
        chsend(get_barrier_write_desc(find_parent(root)), foo_message, 512); // wysyłamy wiadomość rodzicowi
    }
    else { // jesteśmy liściem
        chsend(get_barrier_write_desc(find_parent(root)), foo_message, 512); // wysyłamy wiadomość rodzicowi
    }


    // Tutaj już wszyscy się zebrali. Przekazujemy dane
    if (world_rank == root) { // jesteśmy korzeniem

        children_wake_up_and_send_data(w, i, n, data, count);
    }
    else if (2 * ((w +i) % n) + 1 < world_size)  { // jesteśmy rodzicem niekorzeniem

        // czekamy na wiadomosc od rodzica
        void* read_message = malloc(496); // Ten bufor może zostać powiększony
        read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &read_message, &foo_fragment_tag, &foo_count);

        // zapisujemy dane od root'a
        memcpy(data, read_message, count);
        free(read_message);

        children_wake_up_and_send_data(w, i, n, data, count);
    }
    else { // jesteśmy liściem

        // czekamy na wiadomosc od rodzica
        void* read_message = malloc(496); // Ten bufor może zostać powiększony
        read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &read_message, &foo_fragment_tag, &foo_count);

        // zapisujemy dane od root'a
        memcpy(data, read_message, count);
        free(read_message);
    }

    free(buffer);
    free(foo_message);

    return MIMPI_SUCCESS;
}

// Korzystamy tylko z synchronizującego efektu Bcast
MIMPI_Retcode MIMPI_Barrier() {
    void* foo_data = malloc(512);
    memset(foo_data, 0, 512);

    if (world_size == 1) {
        free(foo_data);
        return MIMPI_SUCCESS;
    }
    MIMPI_Retcode ret = MIMPI_Bcast(foo_data, 1, 0);
    free(foo_data);
    return ret;
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
    chrecv(60, is_active, sizeof(int) * (MIMPI_World_size() + 1));
    for (int i = 0; i < MIMPI_World_size() + 1; i++) {
        printf("%d ", is_active[i]);
    }
    printf("\n");

    chsend(61, is_active, sizeof(int) * (MIMPI_World_size() + 1));
}