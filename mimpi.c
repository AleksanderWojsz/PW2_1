/**
 * This file is for implementation of MIMPI library.
 * */
#include "channel.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include "moja.h"

#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

#define POM_PIPES 90
#define OUT_OF_MPI_BLOCK (-1)
#define META_INFO_SIZE 4

Message* received_list = NULL;
bool finished[16] = {false};
bool someone_already_finished = false;

// Wszystkie wiadomości mają rozmiar 512B, jak wiadomość jest za mała, to jest dopełniana zerami
// Zakładam, że jeśli podczas wysyłania bardzo długiej wiadomości odbiorca się skończy, to przestajemy przesyłać wiadomość,
// żeby nie zawiesić się na pipe

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


Message* sent_messages = NULL;

// zakleszczenia od 70 do 119
// 111 C
// 113 A
// 115 source
// 117 count
// 119 tag

bool deadlock_detection = false;

// mutexy
int get_deadlock_read_desc(int rank) {
    return 70 + rank * 2;
}

int get_deadlock_write_desc(int rank) {
    return get_deadlock_read_desc(rank) + 1;
}

// TODO dodac obsluge do reduce
// TODO czyscic oczekujace
// TODO mutex ma obejmowac caly receive?
// TODO duzo wiadomosci zakleszczenia

// od 120 do 159 na odebrane wiadomości + od 160 do 199 na liczniki(mutexy)
int get_deadlock_received_read_desc(int rank) {
    return 120 + rank * 2;
}

int get_deadlock_received_write_desc(int rank) {
    return get_deadlock_received_read_desc(rank) + 1;
}

int get_deadlock_counter_read_desc(int rank) {
    return 160 + rank * 2;
}

int get_deadlock_counter_write_desc(int rank) {
    return get_deadlock_counter_read_desc(rank) + 1;
}

void notify_that_message_received(int who_sent_it_to_us, int count, int tag) {
    int message[3];
    message[0] = world_rank;
    message[1] = count;
    message[2] = tag;

    int no_of_messages;
    chrecv(get_deadlock_counter_read_desc(who_sent_it_to_us), &no_of_messages, sizeof(int)); // Też jako mutex
    no_of_messages++;
    chsend(get_deadlock_received_write_desc(who_sent_it_to_us), message, sizeof(int) * 3);
    chsend(get_deadlock_counter_write_desc(who_sent_it_to_us), &no_of_messages, sizeof(int));
}

Message* odczytane_wiadomosci_deadlock = NULL;
bool finished_deadlock[16] = {false};
bool rodzic_czeka_na_odczytanie_wszystkich = false;
pthread_mutex_t mutex_pipes;
pthread_mutex_t parent_sleeping;


