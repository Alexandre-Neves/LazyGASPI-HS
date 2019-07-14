#include "lazygaspi.h"
#include <cstdlib>
#include "gaspi_utils.h"
#include <Eigen/Core>

#define ROW_ENTRIES 16
#define ROW Eigen::Matrix<double, 1, ROW_ENTRIES>
#define ROW_SIZE sizeof(ROW)
#define ITERATIONS 20

#define TABLE_AMOUNT 6

#define DEBUG

int main(int argc, char** argv){
    SUCCESS_OR_DIE(lazygaspi_init(TABLE_AMOUNT, 16, ROW_SIZE));

    LazyGaspiProcessInfo* info;
    SUCCESS_OR_DIE(lazygaspi_get_info(&info));

    SUCCESS_OR_DIE(setup_gaspi_output("lazygaspi_hs", info->id, &(info->out)));

    for(lazygaspi_age_t t = 1; t <= 20; t++){
        SUCCESS_OR_DIE(lazygaspi_clock());   

        

        SUCCESS_OR_DIE(lazygaspi_fulfil_prefetches());
    }

    return EXIT_SUCCESS;
}