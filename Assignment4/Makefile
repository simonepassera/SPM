CXX			= g++ -std=c++20
MPICXX		= mpicxx -std=c++20
OPTFLAGS	= -O3
CXXFLAGS	+= -Wall 
INCLUDES	= 
LIBS		= -fopenmp
SOURCES		= $(wildcard *.cpp)
TARGET		= $(SOURCES:.cpp=)

.PHONY: all clean cleanall 

%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $<

%-mpi: %-mpi.cpp
	$(MPICXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $<

%-mpi+omp: %-mpi+omp.cpp
	$(MPICXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $< $(LIBS)

all: $(TARGET)

clean: 
	-rm -fr *.o *~
cleanall: clean
	-rm -fr $(TARGET)



