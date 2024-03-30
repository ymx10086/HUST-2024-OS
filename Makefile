# we assume that the utilities from RISC-V cross-compiler (i.e., riscv64-unknown-elf-gcc and etc.)
# are in your system PATH. To check if your environment satisfies this requirement, simple use 
# `which` command as follows:
# $ which riscv64-unknown-elf-gcc
# if you have an output path, your environment satisfy our requirement.

# ---------------------	macros --------------------------
CROSS_PREFIX 	:= riscv64-unknown-elf-
CC 				:= $(CROSS_PREFIX)gcc
AR 				:= $(CROSS_PREFIX)ar
RANLIB        	:= $(CROSS_PREFIX)ranlib

SRC_DIR        	:= .
OBJ_DIR 		:= obj
SPROJS_INCLUDE 	:= -I.  

HOSTFS_ROOT := hostfs_root
ifneq (,)
  march := -march=
  is_32bit := $(findstring 32,$(march))
  mabi := -mabi=$(if $(is_32bit),ilp32,lp64)
endif

CFLAGS        := -Wall -Werror  -fno-builtin -nostdlib -D__NO_INLINE__ -mcmodel=medany -g -Og -std=gnu99 -Wno-unused -Wno-attributes -fno-delete-null-pointer-checks -fno-PIE $(march)
COMPILE       	:= $(CC) -MMD -MP $(CFLAGS) $(SPROJS_INCLUDE)

