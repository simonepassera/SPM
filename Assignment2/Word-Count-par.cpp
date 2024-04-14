#include <omp.h>
#include <cstring>
#include <vector>
#include <set>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <numeric>

using umap = std::unordered_map<std::string, uint64_t>;
using pair = std::pair<std::string, uint64_t>;

struct Comp {
	bool operator()(const pair& p1, const pair& p2) const {
		return p1.second > p2.second;
	}
};

using ranking = std::multiset<pair, Comp>;

// ------ globals --------
uint64_t total_words{0};
volatile uint64_t extraworkXline{0};

void tokenize_line(const std::string& line, umap* um, omp_lock_t* filelock, int num_lines) {
	char *tmpstr;
	char *token = strtok_r(const_cast<char*>(line.c_str()), " \r\n", &tmpstr);

	while(token) {
		omp_set_lock(filelock);
		
		um->operator[](std::string(token))++;
		total_words++;

		omp_unset_lock(filelock);

		token = strtok_r(NULL, " \r\n", &tmpstr);
	}

	for(volatile uint64_t j{0}; j < (extraworkXline * num_lines); j++);
}

void compute_file(const std::string& filename, umap* um, int num_lines) {
	omp_lock_t filelock;
	omp_init_lock(&filelock);

	std::ifstream file(filename, std::ios_base::in);

	if (file.is_open()) {
		std::string line;
		std::string multiple_lines;
		int l = num_lines;

		while(!file.eof()) {
			while((l != 0) && !std::getline(file, line).eof())
				if (!line.empty()) {
					multiple_lines += " " + line;
					l--;
				}
			
			#pragma omp task firstprivate(multiple_lines, um, num_lines) shared(filelock)
			{
				tokenize_line(multiple_lines, um, &filelock, num_lines);
			}

			l = num_lines;
			multiple_lines.clear();
		}
	} 

	file.close();

	#pragma omp taskwait
	omp_destroy_lock(&filelock);
}

int main(int argc, char *argv[]) {
	auto usage_and_exit = [argv] () {
		std::printf("use: %s filelist.txt [threads] [lines] [extraworkXline] [topk] [showresults]\n", argv[0]);
		std::printf("     filelist.txt contains one txt filename per line\n");
		std::printf("     threads is the number of threads used, its default value is the maximum number of logical cores\n");
		std::printf("     lines is the maximum number of rows processed per thread, its default value is 1\n");
		std::printf("     extraworkXline is the extra work done for each line, it is an integer value whose default is 0\n");
		std::printf("     topk is an integer number, its default value is 10 (top 10 words)\n");
		std::printf("     showresults is 0 or 1, if 1 the output is shown on the standard output\n\n");
		exit(-1);
	};

	std::vector<std::string> filenames;
	uint64_t num_threads = omp_get_max_threads();
	uint64_t num_lines = 1;
	size_t topk = 10;
	bool showresults = false;

	if (argc < 2 || argc > 7) {
		usage_and_exit();
	}

	if (argc > 2) {
		try {
			num_threads = std::stoul(argv[2]);
		} catch(std::invalid_argument const& ex) {
			std::printf("threads: (%s) is an invalid number (%s)\n", argv[2], ex.what());
			return -1;
		}

		if (num_threads == 0) {
			std::printf("threads: (%s) must be a positive integer\n", argv[2]);
			return -1;
		}

		if (argc > 3) {
			try {
				num_lines = std::stoul(argv[3]);
			} catch(std::invalid_argument const& ex) {
				std::printf("lines: (%s) is an invalid number (%s)\n", argv[3], ex.what());
				return -1;
			}

			if (num_lines == 0) {
				std::printf("lines: (%s) must be a positive integer\n", argv[3]);
				return -1;
			}

			if (argc > 4) {
				try {
					extraworkXline = std::stoul(argv[4]);
				} catch(std::invalid_argument const& ex) {
					std::printf("extraworkXline: (%s) is an invalid number (%s)\n", argv[4], ex.what());
					return -1;
				}

				if (argc > 5) {
					try {
						topk = std::stoul(argv[5]);
					} catch(std::invalid_argument const& ex) {
						std::printf("topk: (%s) is an invalid number (%s)\n", argv[5], ex.what());
						return -1;
					}

					if (topk == 0) {
						std::printf("topk: (%s) must be a positive integer\n", argv[5]);
						return -1;
					}

					if (argc == 7) {
						int tmp;

						try {
							tmp = std::stol(argv[6]);
						} catch(std::invalid_argument const& ex) {
							std::printf("showresults: (%s) is an invalid number (%s)\n", argv[6], ex.what());
							return -1;
						}

						if (tmp == 1) showresults = true;
					}
				}
			}
		}
	}
	
	if (std::filesystem::is_regular_file(argv[1])) {
		std::ifstream file(argv[1], std::ios_base::in);

		if (file.is_open()) {
			std::string line;

			while(std::getline(file, line)) {
				if (std::filesystem::is_regular_file(line))
					filenames.push_back(line);
				else
					std::cout << line << " is not a regular file, skipt it\n";
			}					
		} else {
			std::printf("ERROR: opening file %s\n", argv[1]);
			return -1;
		}

		file.close();
	} else {
		std::printf("%s is not a regular file\n", argv[1]);
		usage_and_exit();
	}

	// used for storing results
	std::vector<umap*> umap_vec;
	umap_vec.reserve(filenames.size());

	// start the time
	auto start = omp_get_wtime();

	#pragma omp parallel num_threads(num_threads) shared(filenames, umap_vec, num_lines)
	{
		#pragma omp master
		{
			for (auto f : filenames) {
				umap *um = new umap();
				
				umap_vec.push_back(um);
				compute_file(f, um, num_lines);
			}
		}
	}

	auto stop1 = omp_get_wtime();

	umap umap_results;

	for (umap *um : umap_vec)
		umap_results = std::accumulate(um->begin(), um->end(), umap_results, [] (umap &m, const pair &p) {return (m[p.first] += p.second, m);});
	
	// sorting in descending order
	ranking rank;

	rank.insert(umap_results.begin(), umap_results.end());

	auto stop2 = omp_get_wtime();

	std::printf("Compute time (s) %f\nSorting time (s) %f\n", stop1 - start, stop2 - stop1);
	
	if (showresults) {
		// show the results
		std::cout << "Unique words " << rank.size() << "\n";
		std::cout << "Total words  " << total_words << "\n";
		std::cout << "Top " << topk << " words:\n";
		
		auto top = rank.begin();
		for (size_t i=0; i < std::clamp(topk, 1ul, rank.size()); ++i)
			std::cout << top->first << '\t' << top++->second << '\n';
	}

	for (umap *um : umap_vec)
		delete um;
}
	
