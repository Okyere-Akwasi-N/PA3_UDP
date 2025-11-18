all: procurement  factory
    
sales: wrappers.c wrappers.h  message.h  
	gcc -pthread  sales.c       wrappers.c             -o sales

procurement: procurement.c  wrappers.c  wrappers.h message.c message.h
	gcc -pthread  procurement.c  wrappers.c  message.c  -o procurement

factory: factory.c  wrappers.c  wrappers.h message.c  message.h
	gcc -pthread  factory.c     wrappers.c  message.c  -o factory

clean:
	rm -f *.o  factory procurement *.log
	ipcrm -a
	rm -f /dev/shm/aboutams_*
