/*
 ***********************************************************************
 *
 *        @version  1.0
 *        @date     03/07/2014 11:52:40 AM
 *        @author   Fridolin Pokorny <fridex.devel@gmail.com>
 *
 ***********************************************************************
 */

/*
 * Print benchmark and debug output?
 */
//#define DEBUG

#include <mpi.h>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include <assert.h>
#include <cmath>
#include <vector>

#ifdef DEBUG
#include <sys/time.h>
#include <sys/resource.h>
#endif // ! DEBUG

/*
 * Tags
 */
#define TAG_X			1
#define TAG_Y			2
#define TAG_D_UP		3
#define TAG_D_DOWN	4

#ifdef DEBUG
//#define DEBUG_PRINT cerr
#define DEBUG_PRINT while(0) cerr
#else
#define DEBUG_PRINT while(0) cerr
#endif

using namespace std;

/**
 * @brief  Name of file with numbers
 */
const char * FILE_NAME = "numbers";

typedef std::vector<char> number_t;
typedef int prop_t;

/**
 * @brief  D values
 */
enum {
	PROPAGATE,
	GENERATE,
	STOP
};

#ifdef DEBUG
double get_time() {
	struct timeval t;
	struct timezone tzp;
	gettimeofday(&t, &tzp);
	return t.tv_sec + t.tv_usec*1e-6;
}
#endif // ! DEBUG

/**
 * @brief  Get log2 from integer
 *
 * @param n number to get log2
 *
 * @return   log2 of n
 */
int log_2(unsigned n) {
	int res = 0;
	while (n >>= 1) ++res;
	return res;
}

/**
 * @brief  Parse input
 *
 * @param x value X from file
 * @param y value Y from file
 *
 * @return   true if parse was successfull
 */
bool get_input(number_t & x, number_t & y) {
	ifstream f;
	number_t * to;
	char c;

	f.open(FILE_NAME);

	if (! f) {
		cerr << "Error: failed to open input!\n";
		return false;
	}

	to = &x;
	while ((c = f.get()), f) {
		if (c != '\n') {
		   if (c == '1' || c == '0') {
			   to->push_back(c);
			} else {
				cerr << "Error: not binary numbers in input file!\n";
				return false;
			}
		} else {
			to = &y;
		}
	}

	if (x.size() != y.size()) {
		cerr << "Error: operand length missmatched!\n";
		return false;
	}

	if (x.size() == 0) {
		cerr << "Error: no input!\n";
		return false;
	}

	return true;
}

/**
 * @brief  Calculate D depending on ops
 *
 * @param op1 operand to use
 * @param op2 operand to use
 *
 * @return   result of operation
 */
prop_t operation_prop(prop_t op1, prop_t op2) {
	switch (op1) {
		case STOP:
			return STOP;
			break;
		case PROPAGATE:
			return op2;
			break;
		case GENERATE:
			return GENERATE;
			break;
		default:
			cerr << op1 << endl;
			assert(! "Unknown prop_t!");
			break;
	}
}

/**
 * @brief  Get string from prop_t
 *
 * @param prop prop_t to use
 *
 * @return   string depending on prop_t
 */
const char * prop_str(prop_t prop) {
	switch (prop) {
		case STOP:
			return "STOP";
			break;
		case PROPAGATE:
			return "PROPAGATE";
			break;
		case GENERATE:
			return "GENERATE";
			break;
		default:
			cerr << prop << endl;
			assert(! "Unknown prop_t!");
			break;
	}

	return NULL;
}

/**
 * @brief  Create sum without truncation
 *
 * @param x op1 t0 sum
 * @param y op2 to sum
 * @param carry carry flag
 *
 * @return   summary
 */
static inline
int sum(char x, char y, int carry) {
	int xn = (x == '1');
	int yn = (y == '1');

	return xn + yn + carry;
}

/**
 * @brief  App's entry point
 *
 * @param argc argument count
 * @param argv[] argument vector
 *
 * @return  0 on success otherwise error code
 */
