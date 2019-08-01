rm -f lazygaspi_hs_*.out
rm -f ~/lazygaspi_hs_*.out
ssh alexandrepc "rm -f ~/lazygaspi_hs_*.out"

args="-k 6 -n 4 -r 8 -2 12 -s 2"
if [[ $# > 0 ]]; then
    args=$@
fi
echo "../GPI/bin/gaspi_run -m machinefile ${PWD}/bin/test.o $args"
../GPI/bin/gaspi_run -m machinefile ${PWD}/bin/test.o $args
    

