SRC = $(wildcard *.c)
#BINDIR = ../bin/
OBJ = $(SRC:%.c=$(BINDIR)%.o)
DEP = $(SRC:%.c=$(BINDIR)%.d)

INCLUDES += -I../include 
INCLUDES += -I$(RIOTCPU)/STM32F10x_StdPeriph_Lib_V3.5.0/Libraries/CMSIS/Include
INCLUDES += -I$(RIOTCPU)/STM32F10x_StdPeriph_Lib_V3.5.0/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x
INCLUDES += -I$(RIOTCPU)/STM32F10x_StdPeriph_Lib_V3.5.0/Libraries/STM32F10x_StdPeriph_Driver/inc
INCLUDES += -I$(RIOTCPU)/stm32f103rey6/include

INCLUDES += -I$(RIOTBASE)/drivers/at86rf231/include
INCLUDES += -I$(RIOTBASE)/sys/net/include

.PHONY: $(BINDIR)agilefox_drivers.a
	
$(BINDIR)agilefox_drivers.a: $(OBJ)
	$(AD) $(AR) rcs $(BINDIR)agilefox_base.a $(OBJ)

# pull in dependency info for *existing* .o files
-include $(OBJ:.o=.d)

# compile and generate dependency info
$(BINDIR)%.o: %.c
	$(AD) $(CC) $(CFLAGS) $(INCLUDES) $(BOARDINCLUDE) $(PROJECTINCLUDE) $(CPUINCLUDE) -c $*.c -o $(BINDIR)$*.o
	$(AD) $(CC) $(CFLAGS) $(INCLUDES) $(BOARDINCLUDE) $(PROJECTINCLUDE) $(CPUINCLUDE) -MM $*.c > $(BINDIR)$*.d
	@printf "$(BINDIR)" | cat - $(BINDIR)$*.d > /tmp/fw_out && mv /tmp/fw_out $(BINDIR)$*.d

# remove compilation products

clean:
	rm -f $(OBJ) $(DEP)