bool check_for_deadlock_in_receive(int count_arg, int source_arg, int tag_arg) {

    // dodajemy nasze zgłoszenie
    int c[world_size];
    int a[world_size];
    int source[world_size];
    int count[world_size];
    int tag[world_size];

    chrecv(110, c, sizeof(int) * world_size);
    chrecv(112, a, sizeof(int) * world_size);
    chrecv(114, source, sizeof(int) * world_size);
    chrecv(116, count, sizeof(int) * world_size);
    chrecv(118, tag, sizeof(int) * world_size);

    c[world_rank] = -1;
    a[world_rank] = -1;
    source[world_rank] = source_arg;
    count[world_rank] = count_arg;
    tag[world_rank] = tag_arg;

    while (true) {

        // usuwamy z listy wysłanych wiadomości te, które już ktoś odebrał
        int no_of_messages_to_remove;
        chrecv(get_deadlock_counter_read_desc(world_rank), &no_of_messages_to_remove, sizeof(int));
        chsend(get_deadlock_counter_write_desc(world_rank), &no_of_messages_to_remove, sizeof(int));
        if (no_of_messages_to_remove > 0) {
            rodzic_czeka_na_odczytanie_wszystkich = true;
            pthread_mutex_lock(&parent_sleeping); // jak są jeszcze jakies nieodbrane wiadomości to czekamy
        }

        chrecv(get_deadlock_counter_read_desc(world_rank), &no_of_messages_to_remove, sizeof(int)); // Jako mutex
        // Usuwamy odczytane wiadomości z listy wysłanych
        while (odczytane_wiadomosci_deadlock != NULL) {
            list_remove(&sent_messages, odczytane_wiadomosci_deadlock->count, odczytane_wiadomosci_deadlock->source, odczytane_wiadomosci_deadlock->tag);
            odczytane_wiadomosci_deadlock = odczytane_wiadomosci_deadlock->next;
        }
        list_clear(&odczytane_wiadomosci_deadlock);
        chsend(get_deadlock_counter_write_desc(world_rank), &no_of_messages_to_remove, sizeof(int));


        // Przetwarzamy zgłoszenia
        for(int i = 0; i < world_size; i++) {
            if (source[i] == world_rank) { // jak ktoś na nas czeka
                Message* message = list_find(sent_messages, count[i], i, tag[i]); // sprawdzamy, czy mamy dla niego wiadomosc

                if (message != NULL) { // mamy dla kogoś wiadomość, więc nic nie robimy
                }
                else { // nie ma wiadomości więc potwierdzamy
                    a[i] = 1; // approved

                    // budzimy
                    void* foo_message = malloc(512);
                    memset(foo_message, 0, 512);
                    chsend(get_deadlock_write_desc(i), foo_message, 512);
                    free(foo_message);
                }
            }
        }

        if (a[world_rank] == 1 && c[world_rank] != 1 && source[source_arg] == world_rank && a[source_arg] == 1 && c[source_arg] != 1) { // jak my na pewno czekamy, nie byliśmy już wcześniej w cyklu, i ktos jest z nami w cyklu // TODO tutaj też drugi proces musi miec A i nie C?
            c[world_rank] = 1; // jestesmy w cyklu
            c[source_arg] = 1; // oznaczamy, że ktoś jest z nami w cyklu
        }

        if (c[world_rank] == 1) { // jak jesteśmy w cyklu, to usuwamy nasz wpis i wychodzimy
            c[world_rank] = -1;
            a[world_rank] = -1;
            source[world_rank] = -1;
            count[world_rank] = -1;
            tag[world_rank] = -1;
            chsend(111, c, sizeof(int) * world_size);
            chsend(113, a, sizeof(int) * world_size);
            chsend(115, source, sizeof(int) * world_size);
            chsend(117, count, sizeof(int) * world_size);
            chsend(119, tag, sizeof(int) * world_size);

            return true;
        }

        // Oddajemy tablice
        chsend(111, c, sizeof(int) * world_size);
        chsend(113, a, sizeof(int) * world_size);
        chsend(115, source, sizeof(int) * world_size);
        chsend(117, count, sizeof(int) * world_size);
        chsend(119, tag, sizeof(int) * world_size);


        // śpimy

        while (true) {
            int fragment_tag;
            int who_finished;
            void* foo_message = malloc(512);
            chrecv(get_deadlock_read_desc(world_rank), foo_message, 512);
            memcpy(&fragment_tag, foo_message + sizeof(int) * 2, sizeof(int));
            memcpy(&who_finished, foo_message + sizeof(int) * 3, sizeof(int));

            if (fragment_tag < 0) {
                if (fragment_tag == -1) {
                    finished_deadlock[who_finished] = true;
                    if (who_finished == source_arg) { // odczytaliśmy wiadomość, że ten na kogo czekamy skończył więc nie będzie z nim deadlocka
                        free(foo_message);
                        return false;
                    }
                    else if (who_finished != source_arg) { // ktos się skonczyl, ale nie czekamy na niego w tym receive
                        // idziemy spac znowu
                    }
                }
                else if (fragment_tag == -2) { // wątek nas obudził, bo mamy wiadomość
                    // usuwamy nasz wpis i wychodzimy

                    chrecv(110, c, sizeof(int) * world_size);
                    chrecv(112, a, sizeof(int) * world_size);
                    chrecv(114, source, sizeof(int) * world_size);
                    chrecv(116, count, sizeof(int) * world_size);
                    chrecv(118, tag, sizeof(int) * world_size);

                    c[world_rank] = -1;
                    a[world_rank] = -1;
                    source[world_rank] = -1;
                    count[world_rank] = -1;
                    tag[world_rank] = -1;

                    chsend(111, c, sizeof(int) * world_size);
                    chsend(113, a, sizeof(int) * world_size);
                    chsend(115, source, sizeof(int) * world_size);
                    chsend(117, count, sizeof(int) * world_size);
                    chsend(119, tag, sizeof(int) * world_size);

                    free(foo_message);

                    return false;
                }
            }
            else{ // (fragment_tag >= 0) Ktos nas obudził, bo przetworzył naszą wiadomość
                break;
            }

            free(foo_message);
        }

        chrecv(110, c, sizeof(int) * world_size);
        chrecv(112, a, sizeof(int) * world_size);
        chrecv(114, source, sizeof(int) * world_size);
        chrecv(116, count, sizeof(int) * world_size);
        chrecv(118, tag, sizeof(int) * world_size);
    }
}












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

