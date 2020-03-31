#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mympi.h"

int main(int argc, char* argv[]) {

    int rank;
    int num_procs;

    myMPIInit(&argc, &argv);

    myMPICommRank(&rank);
    myMPICommSize(&num_procs);

    printf("[%d] of %d\n", rank, num_procs);

    size_t data_size = 12;
    char* data = (char*)malloc(sizeof(char)* data_size);
    
    if (rank == 0) {
        strcpy(data, "hello proc0");

        myMPISend((void*)data, data_size, sizeof(char), 2, 0);
    }

    if (rank == 1) {
        strcpy(data, "hello proc1");

        myMPISend((void*)data, data_size, sizeof(char), 2, 0);
    }

    if (rank == 2) {        
        myMPIRecv((void*)data, data_size, sizeof(char), 1, 0);

        printf("Process[%d]: %s\n", rank, data);
    }

    myMPIFinalize();

    return 0;
}
