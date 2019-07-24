#include "lazygaspi_hs.h"
#include "gaspi_utils.h"
#include "utils.h"

/* Allocates: info; rows; cache. Sets n and id for info. Hits barrier for all. */
gaspi_return_t allocate_segments(gaspi_offset_t table_size, gaspi_offset_t table_amount, gaspi_size_t row_size, 
                                 LazyGaspiProcessInfo* info);

gaspi_return_t lazygaspi_init(lazygaspi_id_t table_amount, lazygaspi_id_t table_size, gaspi_size_t row_size, 
                              SizeDeterminer det_amount, void* data_amount, SizeDeterminer det_tablesize, void* data_tablesize, 
                              SizeDeterminer det_rowsize, void* data_rowsize, ShardingOptions shard_options,
                              CachingOptions cache_options){
    if(shard_options.block_size == 0) shard_options.block_size = table_size;
    if(cache_options.hash == nullptr || cache_options.size == 0) 
        cache_options = CachingOptions(LAZYGASPI_HS_HASH_ROW, table_size);

    auto r = gaspi_proc_init(GASPI_BLOCK); ERROR_CHECK;

    LazyGaspiProcessInfo* info;
    r = gaspi_malloc_noblock(SEGMENT_ID_INFO, sizeof(LazyGaspiProcessInfo), &info, GASPI_MEM_INITIALIZED); ERROR_CHECK;

    r = gaspi_proc_num(&(info->n)); ERROR_CHECK;
    r = gaspi_proc_rank(&(info->id)); ERROR_CHECK;

    if(info->n == 0) return GASPI_ERR_INV_RANK;

    if(table_amount == 0) { 
        if(!det_amount) return GASPI_ERR_INV_NUM;
        if(!(table_amount = det_amount(info->id, info->n, data_amount))) return GASPI_ERR_INV_NUM;
    }
    if(table_size == 0) { 
        if(!det_tablesize) return GASPI_ERR_INV_NUM;
        if(!(table_size = det_tablesize(info->id, info->n, data_tablesize))) return GASPI_ERR_INV_NUM;
    }
    if(row_size == 0) { 
        if(!det_rowsize) return GASPI_ERR_INV_NUM;
        if(!(row_size = det_rowsize(info->id, info->n, data_rowsize))) return GASPI_ERR_INV_NUM;
    }

    info->shardOpts = shard_options;
    info->cacheOpts = cache_options;

    r = allocate_segments(table_size, table_amount, row_size, info); ERROR_CHECK;

    info->row_size = row_size;
    info->table_amount = table_amount;
    info->table_size = table_size;
    info->out = nullptr;

    return GASPI_SUCCESS;
}

gaspi_return_t allocate_segments(lazygaspi_id_t table_size, lazygaspi_id_t table_amount, gaspi_size_t row_size, 
                                 LazyGaspiProcessInfo* info){
    //An entry for this segment is a metadata tag, the row itself, and the pending prefetch requests (n slots).
    auto row_amount = get_row_amount(table_size, table_amount, info->n, info->id, info->shardOpts);

    auto rows_table_size = (sizeof(LazyGaspiRowData) + row_size + info->n * sizeof(lazygaspi_age_t)) * row_amount;
    auto r = gaspi_segment_create_noblock(SEGMENT_ID_ROWS, rows_table_size, GASPI_MEM_INITIALIZED);
    ERROR_CHECK;

    //An entry for this segment is a metadata tag and the row itself.
    auto cache_size = info->cacheOpts.size * (sizeof(LazyGaspiRowData) + row_size);
    r = gaspi_segment_create_noblock(SEGMENT_ID_CACHE, cache_size, GASPI_MEM_INITIALIZED);
    ERROR_CHECK;

    PRINT_DEBUG_INTERNAL("Allocated cache with " << cache_size << " bytes (" << info->cacheOpts.size << " entries) and rows with "
                         << rows_table_size << " bytes (" << row_amount << " entries).");

    return GASPI_BARRIER;
}