#---------------------	utils -----------------------
UTIL_CPPS 	:= util/*.c

UTIL_CPPS  := $(wildcard $(UTIL_CPPS))
UTIL_OBJS  :=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(UTIL_CPPS)))


UTIL_LIB   := $(OBJ_DIR)/util.a

#---------------------	kernel -----------------------
KERNEL_LDS  	:= kernel/kernel.lds
KERNEL_CPPS 	:= \
	kernel/*.c \
	kernel/machine/*.c \
	kernel/util/*.c

KERNEL_ASMS 	:= \
	kernel/*.S \
	kernel/machine/*.S \
	kernel/util/*.S

KERNEL_CPPS  	:= $(wildcard $(KERNEL_CPPS))
KERNEL_ASMS  	:= $(wildcard $(KERNEL_ASMS))
KERNEL_OBJS  	:=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(KERNEL_CPPS)))
KERNEL_OBJS  	+=  $(addprefix $(OBJ_DIR)/, $(patsubst %.S,%.o,$(KERNEL_ASMS)))

KERNEL_TARGET = $(OBJ_DIR)/riscv-pke


#---------------------	spike interface library -----------------------
SPIKE_INF_CPPS 	:= spike_interface/*.c

SPIKE_INF_CPPS  := $(wildcard $(SPIKE_INF_CPPS))
SPIKE_INF_OBJS 	:=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(SPIKE_INF_CPPS)))


SPIKE_INF_LIB   := $(OBJ_DIR)/spike_interface.a


#---------------------	user   -----------------------
USER_CPPS 		:= user/app_shell.c user/user_lib.c

USER_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_CPPS)))

USER_TARGET 	:= $(HOSTFS_ROOT)/bin/app_shell

USER_CD_CPPS 		:= user/app_cd.c user/user_lib.c

USER_CD_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_CD_CPPS)))

USER_CD_TARGET 	:= $(HOSTFS_ROOT)/bin/app_cd

USER_E_CPPS 		:= user/app_ls.c user/user_lib.c

USER_E_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_E_CPPS)))

USER_E_TARGET 	:= $(HOSTFS_ROOT)/bin/app_ls

USER_M_CPPS 		:= user/app_mkdir.c user/user_lib.c

USER_M_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_M_CPPS)))

USER_M_TARGET 	:= $(HOSTFS_ROOT)/bin/app_mkdir

USER_SEQ_CPPS 		:= user/app_sequence.c user/user_lib.c

USER_SEQ_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_SEQ_CPPS)))

USER_SEQ_TARGET 	:= $(HOSTFS_ROOT)/bin/app_sequence

USER_HIS_CPPS 		:= user/app_history.c user/user_lib.c

USER_HIS_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_HIS_CPPS)))

USER_HIS_TARGET 	:= $(HOSTFS_ROOT)/bin/app_history

USER_ERR_CPPS 		:= user/app_errorline.c user/user_lib.c

USER_ERR_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_ERR_CPPS)))

USER_ERR_TARGET 	:= $(HOSTFS_ROOT)/bin/app_errorline

USER_SIN_CPPS 		:= user/app_singlepageheap.c user/user_lib.c

USER_SIN_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_SIN_CPPS)))

USER_SIN_TARGET 	:= $(HOSTFS_ROOT)/bin/app_singlepageheap

USER_SEM_CPPS 		:= user/app_semaphore.c user/user_lib.c

USER_SEM_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_SEM_CPPS)))

USER_SEM_TARGET 	:= $(HOSTFS_ROOT)/bin/app_semaphore

USER_COW_CPPS 		:= user/app_cow.c user/user_lib.c

USER_COW_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_COW_CPPS)))

USER_COW_TARGET 	:= $(HOSTFS_ROOT)/bin/app_cow

USER_PW_CPPS 		:= user/app_pwd.c user/user_lib.c

USER_PW_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_PW_CPPS)))

USER_PW_TARGET 	:= $(HOSTFS_ROOT)/bin/app_pwd

USER_T_CPPS 		:= user/app_touch.c user/user_lib.c

USER_T_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_T_CPPS)))

USER_T_TARGET 	:= $(HOSTFS_ROOT)/bin/app_touch

USER_C_CPPS 		:= user/app_cat.c user/user_lib.c

USER_C_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_C_CPPS)))

USER_C_TARGET 	:= $(HOSTFS_ROOT)/bin/app_cat

USER_O_CPPS 		:= user/app_echo.c user/user_lib.c

USER_O_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_O_CPPS)))

USER_O_TARGET 	:= $(HOSTFS_ROOT)/bin/app_echo

USER_CPP0 		:= user/app_alloc0.c user/user_lib.c
USER_CPP1 		:= user/app_alloc1.c user/user_lib.c

USER_CPP0  		:= $(wildcard $(USER_CPP0))
USER_CPP1  		:= $(wildcard $(USER_CPP1))
USER_OBJ0  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_CPP0)))
USER_OBJ1  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_CPP1)))

USER_TARGET0 	:= $(HOSTFS_ROOT)/bin/app_alloc0
USER_TARGET1 	:= $(HOSTFS_ROOT)/bin/app_alloc1
#------------------------targets------------------------
$(OBJ_DIR):
	@-mkdir -p $(OBJ_DIR)	
	@-mkdir -p $(dir $(UTIL_OBJS))
	@-mkdir -p $(dir $(SPIKE_INF_OBJS))
	@-mkdir -p $(dir $(KERNEL_OBJS))
	@-mkdir -p $(dir $(USER_OBJS))
	@-mkdir -p $(dir $(USER_OBJ0))
	@-mkdir -p $(dir $(USER_OBJ1))
	@-mkdir -p $(dir $(USER_E_OBJS))
	@-mkdir -p $(dir $(USER_M_OBJS))
	@-mkdir -p $(dir $(USER_SEQ_OBJS))
	@-mkdir -p $(dir $(USER_HIS_OBJS))
	@-mkdir -p $(dir $(USER_ERR_OBJS))
	@-mkdir -p $(dir $(USER_SIN_OBJS))
	@-mkdir -p $(dir $(USER_SEM_OBJS))
	@-mkdir -p $(dir $(USER_COW_OBJS))
	@-mkdir -p $(dir $(USER_CD_OBJS))
	@-mkdir -p $(dir $(USER_PW_OBJS))
	@-mkdir -p $(dir $(USER_T_OBJS))
	@-mkdir -p $(dir $(USER_C_OBJS))
	@-mkdir -p $(dir $(USER_O_OBJS))
	
$(OBJ_DIR)/%.o : %.c
	@echo "compiling" $<
	@$(COMPILE) -c $< -o $@

$(OBJ_DIR)/%.o : %.S
	@echo "compiling" $<
	@$(COMPILE) -c $< -o $@

$(UTIL_LIB): $(OBJ_DIR) $(UTIL_OBJS)
	@echo "linking " $@	...	
	@$(AR) -rcs $@ $(UTIL_OBJS) 
	@echo "Util lib has been build into" \"$@\"
	
$(SPIKE_INF_LIB): $(OBJ_DIR) $(UTIL_OBJS) $(SPIKE_INF_OBJS)
	@echo "linking " $@	...	
	@$(AR) -rcs $@ $(SPIKE_INF_OBJS) $(UTIL_OBJS)
	@echo "Spike lib has been build into" \"$@\"

$(KERNEL_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(SPIKE_INF_LIB) $(KERNEL_OBJS) $(KERNEL_LDS)
	@echo "linking" $@ ...
	@$(COMPILE) $(KERNEL_OBJS) $(UTIL_LIB) $(SPIKE_INF_LIB) -o $@ -T $(KERNEL_LDS)
	@echo "PKE core has been built into" \"$@\"

$(USER_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_E_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_E_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_E_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_M_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_M_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_M_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_SEQ_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_SEQ_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_SEQ_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_HIS_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_HIS_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_HIS_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_ERR_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_ERR_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_ERR_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_SIN_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_SIN_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_SIN_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_SEM_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_SEM_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_SEM_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_COW_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_COW_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_COW_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_CD_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_CD_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_CD_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_PW_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_PW_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_PW_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_T_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_T_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_T_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_C_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_C_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_C_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_O_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_O_OBJS)
	@echo "linking" $@	...	
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_O_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_TARGET0): $(OBJ_DIR) $(UTIL_LIB) $(USER_OBJ0)
	@echo "linking" $@	...	
	@$(COMPILE) --entry=main $(USER_OBJ0) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	
$(USER_TARGET1): $(OBJ_DIR) $(UTIL_LIB) $(USER_OBJ1)
	@echo "linking" $@	...	
	@$(COMPILE) --entry=main $(USER_OBJ1) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

-include $(wildcard $(OBJ_DIR)/*/*.d)
-include $(wildcard $(OBJ_DIR)/*/*/*.d)

