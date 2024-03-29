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
#include <atomic>
#include <cassert>
#include <hpc_helpers.hpp>

#ifdef TEST
	#define TEST_VAR ,expected_totaltime
	#define TEST_ARG ,uint64_t expected_totaltime
#else
	#define TEST_VAR
	#define TEST_ARG
#endif

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
void wavefront_sequential(const std::vector<int> &M, const uint64_t &N) {
	// for each upper diagonal
	for(uint64_t k = 0; k < N; k++)
		// for each elem. in the diagonal
		for(uint64_t i = 0; i < (N-k); i++)
			work(std::chrono::microseconds(M[i*N + (i+k)]));
}

// Parallel code (Static block-cyclic distribution)
void wavefront_parallel_static(const std::vector<int> &M, uint64_t N, uint64_t num_threads, uint64_t chunk_size TEST_ARG) {
	num_threads = std::min(num_threads, N/chunk_size + (N%chunk_size ? 1 : 0));
	std::barrier sync_point(num_threads);
	
	#ifdef TEST
		std::vector<uint64_t> test_sum (num_threads, 0);
	#endif

	auto block_cyclic = [&] (const uint64_t id) -> void {
        // precompute offset, stride, and number of diagonals
        const uint64_t off = id * chunk_size;
        const uint64_t str = num_threads * chunk_size;
		const uint64_t num_diagonals = N-off;

		// for each upper diagonal
		for(uint64_t k = 0; k < num_diagonals; k++) {
			// for each block of size chunk_size in cyclic order
			for (uint64_t lower = off; lower < (N-k); lower += str) {
				// compute the upper border of the block (exclusive)
				const uint64_t upper = std::min(lower+chunk_size, N-k);

				// compute task
				for (u_int64_t i = lower; i < upper; i++) {
					work(std::chrono::microseconds(M[i*N + (i+k)]));
					
					#ifdef TEST
						test_sum[id] += M[i*N + (i+k)];
					#endif
				}
			}
			
			// barrier
			if (k == (num_diagonals-1))
				sync_point.arrive_and_drop();
			else
				sync_point.arrive_and_wait();
		}
    };

	std::cout << "Number of threads (wavefront_parallel_static) -> " << num_threads << std::endl;

	std::vector<std::thread> threads;

	// spawn threads
    for (uint64_t id = 0; id < num_threads; id++)
        threads.emplace_back(block_cyclic, id);

    for (auto &thread : threads)
        thread.join();

	#ifdef TEST
		uint64_t sum = 0;

		for (auto &time : test_sum)
			sum += time;

		assert(sum == expected_totaltime);
	#endif
}

// Parallel code (Dynamic distribution)
void wavefront_parallel_dynamic(const std::vector<int> &M, uint64_t N, uint64_t num_threads, uint64_t chunk_size TEST_ARG) {
	num_threads = std::min(num_threads, N/chunk_size + (N%chunk_size ? 1 : 0)); 
	uint64_t global_max_num_threads = N/chunk_size + (N%chunk_size ? 1 : 0);
	uint64_t global_diagonal = 0;
	std::atomic<uint64_t> global_current_num_threads {num_threads};
	std::atomic<uint64_t> global_lower {0};

	auto on_completion = [&global_diagonal, &global_lower, &global_max_num_threads, N, chunk_size] {
		global_diagonal++;
		global_max_num_threads = (N-global_diagonal)/chunk_size + ((N-global_diagonal)%chunk_size ? 1 : 0);
		global_lower.store(0);
	};

	std::barrier sync_point(num_threads, on_completion);

	#ifdef TEST
		std::vector<uint64_t> test_sum (num_threads, 0);
	#endif

	auto task = [&] (const uint64_t id) -> void {
		uint64_t lower;
		uint64_t current_num_threads;

		while (true) {
			// fetch and add current global lower
			lower = global_lower.fetch_add(chunk_size);

			if (lower < (N-global_diagonal)) {
				// compute the upper border of the block (exclusive)
				const uint64_t upper = std::min(lower+chunk_size, N-global_diagonal);

				// compute task
				for (u_int64_t i = lower; i < upper; i++) {
					work(std::chrono::microseconds(M[i*N + (i+global_diagonal)]));

					#ifdef TEST
						test_sum[id] += M[i*N + (i+global_diagonal)];
					#endif
				}
			} else {
				current_num_threads = global_current_num_threads.load(std::memory_order_acquire);

				while (current_num_threads > global_max_num_threads) {
					if (global_current_num_threads.compare_exchange_weak(current_num_threads, current_num_threads-1, std::memory_order_release, std::memory_order_acquire)) {
						// barrier
						if (global_current_num_threads != 0)
							sync_point.arrive_and_drop();
						
						return;
					}
				}
				
				// barrier
				sync_point.arrive_and_wait();
			}
		}
    };

	std::cout << "Number of threads (wavefront_parallel_dynamic) -> " << num_threads << std::endl;

	std::vector<std::thread> threads;

	// spawn threads
    for (uint64_t id = 0; id < num_threads; id++)
        threads.emplace_back(task, id);

    for (auto &thread : threads)
        thread.join();

	#ifdef TEST
		uint64_t sum = 0;

		for (auto &time : test_sum)
			sum += time;

		assert(sum == expected_totaltime);
	#endif
}

int main(int argc, char *argv[]) {
	int min    			 = 0;    // default minimum time (in microseconds)
	int max    			 = 1000; // default maximum time (in microseconds)
	uint64_t N 			 = 512;  // default size of the matrix (NxN)
	uint64_t num_threads = 40;	 // default number of threads 
	uint64_t chunk_size  = 1;	 // default chunk size
	
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

	// allocate the matrix
	std::vector<int> M(N*N, -1);

	uint64_t expected_totaltime = 0;

	// init function
	auto init = [&] () {
		for(uint64_t k = 0; k < N; k++) {  
			for(uint64_t i = 0; i < (N-k); i++) {  
				int t = random(min, max);
				M[i*N + (i+k)] = t;
				expected_totaltime += t;				
			}
		}
	};
	
	init();

	std::printf("Estimated compute time ~ %f (s)\n", expected_totaltime/1000000.0);

	TIMERSTART(wavefront_sequential);
	wavefront_sequential(M, N);
    TIMERSTOP(wavefront_sequential);
	
	TIMERSTART(wavefront_parallel_static);
	wavefront_parallel_static(M, N, num_threads, chunk_size TEST_VAR);
    TIMERSTOP(wavefront_parallel_static);

	TIMERSTART(wavefront_parallel_dynamic);
	wavefront_parallel_dynamic(M, N, num_threads, chunk_size TEST_VAR);
    TIMERSTOP(wavefront_parallel_dynamic);

    return EXIT_SUCCESS;
}
