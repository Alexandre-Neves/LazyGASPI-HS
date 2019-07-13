#include "lazygaspi.h"
#include "utils.h"
#include "gaspi_utils.h"

#define ERROR_CHECK if(r != GASPI_SUCCESS) return r

gaspi_return_t lazygaspi_fulfil_prefetches(){

    Notification notif;
    auto r = get_notification(SEGMENT_ID_ROWS, NOTIF_ID_ROW_WRITTEN, 1, &notif, GASPI_TEST);
    if(r == GASPI_TIMEOUT) return GASPI_SUCCESS;    //No "new row" notice, no prefetching necessary.
    ERROR_CHECK;

    LazyGaspiProcessInfo* info;
    r = lazygaspi_get_info(&info); ERROR_CHECK;

    gaspi_pointer_t rows_table;
    r = gaspi_segment_ptr(SEGMENT_ID_ROWS, &rows_table); ERROR_CHECK;

    const auto entry_size = sizeof(LazyGaspiProcessInfo) + info->row_size + info->n * sizeof(lazygaspi_age_t);
    const auto row_amount = get_row_amount(info->table_size, info->table_amount, info->n, info->id);

    for(gaspi_rank_t rank = 0; rank < info->n; rank++)
    for(gaspi_offset_t i = 0; i < row_amount; i++){
        if(auto min = get_prefetch(info, rows_table, entry_size * i, rank)){
            auto data = (LazyGaspiRowData*)((char*)rows_table + entry_size * i);
            if(data->age >= min){
                if(info->table_size == 0) return GASPI_ERR_NOINIT;
                r = write(SEGMENT_ID_ROWS, SEGMENT_ID_CACHE, entry_size * i, get_offset_in_cache(info, i % info->table_size),
                      sizeof(LazyGaspiRowData) + info->row_size, rank);
                ERROR_CHECK;
            }
        } 
    }
    return GASPI_SUCCESS;
}

gaspi_return_t lazygaspi_prefetch(lazygaspi_id_t row_id, lazygaspi_id_t table_id, lazygaspi_slack_t slack){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK;
    if(row_id >= info->table_size || table_id >= info->table_amount) return GASPI_ERR_INV_NUM;

    auto rank = get_rank_of_table(table_id, info->n);
    
    auto flag_offset = row_id * (sizeof(LazyGaspiRowData) + info->row_size + info->n * sizeof(lazygaspi_age_t)) + 
                                 sizeof(LazyGaspiRowData) + info->row_size + rank * sizeof(lazygaspi_age_t);
    info->communicator = get_min_age(info->age, slack);
    return write(SEGMENT_ID_INFO, SEGMENT_ID_ROWS, offsetof(LazyGaspiProcessInfo, communicator), flag_offset, sizeof(lazygaspi_age_t),
                 rank);
}