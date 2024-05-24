#include <cstdio>
#include <random>
#include <map>
#include <vector>
#include <mpi.h>
#include <set>
#include <sys/time.h>

const long SIZE = 64;
const long STREAM_SIZE = 256;

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

int main(int argc, char* argv[]) {
	MPI_Init(&argc, &argv);

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (argc < 3) {
		if (!rank) {
			std::printf("use: %s nkeys length [print(0|1)]\n", argv[0]);
			std::printf("     print: 0 disabled, 1 enabled\n");
		}

		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	// Number of processes in the group of MPI_COMM_WORLD
	int size; 
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	long nkeys  = std::stol(argv[1]); // Total number of keys
	long length = std::stol(argv[2]); // Stream length
	bool print = false;

	if (argc == 4)
		print = (std::stoi(argv[3]) == 1) ? true : false;

	std::set<long> myKeys;
	std::map<long, float> V;

	long block_size = nkeys / size;
	long r = nkeys % size;

	long lower = (rank * block_size) + (rank < r ? rank : r);
	long upper = lower + block_size + (rank < r ? 1 : 0);

	for (long i = lower; i < upper; i++) {
		myKeys.insert(i);
		V[i] = 0;
	}

	std::vector<long> map(nkeys, 0);
	long key1, key2, newKey1, count;
	long sequence[STREAM_SIZE*2], sequence_size, total_sequence = 0;
	float r1 = 0, r2 = 0;
	bool resetkey1 = false, resetkey2 = false;

	MPI_Request ibcast_request;
	std::vector<MPI_Request> request_key1(size-1);
	std::vector<MPI_Request> request_key2(size-1);

	MPI_Comm ibcast_communicator;
    MPI_Comm_dup(MPI_COMM_WORLD, &ibcast_communicator);

	// Measure the current time
	MPI_Barrier(MPI_COMM_WORLD);
	double start = MPI_Wtime();

	while(total_sequence < length) {
		sequence_size = std::min(length-total_sequence, STREAM_SIZE);
		total_sequence += sequence_size;

		if (rank == 0) {
			for (int j = 0, i = 0; j < sequence_size; j++, i+=2) {
				sequence[i] = random(0, nkeys-1);
				sequence[i+1] = random(0, nkeys-1);
				
				if (sequence[i] == sequence[i+1])
					sequence[i] = (sequence[i] + 1) % nkeys;
			}	
		}

		MPI_Ibcast(&sequence, sequence_size * 2, MPI_LONG, 0, ibcast_communicator, &ibcast_request);
		if (rank != 0) MPI_Wait(&ibcast_request, MPI_STATUS_IGNORE);

		for (int j = 0, i = 0; j < sequence_size; j++, i+=2) {
			key1 = sequence[i];
			key2 = sequence[i+1];

			map[key1]++;
			map[key2]++;

			if (myKeys.contains(key1)) {
				if (map[key1] == SIZE) {
					if (map[key2] > SIZE) {
						count = map[key2] - SIZE;

						while (true) {
							MPI_Recv(&map.data()[key2], 1, MPI_LONG, MPI_ANY_SOURCE, key2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

							if (count > (SIZE - map[key2])) count -= (SIZE - map[key2]);
							else break;
						}

						map[key2] += count;
					}
					
					if (map[key2] != 0) {
						r1 = compute(map[key1], map[key2], key1, key2);
						V[key1] += r1;

						auto _r1 = static_cast<unsigned long>(r1) % SIZE;
						newKey1 = (_r1>(SIZE/2)) ? 0 : _r1;
						resetkey1 = true;

						for (int dest = 0, i = 0; dest < size; dest++)
							if (dest != rank)
								MPI_Isend(&newKey1, 1, MPI_LONG, dest, key1, MPI_COMM_WORLD, &request_key1.data()[i++]);
					}
				}
			}

			if (myKeys.contains(key2)) {
				if (map[key2] == SIZE) {
					if (map[key1] > SIZE) {
						count = map[key1] - SIZE;

						while (true) {
							MPI_Recv(&map.data()[key1], 1, MPI_LONG, MPI_ANY_SOURCE, key1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

							if (count > (SIZE - map[key1])) count -= (SIZE - map[key1]);
							else break;
						}
						
						map[key1] += count;
					}
					
					if (map[key1] != 0) {
						r2 = compute(map[key2], map[key1], key2, key1);
						V[key2] += r2;

						auto _r2 = static_cast<unsigned long>(r2) % SIZE;
						map[key2] = (_r2>(SIZE/2)) ? 0 : _r2;
						resetkey2 = true;

						for (int dest = 0, i = 0; dest < size; dest++)
							if (dest != rank)
								MPI_Isend(&map.data()[key2], 1, MPI_LONG, dest, key2, MPI_COMM_WORLD, &request_key2.data()[i++]);
					}
				}
			}

			if (resetkey1) {
				MPI_Waitall(request_key1.size(), request_key1.data(), MPI_STATUSES_IGNORE);
				map[key1] = newKey1;
				resetkey1 = false;
			}

			if (resetkey2) {
				MPI_Waitall(request_key2.size(), request_key2.data(), MPI_STATUSES_IGNORE);
				resetkey2 = false;
			}
		}

		if (rank == 0) MPI_Wait(&ibcast_request, MPI_STATUS_IGNORE);
	}

	// compute the last values
	for(long i = 0; i < nkeys; i++) {
		for(long j = 0; j < nkeys; j++) {
			if (i == j) continue;

			if (myKeys.contains(i)) {
				if (map[i] > 0) {
					if (!myKeys.contains(j) && (map[j] >= SIZE)) {
						count = map[j] - SIZE;

						while (true) {
							MPI_Recv(&map.data()[j], 1, MPI_LONG, MPI_ANY_SOURCE, j, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

							if (count >= (SIZE - map[j])) count -= (SIZE - map[j]);
							else break;
						}
					
						map[j] += count;
					}

					if (map[j] > 0)
						V[i] += compute(map[i], map[j], i, j);
				}
			}

			if (myKeys.contains(j)) {
				if (map[j] > 0) {
					if (!myKeys.contains(i) && (map[i] >= SIZE)) {
						count = map[i] - SIZE;

						while (true) {
							MPI_Recv(&map.data()[i], 1, MPI_LONG, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

							if (count >= (SIZE - map[i])) count -= (SIZE - map[i]);
							else break;
						}
					
						map[i] += count;
					}

					if (map[i] > 0)
						V[j] += compute(map[j], map[i], j, i);
				}
			}
		}
	}

	float *results = nullptr;
	int *recvcounts = nullptr;
	int *displs = nullptr;

	if (rank == 0) {
		results = new float[nkeys];
		recvcounts = new int[size];
		displs = new int[size];

		int displ = 0;

		for (int i = 0; i < size; i++) {
			recvcounts[i] = block_size;

			if (r > 0) {
				recvcounts[i]++;
				r--;
			}

			displs[i] = displ;
			displ += recvcounts[i];
		}
	}
	
	std::vector<float> myResults(V.size());

    for (int i = lower, j = 0; i < upper; i++, j++)
		myResults[j] = V[i];

	MPI_Gatherv(myResults.data(), myResults.size(), MPI_FLOAT, results, recvcounts, displs, MPI_FLOAT, 0, MPI_COMM_WORLD);

	// Measure the current time
	double end = MPI_Wtime();

	MPI_Finalize();
	
	if (rank == 0) {
		std::printf("Total time: %f (S)\n", end-start);

		// printing the results
		if (print)
			for(long i = 0; i < nkeys; i++)
	 			std::printf("key %ld : %f\n", i, results[i]);
		
		delete[] results;
		delete[] recvcounts;
		delete[] displs;
	}
}