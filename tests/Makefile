CC ?= gcc
COMMON_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O1 -fsanitize=address,undefined -I../include
LDFLAGS := -fsanitize=address,undefined

TARGET_BAREMETAL := test_fsm
TARGET_FREERTOS  := test_fsm_freertos

.PHONY: all run run-unit run-freertos run-examples run-mermaid run-linter clean

all: $(TARGET_BAREMETAL) $(TARGET_FREERTOS)

$(TARGET_BAREMETAL): test_fsm.c ../fsm.c ../ring_buff.c ../include/fsm.h ../include/ring_buff.h
	$(CC) $(COMMON_CFLAGS) test_fsm.c ../fsm.c ../ring_buff.c -o $(TARGET_BAREMETAL) $(LDFLAGS)

$(TARGET_FREERTOS): test_fsm.c ../fsm.c ../include/fsm.h mock_freertos/freertos/FreeRTOS.h mock_freertos/freertos/queue.h
	$(CC) $(COMMON_CFLAGS) -DCONFIG_FSM_USE_FREERTOS_API=1 -Imock_freertos test_fsm.c ../fsm.c -o $(TARGET_FREERTOS) $(LDFLAGS)

run-unit: $(TARGET_BAREMETAL)
	./$(TARGET_BAREMETAL)

run-freertos: $(TARGET_FREERTOS)
	./$(TARGET_FREERTOS)

run-examples:
	CC="$(CC)" python3 -B test_examples.py -v

run-mermaid:
	python3 -B test_mermaid.py -v

run-linter:
	python3 -B test_linter.py -v
	python3 -B ../tools/fsm_check.py --strict ../example/

run: run-unit run-freertos run-examples run-mermaid run-linter

clean:
	rm -f $(TARGET_BAREMETAL) $(TARGET_FREERTOS)
