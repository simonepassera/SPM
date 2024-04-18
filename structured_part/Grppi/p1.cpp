#include <iostream>
#include <vector>
#include <functional>
#include <optional>
#include <grppi/common/patterns.h>

#include <grppi/pipeline.h>
#include <dyn/dynamic_execution.h>

using namespace std;
using namespace std::literals::chrono_literals;

#include "utimer.hpp"

int main(int argc, char * argv[]) {

  if(argc!=2) {
    cout << "Usage is: " << argv[0] << " m (number of items in the stream) " << endl;
    return(-1);
  }
  auto m = atoi(argv[1]);
  
  grppi::dynamic_execution seq = grppi::sequential_execution{};
  grppi::dynamic_execution thr = grppi::parallel_execution_native{};
  grppi::dynamic_execution omp = grppi::parallel_execution_omp{};
  {
    utimer t("Pipeline execution"); 
    grppi::pipeline(thr,                                      // execution engine: native threads here

		                                              // stream generator stage
		    [m]() -> optional<int> {    // this is a function with no parameters, m is taken by value 
		      static int x = 0;                       // count of tasks to be returned
		      if (x<m) return x++;                    // if not finished, than return current count and increase it
		      else return {};                         // else return end of stream
		    },
		    
		                                              // stream collapser stage
		    [] (int x) { cout << x << endl; }         // simply print what it gets

		    );                                        // end of the pipeline parameters, end of decl, executions starts
  }
  
  return(0);
}
