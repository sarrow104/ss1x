.PHONY: all release debug clean clean-debug clean-release
all: release

release:
	@mkdir -p ../../Release
	cd ../../Release && cmake -DCMAKE_BUILD_TYPE=Release ../include/ss1x && make

debug:
	@mkdir -p ../../Debug
	cd ../../Debug && cmake -DCMAKE_BUILD_TYPE=Debug ../include/ss1x && make

clean: clean-debug clean-release

clean-debug:
	if [ -d ../../Debug ]; then cd ../../Debug && make clean && rm -rf ./* ; fi

clean-release:
	if [ -d ../../Release ]; then cd ../../Release && make clean && rm -rf ./* ; fi
	#cd ../../Release && make clean && rm -rf ./*
