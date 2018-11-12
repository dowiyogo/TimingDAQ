CXX = $(shell root-config --cxx)
LD = $(shell root-config --ld)
INC = $(shell pwd)

CPPFLAGS := $(shell root-config --cflags) -I$(INC)/include -Wl -rpath $(shell root-config --prefix)/lib
LDFLAGS := $(shell root-config --glibs)
#CPPFLAGS += -g -std=c++14
CPPFLAGS += -g

TARGETS = VMEDat2Root DRSDat2Root DRSclDat2Root NetScopeDat2Root NetScopeStandaloneDat2Root ETL_ASIC_Dat2Root
SRC = src/Configuration.cc src/Interpolator.cc src/DatAnalyzer.cc

all : $(TARGETS)

$(TARGETS) : %Dat2Root : $(SRC:.cc=.o) src/%Analyzer.o app/%Dat2Root.cc
	@echo Building $@
	$(LD) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.cc
	@echo $@
	$(CXX) $(CPPFLAGS) -o $@ -c $<
clean :
	rm -rf *.o app/*.o src/*.o $(TARGETS) *~ *.dSYM
