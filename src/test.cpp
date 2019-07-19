#include "lazygaspi.h"
#include "gaspi_utils.h"
#include "utils.h"
#include <Eigen/Core>
#include <cstdlib>
#include <unistd.h>

#include <Eigen/Sparse>

using namespace Eigen;

#define ROW_DATA_TYPE double
#define ROW Matrix<ROW_DATA_TYPE, 1, Dynamic>
#define ROW_SIZE (sizeof(ROW) + row_size * sizeof(ROW_DATA_TYPE))

//Default values
#define SLACK 2
#define ITERATIONS 20

#include <vector>

void print_usage();

int main(int argc, char** argv){
    int                 ch;
    lazygaspi_slack_t   slack = SLACK;
    lazygaspi_age_t     max_iter = ITERATIONS;
    auto                should_prefetch = false;
    auto                separate_comp_write = false;
    lazygaspi_id_t      table_size = 0, table_amount = 0;
    gaspi_size_t        row_size = 0;

    for(int i = 1; i < argc; i++)
        if(!strcmp(argv[i], "sep")) separate_comp_write = true;

    while((ch = getopt(argc, argv, "k:n:r:s:i:p")) != -1) {
        switch(ch){
            case 'k': table_size = atol(optarg); break;
            case 'n': table_amount = atol(optarg); break;
            case 'r': row_size = atol(optarg); break;
            case 's': slack = atol(optarg); break;
            case 'i': max_iter = atol(optarg); break;
            case 'p': should_prefetch = true; break;
            case '?':
            case 'h':
            default : print_usage(); exit(EXIT_FAILURE);
        }
    }

    if(table_size == 0 || table_amount == 0 || row_size == 0){
        print_usage(); exit(EXIT_FAILURE);
    }
    
    SUCCESS_OR_DIE(lazygaspi_init(table_amount, table_size, row_size));

    LazyGaspiProcessInfo* info;
    SUCCESS_OR_DIE(lazygaspi_get_info(&info));

    SUCCESS_OR_DIE(setup_gaspi_output("lazygaspi_hs", info->id, &(info->out)));

    ASSERT(info->n != 0, "main");

    const auto proc_table_amount = table_size / info->n * table_amount + (info->id < table_amount % info->n);
    //Vector of table ID's.
    auto in_charge = (lazygaspi_id_t*)malloc(proc_table_amount * sizeof(lazygaspi_id_t));

    for(lazygaspi_id_t table = info->id, i = 0; i < proc_table_amount; i++, table += info->n) 
        in_charge[i] = table;

    auto rows = (ROW*)malloc(ROW_SIZE * table_size * proc_table_amount);
    ROW read_row = ROW(row_size);
    ROW average = ROW(row_size);
    auto data = LazyGaspiRowData();

                            PRINT_DEBUG_TEST("Starting cycle...");

    auto beg_cycle = get_time();
    for(lazygaspi_age_t iteration = 0; iteration < max_iter; iteration++){
        PRINT_DEBUG_TEST("Started iteration " << iteration << "...");

        //Clock
        SUCCESS_OR_DIE(lazygaspi_clock());

        //Prefetch
        if(should_prefetch)
        for(lazygaspi_id_t row = 0; row < table_size; row++) 
        for(lazygaspi_id_t table = 0; table < table_amount; table++)
            SUCCESS_OR_DIE(lazygaspi_prefetch(row, table, slack));
        
        if(separate_comp_write){
            //Computation
            auto beg_comp = get_time();
            for(lazygaspi_id_t proc_table = 0; proc_table < proc_table_amount; proc_table++)
            for(lazygaspi_id_t row = 0; row < table_size; row++) {
                auto index = proc_table * table_size + row;
                if(iteration){ 
                    average.setZero();
                    for(lazygaspi_id_t table = 0; table < table_amount; table++){
                        SUCCESS_OR_DIE(lazygaspi_read(row, table, slack, &read_row, &data));                                                
                        PRINT_DEBUG_TEST("Read row " << row_id << " from table " << table << " with age " << data.age << '.');

                        average += read_row;
                    }
                    average /= table_amount;
                    rows[index] += average;                
                } else {    
                    //Initialization
                    rows[index].setConstant(1);
                }
            }
            auto end_comp = get_time();
            //Write
            for(lazygaspi_id_t proc_table = 0; proc_table < proc_table_amount; proc_table++)
            for(lazygaspi_id_t row = 0; row < table_size; row++)
            for(size_t i = 0; i < table_size * proc_table_amount; i++){
                auto index = proc_table * table_size + row;
                SUCCESS_OR_DIE(lazygaspi_write(row, in_charge[proc_table], &rows[index]));
                PRINT_DEBUG_TEST("Wrote row " << row_id << " from table " << in_charge[proc_table] << " (" << rows[index] << ')');
            }
            auto end_write = get_time();
            PRINT_DEBUG_PERF("Finished read, computation and write. Times:\n\tAll: " << end_write - beg_comp << " sec.\n\t"
                             << "Read+Computation: " << end_comp - beg_comp << " sec.\n\tWrite: " << end_write - end_comp << " sec.");
        }
        else {
            auto beg_comp = get_time();
            for(lazygaspi_id_t proc_table = 0; proc_table < proc_table_amount; proc_table++)
            for(lazygaspi_id_t row = 0; row < table_size; row++) {
                auto index = proc_table * table_size + row;
                if(iteration){ 
                    average.setZero();
                    for(lazygaspi_id_t table = 0; table < table_amount; table++){
                        SUCCESS_OR_DIE(lazygaspi_read(row, table, slack, &read_row, &data));                                                
                        PRINT_DEBUG_TEST("Read row " << row_id << " from table " << table << " with age " << data.age << '.');
                        average += read_row;
                    }
                    average /= table_amount;
                    rows[index] += average;                
                } else {    
                    //Initialization
                    rows[index].setConstant(1);
                }
                SUCCESS_OR_DIE(lazygaspi_write(row, in_charge[proc_table], &rows[index]));
                PRINT_DEBUG_TEST("Wrote row " << row_id << " from table " << in_charge[proc_table] << " (" << rows[index] << ')');
            }
            auto end_comp = get_time();
            PRINT_DEBUG_PERF("Finished computation. Time: " << end_comp - beg_comp << " sec.");
        }
        //Fulfil prefetches
        if(should_prefetch)
        SUCCESS_OR_DIE(lazygaspi_fulfil_prefetches());
    }
    auto end_cycle = get_time();

    PRINT_DEBUG_PERF("Finished cycle. Time: " << end_cycle - beg_cycle << " sec.");

    SUCCESS_OR_DIE(GASPI_BARRIER);

    PRINT_DEBUG_TEST("All finished.");

    /** TODO: DETERMINE MEASURE OF GOODNESS 
    for(lazygaspi_id_t proc_table = 0; proc_table < proc_table_amount; proc_table++)

        for(size_t i = 0; i < TABLE_SIZE; i++){
            lazygaspi_read(i, info->id, SLACK, &other);
            PRINT_DEBUG_INTERNAL(other);
        }
    */
    SUCCESS_OR_DIE(GASPI_BARRIER);

    SUCCESS_OR_DIE(lazygaspi_term());

    free(rows);

    return EXIT_SUCCESS;
}

void print_usage(){
    std::cout << "Usage: gaspi_run <...args...> -k <rows_per_table> -n <amount_of_tables> -r <size_of_row> [-s <slack>] [-i <iter>] [-p]\n\n"
              << "Parameters:\n"
              << "  -k <rows_per_table>:    The amount of rows in one table.\n"
              << "  -n <amount_of_tables>:  The total amount of tables.\n"
              << "  -r <size_of_row>:       The size of a single row, in bytes.\n"
              << "  [-s <slack>]:           The amount of slack used. Default is 2.\n"
              << "  [-i <iter>]:            The maximum number of iterations. Default is 20.\n"
              << "  [-p]:                   Indicates that prefetching should occur. Omit for no prefetching."
              << std::endl;
}
