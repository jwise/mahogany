include ../makeopts

NAME            = M.res
RESCOMP         = windres
RCINPUTSWITCH   = -i
RCOUTPUTSWITCH  = -o
RCINCSWITCH     = --include-dir

all :		$(NAME)

$(NAME) :	.src/M.rc
		$(RESCOMP) $(RESFLAGS) $< -O coff -o $@

clean :
		rm -f *.o *.a *.sh *.d core a.out $(NAME)

install :
