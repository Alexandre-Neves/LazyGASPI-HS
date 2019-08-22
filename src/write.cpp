#include "lazygaspi_hs.h"
#include "utils.h"
#include "gaspi_utils.h"
#include <cstring>

#ifdef LOCKED_OPERATIONS
gaspi_return_t lock_row_for_write(const LazyGaspiProcessInfo* info, const gaspi_segment_id_t seg, const gaspi_offset_t offset, 
                    const gaspi_rank_t rank){
    gaspi_atomic_value_t oldval;
    gaspi_return_t r;
    PRINT_DEBUG_INTERNAL(" | : Locking row from segment " << (int)seg << " at offset " << offset << " of rank " << rank << " for WRITE.");
    do{
        r = gaspi_atomic_compare_swap(seg, offset, rank, 0, LOCK_MASK_WRITE, &oldval, GASPI_BLOCK); ERROR_CHECK;
        PRINT_DEBUG_INTERNAL(" | : > Compare and swap saw " << oldval);
    } while(oldval != 0); //While write operations are still locked (Row is being read or row is being written by another proc)
}

gaspi_return_t unlock_row_from_write(LazyGaspiProcessInfo* info, const gaspi_segment_id_t seg, const gaspi_offset_t offset,
                      const gaspi_rank_t rank, const gaspi_queue_id_t q, bool wait_on_q){
    gaspi_return_t r;
    PRINT_DEBUG_INTERNAL(" | : Unlocking row from segment " << (int)seg << " at offset " << offset << " of rank " << rank << " from WRITE.");

    if(wait_on_q) r = gaspi_wait(q, GASPI_BLOCK); ERROR_CHECK;
    info->communicator = 0;
    r = gaspi_write(LAZYGASPI_ID_INFO, offsetof(LazyGaspiProcessInfo, communicator), rank, seg, offset, 
                    sizeofmember(LazyGaspiProcessInfo, communicator), q, GASPI_BLOCK);
    ERROR_CHECK;
    r = gaspi_wait(q, GASPI_BLOCK); ERROR_CHECK;
    return GASPI_SUCCESS;
}
#endif

gaspi_return_t lazygaspi_write(lazygaspi_id_t row_id, lazygaspi_id_t table_id, void* row){

    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK_COUT;

    PRINT_DEBUG_INTERNAL("Writing row " << row_id << " of table " << table_id << "...");

    if(row == nullptr){
        PRINT_DEBUG_INTERNAL(" | Error: tried to write nullptr as a row.");
        return GASPI_ERR_NULLPTR;
    }
    if(row_id >= info->table_size || table_id >= info->table_amount){
        PRINT_DEBUG_INTERNAL(" | Error: row/table ID was out of bounds.");
        return GASPI_ERR_INV_NUM;
    }

    gaspi_rank_t rank; 
    gaspi_offset_t offset;
    std::tie(rank, offset) = get_row_location(info, row_id, table_id); 
    offset *= ROW_SIZE_IN_TABLE;

    auto offset_cache = get_offset_in_cache(info, row_id, table_id) * ROW_SIZE_IN_CACHE;

    PRINT_DEBUG_INTERNAL(" | Writing row to rank " << rank << " and an age of " << info->age << ", where the rows offset is " 
                        << offset + ROW_METADATA_OFFSET << " bytes and cache offset is " <<  offset_cache + ROW_METADATA_OFFSET 
                        << " bytes. Cache size is " << info->cacheOpts.size << " entries.");

    //Write to cache.
    gaspi_pointer_t cache;
    r = gaspi_segment_ptr(LAZYGASPI_ID_CACHE, &cache); ERROR_CHECK;
    auto data = LazyGaspiRowData(info->age, row_id, table_id);

    #ifdef LOCKED_OPERATIONS
        lock_row_for_write(info, LAZYGASPI_ID_CACHE, offset_cache + ROW_LOCK_OFFSET, info->id);
    #endif

    //Save the row in the cache first
    memcpy((char*)cache + offset_cache + ROW_METADATA_OFFSET, &data, sizeof(LazyGaspiRowData));
    memcpy((char*)cache + offset_cache + ROW_DATA_OFFSET, row, info->row_size);

    #ifdef LOCKED_OPERATIONS
        r = unlock_row_from_write(info, LAZYGASPI_ID_CACHE, offset_cache + ROW_LOCK_OFFSET, info->id, 0, false);
        ERROR_CHECK;
        r = lock_row_for_read(info, LAZYGASPI_ID_CACHE, offset_cache + ROW_LOCK_OFFSET, info->id);
        ERROR_CHECK;
        r = lock_row_for_write(info, LAZYGASPI_ID_ROWS, offset + ROW_LOCK_OFFSET, info->id);
        ERROR_CHECK;
    #endif

    //Write to rows segment of proper rank.
    r = writenotify(LAZYGASPI_ID_CACHE, LAZYGASPI_ID_ROWS, offset_cache + ROW_METADATA_OFFSET, offset + ROW_METADATA_OFFSET, 
            ROW_SIZE_IN_CACHE, rank, NOTIF_ID_ROW_WRITTEN);
    ERROR_CHECK;

    #ifdef LOCKED_OPERATIONS
        r = unlock_row_from_write(info, LAZYGASPI_ID_ROWS, offset + ROW_LOCK_OFFSET, info->id);
        ERROR_CHECK;
        r = unlock_row_from_read(info, LAZYGASPI_ID_CACHE, offset_cache + ROW_LOCK_OFFSET, info->id);
        if(r != GASPI_SUCCESS) PRINT_ON_ERROR(r);
        return r;
    #else 
        return gaspi_wait(0, GASPI_BLOCK);  //Make sure write request is fulfilled before cache is used again for another write.
    #endif
}

