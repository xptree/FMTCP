#include <cstdio>
#include <cstdlib>
#include <iostream>
using namespace std;

int f()
{
	int x = 0;
	return x++;
}
int main()
{
	cerr << f() << endl;
}
