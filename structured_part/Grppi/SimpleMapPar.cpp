#include <iostream>
#include <vector>
#include <functional>
#include <chrono>
#include <experimental/optional>
#include <grppi/common/patterns.h>

#include <grppi/map.h>
#include <dyn/dynamic_execution.h>
#include <algorithm>
#include <numeric>

using namespace std;
using namespace std::literals::chrono_literals;

#include "utimer.cpp"

int inc(int x) { return ++x; }
int sq (int x) { return x*x; }
int cni(int x) { return --x; }

template<typename T> function<T(T)> delayFun(function<T(T)> f, auto delay) {
  auto fd = [delay, f](T x) { this_thread::sleep_for(delay); return(f(x)); };
  return fd;
}

int main(int argc, char * argv[]) {

  if(argc != 3) {
    cout << "Usage is " << argv[0] << " m nw " <<endl;
    return(-1);
  }
  auto m = atoi(argv[1]);
  auto nw = atoi(argv[2]); 
  auto delay = 10ms;

  vector<int> x(m);
  std::iota(x.begin(),x.end(),1);
  
  grppi::dynamic_execution seq = grppi::sequential_execution{};
  grppi::dynamic_execution thr = grppi::parallel_execution_native{nw};

  // cout << "Before: ";
  // for(auto &it : x)
    // cout << " " << it;
  // cout << endl;

  {
    utimer t("Map execution"); 
    grppi::map(thr,
	       x.begin(), x.end(),
	       x.begin(),            
	       delayFun<int>(inc,delay) 
	       );
  }

  // cout << "After: ";
  // for(auto &it : x)
    // cout << " " << it;
  // cout << endl;
  
  cout << "Expected computing time was  " << (m) * delay.count() * 1000 / nw << " usec" << endl; 
  
  return(0);
}
