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

void tokenize_line(const std::string* line, std::vector<umap>* umap_results) {
	char *tmpstr;
	char *token = strtok_r(const_cast<char*>(line->c_str()), " \r\n", &tmpstr);

	while(token) {
		(*umap_results)[omp_get_thread_num()][std::string(token)]++;
		token = strtok_r(NULL, " \r\n", &tmpstr);

		#pragma omp atomic
		total_words++;
	}

	for(volatile uint64_t j{0}; j < extraworkXline; j++);
}

void compute_file(const std::string& filename, std::vector<umap>* umap_results, uint64_t num_lines) {
	std::ifstream file(filename, std::ios_base::in);

	if (file.is_open()) {
		std::vector<std::string*> *lines;
		std::string *line;
		int l;

		while(!file.eof()) {
			lines = new std::vector<std::string*>();
			lines->reserve(num_lines);
			l = num_lines;

			while(l != 0) {
				line = new std::string();

				if (std::getline(file, *line).eof()) {
					delete line;
					break;
				}

				if (!line->empty()) {
					lines->push_back(line);
					l--;
				} else {
					delete line;
				}
			}
			
			#pragma omp task default(none) firstprivate(lines, umap_results)
			{
				for (auto i = lines->begin(); i != lines->end(); i++) {
					tokenize_line(*i, umap_results);
					delete *i;
				}

				delete lines;
			}
		}
	} 

	file.close();
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
	std::vector<umap> *umap_results = new std::vector<umap>(num_threads);

	// start the time
	auto start = omp_get_wtime();

	#pragma omp parallel master num_threads(num_threads) default(shared)
	{
		#pragma omp taskloop default(shared)
		for (auto f : filenames)
			compute_file(f, umap_results, num_lines);
	}

	auto stop1 = omp_get_wtime();

	if (num_threads > 1) {
		for (int id = 1; id < num_threads; id++)
			for (auto i = (*umap_results)[id].begin(); i != (*umap_results)[id].end(); i++)
				(*umap_results)[0][i->first] += i->second;
	}

	// sorting in descending order
	ranking rank;
	rank.insert((*umap_results)[0].begin(), (*umap_results)[0].end());

	delete umap_results;

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
}