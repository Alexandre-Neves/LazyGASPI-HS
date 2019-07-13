#include "lazygaspi.h"
#include "gaspi_utils.h"
#include "utils.h"

#define ERROR_CHECK if(r != GASPI_SUCCESS) return r

/* Allocates: info; rows; cache. Sets n and id for info. Hits barrier for all. */
LazyGaspiProcessInfo* allocate_segments(gaspi_offset_t table_size, gaspi_offset_t table_amount, gaspi_size_t row_size);

gaspi_return_t lazygaspi_init(lazygaspi_id_t table_size, lazygaspi_id_t table_amount, gaspi_size_t row_size){
    auto r = gaspi_proc_init(GASPI_BLOCK); ERROR_CHECK;

    LazyGaspiProcessInfo* info;
    r = allocate_segments(table_size, table_amount, row_size, &info); ERROR_CHECK;

    info->row_size = row_size;
    info->table_amount = table_amount;
    info->table_size = table_size;

    return GASPI_SUCCESS;
}

gaspi_return_t allocate_segments(lazygaspi_id_t table_size, lazygaspi_id_t table_amount, gaspi_size_t row_size, 
                                 LazyGaspiProcessInfo** info){
    auto r = gaspi_malloc_noblock(SEGMENT_ID_INFO, sizeof(LazyGaspiProcessInfo), info); ERROR_CHECK;

    r = gaspi_proc_num(&((*info)->n)); ERROR_CHECK;
    r = gaspi_proc_rank(&((*info)->id)); ERROR_CHECK;

    if((*info)->n == 0) return GASPI_ERR_INV_RANK;

    //An entry for this segment is a metadata tag, the row itself, and the pending prefetch requests (n slots).
    r = gaspi_segment_create_noblock(SEGMENT_ID_ROWS, ( sizeof(LazyGaspiRowData) + row_size + (*info)->n * sizeof(lazygaspi_age_t) ) * 
                                     get_row_amount(table_size, table_amount, (*info)->n, (*info)->id), GASPI_MEM_INITIALIZED);
    ERROR_CHECK;

    //An entry for this segment is a metadata tag and the row itself.
    r = gaspi_segment_create_noblock(SEGMENT_ID_CACHE, (sizeof(LazyGaspiRowData) + row_size) * table_size, GASPI_MEM_INITIALIZED);
    ERROR_CHECK;

    return GASPI_BARRIER;
}