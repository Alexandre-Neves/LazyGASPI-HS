#include "lazygaspi.h"
#include "gaspi_utils.h"

#define ERROR_CHECK if(r != GASPI_SUCCESS) return r

LazyGaspiProcessInfo* allocate_segments(gaspi_offset_t table_size, gaspi_offset_t table_amount, gaspi_size_t row_size);


gaspi_return_t lazygaspi_init(gaspi_offset_t table_size, gaspi_offset_t table_amount, gaspi_size_t row_size){
    auto r = gaspi_proc_init(GASPI_BLOCK); ERROR_CHECK;

    LazyGaspiProcessInfo* info;
    r = allocate_segments(table_size, table_amount, row_size, &info); ERROR_CHECK;

    

}

gaspi_return_t allocate_segments(gaspi_offset_t table_size, gaspi_offset_t table_amount, gaspi_size_t row_size, LazyGaspiProcessInfo** info){
    auto r = gaspi_malloc_noblock(SEGMENT_ID_INFO, sizeof(LazyGaspiProcessInfo), info); ERROR_CHECK;

    r = gaspi_proc_num(&((*info)->n)); ERROR_CHECK;
    r = gaspi_proc_rank(&((*info)->id)); ERROR_CHECK;

    if((*info)->n == 0) return GASPI_ERR_INV_RANK;

    auto row_amount = (table_amount / (*info)->n + (unsigned long)((*info)->id < table_amount % (*info)->n)) * table_size;
    r = gaspi_segment_create_noblock(SEGMENT_ID_ROWS, (sizeof(LazyGaspiRowData) + row_size) * row_amount, GASPI_MEM_INITIALIZED);
    ERROR_CHECK;

    r = gaspi_segment_create_noblock(SEGMENT_ID_CACHE, (sizeof(LazyGaspiRowData) + row_size) * table_size, GASPI_MEM_INITIALIZED);
    ERROR_CHECK;

    return GASPI_BARRIER;
}