# NOVA Compiler C Components Makefile
# Top-level Makefile for building lexer and parser

.PHONY: all clean test ffi

all:
	$(MAKE) -C lexer all
	$(MAKE) -C parser all

clean:
	$(MAKE) -C lexer clean
	$(MAKE) -C parser clean

test:
	$(MAKE) -C lexer test

ffi:
	$(MAKE) -C lexer ffi
	$(MAKE) -C parser ffi

# Install targets
install: all
	cp build/*.a /usr/local/lib/
	cp build/*.so /usr/local/lib/
	cp -r lexer/include /usr/local/include/nova/
	cp -r parser/include /usr/local/include/nova/

uninstall:
	rm -f /usr/local/lib/libnova_lexer.a
	rm -f /usr/local/lib/libnova_parser.a
	rm -f /usr/local/lib/libnova_lexer.so
	rm -f /usr/local/lib/libnova_parser_ffi.so
	rm -rf /usr/local/include/nova/

help:
	@echo "NOVA Compiler C Components"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build lexer and parser static libraries"
	@echo "  clean   - Remove build artifacts"
	@echo "  test    - Run lexer tests"
	@echo "  ffi     - Build shared libraries for FFI"
	@echo "  install - Install libraries and headers to /usr/local"
	@echo "  uninstall - Remove installed libraries and headers"
	@echo "  help    - Show this help message"
