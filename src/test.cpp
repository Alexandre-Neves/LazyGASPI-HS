#include "lazygaspi.h"
#include "gaspi_utils.h"
#include "utils.h"
#include <Eigen/Core>
#include <cstdlib>

#define ROW_ENTRIES 16
#define TABLE_SIZE 16
#define ROW Eigen::Matrix<double, 1, ROW_ENTRIES>
#define ROW_SIZE sizeof(ROW)
#define ITERATIONS 20

#define TABLE_AMOUNT 3

#define SLACK 2


#include <vector>

int main(int argc, char** argv){
    SUCCESS_OR_DIE(lazygaspi_init(TABLE_SIZE, TABLE_AMOUNT, ROW_SIZE));

    LazyGaspiProcessInfo* info;
    SUCCESS_OR_DIE(lazygaspi_get_info(&info));

    SUCCESS_OR_DIE(setup_gaspi_output("lazygaspi_hs", info->id, &(info->out)));

    ASSERT(info->n != 0, "main");

    auto in_charge = std::vector<std::pair<lazygaspi_id_t, lazygaspi_id_t>>(round(TABLE_SIZE / info->n) * TABLE_AMOUNT);

    size_t index = 0;
    for(lazygaspi_id_t table = 0; table < TABLE_AMOUNT; table++) {
        for(lazygaspi_id_t row_id = (info->id + info->n - table % info->n) % info->n; row_id < TABLE_SIZE; row_id += info->n){
            in_charge.at(index++) = std::make_pair(row_id, table);
        }
    }

    SUCCESS_OR_DIE(GASPI_BARRIER);

    ROW* rows = nullptr;
    if(in_charge.size()) rows = (ROW*)malloc(sizeof(ROW) * in_charge.size());

    ROW other;
    LazyGaspiRowData data;

    PRINT_DEBUG("| TEST | Began computation...");

    for(lazygaspi_age_t t = 1; t <= 20; t++){
        PRINT_DEBUG("| TEST | Started iteration " << t << "...");

        //Clock
        SUCCESS_OR_DIE(lazygaspi_clock());   

        //Prefetch
        for(auto entry : in_charge)
            SUCCESS_OR_DIE(lazygaspi_prefetch((entry.first + 1) % TABLE_SIZE, (entry.second + 1) % TABLE_AMOUNT, SLACK));
        
        //Computation
        const auto size = in_charge.size();
        for(size_t i = 0; i < size; i++){
            if(t == 1) rows[i] = ROW().setZero(); 
            else{
                SUCCESS_OR_DIE(lazygaspi_read((in_charge.at(i).first + 1) % TABLE_SIZE, (in_charge.at(i).second + 1) % TABLE_AMOUNT, 
                                                SLACK, &other, &data));
                PRINT_DEBUG("| TEST | Read row after " << in_charge.at(i).first << " and after table " << in_charge.at(i).second << 
                            ", with age " << data.age << '.');
                rows[i](0, (t - 2) % ROW_ENTRIES) = t;
                rows[i] += other;
            }
        }   

        //Write
        for(size_t i = 0; i < size; i++){
            SUCCESS_OR_DIE(lazygaspi_write(in_charge.at(i).first, in_charge.at(i).second, rows + i));
            PRINT_DEBUG("Wrote " << rows[i]);
        }

        //Fulfil prefetches
        SUCCESS_OR_DIE(lazygaspi_fulfil_prefetches());
    }

    PRINT_DEBUG("| TEST | Finished computation.");

    SUCCESS_OR_DIE(GASPI_BARRIER);

    PRINT_DEBUG("| TEST | All finished.");

    if(info->id < TABLE_AMOUNT)
        for(size_t i = 0; i < TABLE_SIZE; i++){
            lazygaspi_read(i, info->id, SLACK, &other);
            PRINT_DEBUG(other);
        }

    SUCCESS_OR_DIE(GASPI_BARRIER);

    SUCCESS_OR_DIE(lazygaspi_term());

    return EXIT_SUCCESS;
}