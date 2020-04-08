build: libso_stdio.so so_stdio.o

so_stdio.o: so_stdio.c
	gcc -c -std=c89 -Wall -fPIC -o so_stdio.o so_stdio.c
	
libso_stdio.so: so_stdio.o
	gcc -std=c89 -shared so_stdio.o -o libso_stdio.so

clean:
	rm so_stdio.o
