all: aliscan.exe

aliscan.exe: aliscan.cpp
	g++ -lgpib -lpthread aliscan.cpp -o aliscan.exe