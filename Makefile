
all: aliscan.exe

aliscan.exe: aliscan.cpp
	g++ -std=c++0x -I/usr/local/include/alibava -I/home/alibava-user/Downloads/alibava-0.5.0-2/soap -lsoapAlibavaClient -lgpib -lpthread $^ -o $@

clean:
	rm aliscan.exe