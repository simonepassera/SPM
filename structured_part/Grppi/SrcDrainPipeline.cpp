#include <iostream>
#include <vector>
#include <functional>
#include <optional>
#include <grppi/common/patterns.h>

#include <grppi/pipeline.h>
#include <grppi/farm.h>
#include <grppi/dyn/dynamic_execution.h>

using namespace std;
using namespace std::literals::chrono_literals;

#include "utimer.hpp"


int main(int argc, char * argv[]) {

  std::chrono::milliseconds temit, tinc1, tinc2, tdrain;
  long taskNo;
  long nw; 
  
  if(argc == 1)  {
    temit  = 10ms;
    tinc1  = 20ms;
    tinc2  = 40ms;
    tdrain =  1ms;
    taskNo = 8;
    nw = 4;
  } else {
    temit = std::chrono::milliseconds(atoi(argv[1]));
    tinc1 = std::chrono::milliseconds(atoi(argv[2]));
    tinc2 = std::chrono::milliseconds(atoi(argv[3]));
    tdrain= std::chrono::milliseconds(atoi(argv[4]));
    taskNo = atoi(argv[5]);
    nw = atoi(argv[6]);
  }

  std::cout << "Executing " << taskNo << " tasks " << nw <<  " workers (times are " << temit.count() << ", "
	    << tinc1.count() << ", " << tinc2.count() << ", " << tdrain.count() << ")" << std::endl;

  auto m = taskNo;

  auto active_wait = [] (std::chrono::milliseconds ms) {
		       long msecs = ms.count();
		       auto start = std::chrono::high_resolution_clock::now();
		       auto end   = false;
		       while(!end) {
			 auto elapsed = std::chrono::high_resolution_clock::now() - start;
			 auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
			 if(msec>msecs)
			   end = true;
		       }
		       return;
		     };
  
  grppi::dynamic_execution seq = grppi::sequential_execution{};
  grppi::dynamic_execution thr = grppi::parallel_execution_native{nw};
  grppi::dynamic_execution omp = grppi::parallel_execution_omp{};


  {
    utimer t("Pipeline execution"); 
    grppi::pipeline(thr,                                                            // execution engine: native threads here

		                                                                    // stream generator stage
		    [m,temit,&active_wait]() -> optional<int> {                     // this is a function with no parameters, m is taken by value 
		      static int x = 0;                                             // count of tasks to be returned
		      active_wait(temit);
		      if (x<m) return x++;                                          // if not finished, than return current count and increase it
		      else return {};                                               // else return end of stream
		    },

		    [&] (int n) { active_wait(tinc1); return (n+1); },              // first compute stage
	            [&] (int n) { active_wait(tinc2); return (n+1); },              // second compute stage
		    
		                                                                    // stream collapser stage
		    [&] (int x) { active_wait(tdrain); cout << x << endl; }         // simply print what it gets

		    );                                        // end of the pipeline parameters, end of decl, executions starts
  }
  
  return(0);
}
