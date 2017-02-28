

all: dlist
	gcc -g -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast dlist_test.c -lpthread -L$(PWD) -o dlist_test -ldlist

dlist:
	gcc -g -fPIC -shared locker.c dlist.c -o libdlist.so

clean:
	rm -f *test *.exe *.so


