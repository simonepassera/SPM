#include <cstdio>
#include <random>
#include <map>
#include <vector>
#include <sys/time.h>

const long SIZE = 64;

double diffmsec(const struct timeval &a, const struct timeval &b) {
	long sec  = (a.tv_sec  - b.tv_sec);
    long usec = (a.tv_usec - b.tv_usec);
    
    if(usec < 0) {
        sec--;
        usec += 1000000;
    }

    return ((double)(sec*1000)+ (double)usec/1000.0);
}

long random(const int &min, const int &max) {
	static std::mt19937 generator(117);
	std::uniform_int_distribution<long> distribution(min,max);
	return distribution(generator);
}	

void init(auto& M, const long c1, const long c2, const long key) {
	for(long i = 0; i < c1; i++)
		for(long j = 0; j < c2; j++)
			M[i][j] = (key-i-j)/static_cast<float>(SIZE);
}

// matrix multiplication:  C = A x B  A[c1][c2] B[c2][c1] C[c1][c1]
// mm returns the sum of the elements of the C matrix
auto mm(const auto& A, const auto& B, const long c1, const long c2) {
	float sum{0};
	
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

// initialize two matrices with the computed values of the keys
// and execute a matrix multiplication between the two matrices
// to obtain the sum of the elements of the result matrix 
float compute(const long c1, const long c2, long key1, long key2) {
	std::vector<std::vector<float>> A(c1, std::vector<float>(c2,0.0));
	std::vector<std::vector<float>> B(c2, std::vector<float>(c1,0.0));

	init(A, c1, c2, key1);
	init(B, c2, c1, key2);
	auto r = mm(A,B, c1,c2);
	return r;
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		std::printf("use: %s nkeys length [print(0|1)]\n", argv[0]);
		std::printf("     print: 0 disabled, 1 enabled\n");
		return -1;
	}
	
	long nkeys  = std::stol(argv[1]); // total number of keys
	long length = std::stol(argv[2]); // length is the "stream length", i.e., the number of random key pairs generated
	bool print = false;
	if (argc == 4)
		print = (std::stoi(argv[3]) == 1) ? true : false;
	
	long key1, key2;

	std::map<long, long> map;
	for(long i = 0; i < nkeys; i++) map[i]=0;
	
	std::vector<float> V(nkeys, 0);
	bool resetkey1=false;
	bool resetkey2=false;

	struct timeval wt1, wt0;
	gettimeofday(&wt0, NULL);

	for(int i = 0; i < length; i++) {
		key1 = random(0, nkeys-1);  // value in [0,nkeys[
		key2 = random(0, nkeys-1);  // value in [0,nkeys[
		
		if (key1 == key2) // only distinct values in the pair
			key1 = (key1+1) % nkeys;
		
		map[key1]++;  // count the number of key1 keys
		map[key2]++;  // count the number of key2 keys

		float r1;
		float r2;
		
		// if key1 reaches the SIZE limit, then do the computation and then
		// reset the counter ....
		if (map[key1] == SIZE && map[key2]!=0) {		
			r1= compute(map[key1], map[key2], key1, key2);
			V[key1] += r1;  // sum the partial values for key1
			resetkey1 = true;			
		}

		// if key2 reaches the SIZE limit ....
		if (map[key2] == SIZE && map[key1]!=0) {		
			r2= compute(map[key2], map[key1], key2, key1);
			V[key2] += r2;  // sum the partial values for key1
			resetkey2=true;
		}

		if (resetkey1) {
			// updating the map[key1] initial value before restarting the computation
			auto _r1 = static_cast<unsigned long>(r1) % SIZE;
			map[key1] = (_r1>(SIZE/2)) ? 0 : _r1;
			resetkey1 = false;
		}

		if (resetkey2) {
			// updating the map[key2] initial value before restarting the computation
			auto _r2 = static_cast<unsigned long>(r2) % SIZE;
			map[key2] = (_r2>(SIZE/2)) ? 0 : _r2;
			resetkey2 = false;
		}
	}

	// compute the last values
	for(long i = 0; i < nkeys; i++) {
		for(long j = 0; j < nkeys; j++) {
			if (i==j) continue;

			if (map[i]>0 && map[j]>0) {
				
				auto r1= compute(map[i], map[j], i, j);
				auto r2= compute(map[j], map[i], j, i);
				V[i] += r1;
				V[j] += r2;
			}
		}
	}

	gettimeofday(&wt1, NULL);

	std::printf("Total time: %f (S)\n", diffmsec(wt1, wt0)/1000);

	// printing the results
	if (print)
		for(long i = 0; i < nkeys; i++)
			std::printf("key %ld : %f\n", i, V[i]);
}
