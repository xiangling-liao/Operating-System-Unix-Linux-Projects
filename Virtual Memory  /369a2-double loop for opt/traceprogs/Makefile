SRCS = simpleloop.c matmul.c blocked.c mine.c
PROGS = simpleloop matmul blocked mine

all : $(PROGS)

$(PROGS) : % : %.c
	gcc -Wall -g -o $@ $<


traces: $(PROGS)
	./runit simpleloop
	./runit matmul 100
	./runit blocked 100 25
	./runit mine

.PHONY: clean
clean : 
	rm -f simpleloop matmul blocked mine tr-*.ref *.marker *~
