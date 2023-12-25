#include <stdio.h>
#include "mimpi.h"
#include "moja.h"

int main() {
//    printf("Moja ranga to %d, rozmiar świata to %d\n", MIMPI_World_rank(), MIMPI_World_size());

    int data = 0;
    if (MIMPI_World_rank() == 0) {
        data = 42;
        MIMPI_Send(&data, 1, 1, 0);
    } else if (MIMPI_World_rank() == 1) {
        MIMPI_Recv(&data, 1, 0, 0);
        printf("Otrzymałem %d\n", data);
    }

    return 0;
}

/*
 make
 ./mimirun

 cd examples_build
 ./bad_rank

 bash ./test



Procedury pomocnicze:
  void MIMPI_Init(bool enable_deadlock_detection)
  void MIMPI_Finalize()
  int MIMPI_World_size()
  void MIMPI_World_rank()

Procedury do komunikacji punkt-punkt
 MIMPI_Retcode MIMPI_Send(void const *data, int count, int destination, int tag)
 MIMPI_Retcode MIMPI_Recv(void *data, int count, int source, int tag)

 Procedury do komunikacji grupowej
  MIMPI_Retcode MIMPI_Barrier()
  MIMPI_Retcode MIMPI_Bcast(void *data, int count, int root)
  MIMPI_Retcode MIMPI_Reduce(const void *send_data, void *recv_data, int count, MPI_Op op, int root)

 */
