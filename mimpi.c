/**
 * This file is for implementation of MIMPI library.
 * */
#include "channel.h"
#include "mimpi.h"
#include "mimpi_common.h"

#include <stdio.h>
#include <stdlib.h>

void MIMPI_Init(bool enable_deadlock_detection) {
    channels_init();

}

void MIMPI_Finalize() {

    channels_finalize();
}

int MIMPI_World_size() {
    return atoi(getenv("MIMPI_WORLD_SIZE")); // Konwertuje string na int
}

int MIMPI_World_rank() {
    return atoi(getenv("MIMPI_RANK")); // getenv zwraca string
}

int MIMPI_get_read_desc(int src, int dest) {
    return  src * MIMPI_World_size() * 2 + 20 + dest * 2;
}

int MIMPI_get_write_desc(int src, int dest) {
    return  MIMPI_get_read_desc(src, dest) + 1;
}

MIMPI_Retcode MIMPI_Send(
    void const *data,
    int count,
    int destination,
    int tag
) {
    chsend(MIMPI_get_write_desc(MIMPI_World_rank(), destination), data, count);
    return 0;
}

MIMPI_Retcode MIMPI_Recv(
    void *data,
    int count,
    int source,
    int tag
) {
    chrecv(MIMPI_get_read_desc(source, MIMPI_World_rank()), data, count);
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