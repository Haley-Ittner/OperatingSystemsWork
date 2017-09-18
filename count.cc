#include <iostream>
#include <string>
#include <unistd.h>

using namespace std;

int count(string);

int main() 
{
}
int count(string num)
{

	int a;

	a = std::stoi (num, nullptr, 10);

	int counter = 1;
	while (counter <= a)
	{
		cout<<"Process: "<< getpid() <<" " << counter <<endl;
		counter++;
	}

	return a;
}