int main(int argc, char *argv[]) {
	int numprocs;     // number of procecs
	int myid;         // current proc RANK
	MPI_Status stat;  // routine status

	char x;
	char y;
	prop_t d;
	prop_t d_right;
	prop_t d_left;
	prop_t d_tmp;
	int carry;
	size_t leaf_start;
	size_t bits;

	// init
	MPI_Init(&argc, &(argv));
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);

	leaf_start = numprocs / 2;
	bits = numprocs / 2;

	if(myid == 0) { // input proc
		/*
		 * RANK 0 only gets input
		 */
		number_t xn;
		number_t yn;

		if (! get_input(xn, yn)) return 1;

		if (xn.size() != numprocs / 2) {
			cerr << xn.size() << " : " << numprocs / 2 << endl;
			cerr << "Error: bad proc count and bits count missmatched!\n";
			return 1;
		}

		for (size_t i = 0; i < bits; ++i) {
			MPI_Send(&xn[i], 1, MPI_CHAR, leaf_start + i, TAG_X, MPI_COMM_WORLD);
		}

		for (size_t i = 0; i < bits; ++i) {
			MPI_Send(&yn[i], 1, MPI_CHAR, leaf_start + i, TAG_Y, MPI_COMM_WORLD);
		}

	} else if (myid >= leaf_start) { // leaf procs
		/*
		 * Receive values from RANK 0
		 */
		MPI_Recv(&x, 1, MPI_CHAR, 0, TAG_X, MPI_COMM_WORLD, &stat);
		MPI_Recv(&y, 1, MPI_CHAR, 0, TAG_Y, MPI_COMM_WORLD, &stat);

		/*
		 * Calculate initial D
		 */
		if (x == '1' && y == '1')			d = GENERATE;
		else if (x == '0' && y == '0')	d = STOP;
		else										d = PROPAGATE;
	}

	/*
	 * Benchmark
	 */
#ifdef DEBUG
	MPI_Barrier(MPI_COMM_WORLD);
	double time;
	if (myid == 0)
		time = get_time();
	MPI_Barrier(MPI_COMM_WORLD); // force switch to get time
#endif // ! DEBUG

	if (myid != 0) {
		/*
		* UP SWEEP
		*/
		if (myid < leaf_start) {
			MPI_Recv(&d_right, 1, MPI_INT, 2*myid + 1, TAG_D_UP, MPI_COMM_WORLD, &stat);
			MPI_Recv(&d_left, 1, MPI_INT, 2*myid, TAG_D_UP, MPI_COMM_WORLD, &stat);

			d = operation_prop(d_left, d_right);
		}

		if (myid != 1)
			MPI_Send(&d, 1, MPI_INT, myid / 2, TAG_D_UP, MPI_COMM_WORLD);

		DEBUG_PRINT << myid << " ~~ " << prop_str(d) << endl;

		/*
		* DOWN SWEEP
		*/
		if (myid == 1)
			d = PROPAGATE; // neutral

		if (myid != 1) {
			if (myid % 2 == 1)
				MPI_Send(&d, 1, MPI_INT, myid / 2, TAG_D_DOWN, MPI_COMM_WORLD);
			MPI_Recv(&d, 1, MPI_INT, myid / 2, TAG_D_DOWN, MPI_COMM_WORLD, &stat);
		}

		if (myid < leaf_start) { // skip leaf nodes
			MPI_Recv(&d_left, 1, MPI_INT, 2*myid + 1, TAG_D_DOWN, MPI_COMM_WORLD, &stat);
			MPI_Send(&d, 1, MPI_INT, 2*myid + 1, TAG_D_DOWN, MPI_COMM_WORLD);
			d_right = operation_prop(d_left, d);
			MPI_Send(&d_right, 1, MPI_INT, 2*myid, TAG_D_DOWN, MPI_COMM_WORLD);
		}

		DEBUG_PRINT << myid << " -- " <<  prop_str(d) << endl;
	}

	/*
	 * Benchmark
	 */
#ifdef DEBUG
	MPI_Barrier(MPI_COMM_WORLD);
	if (myid == 0) {
		cerr << "TIME: " << get_time() - time << endl;
	}
	MPI_Barrier(MPI_COMM_WORLD); // force switch to print time
#endif

	if (myid != 0 && myid >= leaf_start) {
		int s = sum(x, y, d == GENERATE);

		if (myid == leaf_start && s > 1)
			cout << "overflow\n";

		cout << myid << ":" << s % 2 << endl;
	}

	MPI_Finalize();

	return 0;
}

