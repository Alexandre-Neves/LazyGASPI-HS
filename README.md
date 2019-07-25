# LazyGASPI-HS
### LazyGASPI library with a Homogeneous and Sharded (HS) implementation

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
  
  
  
  
# Test

The test shards data in whole tables and distributes them evenly among all processes. 
If there are $t$ tables and `n` processes, then each will get `t / n` plus one if rank is less than  `t % n`.
Row size is not important, except to increase the amount of data handled. A large table size can show differences between the rows of a given table, which is the result of other processes writing their rows while the current process is calculating its table.

## Compilation & Usage

#### Compilation

Use compile.sh to compile test. Depends on libGPI2.a (GPI-2, version 1.3.0) available at /lib64/ and on the Eigen headers (version 3.3.7).
Arguments passed to compile.sh are relayed to g++ before any other parameters. For example:  
```
  ./compile.sh -D MY_MACRO -D MY_MACRO2 -I~/my_includes
```  
compiles the test with the two predefined macros `MY_MACRO` and `MY_MACRO2` and adds `~/my_includes` to the include path.
Unfortunately libraries must be hardcoded in the script since they must come after the source code.
  
The following macros can be used with compile.sh:  

| Macros            | Description                                                                                           | 
| ----------------- | ------------------------------------------------------------------------------------------------------|
| DEBUG_INTERNAL    | Prints debug messages from the library itself into the output file (see [Output](#Output) section)    |
| DEBUG_TEST        | Prints debug messages from the test program that are not related to performance into the output file (see [Output](#Output) section) |
| DEBUG_PERF        | Prints debug messages from the test program related to performance (run times and quality of the result; see [Output](#Output) section) |  
| DEBUG_GASPI_UTILS | Prints debug from the `gaspi_utils` header (see [Output](#Output) section)                            |
| DEBUG             | Same as defining all of the above macros |
  
  
#### Usage


  
  
  
## Procedure

Let current rank be X. Let I be the stipulated number of iterations (20 by default).  
```
For iter in {0, ..., I-1}:  
    If iter is 0:  
        Initialize every entry of every row of every table to 1.  
    Else:  
        For every table, T, assigned to X:  
            For every row i in T:  
                Read row i of T.  
                For every table T' (including T):  
                    Read i-th row of T'.
                    Add to an average vector.  
                Divide average vector by amount of tables.  
                Add average vector to row i of T.  
                Write row into the same position of T.  
```

## Output

Program outputs to lazygaspi_hs_X.out, except before it has a chance to create the file (before GASPI initializes or 
before rank and rank amount values are obtained). In that case, all output (enabled solely by DEBUG_INTERNAL) generated
before that is directed to the standard output.
All output generated by DEBUG_GASPI_UTILS is also directed to the standard output (this output is errors only).
