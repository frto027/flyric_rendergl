BIN_DIR=./bin
LIBS_DIR=./libs

main_linux.out:$(LIBS_DIR)/libfparser.a
	[ -d $(BIN_DIR) ]||mkdir $(BIN_DIR)
	cd src&&make $@
main_linux_debug.out:$(LIBS_DIR)/libfparser.a
	[ -d $(BIN_DIR) ]||mkdir $(BIN_DIR)
	cd src&&make $@
$(LIBS_DIR)/libfparser.a:
	[ -d $(LIBS_DIR) ]||mkdir $(LIBS_DIR)
	./gen_library.sh

cleanall:clean
	rm -rf ./libs/*

clean:
	rm -rf ./bin/*
