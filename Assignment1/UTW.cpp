//
// First SPM Assignment a.a. 23/24.
//
// compile:
// g++ -std=c++20 -O3 -march=native -I<path-to-include> UTW.cpp -o UTW
//

#include <iostream>
#include <vector>
#include <random>
#include <thread>
#include <barrier>
#include <hpc_helpers.hpp>

int random(const int &min, const int &max) {
	static std::mt19937 generator(117);
	std::uniform_int_distribution<int> distribution(min, max);
	return distribution(generator);
};		

// emulate some work
void work(std::chrono::microseconds w) {
	auto end = std::chrono::steady_clock::now() + w;
    while(std::chrono::steady_clock::now() < end);	
}

// Sequential code
void wavefront(const std::vector<int> &M, const uint64_t &N) {
	// for each upper diagonal
	for(uint64_t k = 0; k < N; ++k) {
		// for each elem. in the diagonal
		for(uint64_t i = 0; i < (N-k); ++i) {
			work(std::chrono::microseconds(M[i*N+(i+k)])); 
		}
	}
}

// Parallel code (Static block-cyclic distribution)
void wavefront_parallel_static(const std::vector<int> &M, uint64_t N, std::barrier<std::__empty_completion> &sync_point, uint64_t num_threads, uint64_t chunk_size) {
	auto block_cyclic = [&] (const uint64_t id) -> void {
        // precompute offset, stride, and number of diagonals
        const uint64_t off = id * chunk_size;
        const uint64_t str = num_threads * chunk_size;
		const uint64_t num_diagonals = N - off;

		// for each upper diagonal
		for(uint64_t k = 0; k < num_diagonals; k++) {
			// for each block of size chunk_size in cyclic order
			for (uint64_t lower = off; lower < (N-k); lower += str) {
				// compute the upper border of the block (exclusive)
				const uint64_t upper = std::min(lower+chunk_size, N-k);

				// compute task
				for (u_int64_t i = lower; i < upper; i++) {
					work(std::chrono::microseconds(M[i*N + (i+k)]));  
				}
			}
			
			// barrier
			if (k == (num_diagonals-1))
				sync_point.arrive_and_drop();
			else
				sync_point.arrive_and_wait();
		}
    };

    std::vector<std::thread> threads;

	std::cout << "Number of threads (wavefront_parallel_static) -> " << num_threads << std::endl;

	// spawn threads
    for (uint64_t id = 0; id < num_threads; id++)
        threads.emplace_back(block_cyclic, id);

    for (auto &thread : threads)
        thread.join();
}

int main(int argc, char *argv[]) {
	int min    			 = 0;    // default minimum time (in microseconds)
	int max    			 = 1000; // default maximum time (in microseconds)
	uint64_t N 			 = 512;  // default size of the matrix (NxN)
	uint64_t num_threads = 40;	 // default number of threads 
	uint64_t chunk_size  = 2;	 // default chunk size
	
	if (argc != 1 && argc != 2 && argc != 6) {
		std::cout << "use: " << argv[0] << " N [min max threads chunk]" << std::endl;
		std::cout << "     N size of the square matrix" << std::endl;
		std::cout << "     min waiting time (us)" << std::endl;
		std::cout << "     max waiting time (us)" << std::endl;
		std::cout << "     threads max" << std::endl;
		std::cout << "     chunk size" << std::endl;

		return EXIT_FAILURE;
	}

	if (argc > 1) {
		N = std::stol(argv[1]);
		
		if (argc > 2) {
			min = std::stol(argv[2]);
			max = std::stol(argv[3]);
			num_threads = std::stol(argv[4]);
			chunk_size = std::stol(argv[5]);
		}
	}

	num_threads = std::min(num_threads, N/chunk_size + (N%chunk_size ? 1 : 0));

	// allocate the matrix
	std::vector<int> M(N*N, -1);

	// create barrier
	std::barrier sync_point(num_threads);

	uint64_t expected_totaltime = 0;

	// init function
	auto init=[&]() {
		for(uint64_t k = 0; k < N; k++) {  
			for(uint64_t i = 0; i < (N-k); i++) {  
				int t = random(min,max);
				M[i*N + (i+k)] = t;
				expected_totaltime += t;				
			}
		}
	};
	
	init();

	std::printf("Estimated compute time ~ %f (ms)\n", expected_totaltime/1000.0);

	TIMERSTART(wavefront_sequential);
	wavefront(M, N);
    TIMERSTOP(wavefront_sequential);
	
	TIMERSTART(wavefront_parallel_static);
	wavefront_parallel_static(M, N, sync_point, num_threads, chunk_size);
    TIMERSTOP(wavefront_parallel_static);

    return EXIT_SUCCESS;
}
