FLASCHEN_TASCHEN_API_DIR=ft/api
FT_UTILS_ROOT_DIR=ft-utils

CXXFLAGS=-Wall -O3 -I$(FLASCHEN_TASCHEN_API_DIR)/include -I$(FT_UTILS_ROOT_DIR)/include -I.
LDFLAGS=-L$(FLASCHEN_TASCHEN_API_DIR)/lib -lftclient
FTLIB=$(FLASCHEN_TASCHEN_API_DIR)/lib/libftclient.a

ALL=simple-example simple-animation random-dots quilt black plasma nb-logo blur lines hack fractal midi kbd2midi words life maze sierpinski matrix

all : $(ALL)

% : src/%.cc $(FTLIB)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

black : src/black.cc $(FTLIB) $(FT_UTILS_ROOT_DIR)/src/utils/ft-logger.cc
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)	

$(FTLIB):
	make -C $(FLASCHEN_TASCHEN_API_DIR)/lib

clean:
	rm -f $(ALL)