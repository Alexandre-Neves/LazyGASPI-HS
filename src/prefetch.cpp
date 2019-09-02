#include "lazygaspi_hs.h"
#include "utils.h"
#include "gaspi_utils.h"

gaspi_return_t lazygaspi_fulfill_prefetches(){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK;

    PRINT_DEBUG_INTERNAL("Fulfillling prefetch requests...");

    Notification notif;
    r = get_notification(LAZYGASPI_ID_ROWS, NOTIF_ID_ROW_WRITTEN, 1, &notif, GASPI_TEST);
    if(r == GASPI_TIMEOUT){
        PRINT_DEBUG_INTERNAL("No notice of new rows was found.");
        return GASPI_SUCCESS;    //No "new row" notice, no prefetching necessary.
    }
    ERROR_CHECK;

    gaspi_pointer_t rows_table;
    r = gaspi_segment_ptr(LAZYGASPI_ID_ROWS, &rows_table); ERROR_CHECK;

    const auto entry_size = ROW_SIZE_IN_TABLE_WITH_LOCK;
    const auto row_amount = get_row_amount(info->table_size, info->table_amount, info->n, info->id, info->shardOpts);

    for(gaspi_rank_t rank = 0; rank < info->n; rank++)
    for(gaspi_offset_t i = 0; i < row_amount; i++){
        if(auto min = get_prefetch(info, rows_table, entry_size * i, rank)){
            auto data = (LazyGaspiRowData*)((char*)rows_table + entry_size * i);
            //If a new row is written while this is happening, the new row will be expected to have an age bigger than 
            //the previous age (TODO: not actually made explicit), which will also satisfy this condition.
            if(data->age >= min){ 
                if(info->table_size == 0) return GASPI_ERR_NOINIT;
                PRINT_DEBUG_INTERNAL("Writing row to requesting rank. Minimum age was " << min << ", current age was " << data->age 
                            << ". ID's were " << data->row_id << '/' << data->table_id << '.');

                const auto cache_offset = get_offset_in_cache(info, data->row_id, data->table_id) * ROW_SIZE_IN_CACHE_WITH_LOCK;
                #ifdef LOCKED_OPERATIONS
                    r = lock_row_for_read(info, LAZYGASPI_ID_ROWS, entry_size * i + ROW_LOCK_OFFSET, info->id); ERROR_CHECK;
                    r = lock_row_for_write(info, LAZYGASPI_ID_CACHE, cache_offset + ROW_LOCK_OFFSET, rank); ERROR_CHECK;
                #endif

                r = write(LAZYGASPI_ID_ROWS, LAZYGASPI_ID_CACHE, entry_size * i + ROW_METADATA_OFFSET, 
                          cache_offset + ROW_METADATA_OFFSET, ROW_SIZE_IN_CACHE, rank);
                ERROR_CHECK;

                #ifdef LOCKED_OPERATIONS
                    r = unlock_row_from_write(info, LAZYGASPI_ID_CACHE, cache_offset + ROW_LOCK_OFFSET, rank); ERROR_CHECK;
                    r = unlock_row_from_read(info, LAZYGASPI_ID_ROWS, entry_size * i + ROW_LOCK_OFFSET, info->id); ERROR_CHECK;
                #endif
            }
        } 
    }
    return GASPI_SUCCESS;
}

gaspi_return_t lazygaspi_prefetch(lazygaspi_id_t* row_vec, lazygaspi_id_t* table_vec, size_t size, lazygaspi_slack_t slack){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK;
    
    #ifdef SAFETY_CHECKS
    if(info->age == 0){
        PRINT_ON_ERROR("Clock must be called at least once before prefetch.");
        return GASPI_ERR_NOINIT;
    }
    #endif

    gaspi_rank_t rank;
    gaspi_offset_t offset;
    info->communicator = get_min_age(info->age, slack, info->offset_slack);
    PRINT_DEBUG_INTERNAL(" Writing " << size << " prefetch requests with minimum age " << info->communicator << "...");
    while(size--){
        PRINT_DEBUG_INTERNAL(" | : Requesting row " << *row_vec << " from table " << *table_vec << " with minimum age " << 
                            info->communicator << "...");
        #ifdef SAFETY_CHECKS
        if(*row_vec >= info->table_size || *table_vec >= info->table_amount){
            PRINT_ON_ERROR("Row/table ID was out of bounds.");
            return GASPI_ERR_INV_NUM;
        }
        #endif
        
        std::tie(rank, offset) = get_row_location(info, *row_vec, *table_vec);
        if(rank == info->id){
            PRINT_DEBUG_INTERNAL(" | : > Tried to prefetch from own rows table. Ignoring request.");
            continue;
        }
        auto flag_offset = offset * ROW_SIZE_IN_TABLE_WITH_LOCK + ROW_REQUEST_OFFSET(info->id);

        r = write(LAZYGASPI_ID_INFO, LAZYGASPI_ID_ROWS, offsetof(LazyGaspiProcessInfo, communicator), 
                  flag_offset,  sizeofmember(LazyGaspiProcessInfo, communicator), rank);
        ERROR_CHECK;
    }

    PRINT_DEBUG_INTERNAL(" | Wrote all prefetch requests. Waiting on queue 0...");
    return gaspi_wait(0, GASPI_BLOCK);
}

gaspi_return_t lazygaspi_prefetch_all(lazygaspi_slack_t slack){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK_COUT;    
    if(info->age == 0){
        PRINT_DEBUG_INTERNAL("Error: clock must be called at least once before prefetch.");
        return GASPI_ERR_NOINIT;
    }   
    info->communicator = get_min_age(info->age, slack, info->offset_slack);

    PRINT_DEBUG_INTERNAL("Writing prefetch requests for all rows of all tables...");

    gaspi_rank_t rank;
    gaspi_offset_t offset;

    for(lazygaspi_id_t table = 0; table < info->table_amount; table++)
    for(lazygaspi_id_t row = 0; row < info->table_size; row++){
        PRINT_DEBUG_INTERNAL(" | Prefetching row " << row << " of table " << table << "...");

        std::tie(rank, offset) = get_row_location(info, row, table);
        if(rank == info->id){
            PRINT_DEBUG_INTERNAL(" | : Tried to prefetch from own rows table. Ignoring request.");
            continue;
        }
        auto flag_offset = offset * ROW_SIZE_IN_TABLE_WITH_LOCK + ROW_REQUEST_OFFSET(info->id);

        r = write(LAZYGASPI_ID_INFO, LAZYGASPI_ID_ROWS, offsetof(LazyGaspiProcessInfo, communicator), 
                 flag_offset, sizeofmember(LazyGaspiProcessInfo, communicator), rank);
        ERROR_CHECK;
    }

    PRINT_DEBUG_INTERNAL(" | Wrote all prefetch requests.");
    return gaspi_wait(0, GASPI_BLOCK);
}