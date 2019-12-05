TAG := ctags

srcDir := src

.PHONY: all release debug tags clean

all: release

release:
	${MAKE} -C ${srcDir} $@

debug:
	${MAKE} -C ${srcDir} $@

tags:
	${TAG} -R ${srcDir}

clean:
	${MAKE} -C ${srcDir} clean
