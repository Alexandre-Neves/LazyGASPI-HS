#include "lazygaspi_hs.h"
#include "utils.h"
#include "gaspi_utils.h"

#include <cstring>

#ifdef LOCKED_OPERATIONS
gaspi_return_t lock_row_for_read(const LazyGaspiProcessInfo* info, const gaspi_segment_id_t seg, const gaspi_offset_t offset, 
                       const gaspi_rank_t rank){
    gaspi_atomic_value_t oldval;
    gaspi_return_t r;
    
    PRINT_DEBUG_INTERNAL(" | : Locking row from segment " << (int)seg << " at offset " << offset << " of rank " << rank << " for READ.");

    wait_for_write:
    do {
        r = gaspi_atomic_compare_swap(seg, offset, rank, 0, 1, &oldval, GASPI_BLOCK); ERROR_CHECK;
        PRINT_DEBUG_INTERNAL(" | : > Compare and swap saw " << oldval);
    } while((oldval & LOCK_MASK_WRITE) != 0);   //While row is being written, keep trying to lock

    //Lock was unlocked by writer, but another process became the "first reader" faster. Use fetch&add
    if(oldval != 0) { 
        PRINT_DEBUG_INTERNAL(" | : > Row was locked for reading before. Increasing lock count to " << oldval + 1 << "...");
        r = gaspi_atomic_fetch_add(seg, offset, rank, 1, &oldval, GASPI_BLOCK); ERROR_CHECK;
        //This conditional can only be true if the following happens:
        //Lock is free for writes (0 at the write bit); Read lock is set (`n` at the read bits); Before fetch&add of this proc, 
        //all `n` readers unlock the lock (becomes 0 again); Another writer process locks (sets write bit to 1)
        if((oldval & LOCK_MASK_WRITE) != 0){
            PRINT_DEBUG_INTERNAL(" | : > Write lock was placed before read lock could have. Retrying...");
            goto wait_for_write; 
        }
    }

    return GASPI_SUCCESS;
}

gaspi_return_t unlock_row_from_read(const LazyGaspiProcessInfo* info, const gaspi_segment_id_t seg, const gaspi_offset_t offset,
                                    const gaspi_rank_t rank){
    gaspi_atomic_value_t val;

    PRINT_DEBUG_INTERNAL(" | : Unlocking row from segment " << (int)seg << " at offset " << offset << " of rank " << rank << " from READ.");

    auto r = gaspi_atomic_fetch_add(seg, offset, rank, (gaspi_atomic_value_t)-1, &val, GASPI_BLOCK); 
    PRINT_DEBUG_INTERNAL(" | : > Old value was " << val);
    if(r != GASPI_SUCCESS) PRINT_ON_ERROR(r);
    return r;
}
#endif

