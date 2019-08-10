#include "lazygaspi_hs.h"
#include "gaspi_utils.h"
#include "utils.h"

/* Allocates: rows; cache. Sets n and id for info. Hits barrier for all. */
gaspi_return_t allocate_segments(LazyGaspiProcessInfo* info);

gaspi_return_t lazygaspi_init(lazygaspi_id_t table_amount, lazygaspi_id_t table_size, gaspi_size_t row_size, 
                              ShardingOptions shard_options, CachingOptions cache_options, OutputCreator outputCreator,
                              SizeDeterminer det_amount, void* data_amount, SizeDeterminer det_tablesize, void* data_tablesize, 
                              SizeDeterminer det_rowsize, void* data_rowsize){

    PRINT_DEBUG_INTERNAL_COUT("Initializing LazyGASPI...");

    auto r = gaspi_proc_init(GASPI_BLOCK); ERROR_CHECK_COUT;

    LazyGaspiProcessInfo* info;
    r = gaspi_malloc_noblock(LAZYGASPI_ID_INFO, sizeof(LazyGaspiProcessInfo), &info, GASPI_MEM_INITIALIZED); 
    ERROR_CHECK_COUT;

    r = gaspi_proc_num(&(info->n)); ERROR_CHECK_COUT;
    r = gaspi_proc_rank(&(info->id)); ERROR_CHECK_COUT;


    if(info->n == 0) return GASPI_ERR_INV_RANK;
    if(outputCreator){
        outputCreator(info);
        if(info->out == nullptr){
            PRINT_ON_ERROR_COUT("Output stream was nullptr (not created).");
            return GASPI_ERR_NULLPTR;
        }
    }
    else info->out = &std::cout;
    
    PRINT_DEBUG_INTERNAL("Allocated info segment for rank " << info->id << ". Total amount of ranks: " << info->n);
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

    if(shard_options.block_size == 0) shard_options.block_size = table_size;
    if(cache_options.hash == nullptr || cache_options.size == 0) 
        cache_options = CachingOptions(LAZYGASPI_HS_HASH_ROW, table_size);

    PRINT_DEBUG_INTERNAL("Table amount: " << table_amount << " | Table size: " << table_size << " | Row size: " << row_size);

    info->shardOpts = shard_options;
    info->cacheOpts = cache_options;
    info->row_size = row_size;
    info->table_amount = table_amount;
    info->table_size = table_size;
    info->offset_slack = true;

    r = allocate_segments(info); ERROR_CHECK;

    return GASPI_SUCCESS;
}

gaspi_return_t allocate_segments(LazyGaspiProcessInfo* info){
    auto row_amount = get_row_amount(info->table_size, info->table_amount, info->n, info->id, info->shardOpts);
    auto rows_table_size = (sizeof(LazyGaspiRowData) + info->row_size + info->n * sizeof(lazygaspi_age_t)) * row_amount;
    auto cache_size = info->cacheOpts.size * (sizeof(LazyGaspiRowData) + info->row_size);

    PRINT_DEBUG_INTERNAL("Allocating cache with " << cache_size << " bytes (" << info->cacheOpts.size << " entries) and rows with "
                         << rows_table_size << " bytes (" << row_amount << " entries)... Sharding options block size was "
                         << info->shardOpts.block_size);

    //An entry for this segment is a metadata tag, the row itself, and the pending prefetch requests (n slots).
    gaspi_return_t r;
    if(row_amount){
        r = gaspi_segment_create_noblock(LAZYGASPI_ID_ROWS, rows_table_size, GASPI_MEM_INITIALIZED);
        ERROR_CHECK;
    }

    //An entry for this segment is a metadata tag and the row itself.
    r = gaspi_segment_create_noblock(LAZYGASPI_ID_CACHE, cache_size, GASPI_MEM_INITIALIZED);
    ERROR_CHECK;

    return GASPI_BARRIER;
}