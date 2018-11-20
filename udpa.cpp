#include "UdpSocket.h"
#include "Timer.h"
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <time.h>

using namespace std;

// Test2: client stop-and-wait message send -----------------------------------
// client sends 20000 message to the server, waits till receiving acknowledgement. 
// if time out occur, client resend the message and counting the number of 
// retransmission. 
int clientStopWait(UdpSocket &sock, const int max, int message[])
{
	cerr << "Client: stop-and-wait test:" << endl;
	
	int totalCount = 0;	// counting the number of transmits 
	int sequence = 0;	// sequnce number for the message 


	while (sequence < max)
	{
		// create message 
		message[0] = sequence;
		cerr << "message = " << message[0] << endl;

		// send message
		sock.sendTo((char *)message, MSGSIZE);
		totalCount++;

		// start timer 
		Timer clock;
		clock.start();
		while (clock.lap() <= 1500)
		{
			// while waiting for timeout checks if any message has arrived. 
			if (sock.pollRecvFrom() > 0)
			{
				// receive message 
				sock.recvFrom((char *)message, MSGSIZE);
				// cerr << "received message = " << message[0] << endl;

				// check if the message is what client is expecting 
				if (message[0] == sequence)
				{
					// increase the sequence number to send after this. 
					sequence++;

					// since client received expecting message, do not have to 
					// wait for time out. 
					break;
				}
			}
		}
		// time out 
	}

	// returns the number of message retransmitted 
	return totalCount - max;
}

// Test3: server reliable message receive -------------------------------------
// Server receives message from client untill it sees the sequence 19999. 
// every receiving message, the server sends acknowledgement to the client.
void serverReliable(UdpSocket &sock, const int max, int message[])
{
	cerr << "server reliable test:" << endl;
	// initialize expecting sequence number 
	int sequence = 0;

	while (sequence < max)
	{
		// receive message from the client. 
		sock.recvFrom((char *)message, MSGSIZE);
		// cerr << message[0] << endl;

		// check if the message is what server is expecting 
		if (message[0] == sequence)
		{
			// increase the next expecting sequence number 
			sequence++;
		}

		// just in case of ack transmission failed, timed out, 
		// and knowing that client will send message in increasing 
		// sequence number(no message bigger than expecting sequence), 
		// ack for every message that has arrived. 
		sock.ackTo((char *)message, MSGSIZE);
	}
}

int clientSlidingWindow(UdpSocket &sock, const int max, int message[], int windowSize)
{
cerr << "Client: sliding window test:" << endl;
	// number of transmits 
	int count = 0;

	// initializing start and end index for sliding window
	int sendBase = 0;
	int endIndex = sendBase + windowSize;

	while (sendBase < max)
	{
		// reseting sequence number to the beginning
		// of the window  
		int sequence = sendBase;

		// send message to the server as long as sequence number
		// is within the window boundary. 
		while (sequence < endIndex)
		{
			// send message to the server 
			message[0] = sequence;
			sock.sendTo((char *)message, MSGSIZE);
			cerr << "message = " << message[0] << endl;
			count++;

			// if message has arrived from the server, 
			// check the message(s) 
			while (sock.pollRecvFrom() > 0)
			{
				sock.recvFrom((char *)message, MSGSIZE);
				int ack = message[0];
				// if the received message is within the boundary,
				// slide the window for the number of messages, 
				// completed sending. 
				if (ack >= sendBase && ack < endIndex)
				{
					int increment = ack - sendBase + 1;
					sendBase += increment;
					endIndex += increment;
					if (endIndex > max)
					{
						endIndex = max;
					}
				}
			}
			sequence++;
		}

		// if sequence reachead the end of window, start timer 
		Timer clock;
		clock.start();
		while (clock.lap() <= 1500)
		{
			// check if any message arrived from the server. 
			if (sock.pollRecvFrom() > 0)
			{
				sock.recvFrom((char *)message, MSGSIZE);
				int ack = message[0];

				// if server is sending -1, which means it 
				// finished this session by accepting all 
				// messages and started new session. (possible)
				// transmission failed. 
				if (ack == -1 && sendBase != 0)
				{
					// finish this session. 
					while (sendBase < max)
					{
						sendBase++;
					}
					cerr << ack << endl;
					cerr << sendBase << endl;
					break;
				}
				else if (ack >= sendBase && ack < endIndex)
				{	
					// if ack is within the window boundary, 
					// slide the window for the amount of 
					// last acked message from sending Base 
					while (ack >= sendBase)					
					{
						sendBase++;
						if (endIndex < max)
						{
							endIndex++;
						}
					}
					if (sendBase == sequence)
					{
						break;
					}
				}
			}
		}
		// time out or received on time. 
	}

	// return the number of retransmits 
	return count - max;
}

// modified so that packets are randomly dropped every n% of the time
// where n is given from the main function.
void serverEarlyRetrans(UdpSocket &sock, const int max,
						int message[], int windowSize, int dropRate)
{
	// initialize random seed 
	srand(time(NULL));
	cerr << "server early retrans test:" << endl;
	
	// message memory 
	vector<bool> savedMessage(max, false);

	// initializing window starting and end index 
	int endIndex = windowSize;
	int rcvBase = -1;

	// once receives the lsat message 19999 from the client 
	// server ends its session 
	while (rcvBase < max - 1)
	{
		// receive message from the client 
		sock.recvFrom((char *)message, MSGSIZE);
		int ack = message[0];
		// cerr << ack << endl;
		
		// decides whether to drop the message 
		// based on the given dropRate 
		bool drop = false;
		if (dropRate > 0)
		{
			int random = rand() % 100;
			if (random < dropRate)
			{
				drop = true;
			}
		}

		// if decided not to drop 
		if (!drop)
		{
			// save the received message 
			if (ack > rcvBase && ack < endIndex)
			{
				savedMessage[ack] = true;
			}

			// slide the window if the message is 
			// what server is expecting to slide the window 
			if (ack == (rcvBase + 1))
			{
				while (savedMessage[rcvBase + 1])
				{
					rcvBase++;
					if (endIndex < max)
					{
						endIndex++;
					}
				}
			}

			// sending ack message to client which contains the 
			// last sequence number received in order 
			message[0] = rcvBase;
			// cerr << "receive base = " << rcvBase << endl;
			sock.ackTo((char *)message, MSGSIZE);
		}
	}
}
