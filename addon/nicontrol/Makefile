#
# Makefile
#

CIRCLEHOME = ../..

OBJS	= nicontroller.o nikkm32.o mykontrol.o

libnicontrol.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
