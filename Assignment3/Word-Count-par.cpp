//
// Third SPM Assignment a.a. 23/24.
//
// compile:
// g++ -std=c++17 -O3 -march=native -pthread -I /home/simo/Documents/SPM/fastflow -Wall -DNDEBUG Word-Count-par.cpp -o Word-Count-par
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
uint64_t total_words {0};
volatile uint64_t extraworkXline {0};

struct Reader : ff_node_t<std::string> {
    Reader(const std::vector<std::string> &filenames_) : filenames(filenames_) {}

    std::string* svc(std::string*) {
		for (std::string f : filenames) {
			std::ifstream file(f, std::ios_base::in);

			if (file.is_open()) {
				std::string line;
		
				while(std::getline(file, line))
					if (!line.empty())
						ff_send_out(new std::string(line));
			}

			file.close();
		}

		return EOS;
	}
	
    const std::vector<std::string> &filenames;
};


struct Tokenize: ff_node_t<std::string> {
	Tokenize(umap &um_) : um(um_) {}

    std::string* svc(std::string* line) {
		char *tmpstr;
		char *token = strtok_r(const_cast<char*>(line->c_str()), " \r\n", &tmpstr);

		while(token) {
			um[std::string(token)]++;
			token = strtok_r(NULL, " \r\n", &tmpstr);

			total_words++;
		}

		for(volatile uint64_t j {0}; j < extraworkXline; j++);

		delete line;

		return GO_ON;
	}

	umap &um;
};

int main(int argc, char *argv[]) {
	auto usage_and_exit = [argv] () {
		std::printf("use: %s filelist.txt [threads] [lines] [extraworkXline] [topk] [showresults]\n", argv[0]);
		std::printf("     filelist.txt contains one txt filename per line\n");
		std::printf("     workers is the number of workers used, its default value is the maximum number of logical cores\n");
		std::printf("     lines is the maximum number of rows processed per thread, its default value is 1\n");
		std::printf("     extraworkXline is the extra work done for each line, it is an integer value whose default is 0\n");
		std::printf("     topk is an integer number, its default value is 10 (top 10 words)\n");
		std::printf("     showresults is 0 or 1, if 1 the output is shown on the standard output\n\n");
		exit(-1);
	};

	std::vector<std::string> filenames;
	uint64_t num_workers = ff_numCores();
	uint64_t num_lines = 1;
	size_t topk = 10;
	bool showresults = false;

	if (argc < 2 || argc > 7) {
		usage_and_exit();
	}

	if (argc > 2) {
		try {
			num_workers = std::stoul(argv[2]);
		} catch(std::invalid_argument const& ex) {
			std::printf("workers: (%s) is an invalid number (%s)\n", argv[2], ex.what());
			return -1;
		}

		if (num_workers == 0) {
			std::printf("workers: (%s) must be a positive integer\n", argv[2]);
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
	std::vector<umap> umap_results(num_workers);

	Reader emitter(filenames);

    ff_Farm<std::string> farm([&]() {	
								   	    std::vector<std::unique_ptr<ff_node>> w;
								   		for(uint64_t i = 0; i < num_workers; i++)
									   		w.push_back(make_unique<Tokenize>(umap_results[i]));
								   		return w;
							   		 } (),
							   emitter);
	farm.remove_collector();
	farm.set_scheduling_ondemand();

    if (farm.run_and_wait_end() < 0) {
        error("running farm");
        return -1;
    }

	if (num_workers > 1) {
		for (uint64_t id = 1; id < num_workers; id++)
			for (auto i = umap_results[id].begin(); i != umap_results[id].end(); i++)
				umap_results[0][i->first] += i->second;
	}

	// sorting in descending order
	ranking rank;
	rank.insert(umap_results[0].begin(), umap_results[0].end());

	std::printf("Total time (s) %f\n", farm.ffTime());
	
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