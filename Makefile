SHELL := /usr/bin/bash
.PHONY: all core test linux android install uninstall clean

all: core

core:
	./scripts/build_core.sh

test:
	./scripts/run_tests.sh

linux:
	./scripts/build_linux.sh

android:
	./scripts/build_android.sh

install:
	./scripts/install_linux.sh

uninstall:
	./scripts/uninstall_linux.sh

clean:
	rm -rf core/build core/build-android
	cd app && flutter clean || true
