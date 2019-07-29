#!/bin/bash
#First arg is array name, second is element
function contains_but_not_last {
    local array=$1[@]
    eval local amount=\${#$array}
    if [[ $amount < 2 ]]; then
        return 0
    fi
    eval local indices=\${!$1[@]}
    for other in $indices; do
        if [[ $other = $(( $amount - 1 )) ]]; then
            return 0
        fi        
        eval local elem=\${$1[$other]}
        echo "Checked $2 for $elem"
        eval [[ $2 = $elem ]]
        if ! (( $? )); then
            return 1
        	break
        fi
    done
    return 0
}

function error_split_host {
	printf "Error: Multiple instances of the same node cannot be split!\n"
    rm -f machinefile
}

function print_usage {
	printf "Creates a machinefile to run GASPI programs.\n"
	printf "Usage: nodes.sh <node1> <amount1> ... <node_n> <amount_n>\n"
}


if [[ $1 = "-h" ]] || [[ $1 = "--help" ]] || [[ $1 = "-?" ]]; then
	print_usage
	exit 0
fi

if [[ $(( $# % 2 )) != 0 ]]; then
	echo "Error: invalid syntax"
	print_usage
	exit -1
fi

printf "" > machinefile
index=1
SEEN=()
while [[ $index < $# ]]; do
    host=${@:$index:1}
	contains_but_not_last SEEN $host
	if (( $? )); then
		error_split_host
		exit 1
	fi
	SEEN+=( $host )
    amount=${@:$(( $index + 1 )):1}
    i=0
    while [[ $i < $amount ]]; do		#Print host name and new line as many times as indicated
		printf "%s\n" $host	>> machinefile	
	    i=$(( $i + 1 ))
    done
	index=$(( $index + 2 ))
done

cat machinefile














