#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

int main(int argc, char **argv) {

	int val = 1;
	int ret = 0;

	//refork:
	ret = fork();
	if (ret == -1) {
		printf("fork failed with error %d: %s\n", errno, strerror(errno));
		return 1;
	}
	else if (ret == 0) {
		printf("fork(): in child! pid = %d, val = %d\n", getpid(), val);
		val += 10;
		printf("child: val = %d after increment by 10\n", val);
		sleep(3);
		//goto refork;
	}
	else {
		printf("fork(): in parent; child has pid %d, val = %d\n", ret, val);
		val += 20;
		printf("parent: val = %d after increment by 20\n", val);
		int st;
		//sleep(1);
		int p = wait(&st);
		printf("parent: wait returned (child %d exited): status = %d (%04x)\n", p, st, st);
	}

	printf("Exiting from %s\n", ret == 0 ? "child" : "parent");

	return 0;
}