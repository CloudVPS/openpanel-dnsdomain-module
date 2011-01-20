# This file is part of OpenPanel - The Open Source Control Panel
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

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
	/usr/lib/openpanel-core/libcoremodule.a

install:
	mkdir -p ${DESTDIR}/var/openpanel/modules/DNSDomain.module
	mkdir -p ${DESTDIR}/var/openpanel/conf/staging/DNSDomain
	cp -rf ./dnsdomainmodule.app    ${DESTDIR}/var/openpanel/modules/DNSDomain.module/
	ln -sf dnsdomainmodule.app/exec ${DESTDIR}/var/openpanel/modules/DNSDomain.module/action
	cp     module.xml          ${DESTDIR}/var/openpanel/modules/DNSDomain.module/module.xml
	install -m 755 verify      ${DESTDIR}/var/openpanel/modules/DNSDomain.module/verify
	cp *.html techsupport.* ${DESTDIR}/var/openpanel/modules/DNSDomain.module

clean:
	rm -f *.o *.exe
	rm -rf dnsdomainmodule.app
	rm -f dnsdomainmodule

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I/usr/include/opencore -c -g $<
