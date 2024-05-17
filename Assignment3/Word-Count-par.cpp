//
// Third SPM Assignment a.a. 23/24.
//
// compile:
// g++ -std=c++17 -O3 -march=native -I ./fastflow -DNO_DEFAULT_MAPPING Word-Count-par.cpp -o Word-Count-par
//

#include <cstring>
#include <vector>
#include <set>
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <ff/ff.hpp>

using namespace ff;

using umap = std::unordered_map<std::string, uint64_t>;
using pair = std::pair<std::string, uint64_t>;

struct Comp {
	bool operator()(const pair& p1, const pair& p2) const {
		return p1.second > p2.second;
	}
};

using ranking = std::multiset<pair, Comp>;

// ------ globals --------
std::atomic<uint64_t> total_words {0};
volatile uint64_t extraworkXline {0};

struct Reader : ff_monode_t<std::vector<std::string>> {
    Reader(const std::vector<std::string> &filenames_, const uint64_t num_lines_, const uint64_t left_workers_) : filenames(filenames_), num_lines(num_lines_), left_workers(left_workers_) {}

    std::vector<std::string>* svc(std::vector<std::string>*) {
		uint64_t id = get_my_id();
		uint64_t block_size = filenames.size() / left_workers;
		uint64_t r = filenames.size() % left_workers;

		long lower = (id * block_size) + (id < r ? id : r);
		long upper = lower + block_size + (id < r ? 1 : 0);

		for (uint64_t i = lower; i < upper; i++) {
			std::ifstream file(filenames[i], std::ios_base::in);

			if (file.is_open()) {
				std::vector<std::string> *lines;
				std::string line;
				int l;

				while(!file.eof()) {
					lines = new std::vector<std::string>();
					lines->reserve(num_lines);
					l = num_lines;

					while((l != 0) && !std::getline(file, line).eof())
						if (!line.empty()) {
							lines->emplace_back(line);
							l--;
						}

					ff_send_out(lines);
				}
			}

			file.close();
		}

		return EOS;
	}
	
    const std::vector<std::string> &filenames;
	const uint64_t num_lines;
	const uint64_t left_workers;
};

struct Tokenize : ff_minode_t<std::vector<std::string>> {
	Tokenize(umap &um_) : um(um_) {}

    std::vector<std::string>* svc(std::vector<std::string>* lines) {
		for (auto l = lines->begin(); l != lines->end(); l++) {
			char *tmpstr;
			char *token = strtok_r(const_cast<char*>(l->c_str()), " \r\n", &tmpstr);

			while(token) {
				um[std::string(token)]++;
				token = strtok_r(NULL, " \r\n", &tmpstr);

				total_words++;
			}

			for(volatile uint64_t j {0}; j < extraworkXline; j++);
		}

		delete lines;
		return GO_ON;
	}

	umap &um;
};

