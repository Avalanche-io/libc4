#include <stdlib.h>

typedef struct error error;

typedef struct error {
	char *message;
	char* (*Error)(error*);
} error;

char* Error(struct error* this) {
	return this->message;
}

struct error* NewError(char *message) {
	struct error* err = malloc(sizeof(error));
	err->message = message;
	err->Error = Error;
	return err;
}

void ReleaseError(struct error* err) {
	free(err->message);
	free(err);
}

