CC		=	g++-7
CFLAG	=	-std=c++14 -g -lpthread -Icore
CPPS	=	test/test_ttlcache.cpp

a.out : $(CPPS)
	$(CC) $(CFLAG) $^ -o $@
	