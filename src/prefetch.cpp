#include "lazygaspi_hs.h"
#include "utils.h"
#include "gaspi_utils.h"

gaspi_return_t lazygaspi_fulfil_prefetches(){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK;

    PRINT_DEBUG_INTERNAL("Fulfilling prefetch requests...");

    Notification notif;
    r = get_notification(SEGMENT_ID_ROWS, NOTIF_ID_ROW_WRITTEN, 1, &notif, GASPI_TEST);
    if(r == GASPI_TIMEOUT){
        PRINT_DEBUG_INTERNAL("No notice of new rows was found.");
        return GASPI_SUCCESS;    //No "new row" notice, no prefetching necessary.
    }
    ERROR_CHECK;

    gaspi_pointer_t rows_table;
    r = gaspi_segment_ptr(SEGMENT_ID_ROWS, &rows_table); ERROR_CHECK;

    const auto entry_size = get_row_entry_size(info);
    const auto row_amount = get_row_amount(info->table_size, info->table_amount, info->n, info->id, info->shardOpts);

    for(gaspi_rank_t rank = 0; rank < info->n; rank++)
    for(gaspi_offset_t i = 0; i < row_amount; i++){
        if(auto min = get_prefetch(info, rows_table, entry_size * i, rank)){
            auto data = (LazyGaspiRowData*)((char*)rows_table + entry_size * i);
            if(data->age >= min){
                if(info->table_size == 0) return GASPI_ERR_NOINIT;

                PRINT_DEBUG_INTERNAL("Writing row to requesting rank. Minimum age was " << min << ", current age was " << data->age 
                            << ". ID's were " << data->row_id << '/' << data->table_id << '.');

                r = write(SEGMENT_ID_ROWS, SEGMENT_ID_CACHE, entry_size * i, get_offset_in_cache(info, data->row_id, data->table_id), 
                          sizeof(LazyGaspiRowData) + info->row_size, rank);
                ERROR_CHECK;
            }
        } 
    }
    return GASPI_SUCCESS;
}

gaspi_return_t lazygaspi_prefetch(lazygaspi_id_t* row_vec, lazygaspi_id_t* table_vec, size_t size, lazygaspi_slack_t slack){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK;
    if(info->age == 0){
        PRINT_DEBUG_INTERNAL("Error: clock must be called at least once before prefetch.");
        return GASPI_ERR_NOINIT;
    }

    gaspi_rank_t rank;
    gaspi_offset_t offset;
    info->communicator = get_min_age(info->age, slack, info->offset_slack);
    PRINT_DEBUG_INTERNAL(" Writing " << size << " prefetch requests with minimum age " << info->communicator << "...");
    while(size--){
        PRINT_DEBUG_INTERNAL(" | : Requesting row " << *row_vec << " from table " << *table_vec << " with minimum age " << 
                            info->communicator << "...");
        if(*row_vec >= info->table_size || *table_vec >= info->table_amount){
            PRINT_DEBUG_INTERNAL(" | : > Error: row/table ID was out of bounds.");
            return GASPI_ERR_INV_NUM;
        }
        std::tie(rank, offset) = get_row_location(info, *row_vec, *table_vec);
        if(rank == info->id){
            PRINT_DEBUG_INTERNAL(" | : > Tried to prefetch from own rows table. Ignoring request.");
            return GASPI_SUCCESS;
        }
        auto flag_offset = offset * get_row_entry_size(info) + get_prefetch_req_offset(info);

        r = write(SEGMENT_ID_INFO, SEGMENT_ID_ROWS, offsetof(LazyGaspiProcessInfo, communicator), 
                 flag_offset,  sizeofmember(LazyGaspiProcessInfo, communicator), rank);
    }

    PRINT_DEBUG_INTERNAL(" | Wrote all prefetch requests. Waiting on queue 0...");
    return gaspi_wait(0, GASPI_BLOCK);
}

gaspi_return_t lazygaspi_prefetch_all(lazygaspi_slack_t slack){
    LazyGaspiProcessInfo* info;
    auto r = lazygaspi_get_info(&info); ERROR_CHECK_COUT;    

    info->communicator = get_min_age(info->age, slack, info->offset_slack);

    PRINT_DEBUG_INTERNAL("Writing prefetch requests for all rows of all tables...");

    gaspi_rank_t rank;
    gaspi_offset_t offset;

    for(lazygaspi_id_t table = 0; table < info->table_amount; table++)
    for(lazygaspi_id_t row = 0; row < info->table_size; row++){
        PRINT_DEBUG_INTERNAL(" | Prefetching row " << row << " of table " << table << "...");

        std::tie(rank, offset) = get_row_location(info, row, table);
        auto flag_offset = offset * get_row_entry_size(info) + get_prefetch_req_offset(info);
        r = write(SEGMENT_ID_INFO, SEGMENT_ID_ROWS, offsetof(LazyGaspiProcessInfo, communicator), 
                 flag_offset, sizeofmember(LazyGaspiProcessInfo, communicator), rank);
        ERROR_CHECK;
    }

    PRINT_DEBUG_INTERNAL(" | Wrote all prefetch requests.");
    return gaspi_wait(0, GASPI_BLOCK);
}