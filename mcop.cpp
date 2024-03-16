#include <iostream>
#include <vector>
#include <limits>

// more info here about MCOP (Matrix Chain Ordering Problem):
// https://www.cs.ucdavis.edu/~bai/ECS122A/Notes/MatrixChain.pdf
//
// compile with g++ -std=c++20 mcop.cpp -o mcop
//

// dimensions:
// M1 7x3 M2 3x8 M3 8x21 M4 21x5 M5 5x27 M6 27x11
const int p[]={ 7,3,8,21,5,27,11 };

// n is given by the length of p
const int n = (sizeof(p)/sizeof(int));

void printF(auto &F, auto n) {
	for(int i=1;i<n;++i) {
		for(int j=1; j<n; ++j) 
			std::printf("%10lu ", F[i][j]);
		std::cout << std::endl;
	}
}
void printS(auto &S, int i, int j) {	
	if (i==j)
		std::printf("M%d", i);
	else {
		std::printf("(");
		printS(S, i, S[i][j]);
		printS(S, S[i][j] + 1, j);
		std::printf(")");
	}
}

int main() {
	
	std::vector<std::vector<uint64_t>> F(n, std::vector<uint64_t>(n,0));
	//  S[i][j] stores the index k at which to split the product M(i) x ... x M(j)
	std::vector<std::vector<uint64_t>> S(n, std::vector<uint64_t>(n,0));
	
	for(int l = 2; l< n; ++l) {
		for(int i= 1; i< (n-l+1); ++i) {
			int j= i + l -1;
			F[i][j] = std::numeric_limits<uint64_t>::max();
			for(int k=i; k<j; ++k) {
				uint64_t q = F[i][k] + F[k+1][j] + p[i-1]*p[k]*p[j];
				if (q < F[i][j]) {
					F[i][j] = q;
					S[i][j] = k; 
				}
			}
		}
	}
	std::cout << "F matrix:\n";
	printF(F, n);
	std::cout << "\n";
	std::cout << "S matrix:\n";
	printF(S, n);
	std::cout << "Optimal computation:\n";
	printS(S, 1, n-1); 
	std::cout << "\n";

}
