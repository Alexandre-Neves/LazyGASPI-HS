#include "lazygaspi.h"
#include "utils.h"
#include "gaspi_utils.h"
#include <cstring>

#define ERROR_CHECK if(r != GASPI_SUCCESS) return r

gaspi_return_t lazygaspi_write(lazygaspi_id_t row_id, lazygaspi_id_t table_id, void* row){
    if(row == nullptr) return GASPI_ERR_NULLPTR;

    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK;

    if(row_id >= info->table_size || table_id >= info->table_amount) return GASPI_ERR_INV_NUM;
    
    auto rank = get_rank_of_table(table_id, info->n);
    auto offset_rows = get_offset_in_rows_segment(info, row_id, table_id);
    auto offset_cache = get_offset_in_cache(info, row_id);

    //Write to cache.
    gaspi_pointer_t ptr;
    r = gaspi_segment_ptr(SEGMENT_ID_CACHE, &ptr); ERROR_CHECK;
    auto data = LazyGaspiRowData(row_id, table_id, info->age);
    memcpy((char*)ptr + offset_cache, &data, sizeof(LazyGaspiRowData));
    memcpy((char*)ptr + offset_cache + sizeof(LazyGaspiRowData), row, info->row_size);

    //Write to rows segment of proper rank.
    return writenotify(SEGMENT_ID_CACHE, SEGMENT_ID_ROWS, offset_cache, offset_rows, 
                sizeof(LazyGaspiRowData) + info->row_size, rank, NOTIF_ID_ROW_WRITTEN);
}