// Przekazany bufor ma mieć rozmiar 496B // TODO zmienic na pusty bufor
void read_message_from_pom_pipe(int descriptor, void** result_buffer, int* count, int* tag) {

    long long int result_buffer_size = 496;
    free(*result_buffer); // TODO dodalem
    *result_buffer = malloc(result_buffer_size); // TODO dodalem

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

            // TODO sprawdzenie deadlocka robimy tylko jak tag jest >= 0, wiec wątek nie będzie czekał konkretnie na specjalną wiadomość
            // TODO zakładamy że z wiadomościami specjalnymi nie ma zakleszczeń
            if (deadlock_detection == true && fragment_tag >= 0) {
                void *message = malloc(512);
                memset(message, 0, 512);
                int tag = -2;
                memcpy(message + sizeof(int) * 2, &tag, sizeof(int));
                chsend(get_deadlock_write_desc(world_rank), message, 512);
                free(message);
            }

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





void *czytaj_odczytane_wiadomosci_deadlock() {

    // Czytamy wiadomości które ktoś już odebrał i zapisujemy je na listę
    while (true) {
        int message[3];
        chrecv(get_deadlock_received_read_desc(world_rank), message, sizeof(int) * 3);

        int no_of_messages_to_remove;
        chrecv(get_deadlock_counter_read_desc(world_rank), &no_of_messages_to_remove, sizeof(int)); // Też jako mutex
        if (message[2] == -1) {
            chsend(get_deadlock_counter_write_desc(world_rank), &no_of_messages_to_remove, sizeof(int)); // Oddajemy mutexa
            return NULL;
        }
        list_add(&odczytane_wiadomosci_deadlock, NULL, message[1], message[0], message[2]);
        for (int i = 0; i < no_of_messages_to_remove - 1; i++) { // -1 bo juz pierwszą wiadomość odczytaliśmy
            list_add(&odczytane_wiadomosci_deadlock, NULL, message[1], message[0], message[2]);
            chrecv(get_deadlock_received_read_desc(world_rank), message, sizeof(int) * 3);
            if (message[2] == -1) {
                chsend(get_deadlock_counter_write_desc(world_rank), &no_of_messages_to_remove, sizeof(int)); // Oddajemy mutexa
                return NULL;
            }
        }
        no_of_messages_to_remove = 0;

        if (rodzic_czeka_na_odczytanie_wszystkich == true) {
            rodzic_czeka_na_odczytanie_wszystkich = false;
            pthread_mutex_unlock(&parent_sleeping);
        }
        chsend(get_deadlock_counter_write_desc(world_rank), &no_of_messages_to_remove, sizeof(int));
    }
}



pthread_t threads[16];
pthread_t czytajacy_odczytane_wiadomosci_deadlock;

void MIMPI_Init(bool enable_deadlock_detection) {

    if (enable_deadlock_detection) {
        deadlock_detection = true;
    }

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

    // wątek czytający odczytane wiadomosci
    if (deadlock_detection == true) {
        pthread_create(&czytajacy_odczytane_wiadomosci_deadlock, NULL, czytaj_odczytane_wiadomosci_deadlock, NULL);
    }

}

int send_message_to_pipe(int descriptor, void const *data, int count, int tag, bool check_if_dest_active, int destination) {
    // Dzielimy wiadomość na części rozmiaru ile + z_ilu + tag + current_message_size + wiadomość = 512B
    int max_message_size = 512 - sizeof(int) * META_INFO_SIZE;
    int liczba_czesci = (int)(((long long int)count + ((long long int)max_message_size - 1)) / (long long int)max_message_size); // ceil(A/B) use (A + (B-1)) / B

    int nr_czesci = 1;

    // tworzenie wiadomosci
    for (int i = 0; i < liczba_czesci; i++) {

        // Na wypadek, gdyby odbiorca się zakończył przy wysłaniu długiej wiadomości
        if (check_if_dest_active && is_in_MPI_block(destination) == false) {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }

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

int get_barrier_read_desc(int rank) {
    return 20 + rank * 2;
}

int get_barrier_write_desc(int rank) {
    return get_barrier_read_desc(rank) + 1;
}

int no_of_barrier = 0;

void MIMPI_Finalize() {

    notify_iam_out(); // Zabraniamy wysłać do nas wiadomości poprzez MIMPI_SEND

    for (int i = 0; i < MIMPI_World_size(); i++) { // Wysyłamy do pipe'ów od babiery, wiadomość z tagiem -1, że nas nie będzie
        void *message = malloc(512);
        memset(message, 99, 512);
        if (i != MIMPI_World_rank()) {
            send_message_to_pipe(get_barrier_write_desc(i), message, 100, -no_of_barrier - 1, false, 13); // Tag oznacza liczbe barier przez które przeszliśmy na minusie - 1
        }
        free(message);
    }

    if (deadlock_detection) {
        for (int i = 0; i < MIMPI_World_size(); i++) { // Wysyłamy do pipe'ów od deadlock, że skonczyliśmy i nie będzie z nami deadlocka już nigdy
            if (i != MIMPI_World_rank()) {
                void *message = malloc(512);
                int tag = -1;
                memset(message, 0, 512);
                memcpy(message + sizeof(int) * 2, &tag, sizeof(int)); // ze to wiadomość specjalna
                memcpy(message + sizeof(int) * 3, &world_rank, sizeof(int)); // i to kim jestesmy
                chsend(get_deadlock_write_desc(i), message, 512);
                free(message);
            }
        }

        // TODO powiedzieliśmy już że nie mozna wysyłać do nas wiadomości, teraz kończymy wątek
//
//        MIMPI_Barrier();

        int message[3];
        message[0] = 10;
        message[1] = 10;
        message[2] = -1;
        chsend(get_deadlock_received_write_desc(world_rank), message, sizeof(int) * 3);
        pthread_join(czytajacy_odczytane_wiadomosci_deadlock, NULL);
    }


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
    list_clear(&sent_messages);
    list_clear(&odczytane_wiadomosci_deadlock);

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
        someone_already_finished = true;
        return MIMPI_ERROR_REMOTE_FINISHED;
    }

    if (deadlock_detection && tag >= 0) { // Do listy dodajemy tylko wiadomości z tagiem >= 0, bo nikt nigdy nie będzię czekał na wiadomość specjalną
        list_add(&sent_messages, data, count, destination, tag);
    }

    int result = send_message_to_pipe(MIMPI_get_write_desc(MIMPI_World_rank(), destination), data, count, tag, true, destination);

    return result;
}


MIMPI_Retcode MIMPI_Recv(
        void* data,
        int count,
        int source,
        int tag
) {

//    printf("Jestem %d, robie receive %d %d %d\n", world_rank, count, source, tag);

    if (check_arguments_correctness(source) != 0) {
        return check_arguments_correctness(source);
    }

    // Potrzebna jakby ktoś kilka razy zrobił recv od zakończonego nadawcy
    pthread_mutex_lock(&mutex_pipes);
    if (finished[source] == true) {
        pthread_mutex_unlock(&mutex_pipes);
        someone_already_finished = true;
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
                someone_already_finished = true;
                return MIMPI_ERROR_REMOTE_FINISHED;
            } else {
                assert(0 == 1); // Nieobsłużona wiadomość specjalna
            }
        }

        memcpy(data, message->data, message->count); // Przepisanie danych do bufora użytkownika
        list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
        pthread_mutex_unlock(&mutex_pipes);

        if (deadlock_detection && is_in_MPI_block(source) == true && tag >= 0) {
            notify_that_message_received(source, count, tag);
        }

        return MIMPI_SUCCESS;
    }

    // Nie znaleźliśmy wiadomości, więc czekamy na nią
    waiting_count = count;
    waiting_source = source;
    waiting_tag = tag;
    pthread_mutex_unlock(&mutex_pipes);

    if (deadlock_detection && tag >= 0 && finished_deadlock[source] == false) {
        if (check_for_deadlock_in_receive(count, source, tag)) {
            return MIMPI_ERROR_DEADLOCK_DETECTED;
        }
    }


    pthread_mutex_lock(&parent_sleeping); // idziemy spać


    // W tym miejscu na liście jest już nasza wiadomość, tylko musimy ją znaleźć
    pthread_mutex_lock(&mutex_pipes);

    message = list_find(received_list, count, source, tag);
    if (message->tag < 0) { // Wiadomość specjalna
        if (message->tag == -1) {
            finished[source] = true;
            list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
            pthread_mutex_unlock(&mutex_pipes);
            someone_already_finished = true;
            return MIMPI_ERROR_REMOTE_FINISHED;
        } else {
            assert(0 == 1); // Nieobsłużona wiadomość specjalna
        }
    }
    memcpy(data, message->data, message->count); // Przepisanie danych do bufora użytkownika
    list_remove(&received_list, count, source, tag); // Usunięcie wiadomości z listy
    pthread_mutex_unlock(&mutex_pipes);


    if (deadlock_detection && is_in_MPI_block(source) == true && tag >= 0) {
        notify_that_message_received(source, count, tag);
    }

    return MIMPI_SUCCESS;
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

bool handle_fragment_tags(int* fragment_tag, void** buffer, int* foo_count, int read_descriptor) {
    while (*fragment_tag < 0) {
        if (no_of_barrier == -*fragment_tag - 1) {
            someone_already_finished = true;
        }
        else if (no_of_barrier == -*fragment_tag) {
            free(*buffer);
            someone_already_finished = true;
            no_of_barrier--;
            return true;
        }
        else {
            assert(0 == 1); // Nieobsłużona wiadomość specjalna
        }
        read_message_from_pom_pipe(read_descriptor, buffer, foo_count, fragment_tag); // Czekamy na dowolną wiadomość
    }
    return false;
}


MIMPI_Retcode MIMPI_Bcast(
        void *data,
        int count,
        int root
) {

    if (someone_already_finished == true) {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    no_of_barrier++; // To musi być po, bo przecież nie weszliśmy

    void* buffer = malloc(512);



    int fragment_tag = 0;
    int foo_count = 0;

    int i = world_size - root;
    int n = world_size;
    int w = world_rank;

    // Czekanie aż wszyscy się zbiorą
    if (world_rank == root) { // jesteśmy korzeniem
        read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &buffer, &foo_count, &fragment_tag); // Czekamy na wiadomość od rodzica

        if (handle_fragment_tags(&fragment_tag, &buffer, &foo_count, get_barrier_read_desc(world_rank))) {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }

        if (2 * ((w +i) % n) + 2 < world_size) { // Jak mamy prawe dziecko to czekamy na drugą wiadomość
            read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &buffer, &foo_count, &fragment_tag); // Czekamy na wiadomość od rodzica
            if (handle_fragment_tags(&fragment_tag, &buffer, &foo_count, get_barrier_read_desc(world_rank))) {
                return MIMPI_ERROR_REMOTE_FINISHED;
            }
        }
    }
    else if (2 * ((w +i) % n) + 1 < world_size) { // jesteśmy rodzicem niekorzeniem
        read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &buffer, &foo_count, &fragment_tag); // Czekamy na wiadomość od rodzica
        if (handle_fragment_tags(&fragment_tag, &buffer, &foo_count, get_barrier_read_desc(world_rank))) {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
        if (2 * ((w +i) % n) + 2 < world_size) { // Jak mamy prawe dziecko to czekamy na drugą wiadomość
            read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &buffer, &foo_count, &fragment_tag); // Czekamy na wiadomość od rodzica
            if (handle_fragment_tags(&fragment_tag, &buffer, &foo_count, get_barrier_read_desc(world_rank))) {
                return MIMPI_ERROR_REMOTE_FINISHED;
            }
        }
        void* foo_message = malloc(512);
        memset(foo_message, 0, 512);
        chsend(get_barrier_write_desc(find_parent(root)), foo_message, 512); // wysyłamy wiadomość rodzicowi // TODO tu 512 dziala bo to nie send
        free(foo_message);
    }
    else { // jesteśmy liściem
        void* foo_message = malloc(512);
        memset(foo_message, 0, 512);
        chsend(get_barrier_write_desc(find_parent(root)), foo_message, 512); // wysyłamy wiadomość rodzicowi
        free(foo_message);
    }

    free(buffer);

    // Tutaj już wszyscy się zebrali. Przekazujemy dane
    if (world_rank == root) { // jesteśmy korzeniem

        children_wake_up_and_send_data(w, i, n, data, count);
    }
    else if (2 * ((w +i) % n) + 1 < world_size)  { // jesteśmy rodzicem niekorzeniem

        // czekamy na wiadomosc od rodzica
        void* read_message = malloc(496); // Ten bufor może zostać powiększony
        read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &read_message, &foo_count, &fragment_tag);

        if (handle_fragment_tags(&fragment_tag, &read_message, &foo_count, get_barrier_read_desc(world_rank))) {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }

        // zapisujemy dane od root'a
        memcpy(data, read_message, count);
        free(read_message);

        children_wake_up_and_send_data(w, i, n, data, count);
    }
    else { // jesteśmy liściem

        // czekamy na wiadomosc od rodzica
        void* read_message = malloc(496); // Ten bufor może zostać powiększony
        read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &read_message, &foo_count, &fragment_tag);

        if (handle_fragment_tags(&fragment_tag, &read_message, &foo_count, get_barrier_read_desc(world_rank))) {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }

        // zapisujemy dane od root'a
        memcpy(data, read_message, count);
        free(read_message);

    }

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

