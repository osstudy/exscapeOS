#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define MAX_GUESSES 10

int main(int argc, char **argv) {
	srand(848214); // TODO: time()
	int num = 1 + (rand() % 100);
	int guess = -1;
	int num_guesses = 0;

	char buf[16] = {0};

	while (guess != num) {
		printf("%d guesses remaining\n", MAX_GUESSES - num_guesses);
		fputs("Guess the number, from user mode (1-100): ", stdout);
		fflush(stdout);

		char *end;
		fgets(buf, 16, stdin);
		guess = strtol(buf, &end, 10);
		if (*end != 0 && *end != '\n') {
			printf("That's not a number! Try again.\n");
			continue;
		}

		num_guesses++;

		if (guess == num) {
			printf("You got it on guess %d!\n", num_guesses);
			break;
		}
		else if (num_guesses >= MAX_GUESSES) {
			printf("Sorry, you lost! The number was %d.\n", num);
			return 0;
		}
		else if (guess > num) {
			puts("Nope. Try lower.");
		}
		else if (guess < num) {
			puts("Nope. Try higher.");
		}
	}

	return 0;
}
