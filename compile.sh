function is_uint {
	local regex="^[0-9]+$"
	! [[ $1 =~ $regex ]]
	return $?
}

function print_usage {
  printf "\nUsage:\n\n\t(1) compile.sh <split> <args1 ...> <args2 ...>\n"
  printf "\t(2) compile.sh <args ...> 			 (called if <split> from (1) is not a positive integer)\n"
  printf "\nParameters:\n\n"
  printf "\t(1)	<split> 	Indicates the amount of elements in args1.\n"
  printf "\t	<args1 ...> 	A sequence of arguments of size <split>. These will be passed to g++ before any other parameters.\n"
  printf "\t	<args2 ...>	A sequence of arguments that will be passed to g++ after everything else.\n\n"
  printf	"\t(2)	<args ...>      Same as <args1 ...> being <args ...> and <args2 ...> being \"\" in (1). Can be an empty sequence.\n\n"
}

if [ $# > 1 ]; then 										     #If number of arguments is bigger than 1 (more than just the command itself).
  if [[ $1 = "-h" ]] || [[ $1 = "--help" ]]; then 
    print_usage
    exit 0
  fi

  rm -f bin/test.o

  is_uint $1										  		#See if first argument is a positive integer
  if (( $? )); then                                        #If it is, then it will indicate where to split the arguments 
    printf "Compiling test with:\n\t%s\n" "g++ ${*:2:$1} -pthread -Iinclude -o bin/test.o src/*.cpp -Llib64 -lGPI2 ${*:$(( $1 + 2 ))}"
    g++ ${@:2:$1} -pthread -Iinclude -o bin/test.o src/*.cpp -Llib64 -lGPI2 ${@:$(( $1 + 2 ))}   	#First part goes before anything else and second part goes after everything else
    gpp_success=$? 
  else
    printf "Compiling test with:\n\t%s\n" "g++ ${*:1} -pthread -Iinclude -o bin/test.o src/*.cpp -Llib64 -lGPI2"
    g++ ${@:1} -pthread -Iinclude -o bin/test.o src/*.cpp -Llib64 -lGPI2
    gpp_success=$?
  fi
fi

if ( ! (( $gpp_success )) ) && [[ ${1:0:3} = "000" ]]; then
    echo "Sending test to alexandrepc..."
	ssh alexandrepc "if [ ! -d \"${PWD}/bin\" ]; then mkdir -p ${PWD}/bin; fi; exit"
	scp bin/test.o alexandrepc:${PWD}/bin/test.o
fi

