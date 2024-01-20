CFLAGS = -Wall -Werror -g
CC = gcc $(CFLAGS)
AN = proj2
SHELL = /bin/bash
CWD = $(shell pwd | sed 's/.*\///g')

all: swish slow_write

swish: swish.c string_vector.o job_list.o swish_funcs.o
	$(CC) -o $@ $^

job_list.o: job_list.h job_list.c
	$(CC) -c job_list.c

string_vector.o: string_vector.h string_vector.c
	$(CC) -c string_vector.c

swish_funcs.o: job_list.o string_vector.o swish_funcs.c
	$(CC) -c swish_funcs.c

slow_write: test_cases/resources/slow_write.c
	$(CC) -o $@ $^

clean:
	rm -f *.o swish slow_write

test-setup:
	@chmod u+x testius
	rm -f out.txt out2.txt

ifdef testnum
test: test-setup swish slow_write
	./testius test_cases/test_swish.json -v -n $(testnum)
else
test: test-setup swish slow_write
	./testius test_cases/test_swish.json
endif

clean-tests:
	rm -rf test_results out.txt out2.txt test_cases/out.txt

zip: clean clean-tests
	rm -f $(AN)-code.zip
	cd .. && zip "$(CWD)/$(AN)-code.zip" -r "$(CWD)" -x "$(CWD)/test_cases/*" "$(CWD)/testius" "$(CWD)/slow_write" "$(CWD)/.git/*"
	@echo Zip created in $(AN)-code.zip
	@if (( $$(stat -c '%s' $(AN)-code.zip) > 10*(2**20) )); then echo "WARNING: $(AN)-code.zip seems REALLY big, check there are no abnormally large test files"; du -h $(AN)-code.zip; fi
	@if (( $$(unzip -t $(AN)-code.zip | wc -l) > 256 )); then echo "WARNING: $(AN)-code.zip has 256 or more files in it which may cause submission problems"; fi