.DEFAULT_GOAL := $(all)

all: $(KERNEL_TARGET) $(USER_TARGET) $(USER_E_TARGET) $(USER_M_TARGET) $(USER_SEQ_TARGET) $(USER_HIS_TARGET) $(USER_ERR_TARGET) $(USER_SIN_TARGET) $(USER_SEM_TARGET) $(USER_COW_TARGET) $(USER_CD_TARGET) $(USER_PW_TARGET) $(USER_T_TARGET) $(USER_C_TARGET) $(USER_O_TARGET) $(USER_TARGET0) $(USER_TARGET1)
.PHONY:all

run: $(KERNEL_TARGET) $(USER_TARGET) $(USER_E_TARGET) $(USER_M_TARGET) $(USER_SEQ_TARGET) $(USER_HIS_TARGET) $(USER_ERR_TARGET) $(USER_SIN_TARGET) $(USER_SEM_TARGET) $(USER_COW_TARGET) $(USER_CD_TARGET) $(USER_PW_TARGET) $(USER_T_TARGET) $(USER_C_TARGET) $(USER_O_TARGET) $(USER_TARGET0) $(USER_TARGET1)
	@echo "********************HUST PKE********************"
	spike -p2 $(KERNEL_TARGET) /bin/app_shell /bin/app_alloc0

# need openocd!
gdb:$(KERNEL_TARGET) $(USER_TARGET)
	spike --rbb-port=9824 -H $(KERNEL_TARGET) $(USER_TARGET) &
	@sleep 1
	openocd -f ./.spike.cfg &
	@sleep 1
	riscv64-unknown-elf-gdb -command=./.gdbinit

# clean gdb. need openocd!
gdb_clean:
	@-kill -9 $$(lsof -i:9824 -t)
	@-kill -9 $$(lsof -i:3333 -t)
	@sleep 1

objdump:
	riscv64-unknown-elf-objdump -d $(KERNEL_TARGET) > $(OBJ_DIR)/kernel_dump
	riscv64-unknown-elf-objdump -d $(USER_TARGET) > $(OBJ_DIR)/user_dump

cscope:
	find ./ -name "*.c" > cscope.files
	find ./ -name "*.h" >> cscope.files
	find ./ -name "*.S" >> cscope.files
	find ./ -name "*.lds" >> cscope.files
	cscope -bqk

format:
	@python ./format.py ./

clean:
	rm -fr ${OBJ_DIR} ${HOSTFS_ROOT}/bin
