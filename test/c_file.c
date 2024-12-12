#include <stdio.h>

int main()
{
	int* x;
	void* y = 0;

	x = y;			/* should only work on C */
	if(x == 0)
	{
		printf("C compilation: OK!\n");
	}

	return 0;
}
