/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ***************************************************************************
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as 
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  As a special exception, you may use this file as part of a free software
 *  library without restriction.  Specifically, if other files instantiate
 *  templates or use macros or inline functions from this file, or you compile
 *  this file and link it with other files to produce an executable, this
 *  file does not by itself cause the resulting executable to be covered by
 *  the GNU General Public License.  This exception does not however
 *  invalidate any other reasons why the executable file might be covered by
 *  the GNU General Public License.
 *
 ****************************************************************************
 */
/* 
 * Author: Massimo Torquati <torquati@di.unipi.it> 
 * Date:   October 2014
 */
/* This program prints to the STDOUT all prime numbers 
 * in the range (n1,n2) where n1 and n2 are command line 
 * parameters.
 *
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ff/utils.hpp>

using ull = unsigned long long;

// see http://en.wikipedia.org/wiki/Primality_test
static bool is_prime(ull n) {
    if (n <= 3)  return n > 1; // 1 is not prime !
    
    if (n % 2 == 0 || n % 3 == 0) return false;

    for (ull i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) 
            return false;
    }
    return true;
}

int main(int argc, char *argv[]) {    
    if (argc<3) {
        std::cerr << "use: " << argv[0]  << " number1 number2 [print=off|on]\n";
        return -1;
    }
    ull n1          = std::stoll(argv[1]);
    ull n2          = std::stoll(argv[2]);  
    bool print_primes = false;
    if (argc >= 4)  print_primes = (std::string(argv[3]) == "on");

    ff::ffTime(ff::START_TIME);
    std::vector<ull> results;
    results.reserve((size_t)(n2-n1)/log(n1)); // estimation of how many primes in (n1,n2), iff (n2-b1) >> log(n1)
    ull prime;

    while( (prime=n1++) <= n2 ) 
        if (is_prime(prime)) results.push_back(prime);

    const size_t n = results.size();
    std::cout << "Found " << n << " primes\n";
    ff::ffTime(ff::STOP_TIME);

    if (print_primes) {
        for(size_t i=0;i<n;++i) 
            std::cout << results[i] << " ";
        std::cout << "\n\n";
    }
    std::cout << "Time: " << ff::ffTime(ff::GET_TIME) << " (ms)\n";
    return 0;
}
