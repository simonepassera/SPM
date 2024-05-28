#include <cstdio>
#include <random>
#include <map>
#include <vector>
#include <mpi.h>
#include <set>
#include <sys/time.h>

const long SIZE = 64;

long random(const int &min, const int &max) {
	static std::mt19937 generator(117);
	std::uniform_int_distribution<long> distribution(min, max);
	return distribution(generator);
}

void init(auto &M, const long c1, const long c2, const long key) {
	for(long i = 0; i < c1; i++)
		for(long j = 0; j < c2; j++)
			M[i][j] = (key-i-j) / static_cast<float>(SIZE);
}

// Matrix multiplication: C = A x B  A[c1][c2] B[c2][c1] C[c1][c1]
// mm returns the sum of the elements of the C matrix
auto mm(const auto& A, const auto& B, const long c1, const long c2) {
	float sum {0};
	
    for (long i = 0; i < c1; i++) {
        for (long j = 0; j < c1; j++) {
            auto accum = float(0.0);
            
			for (long k = 0; k < c2; k++)
            	accum += A[i][k] * B[k][j];
            
			sum += accum;
		}
	}

	return sum;
}

// Initialize two matrices with the computed values of the keys
// and execute a matrix multiplication between the two matrices
// to obtain the sum of the elements of the result matrix 
float compute(const long c1, const long c2, long key1, long key2) {
	std::vector<std::vector<float>> A(c1, std::vector<float>(c2, 0.0));
	std::vector<std::vector<float>> B(c2, std::vector<float>(c1, 0.0));

	init(A, c1, c2, key1);
	init(B, c2, c1, key2);
	auto r = mm(A, B, c1, c2);
	
	return r;
}

int nextDest(int curDest) {
	if (curDest == SIZE-1)
		return 1;
	else
		return curDest + 1;
}

