
#DEBUG	= 	-g -O0
DEBUG	= 	-O3
CC	= 	g++
INCLUDE	= 	-I. -I/usr/local/include
DEFS	= 	-DBB_BLACK_ARCH
CFLAGS	= 	$(DEBUG) $(DEFS) -Wall $(INCLUDE) -pipe
LDLIBS  = 	-L/usr/local/lib -lwiringBBB -lTarts -lpthread -lm -lrt 

SRC	=	TartsMotionClient.cpp
		
OBJ	=	$(SRC:.cpp=.o)
BINS	=	$(SRC:.cpp=)

all:		$(OBJ) $(BINS)

TartsMotionClient: TartsMotionClient.o
		@$(CC) -o $@ $< -lncurses -lmosquitto -ljsoncpp $(LDLIBS) 

.cpp.o:		
		@echo [Compiling] $<
		@$(CC) -c $(CFLAGS) $< -o $@

clean:		
		@echo "[Cleaning all example object and executable files]"
		@rm -f $(OBJ)
		@rm -f $(BINS)
		@rm -f utils.o

