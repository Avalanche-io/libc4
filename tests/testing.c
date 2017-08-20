#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

#include "id_test.c"

void handler(int sig) {
	void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

// test runner
int main() {
	signal(SIGSEGV, handler);   // install our handler
	error* err;
	int status = 0;

	tests[0] = TestAllFFFF;
	tests[1] = TestAll0000;
	tests[2] = TestAppendOrder;
	err = setup();
	if (err != NULL) {
		status = -1;
		printf("Fail: setup failed %s\n", err->Error(err));
		return status;
	}

	for ( int i = 0; i<test_count; i++ ) {
		err = tests[i]();
		if (err != NULL) {
			status = -1;
			printf("Test %d - Fail: %s\n", i, err->Error(err));
			return status;
		}
	}

	err = teardown();
	if (err != NULL) {
		status = -1;
		printf("Fail: teardown failed %s\n", err->Error(err));
		return status;
	}

	// We create an encoder to compute the c4 id for an amount of data
	c4id_encoder_t *e = c4id_new_encoder();

	// We update the encoder with blocks of data as many times as needed
	c4id_encoder_update(e, (unsigned char*)&"hello, world!", 13);

	// We then get the id from the encoder
	c4_id_t *id = c4id_encoded_id(e);

	// We can now get the c4 id string
	char *str = c4id_string(id);

	printf("%s\n", str);

	// We test to see if we got the ID we expected to get
	if (strcmp(str, expected) != 0) {
		printf("Fail:\n");
		printf("\tgot:      \"%s\"\n", str);
		printf("\texpected: \"%s\"\n", expected);
		return -1;
	}

	// release resource allocations
	c4id_release_encoder(e);
	c4id_release(id);
	free(str);

	return 0;
}