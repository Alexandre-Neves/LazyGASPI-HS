#include "lazygaspi.h"
#include "gaspi_utils.h"

void allocate_initial_segments();

lazygaspi_return_t lazygaspi_init(gaspi_offset_t table_size, gaspi_offset_t table_amount, gaspi_size_t row_size){
    allocate_initial_segments();
}