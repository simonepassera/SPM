CXX                = g++ -std=c++20
MPICXX			   = mpicxx -std=c++20
OPTFLAGS	   = -O3
CXXFLAGS          += -Wall 
INCLUDES	   = 
LIBS               = 
SOURCES            = $(wildcard *.cpp)
TARGET             = $(SOURCES:.cpp=)

.PHONY: all clean cleanall 

%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $< $(LIBS)

%-mpi: %-mpi.cpp
	$(MPICXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $< $(LIBS)

%-mpi+omp: %-mpi+omp.cpp
	$(MPICXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -fopenmp -o $@ $< $(LIBS)

all: $(TARGET)

clean: 
	-rm -fr *.o *~
cleanall: clean
	-rm -fr $(TARGET)



