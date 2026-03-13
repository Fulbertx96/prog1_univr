BUILD_DIR = build

all:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
	rm -f /tmp/fifo_in /tmp/resp_*

run-server:
	./$(BUILD_DIR)/hash_server

run-client:
	./$(BUILD_DIR)/collector_client

.PHONY: all clean run-server run-client