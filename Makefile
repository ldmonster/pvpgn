BUILD_DIR ?= build

.PHONY: all clean install rebuild configure

all: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR) $(MAKECMDGOALS)

$(BUILD_DIR)/Makefile:
	cmake -S . -B $(BUILD_DIR)

configure:
	cmake -S . -B $(BUILD_DIR)

clean:
	$(MAKE) -C $(BUILD_DIR) clean

install:
	$(MAKE) -C $(BUILD_DIR) install

rebuild: clean all
