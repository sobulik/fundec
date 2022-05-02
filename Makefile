CPPFLAGS = -Wall -W -Wfloat-equal -Wshadow \
           -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings \
           -Wconversion -ansi -pedantic -Wno-long-long

EXECUTABLE := functional_decomposition

$(EXECUTABLE): $(EXECUTABLE).o
	mpiCC -o $@ $^

$(EXECUTABLE).o: %.o: %.cpp
	mpiCC -DRND ${CPPFLAGS} -o $@ -c $^

run:
	mpirun -np 4 $(EXECUTABLE) 5

clean:
	rm -f $(EXECUTABLE).o $(EXECUTABLE)
