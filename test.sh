#!/bin/bash

FILENAME="numbers"

function print_help() {
	echo    "OpenMPI implementation of Carry Look Ahead Parallel Binary Adder"
	echo    "Usage:"
	echo -e "\t$0 BIT_COUNT"
	echo -e "\nFile 'numbers' containing 2 numbers with bit count of BIT_COUNT"
	echo -e "on 2 lines in binary form has to exist."
}

function check_file() {
	[ -s "$FILENAME" ] \
		|| { echo "$FILENAME: no such file" >&2; print_help $0; return 1; }

	[ `wc -l "$FILENAME" | cut -f 1 -d' '` -eq 2 ] \
		|| { echo "$FILENAME: bad line count" >&2; print_help $0; return 1; }

	[ `head -n 1 "$FILENAME" | wc -c` -eq `tail -n 1 "$FILENAME" | wc -c` ] \
		|| { echo "$FILENAME: bit count missmatched" >&2; print_help $0; return 1; }

	return 0;
}

function get_proc_count() {
	#echo $((($1*2)-1))
	# Rank 0 only reads, not used to compute
	echo $((($1*2)))
}

[ $# -eq 1 ] \
	|| { echo "Invalid arguments" >&2; print_help; exit 1; }

[ "$1" -eq "$1" ] 2>/dev/null  \
	|| { echo "Bad number: '$1'" >&2; print_help; exit 1; }


check_file "$1" || exit 1
bits=`get_proc_count $1`

mpic++ --prefix /usr/local/share/OpenMPI -o mm mm.cpp
#mpic++ -o mm mm.cpp || exit 1
mpirun --prefix /usr/local/share/OpenMPI -np $bits mm
#LD_PRELOAD="/usr/lib64/openmpi/lib/libmpi_cxx.so.1 /usr/lib64/openmpi/lib/libmpi.so.1" mpirun -np $bits mm
rm -f mm

