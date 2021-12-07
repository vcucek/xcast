SYSTEM?=linux
DESTDIR?=/usr/local/

CC=g++
CXX=g++
AR=ar
CFLAGS=-O3 -Wno-deprecated -Wno-deprecated-declarations -fPIC

INSTALL=/usr/bin/install

BUILD_DIR=build
DIST_DIR=dist
LIB_DIR=lib

SRC_DIR=src
INCLUDES=-Iinclude -Isrc -Iext/include
LIBS=-Lext/lib

#different os configurations
############################

ifeq ($(SYSTEM),linux)

# Preprocessor directives
CFLAGS+=-DSYSTEM_LINUX

LDFLAGS_S=-lpthread -lz -lm -lva -llzma -lX11 -lEGL -lGLESv2 \
		-lavformat -lavcodec -lavutil -lswresample -lswscale

LDFLAGS_L=-lpthread -lz -lm -lva -llzma -lX11 -lEGL -lGLESv2 \
		-lavformat -lavcodec -lavutil -lswresample -lswscale

OBJ_L=	render/BGRARender.o \
	display/XCDisplay.o \
	display/XCDisplay_x11.o

OBJ_S=xcast.o

endif

ifeq ($(SYSTEM),rasp)

# Preprocessor directives
CFLAGS+=-DSYSTEM_RASP

CFLAGS+=-DRPI_NO_X
CFLAGS+=-I/opt/vc/include
CFLAGS+=-I/opt/vc/include/interface/vcos/pthreads
CFLAGS+=-I/opt/vc/include/interface/vmcs_host/linux

LIBS+=-L/opt/vc/lib
LDFLAGS=-lEGL -lGLESv2 -lbcm_host -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt -lm -lilclient

LDFLAGS_S=$(LDFLAGS)
LDFLAGS_L=$(LDFLAGS)

OBJ_L=	codec/RaspPlayer.o \
	display/XCDisplay.o \
	display/XCDisplay_rasp.o

OBJ_S=xcast.o

endif

OBJECTS_S=$(addprefix $(BUILD_DIR)/, $(OBJ_S))
OBJECTS_L=$(addprefix $(BUILD_DIR)/, $(OBJ_L))

############################

APP=xcast
LIB=xcast

.PHONY: all debug app install uninstall clean

all: lib app

debug: CXX += -DDEBUG -g
debug: CC += -DDEBUG -g
debug: all

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(OBJECTS_S): | $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@dirname $@ | xargs mkdir -p
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -o $@

app: $(OBJECTS_L) $(OBJECTS_S)
	@mkdir -p $(DIST_DIR)
	$(CC) $^ $(LIBS) $(LDFLAGS_S) -o $(DIST_DIR)/$(APP)

lib: $(OBJECTS_L)
	@mkdir -p $(LIB_DIR)
	$(CC) -shared -Wl,-soname,lib$(LIB).so.1 $(LIBS) $(LDFLAGS_L) -o $(LIB_DIR)/lib$(LIB).so.1.0 $^
	ln -sf $(LIB_DIR)/lib$(LIB).so.1.0 $(LIB_DIR)/lib$(LIB).so.1
	ln -sf $(LIB_DIR)/lib$(LIB).so.1.0 $(LIB_DIR)/lib$(LIB).so
	$(AR) rcs $(LIB_DIR)/lib$(LIB).a $^

install: $(DIST_DIR)/$(APP)
	$(INSTALL) -d $(DESTDIR)bin
	$(INSTALL) -t $(DESTDIR)bin $(DIST_DIR)/$(APP)

uninstall:
	$(RM) -f $(DESTDIR)bin/$(APP)

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR) $(LIB_DIR)
