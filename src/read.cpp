#include "lazygaspi_hs.h"
#include "utils.h"
#include "gaspi_utils.h"

#include <cstring>

gaspi_return_t lazygaspi_read(lazygaspi_id_t row_id, lazygaspi_id_t table_id, lazygaspi_slack_t slack, void* row,
                              LazyGaspiRowData* data){
    if(row == nullptr) return GASPI_ERR_NULLPTR;

    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK;

    if(row_id >= info->table_size || table_id >= info->table_amount) return GASPI_ERR_INV_NUM;

    auto min = get_min_age(info->age, slack);
    PRINT_DEBUG_INTERNAL("Reading row " << row_id << " of table " << table_id << " with slack " << slack << " with age " << info->age 
                         << ". Minimum was " << min);

    gaspi_pointer_t cache;
    r = gaspi_segment_ptr(SEGMENT_ID_CACHE, &cache); ERROR_CHECK;

    auto offset_cache = get_offset_in_cache(info, row_id, table_id) * (sizeof(LazyGaspiRowData) + info->row_size);
    auto rowData = (LazyGaspiRowData*)((char*)cache + offset_cache);

    gaspi_rank_t rank;
    gaspi_offset_t offset;
    std::tie(rank, offset) = get_row_location(info, row_id, table_id);
    offset *= sizeof(LazyGaspiRowData) + info->row_size + info->n * sizeof(lazygaspi_age_t);

    #if defined(DEBUG) || defined(DEBUG_INTERNAL)
    unsigned attempt_counter = 0;
    if(rowData->age >= min && rowData->row_id == row_id && rowData->table_id == table_id) 
         { PRINT_DEBUG_INTERNAL("Found row in cache."); }
    else { PRINT_DEBUG_INTERNAL("Could not find row in cache... Reading from server."); }
    #endif

    PRINT_DEBUG_INTERNAL("Rows offset was " << offset << " and cache offset was " << offset_cache << " on rank " << rank 
                        << " with a row size of " << info->row_size << " plus data of " << sizeof(LazyGaspiRowData) << "bytes.");

    while(rowData->age < min || rowData->row_id != row_id || rowData->table_id != table_id){ 
        r = read(SEGMENT_ID_ROWS, SEGMENT_ID_CACHE, offset, offset_cache, sizeof(LazyGaspiRowData) + info->row_size, rank);
        ERROR_CHECK;
        //TODO: this is either for previous bug of posting write req but writing after data change OR
        //when prefetching keeps occurring (incredibly unlikely, still possible) and muddying wanted cache entry.
        //Solution: create special segment of the size of one row+data that ONLY receives data from here. Copy that data
        //to cache AND user's row pointer.
        #if defined(DEBUG) || defined(DEBUG_INTERNAL)
        if(rowData->row_id != row_id || rowData->table_id != table_id) attempt_counter++;
        if(attempt_counter % 1000 == 0) PRINT_DEBUG_INTERNAL("Attempts are now at " << attempt_counter << ".\nRow Data: " << 
                                    rowData->age << ", " << rowData->row_id << ", " << rowData->table_id << "\nRequirements: " <<
                                    min << ", " << row_id << ", " << table_id);
        if(attempt_counter == 10000) return GASPI_ERR_INV_SEG;
        #endif
    }    

    PRINT_DEBUG_INTERNAL("Read fresh row. Age was " << rowData->age);

    memcpy(row, (void*)((char*)rowData + sizeof(LazyGaspiRowData)), info->row_size);
    if(data) *data = *rowData;

    return GASPI_SUCCESS;
}