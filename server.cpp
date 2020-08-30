#include <cassert>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <sys/types.h>
#include <sys/stat.h>

#include <thread>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <math.h>
#include <unistd.h>
#include "Reqchannel.h"
#include "FIFOreqchannel.h"
#include "MQreqchannel.h"
#include "TCPreqchannel.h"
using namespace std;

int buffCapacity = MAX_MESSAGE;
int channelCount = 0;
char *buffer = NULL;
pthread_mutex_t newchannel_lock;
void handle_process_loop(RequestChannel *_channel);
string ival = "f";
vector<string> all_data[NUM_PERSONS];

void process_newchannel_request(RequestChannel *_channel)
{
	channelCount++;
	string new_channel_name = "data" + to_string(channelCount) + "_";
	char buf[30];
	strcpy(buf, new_channel_name.c_str());
	_channel->cwrite(buf, new_channel_name.size() + 1);

	RequestChannel *data_channel = NULL;
	if (ival == "f")
	{
		data_channel = new FIFORequestChannel(new_channel_name, RequestChannel::SERVER_SIDE);
	}
	else if (ival == "q")
	{
		data_channel = new MQRequestChannel(new_channel_name, RequestChannel::SERVER_SIDE, buffCapacity);
	}
	thread thread_for_client(handle_process_loop, data_channel);
	thread_for_client.detach();
}

void populate_file_data(int person)
{
	string filename = "BIMDC/" + to_string(person) + ".csv";
	char line[100];
	ifstream ifs(filename.c_str());
	if (ifs.fail())
	{
		EXITONERROR("Data file: " + filename + " does not exist in the BIMDC/ directory");
	}
	int count = 0;
	while (!ifs.eof())
	{
		line[0] = 0;
		ifs.getline(line, 100);
		if (ifs.eof())
			break;
		double seconds = stod(split(string(line), ',')[0]);
		if (line[0])
			all_data[person - 1].push_back(string(line));
	}
}

double get_data_from_memory(int person, double seconds, int ecgno)
{
	int index = (int)round(seconds / 0.004);
	string line = all_data[person - 1][index];
	vector<string> parts = split(line, ',');
	double sec = stod(parts[0]);
	double ecg1 = stod(parts[1]);
	double ecg2 = stod(parts[2]);
	if (ecgno == 1)
		return ecg1;
	else
		return ecg2;
}

void process_file_request(RequestChannel *rc, char *request)
{

	filemsg f = *(filemsg *)request;
	string filename = request + sizeof(filemsg);
	filename = "BIMDC/" + filename;
	cout << "Received request for " << filename << endl;

	if (f.offset == 0 && f.length == 0)
	{
		__int64_t fs = get_file_size(filename);
		rc->cwrite((char *)&fs, sizeof(__int64_t));
		return;
	}
	char *response = request;

	if (f.length > buffCapacity)
	{
		cerr << "Client is requesting a chunk bigger than server's capacity" << endl;
		cerr << "Returning nothing (i.e., 0 bytes) in response" << endl;
		rc->cwrite(response, 0);
	}
	FILE *fp = fopen(filename.c_str(), "rb");
	if (!fp)
	{
		cerr << "Server received request for file: " << filename << " which cannot be opened" << endl;
		rc->cwrite(buffer, 0);
		return;
	}
	fseek(fp, f.offset, SEEK_SET);
	int nbytes = fread(response, 1, f.length, fp);
	assert(nbytes == f.length);
	rc->cwrite(response, nbytes);
	fclose(fp);
}

void process_data_request(RequestChannel *rc, char *request)
{
	datamsg *d = (datamsg *)request;
	double data = get_data_from_memory(d->person, d->seconds, d->ecgno);
	rc->cwrite((char *)&data, sizeof(double));
}

void process_unknown_request(RequestChannel *rc)
{
	char a = 0;
	rc->cwrite(&a, sizeof(a));
}

int process_request(RequestChannel *rc, char *_request)
{
	MESSAGE_TYPE m = *(MESSAGE_TYPE *)_request;
	if (m == DATA_MSG)
	{
		usleep(rand() % 5000);
		process_data_request(rc, _request);
	}
	else if (m == FILE_MSG)
	{
		process_file_request(rc, _request);
	}
	else if (m == NEWCHANNEL_MSG)
	{
		process_newchannel_request(rc);
	}
	else
	{
		process_unknown_request(rc);
	}
}

void handle_process_loop(RequestChannel *channel)
{
	char *buffer = new char[buffCapacity];
	if (!buffer)
	{
		EXITONERROR("Cannot allocate memory for server buffer");
	}
	while (true)
	{
		int nbytes = channel->cread(buffer, buffCapacity);
		if (nbytes < 0)
		{
			cerr << "Client-side terminated abnormally" << endl;
			break;
		}
		else if (nbytes == 0)
		{
			continue;
		}
		MESSAGE_TYPE m = *(MESSAGE_TYPE *)buffer;
		if (m == QUIT_MSG)
		{
			cout << "Client-side is done and exited" << endl;
			break;
		}
		process_request(channel, buffer);
	}
	delete buffer;
}

int main(int argc, char *argv[])
{
	buffCapacity = MAX_MESSAGE;
	int opt;
	string port;
	while ((opt = getopt(argc, argv, "m:i:r:")) != -1)
	{
		switch (opt)
		{
		case 'm':
			buffCapacity = atoi(optarg);
			break;
		case 'i':
			ival = optarg;
			break;
		case 'r':
			port = optarg;
			break;
		}
	}

	srand(time_t(NULL));

	for (int i = 0; i < NUM_PERSONS; i++)
	{
		populate_file_data(i + 1);
	}

	RequestChannel *control_channel = NULL;
	if (ival == "f")
	{
		control_channel = new FIFORequestChannel("control", RequestChannel::SERVER_SIDE);
		handle_process_loop(control_channel);
	}
	else if (ival == "q")
	{
		control_channel = new MQRequestChannel("control", RequestChannel::SERVER_SIDE, buffCapacity);
		handle_process_loop(control_channel);
	}
	else if (ival == "t")
	{
		struct sockaddr_storage their_addr;
		socklen_t sin_size;
		control_channel = new TCPRequestChannel("", port, RequestChannel::SERVER_SIDE);
		while (1)
		{ 
			sin_size = sizeof their_addr;
			int slave_socket = accept(((TCPRequestChannel *)control_channel)->sockfd, (struct sockaddr *)&their_addr, &sin_size);
			if (slave_socket == -1)
			{
				perror("accept");
				continue;
			}
			TCPRequestChannel *slave_channel = new TCPRequestChannel(slave_socket);

			thread t(handle_process_loop, (RequestChannel *)slave_channel);
			t.detach();
		}
	}

	cout << "Server terminated" << endl;
}