int main(int argc, char* argv[]) {
	MPI_Init(&argc, &argv);

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (argc < 3) {
		if (!rank) {
			std::printf("use: %s nkeys length [print(0|1)]\n", argv[0]);
			std::printf("     print: 0 disabled, 1 enabled\n");
		}

		MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
	}

	// Number of processes in the group of MPI_COMM_WORLD
	int size; 
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if (size < 2) {
		std::printf("Launch the program with at least 2 processes\n");
		MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
	}

	long nkeys  = std::stol(argv[1]); // Total number of keys
	long length = std::stol(argv[2]); // Stream length
	bool print = false;

	if (argc == 4)
		print = (std::stoi(argv[3]) == 1) ? true : false;

	std::vector<float> V(nkeys, 0);
	std::vector<long> map(nkeys, 0);
	std::vector<bool> send_for1(nkeys, false);

	long key1, key2, count, buf1[4], buf2[4], EOS = -1;
	float r = 0;
	int dest = 0;

	MPI_Request request_key1, request_key2;

	// Measure the current time
	MPI_Barrier(MPI_COMM_WORLD);
	double start = MPI_Wtime();

	if (rank == 0) {
		for(int i = 0; i < length; i++) {
			key1 = random(0, nkeys-1);
			key2 = random(0, nkeys-1);
		
			if (key1 == key2)
				key1 = (key1+1) % nkeys;
		
			map[key1]++;
			map[key2]++;

			if (send_for1[key1]) {
				MPI_Recv(&r, 1, MPI_FLOAT, MPI_ANY_SOURCE, key1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				
				V[key1] += r;
				if (nkeys == 1) V[key1] += r; 
				
				auto _r = static_cast<unsigned long>(r) % SIZE;
				count = map[key1] - SIZE;
				map[key1] = (_r>(SIZE/2)) ? 0 : _r;
				map[key1] += count;
				
				send_for1[key1] = false;
			}

			if (send_for1[key2]) {
				MPI_Recv(&r, 1, MPI_FLOAT, MPI_ANY_SOURCE, key2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				
				V[key2] += r;
				
				auto _r = static_cast<unsigned long>(r) % SIZE;
				count = map[key2] - SIZE;
				map[key2] = (_r>(SIZE/2)) ? 0 : _r;
				map[key2] += count;
				
				send_for1[key2] = false;
			}

			if (map[key1] == SIZE && map[key2] != 0) {
				buf1[0] = map[key1];
				buf1[1] = map[key2];
				buf1[2] = key1;
				buf1[3] = key2;

				MPI_Isend(buf1, 4, MPI_LONG, nextDest(dest), 0, MPI_COMM_WORLD, &request_key1);
				send_for1[key1] = true;		
			}

			if (map[key2] == SIZE && map[key1] != 0 && nkeys != 1) {
				buf2[0] = map[key2];
				buf2[1] = map[key1];
				buf2[2] = key2;
				buf2[3] = key1;

				MPI_Isend(buf2, 4, MPI_LONG, nextDest(dest), 0, MPI_COMM_WORLD, &request_key2);
				send_for1[key2] = true;	
			}

			if (send_for1[key1]) MPI_Wait(&request_key1, MPI_STATUS_IGNORE);
			if (send_for1[key2] && nkeys != 1) MPI_Wait(&request_key2, MPI_STATUS_IGNORE);
		}

		std::vector<bool> send_for2(nkeys, false);

		// compute the last values
		for(long i = 0; i < nkeys; i++) {
			for(long j = 0; j < nkeys; j++) {
				if (i == j) continue;

				if (send_for1[i]) {
					MPI_Recv(&r, 1, MPI_FLOAT, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					
					V[i] += r;
					auto _r = static_cast<unsigned long>(r) % SIZE;
					map[i] = (_r>(SIZE/2)) ? 0 : _r;
					send_for1[i] = false;
				}

				if (send_for1[j]) {
					MPI_Recv(&r, 1, MPI_FLOAT, MPI_ANY_SOURCE, j, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					
					V[j] += r;
					auto _r = static_cast<unsigned long>(r) % SIZE;
					map[j] = (_r>(SIZE/2)) ? 0 : _r;
					send_for1[j] = false;
				}

				if (map[i] > 0 && map[j] > 0) {
					if (send_for2[i]) {
						MPI_Recv(&r, 1, MPI_FLOAT, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						V[i] += r;
						send_for2[i] = false;
					}

					buf1[0] = map[i];
					buf1[1] = map[j];
					buf1[2] = i;
					buf1[3] = j;

					MPI_Isend(buf1, 4, MPI_LONG, nextDest(dest), 0, MPI_COMM_WORLD, &request_key1);
					send_for2[i] = true;

					if (send_for2[j]) {
						MPI_Recv(&r, 1, MPI_FLOAT, MPI_ANY_SOURCE, j, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						V[j] += r;
						send_for2[j] = false;
					}

					buf2[0] = map[j];
					buf2[1] = map[i];
					buf2[2] = j;
					buf2[3] = i;

					MPI_Isend(buf2, 4, MPI_LONG, nextDest(dest), 0, MPI_COMM_WORLD, &request_key2);
					send_for2[j] = true;

					MPI_Wait(&request_key1, MPI_STATUS_IGNORE);
					MPI_Wait(&request_key2, MPI_STATUS_IGNORE);
				}
			}
		}

		if(nkeys == 1) {
			if (send_for1[0]) {
				MPI_Recv(&r, 1, MPI_FLOAT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				V[0] += r;
				V[0] += r;
			}
		}

		for (int i = 0; i < nkeys; i++) {
			if (send_for2[i]) {
				MPI_Recv(&r, 1, MPI_FLOAT, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				V[i] += r;
			}
		}

		buf1[2] = EOS;
		std::vector<MPI_Request> request_all(size-1);

		for (int i = 1; i < size; i++)
			MPI_Isend(buf1, 4, MPI_LONG, i, 0, MPI_COMM_WORLD, &request_all.data()[i-1]);

		MPI_Waitall(size-1, request_all.data(), MPI_STATUSES_IGNORE);

		// Measure the current time
		double end = MPI_Wtime();

		MPI_Finalize();

		std::printf("Total time: %f (S)\n", end-start);

		// printing the results
		if (print)
			for(long i = 0; i < nkeys; i++)
	 			std::printf("key %ld : %f\n", i, V[i]);
	} else {
		MPI_Request request = MPI_REQUEST_NULL;

		while (true) {
			MPI_Recv(buf1, 4, MPI_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			if (buf1[2] == EOS) {
				if (request != MPI_REQUEST_NULL)
					MPI_Wait(&request, MPI_STATUS_IGNORE);
				
				break;
			}

			r = compute(buf1[0], buf1[1], buf1[2], buf1[3]);

			if (request != MPI_REQUEST_NULL) MPI_Wait(&request, MPI_STATUS_IGNORE);
			MPI_Isend(&r, 1, MPI_FLOAT, 0, buf1[2], MPI_COMM_WORLD, &request);
		}

		MPI_Finalize();
	}

	return EXIT_SUCCESS;
}