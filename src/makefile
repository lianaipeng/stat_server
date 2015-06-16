
TARGET=stat_server
ROOT_DIR=$(shell pwd)
COMMON_DIR=$(ROOT_DIR)/common
ifeq ($(debug), y)
	CPPFLAGS=-std=c++11 -gdwarf-2 -DDEBUG -DNO_FCGI_DEFINES -DBOOST_LOG_DYN_LINK -Werror -O0 #-pg
else
	CPPFLAGS=-std=c++11 -gdwarf-2 -DNO_FCGI_DEFINES -DBOOST_LOG_DYN_LINK -Werror -O0 #-pg
endif
CXX=/opt/rh/devtoolset-2/root/usr/bin/g++

DEP_LIBS=-lpthread\
	-lhiredis\
	-ljsoncpp\
	-levent\
	-lcurl\
	-lboost_regex\
	-lboost_system\
	-lboost_filesystem\
	-lboost_log\
	-lboost_thread\
	-lboost_log_setup\
	-lboost_date_time\
	-lmysqlcppconn\
	-lactivemq-cpp\
	-L/usr/lib\
	-L/usr/local/lib\

DEP_INCS=-I.\
	-I/usr/include\
	-I/usr/local/include\
	-I/usr/local/include/boost\
	-I/usr/local/include/hiredis\
	-I/usr/local/include\
	-I/usr/local/include/activemq-cpp-3.8.3\
	-I./common\

DIRS=$(shell find . -maxdepth 3 -type d)
FILES=$(foreach dir,$(DIRS),$(wildcard $(dir)/*.cpp))
OBJS=$(patsubst %.cpp,%.o, $(FILES))

VPATH = .
.cpp.o:
	$(CXX) -c $(CPPFLAGS) $< -o $@ $(DeP_LIBS) $(DEP_INCS) 

all: common $(TARGET)

common:
	mkdir common	
	cd $(COMMON_DIR) && ln -s ../../../common/* . 
	@echo "current work directory:"$(ROOT_DIR)
	@echo "common directory:"$(COMMON_DIR)
	touch $(COMMON_DIR)/makefile
	@echo ''''''                                                          >$(COMMON_DIR)/makefile
	@echo '''DIRS=$$(shell find . -maxdepth 2 -type d)'''                 >>$(COMMON_DIR)/makefile
	@echo '''FILES=$$(foreach dir,$$(DIRS),$$(wildcard $$(dir)/*.cpp))''' >>$(COMMON_DIR)/makefile
	@echo '''CXX=/opt/rh/devtoolset-2/root/usr/bin/g++'''                 >>$(COMMON_DIR)/makefile
	@echo ''''''                                                          >>$(COMMON_DIR)/makefile
	@echo '''all: common'''                                               >>$(COMMON_DIR)/makefile
	@echo ''''''                                                          >>$(COMMON_DIR)/makefile
	@echo '''common:'''                                                   >>$(COMMON_DIR)/makefile
	@echo '''	$$(CXX) -I/usr/local/include/activemq-cpp-3.8.3 -DBOOST_LOG_DYN_LINK -c $$(FILES)'''>>$(COMMON_DIR)/makefile
	@echo ''''''                                                          >>$(COMMON_DIR)/makefile
	@echo '''clean:'''                                                    >>$(COMMON_DIR)/makefile
	@echo '''	-rm *.o'''                                                >>$(COMMON_DIR)/makefile
	cd $(COMMON_DIR) && make


$(TARGET): $(OBJS)
	$(CXX)  -o $@ $(OBJS) $(CPPFLAGS) $(DEP_LIBS) $(DEP_INCS)

clean:
	rm -rf $(TARGET) $(OBJS) $(COMMON_DIR)