gaspi_return_t lazygaspi_read(lazygaspi_id_t row_id, lazygaspi_id_t table_id, lazygaspi_slack_t slack, void* row,
                              LazyGaspiRowData* data){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK_COUT;
    
    PRINT_DEBUG_INTERNAL("Reading row " << row_id << " of table " << table_id << "...");

    #ifdef SAFETY_CHECKS
    if(row == nullptr){
        PRINT_ON_ERROR(" | Error: read was called with row = nullptr.");
        return GASPI_ERR_NULLPTR;
    }
    if(row_id >= info->table_size || table_id >= info->table_amount){
        PRINT_ON_ERROR(" | Error: row/table ID was out of bounds.");
        return GASPI_ERR_INV_NUM;
    }
    if(info->age == 0){
        PRINT_ON_ERROR(" | Error: clock must be called at least once before prefetch.");
        return GASPI_ERR_NOINIT;
    }
    #endif
  
    auto min = get_min_age(info->age, slack, info->offset_slack);

    gaspi_pointer_t cache;
    r = gaspi_segment_ptr(LAZYGASPI_ID_CACHE, &cache); ERROR_CHECK;

    auto offset_cache = get_offset_in_cache(info, row_id, table_id) * ROW_SIZE_IN_CACHE_WITH_LOCK;
    auto rowData = (LazyGaspiRowData*)((char*)cache + offset_cache + ROW_METADATA_OFFSET);

    gaspi_rank_t rank;
    gaspi_offset_t offset;
    std::tie(rank, offset) = get_row_location(info, row_id, table_id);
    offset *= ROW_SIZE_IN_TABLE_WITH_LOCK;

    PRINT_DEBUG_INTERNAL(" | Reading row from rank " << rank << " with slack " << slack 
                        << " and current age " << info->age << ". Minimum age was " << min << ". Rows offset is " << 
                        offset + ROW_METADATA_OFFSET << " bytes and cache offset is " << offset_cache + ROW_METADATA_OFFSET << 
                        " bytes. Row size is " << info->row_size << " bytes plus " << sizeof(LazyGaspiRowData) << " metadata bytes.");

    #if defined(DEBUG) || defined(DEBUG_INTERNAL)
        unsigned attempt_counter = 0;
        if(rowData->age >= min && rowData->row_id == row_id && rowData->table_id == table_id) 
            { PRINT_DEBUG_INTERNAL(" | Found row in cache."); }
        else { PRINT_DEBUG_INTERNAL(" | Could not find row in cache... Reading from server."); }
    #endif

    while(rowData->age < min || rowData->row_id != row_id || rowData->table_id != table_id){ 
        #ifdef LOCKED_OPERATIONS
            //Lock row in cache. Prefetch responders will have to wait until this is done...
            r = lock_row_for_write(info, LAZYGASPI_ID_CACHE, offset_cache + ROW_LOCK_OFFSET, info->id);
            ERROR_CHECK;
            //Lock row in server for reading. Any new updates to that row will have to wait until read is done...
            r = lock_row_for_read(info, LAZYGASPI_ID_ROWS, offset + ROW_LOCK_OFFSET, rank);
            ERROR_CHECK;
            //This read will not wait for queue after posting request, since that will be done by write unlock.
            PRINT_DEBUG_INTERNAL(" | : Reading...");
            r = read(LAZYGASPI_ID_ROWS, LAZYGASPI_ID_CACHE, offset + ROW_METADATA_OFFSET, offset_cache + ROW_METADATA_OFFSET, 
                     ROW_SIZE_IN_CACHE, rank);
            ERROR_CHECK;
            r = unlock_row_from_write(info, LAZYGASPI_ID_CACHE, offset_cache + ROW_LOCK_OFFSET, info->id);
            ERROR_CHECK;
            r = unlock_row_from_read(info, LAZYGASPI_ID_ROWS, offset + ROW_LOCK_OFFSET, rank);
            ERROR_CHECK;
        #else
            r = readwait(LAZYGASPI_ID_ROWS, LAZYGASPI_ID_CACHE, offset + ROW_METADATA_OFFSET, offset_cache + ROW_METADATA_OFFSET, 
                         ROW_SIZE_IN_CACHE, rank);
            ERROR_CHECK;
        #endif
    }    

    PRINT_DEBUG_INTERNAL(" | : Read fresh row. Age was " << rowData->age);

    #ifdef LOCKED_OPERATIONS
        r = lock_row_for_read(info, LAZYGASPI_ID_CACHE, offset_cache + ROW_LOCK_OFFSET, info->id);
        ERROR_CHECK;
    #endif

    memcpy(row, (void*)((char*)rowData + ROW_DATA_OFFSET - ROW_METADATA_OFFSET), info->row_size);
    if(data) *data = *rowData;

    #ifdef LOCKED_OPERATIONS
        r = unlock_row_from_read(info, LAZYGASPI_ID_CACHE, offset_cache + ROW_LOCK_OFFSET, info->id);
        ERROR_CHECK;
    #endif

    return GASPI_SUCCESS;
}