// tablice są typu uint8_t
void reduce(void* reduced_array, void const* array_1, void const* array_2, int count, MIMPI_Op op) {

    uint8_t* reduced = (uint8_t*)reduced_array;
    uint8_t* arr1 = (uint8_t*)array_1;
    uint8_t* arr2 = (uint8_t*)array_2;

    for (int i = 0; i < count; i++) {
        if (op == MIMPI_SUM) {
            reduced[i] = arr1[i] + arr2[i];
        }
        else if (op == MIMPI_PROD) {
            reduced[i] = arr1[i] * arr2[i];
        }
        else if (op == MIMPI_MIN) {
            if (arr1[i] < arr2[i]) {
                reduced[i] = arr1[i];
            }
            else {
                reduced[i] = arr2[i];
            }
        }
        else if (op == MIMPI_MAX) {
            if (arr1[i] > arr2[i]) {
                reduced[i] = arr1[i];
            }
            else {
                reduced[i] = arr2[i];
            }
        }
    }
}





















bool handle_fragment_tags2(int* fragment_tag, void** buffer, int* foo_count, int read_descriptor) {
    while (*fragment_tag < 0) {
        if (no_of_barrier == -*fragment_tag - 1) {
            someone_already_finished = true;
        }
        else if (no_of_barrier == -*fragment_tag) {
            free(*buffer);
            someone_already_finished = true;
            no_of_barrier--;
            return true;
        }
        else {
            assert(0 == 1); // Nieobsłużona wiadomość specjalna
        }
        read_message_from_pom_pipe(read_descriptor, buffer, foo_count, fragment_tag); // Czekamy na dowolną wiadomość
    }
    return false;
}


