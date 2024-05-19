CPPFLAGS += -Wall -O2
#CPPFLAGS += -Wall -Ofast 
all: cld2gcode #gcode2gcode
modular.o cldmain.o gcodemain.o: modular.h
cld2gcode: modular.o cldmain.o
	g++ -o $@ modular.o cldmain.o
gcode2gcode: modular.o gcodemain.o
	g++ -o $@ $?
clean:
	-rm -f *.o cld2gcode gcode2gcode *~