int main(int argc, char *argv[]) {
	auto usage_and_exit = [argv] () {
		std::printf("use: %s filelist.txt [leftWorkers] [rightWorkers] [lines] [extraworkXline] [topk] [showresults]\n", argv[0]);
		std::printf("     filelist.txt contains one txt filename per line\n");
		std::printf("     leftWorkers is the number of left workers used, its default value is 1\n");
		std::printf("     rightWorkers is the number of right workers used, its default value is the maximum number of logical cores\n");
		std::printf("     lines is the maximum number of rows processed per thread, its default value is 1\n");
		std::printf("     extraworkXline is the extra work done for each line, it is an integer value whose default is 0\n");
		std::printf("     topk is an integer number, its default value is 10 (top 10 words)\n");
		std::printf("     showresults is 0 or 1, if 1 the output is shown on the standard output\n\n");
		exit(-1);
	};

	std::vector<std::string> filenames;
	uint64_t left_workers = 1;
	uint64_t right_workers = ff_numCores();
	uint64_t num_lines = 1;
	size_t topk = 10;
	bool showresults = false;

	if (argc < 2 || argc > 8) {
		usage_and_exit();
	}

	if (argc > 2) {
		try {
			left_workers = std::stoul(argv[2]);
		} catch(std::invalid_argument const& ex) {
			std::printf("leftWorkers: (%s) is an invalid number (%s)\n", argv[2], ex.what());
			return -1;
		}

		if (left_workers == 0) {
			std::printf("leftWorkers: (%s) must be a positive integer\n", argv[2]);
			return -1;
		}

		if (argc > 3) {
			try {
				right_workers = std::stoul(argv[3]);
			} catch(std::invalid_argument const& ex) {
				std::printf("rightWorkers: (%s) is an invalid number (%s)\n", argv[3], ex.what());
				return -1;
			}

			if (right_workers == 0) {
				std::printf("rightWorkers: (%s) must be a positive integer\n", argv[3]);
				return -1;
			}

			if (argc > 4) {
				try {
					num_lines = std::stoul(argv[4]);
				} catch(std::invalid_argument const& ex) {
					std::printf("lines: (%s) is an invalid number (%s)\n", argv[4], ex.what());
					return -1;
				}

				if (num_lines == 0) {
					std::printf("lines: (%s) must be a positive integer\n", argv[4]);
					return -1;
				}

				if (argc > 5) {
					try {
						extraworkXline = std::stoul(argv[5]);
					} catch(std::invalid_argument const& ex) {
						std::printf("extraworkXline: (%s) is an invalid number (%s)\n", argv[5], ex.what());
						return -1;
					}

					if (argc > 6) {
						try {
							topk = std::stoul(argv[6]);
						} catch(std::invalid_argument const& ex) {
							std::printf("topk: (%s) is an invalid number (%s)\n", argv[6], ex.what());
							return -1;
						}

						if (topk == 0) {
							std::printf("topk: (%s) must be a positive integer\n", argv[6]);
							return -1;
						}

						if (argc == 8) {
							int tmp;

							try {
								tmp = std::stol(argv[7]);
							} catch(std::invalid_argument const& ex) {
								std::printf("showresults: (%s) is an invalid number (%s)\n", argv[7], ex.what());
								return -1;
							}

							if (tmp == 1) showresults = true;
						}
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

			if (filenames.size() == 0) {
				std::printf("%s is empty!\n", argv[1]);
				return -1;
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

	// left_workers = [1, filenames.size()]
	left_workers = std::min(left_workers, filenames.size());

	// used for storing results
	std::vector<umap> umap_results(right_workers);

	// start the time
	ffTime(START_TIME);

	ff_a2a a2a;

	std::vector<ff_node*> left_w;
    std::vector<ff_node*> right_w;

	for (uint64_t i = 0; i < left_workers; i++)
		left_w.push_back(new Reader(filenames, num_lines, left_workers));

	for (uint64_t i = 0; i < right_workers; i++)
		right_w.push_back(new Tokenize(umap_results[i]));

    a2a.add_firstset(left_w, 1);
    a2a.add_secondset(right_w);
    
    if (a2a.run_and_wait_end() < 0) {
		error("running a2a\n");
		return -1;
    }

	ffTime(STOP_TIME);
	auto time1 = ffTime(GET_TIME) / 1000;

	// start the time
	ffTime(START_TIME);

	if (right_workers > 1) {
		for (uint64_t id = 1; id < right_workers; id++)
			for (auto i = umap_results[id].begin(); i != umap_results[id].end(); i++)
				umap_results[0][i->first] += i->second;
	}

	// sorting in descending order
	ranking rank;
	rank.insert(umap_results[0].begin(), umap_results[0].end());

	ffTime(STOP_TIME);
	auto time2 = ffTime(GET_TIME) / 1000;

	std::printf("Total time (s) %f\nCompute time (s) %f\nSorting time (s) %f\n", time1 + time2, time1, time2);
	
	if (showresults) {
		// show the results
		std::cout << "Unique words " << rank.size() << "\n";
		std::cout << "Total words  " << total_words << "\n";
		std::cout << "Top " << topk << " words:\n";
		
		auto top = rank.begin();
		for (size_t i=0; i < std::clamp(topk, 1ul, rank.size()); ++i)
			std::cout << top->first << '\t' << top++->second << '\n';
	}

	return 0;
}