MIMPI_Retcode MIMPI_Reduce(
        void const *send_data,
        void *recv_data,
        int count,
        MIMPI_Op op,
        int root
) {

    // Wywołujemy barierę, żeby sprawdzić czy wszyscy przyjdą, i nie zakleszczymy się na wysyłaniu dużej tablicy
    // Moja złożoność dla count = 40 000 zaoszczędza 76 czasów działania zwykłej bariery, więc możemy sobie pozwolić na dołożenie jednej
    if (count > 40000) {
        MIMPI_Barrier();
    }

    if (someone_already_finished == true) {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    no_of_barrier++; // To musi być po, bo przecież nie weszliśmy

    int fragment_tag = 0;
    int foo_count = 0;

    void* array_1 = malloc(count);
    void* array_2 = malloc(count);
    void* reduced_array = malloc(count);

    int i = world_size - root;
    int n = world_size;
    int w = world_rank;

    // Wysyłany tag oznacza numer nadawcy, żeby czytający wiedział, od kogo jest dana porcja z pipe'a

    if (world_rank == root || 2 * ((w +i) % n) + 1 < world_size) { // jesteśmy korzeniem lub mamy dziecko

        if (2 * ((w +i) % n) + 1 < world_size && 2 * ((w +i) % n) + 2 >= world_size) { // mamy tylko lewe dziecko
            read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &array_1, &foo_count, &fragment_tag); // czekamy na pierwszą tablicę
            while (fragment_tag < 0) {
                if (no_of_barrier == -fragment_tag - 1) {
                    someone_already_finished = true;
                }
                else if (no_of_barrier == -fragment_tag) {
                    free(array_1);
                    free(array_2);
                    free(reduced_array);
                    someone_already_finished = true;
                    no_of_barrier--;
                    return MIMPI_ERROR_REMOTE_FINISHED;
                }
                else {
                    assert(0 == 1); // Nieobsłużona wiadomość specjalna
                }
                read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &array_1, &foo_count, &fragment_tag); // czekamy na pierwszą tablicę
            }


            reduce(reduced_array, send_data, array_1, count, op);
        }
        else if (2 * ((w +i) % n) + 1 < world_size && 2 * ((w +i) % n) + 2 < world_size) { // mamy dwójkę dzieci

            // Odczytana część może być od dowolnego z dzieci
            int max_message_size = 512 - sizeof(int) * META_INFO_SIZE;
            int liczba_czesci = (int)(((long long int)count + ((long long int)max_message_size - 1)) / (long long int)max_message_size); // ceil(A/B) use (A + (B-1)) / B
            void* buffer = malloc(512);  // Bufor do przechowywania fragmentów i metadanych
            int ile = 0, z_ilu = 1, ranga_nadawcy, current_message_size;
            int received_1 = 0;
            int received_2 = 0;
            int pierwsza_ranga;

            for (int j = 0; j < 2 * liczba_czesci; j++) {

                chrecv(get_barrier_read_desc(world_rank), buffer, 512); // Nie uzywamy read message from pipe bo tutaj mozemy miec fragmenty w dowolnej kolejnosci, wiec robimy fragment po fragmencie
                // Rozpakowywanie metadanych z bufora
                memcpy(&ile, buffer + sizeof(int) * 0, sizeof(int));
                memcpy(&z_ilu, buffer + sizeof(int) * 1, sizeof(int));
                memcpy(&ranga_nadawcy, buffer + sizeof(int) * 2, sizeof(int));
                memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));


                while (ranga_nadawcy < 0) {
                    if (no_of_barrier == -ranga_nadawcy - 1) {
                        someone_already_finished = true;
                    }
                    else if (no_of_barrier == -ranga_nadawcy) {
                        free(buffer);
                        free(array_1);
                        free(array_2);
                        free(reduced_array);
                        someone_already_finished = true;
                        no_of_barrier--;
                        return MIMPI_ERROR_REMOTE_FINISHED;
                    }
                    else {
                        assert(0 == 1); // Nieobsłużona wiadomość specjalna
                    }

                    // Rozpakowywanie metadanych z bufora
                    chrecv(get_barrier_read_desc(world_rank), buffer, 512); // Nie uzywamy read message from pipe bo tutaj mozemy miec fragmenty w dowolnej kolejnosci, wiec robimy fragment po fragmencie
                    memcpy(&ile, buffer + sizeof(int) * 0, sizeof(int));
                    memcpy(&z_ilu, buffer + sizeof(int) * 1, sizeof(int));
                    memcpy(&ranga_nadawcy, buffer + sizeof(int) * 2, sizeof(int));
                    memcpy(&current_message_size, buffer + sizeof(int) * 3, sizeof(int));

                }


                if (j == 0) {
                    pierwsza_ranga = ranga_nadawcy;
                }

                if (ranga_nadawcy == pierwsza_ranga) {
                    memcpy(array_1 + received_1, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
                    received_1 += current_message_size;
                } else {
                    memcpy(array_2 + received_2, buffer + sizeof(int) * META_INFO_SIZE, current_message_size);
                    received_2 += current_message_size;
                }
            }

            free(buffer);

            reduce(reduced_array, send_data, array_1, count, op);
            reduce(reduced_array, reduced_array, array_2, count, op);
        }

        if (world_rank == root) { // jesteśmy korzeniem
            // zapisujemy sobie wynik
            memcpy(recv_data, reduced_array, count);
        } else {
            send_message_to_pipe(get_barrier_write_desc(find_parent(root)), reduced_array, count, world_rank, false, 10); // wysyłamy wiadomość rodzicowi
        }
    }
    else { // jesteśmy liściem
        send_message_to_pipe(get_barrier_write_desc(find_parent(root)), send_data, count, world_rank, false, 10); // wysyłamy naszą tablicę rodzicowi
    }

    free(array_1);
    free(array_2);
    free(reduced_array);



    // Czekamy aż root obudzi wszystkich
    void* foo_message = malloc(512);
    memset(foo_message, -99, 512);
    void* foo_buffer = malloc(512);

    if (world_rank == root) { // jesteśmy korzeniem
        children_wake_up_and_send_data(w, i, n, foo_message, 100);
    }
    else if (2 * ((w +i) % n) + 1 < world_size) { // jesteśmy rodzicem niekorzeniem

        // czekamy na wiadomosc od rodzica
        read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &foo_buffer, &foo_count, &fragment_tag);

        while (fragment_tag < 0) {
            if (no_of_barrier == -fragment_tag - 1) {
                someone_already_finished = true;
            }
            else if (no_of_barrier == -fragment_tag) {
                free(foo_buffer);
                free(foo_message);
                someone_already_finished = true;
                no_of_barrier--;
                return MIMPI_ERROR_REMOTE_FINISHED;
            }
            else {
                assert(0 == 1); // Nieobsłużona wiadomość specjalna
            }
            read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &foo_buffer, &foo_count, &fragment_tag); // czekamy na pierwszą tablicę
        }

        children_wake_up_and_send_data(w, i, n, foo_message, 100);
    }
    else { // jesteśmy liściem

        // czekamy na wiadomosc od rodzica
        read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &foo_buffer, &foo_count, &fragment_tag);

        while (fragment_tag < 0) {
            if (no_of_barrier == -fragment_tag - 1) {
                someone_already_finished = true;
            }
            else if (no_of_barrier == -fragment_tag) {
                free(foo_buffer);
                free(foo_message);
                someone_already_finished = true;
                no_of_barrier--;
                return MIMPI_ERROR_REMOTE_FINISHED;
            }
            else {
                assert(0 == 1); // Nieobsłużona wiadomość specjalna
            }
            read_message_from_pom_pipe(get_barrier_read_desc(world_rank), &foo_buffer, &foo_count, &fragment_tag); // czekamy na pierwszą tablicę
        }
    }

    free(foo_message);
    free(foo_buffer);

    return MIMPI_SUCCESS;
}