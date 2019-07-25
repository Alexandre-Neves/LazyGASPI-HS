# LazyGASPI-HS
## Implementation of the LazyGASPI library with a Homogeneous and Sharded implementation.

|  Implementation of            | at            |
| ----------------------------- | ------------- |
| lazygaspi_init                | init.cpp      |
| lazygaspi_get_info            | general.cpp   |
| lazygaspi_fulfil_prefetches   | prefetch.cpp  |
| lazygaspi_prefetch            | prefetch.cpp  |
| lazygaspi_read                | read.cpp      |
| lazygaspi_write               | write.cpp     |
| lazygaspi_clock               | general.cpp   |
| lazygaspi_term                | general.cpp   |




# Test.cpp

The test shards data in whole tables and distributes them evenly among all processes. 
If there are $t$ tables and $n$ processes, then each will get $t / n$ plus one if rank is $< t % n$.
Row size is not important, except to increase the amount of data handled.

## Compilation and Usage

Use compile.sh to compile test. Depends on libGPI2.a (GPI-2, version 1.3.0) available at /lib64/ and on the Eigen headers (version 3.3.7).
Arguments passed to compile.sh are relayed to g++ before any other parameters. For example:  
    ./compile.sh -D MY_MACRO -D MY_MACRO2  
compiles the test with the two predefined macros MY_MACRO and MY_MACRO2.

Debug Macros:
- DEBUG_INTERNAL: Prints debug messages from the library itself into the output file (see [Output](#Output) section).

## Procedure

Let current rank be $X$. Let $I$ be stipulated number of iterations (20 by default).  
For $iter$ in ${0, ..., I-1}$:  
    If $iter$ is 0:  
        Initialize every entry of every row of every table to 1.  
    Else:  
        For every table, $T$, assigned to %X%:  
            For every row $i$ in $T$:  
                Read row $i$ of $T$.  
                For every table $T_2$ (including $T$):  
                    Read $i$-th row of $T_2$.  
                    Add to an average vector.  
                Divide average vector by amount of tables.  
                Add average vector to row $i$ of $T$.  
                Write row into the same position of $T$.  

## Output

Program outputs to lazygaspi_hs_$X$.out, except when 
