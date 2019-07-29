#include "lazygaspi_hs.h"
#include "utils.h"
#include "gaspi_utils.h"
#include <cstring>

gaspi_return_t lazygaspi_write(lazygaspi_id_t row_id, lazygaspi_id_t table_id, void* row){
    if(row == nullptr) return GASPI_ERR_NULLPTR;

    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK;

    if(row_id >= info->table_size || table_id >= info->table_amount){
        PRINT_DEBUG_INTERNAL("Row was " << row_id << " but max was " << info->table_size << " and table was " << table_id 
                             << " but max was " << info->table_amount);
        return GASPI_ERR_INV_NUM;
    }

    gaspi_rank_t rank; gaspi_offset_t offset_rows;
    std::tie(rank, offset_rows) = get_row_location(info, row_id, table_id);

    offset_rows *= sizeof(LazyGaspiRowData) + info->row_size + info->n * sizeof(lazygaspi_age_t);
    auto offset_cache = get_offset_in_cache(info, row_id, table_id) * (sizeof(LazyGaspiRowData) + info->row_size);

    PRINT_DEBUG_INTERNAL("Writing row " << row_id << " of table " << table_id << " to rank " << rank << " and an age of " 
                         << info->age << ", where the rows offset is " << offset_rows << " bytes and cache offset is " << 
                         offset_cache << " bytes. Cache size is " << info->cacheOpts.size << " bytes.");

    //Write to cache.
    gaspi_pointer_t ptr;
    r = gaspi_segment_ptr(SEGMENT_ID_CACHE, &ptr); ERROR_CHECK;
    auto data = LazyGaspiRowData(info->age, row_id, table_id);
    memcpy((char*)ptr + offset_cache, &data, sizeof(LazyGaspiRowData));
    memcpy((char*)ptr + offset_cache + sizeof(LazyGaspiRowData), row, info->row_size);

    //Write to rows segment of proper rank.
    r = writenotify(SEGMENT_ID_CACHE, SEGMENT_ID_ROWS, offset_cache, offset_rows, 
                sizeof(LazyGaspiRowData) + info->row_size, rank, NOTIF_ID_ROW_WRITTEN);
    ERROR_CHECK;
    return gaspi_wait(0, GASPI_BLOCK);  //Make sure write request is fulfilled before cache is used again for another write.
}

