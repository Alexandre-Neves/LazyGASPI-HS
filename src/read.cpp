#include "lazygaspi.h"
#include "utils.h"
#include "gaspi_utils.h"

#include <cstring>

#define ERROR_CHECK if(r != GASPI_SUCCESS) return r

gaspi_return_t lazygaspi_read(lazygaspi_id_t row_id, lazygaspi_id_t table_id, lazygaspi_slack_t slack, void* row,
                              LazyGaspiRowData* data){
    if(row == nullptr) return GASPI_ERR_NULLPTR;

    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK;

    if(row_id >= info->table_size || table_id >= info->table_amount) return GASPI_ERR_INV_NUM;

    auto min = get_min_age(info->age, slack);

    //Check cache.
    gaspi_pointer_t cache;
    r = gaspi_segment_ptr(SEGMENT_ID_CACHE, &cache); ERROR_CHECK;

    auto offset = get_offset_in_cache(info, row_id);
    auto rowData = (LazyGaspiRowData*)((char*)cache + offset);

    auto rank = get_rank_of_table(table_id, info->n);
    auto offset_other = get_offset_in_rows_segment(info, row_id, table_id);

    for(; rowData->age < min; r = read(SEGMENT_ID_ROWS, SEGMENT_ID_CACHE, offset_other, offset, sizeof(LazyGaspiRowData) + 
                                                                                                info->row_size, rank))
        ERROR_CHECK;

    memcpy(row, (void*)((char*)rowData + sizeof(LazyGaspiRowData)), info->row_size);
    if(data) *data = *rowData;

    return GASPI_SUCCESS;
}