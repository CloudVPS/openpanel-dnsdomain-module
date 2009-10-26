include makeinclude

OBJ	= main.o version.o

all: dnsdomainmodule.exe module.xml
	grace mkapp dnsdomainmodule 

version.cpp:
	grace mkversion version.cpp

module.xml: module.def
	mkmodulexml < module.def > module.xml

dnsdomainmodule.exe: $(OBJ) module.xml
	./addflavor.sh
	$(LD) $(LDFLAGS) -o dnsdomainmodule.exe $(OBJ) $(LIBS) \
	../opencore/api/c++/lib/libcoremodule.a

clean:
	rm -f *.o *.exe
	rm -rf dnsdomainmodule.app
	rm -f dnsdomainmodule

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I../opencore/api/c++/include -c -g $<
