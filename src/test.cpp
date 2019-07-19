#include "lazygaspi.h"
#include "gaspi_utils.h"
#include "utils.h"
#include <Eigen/Core>
#include <cstdlib>
#include <unistd.h>

#include <Eigen/Sparse>

#define ROW Eigen::Matrix<unsigned long, 1, Eigen::Dynamic>
#define ROW_SIZE (sizeof(ROW) + row_size)
#define ITERATIONS 20
#define PREFETCH_FLAG false

//Default slack value
#define SLACK 2

#include <vector>

void print_usage();

int main(int argc, char** argv){
    int ch;
    lazygaspi_slack_t slack = SLACK;
    lazygaspi_age_t max_iter = ITERATIONS;
    auto should_prefetch = PREFETCH_FLAG;
    lazygaspi_id_t table_size = 0, table_amount = 0;
    gaspi_size_t row_size = 0;
    while((ch = getopt(argc, argv, "k:n:r:s:i:p")) != -1) {
        switch(ch){
            case 'k': table_size = atol(optarg); break;
            case 'n': table_amount = atol(optarg); break;
            case 'r': row_size = atol(optarg); break;
            case 's': slack = atol(optarg); break;
            case 'i': max_iter = atol(optarg); break;
            case 'p': should_prefetch = !should_prefetch; break;
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

                            PRINT_DEBUG_TEST("Starting computation...");
                
    auto beg_computation = get_time();
    for(lazygaspi_age_t iteration = 0; iteration < max_iter; iteration++){
        PRINT_DEBUG_TEST("Started iteration " << iteration << "...");

        //Clock
        SUCCESS_OR_DIE(lazygaspi_clock());

        //Prefetch
        for(lazygaspi_id_t row = 0; row < table_size; row++) 
        for(lazygaspi_id_t table = 0; table < table_amount; table++)
            SUCCESS_OR_DIE(lazygaspi_prefetch(row, table, slack));
        
        //Computation
        for(size_t proc_table = 0; proc_table < proc_table_amount; proc_table++)
        for(lazygaspi_id_t row = 0; row < table_size; row++) {
            auto index = proc_table * table_size + row;
            if(iteration){ 
                //SUCCESS_OR_DIE(lazygaspi_read((in_charge.at(i).first + 1) % TABLE_SIZE, (in_charge.at(i).second + 1) % TABLE_AMOUNT, 
                 //                               SLACK, &other, &data));
                PRINT_DEBUG_TEST("Read row after " << in_charge.at(i).first << " and after table " << in_charge.at(i).second << 
                            ", with age " << data.age << '.');
                //rows[index](0, (t - 2) % ROW_ENTRIES) = t;
                //rows[index] += other;
                
            } else {    
                //Initialization
                rows[index].setConstant(1);
            }
        }

        //Write
        for(size_t i = 0; i < size; i++){
            SUCCESS_OR_DIE(lazygaspi_write(in_charge.at(i).first, in_charge.at(i).second, rows + i));
            PRINT_DEBUG_TEST("Wrote " << rows[i]);
        }

        //Fulfil prefetches
        SUCCESS_OR_DIE(lazygaspi_fulfil_prefetches());
    }
    auto end_computation = get_time();

    PRINT_DEBUG_TEST(" Finished computation. Time: " << end_computation - beg_computation << " sec.");

    SUCCESS_OR_DIE(GASPI_BARRIER);

    PRINT_DEBUG_TEST(" All finished.");

    /** TODO: DETERMINE MEASURE OF GOODNESS 
    for(lazygaspi_id_t proc_table = 0; proc_table < proc_table_amount; proc_table++)

        for(size_t i = 0; i < TABLE_SIZE; i++){
            lazygaspi_read(i, info->id, SLACK, &other);
            PRINT_DEBUG_INTERNAL(other);
        }
    */
    SUCCESS_OR_DIE(GASPI_BARRIER);

    SUCCESS_OR_DIE(lazygaspi_term());

    return EXIT_SUCCESS;
}

void print_usage(){
    std::cout << "Usage: gaspi_run <...args...> -k <rows_per_table> -n <amount_of_tables> -r <size_of_row> [-s <slack>] [-i <iter>]\n\n"
              << "Parameters:\n"
              << "  -k <rows_per_table>: The amount of rows in one table.\n"
              << "  -n <amount_of_tables>: The total amount of tables.\n"
              << "  -r <size_of_row>: The size of a single row, in bytes.\n"
              << "  [-s <slack>]: the amount of slack used. Default is 2.\n"
              << "  [-i <iter>]: the maximum number of iterations. Default is 20."
              << std::endl;
}
