#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <vector>
#include <iostream>
#include <fstream>
using namespace std;
#define PORT 8001

bool arrayAllTrue(bool arr[], int n) {
	for (int i = 0; i<n; i++)
		if (arr[i] == false)
			return false;
	return true;
}

int main(int argc, char *argv[]) {
	// port to start the server on
	int SERVER_PORT = PORT;

	// socket address used for the server
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// htons: host to network short: transforms a value in host byte
	// ordering format to a short value in network byte ordering format
	server_address.sin_port = htons(SERVER_PORT);

	// htons: host to network long: same as htons but to long
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);

	// create a UDP socket, creation returns -1 on failure
	int sock;
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("could not create socket\n");
		return 1;
	}
	printf("server socket created\n");
	// bind it to listen to the incoming connections on the created server
	// address, will return -1 on error
	if ((bind(sock, (struct sockaddr *)&server_address,
	          sizeof(server_address))) < 0) {
		printf("could not bind socket\n");
		return 1;
	}
	printf("binding was successful\n");
	// socket address used to store client address
	struct sockaddr_in client_address;
	int client_address_len = sizeof(client_address);
  char client_name[100];

	//////////////////////////////////////////////////////////////////////////////

	// Receive initial data from client: filename, filesize
	char initialmessage[1028];
	int leng = recvfrom(sock, initialmessage, sizeof(initialmessage), 0, (struct sockaddr *)&client_address, &client_address_len);
	if (leng < 0) {
		printf("Read error!\n");
		return -1;
	}
	string m(initialmessage);
	string filename(m.substr(0, m.find(" ")));
	filename = string("server_") + filename;
	int filesize = atoi(m.substr(m.find(" "), m.size()-m.find(" ")).c_str());
	cout << "Filename: " << filename << "\nFilesize: " << filesize << endl;

	vector <char> datavec;
	// Big receive loop
	for (int i = 0; i < filesize/8872 + 1; i++) {
		char octoblock[8888];
		memset(octoblock, 0, 8888);
		bool octolegACK[8] = {false,false,false,false,false,false,false,false};
		char buffer[1111];
		int octolegsize = 1109;
		if ((i+1)*8872 > filesize) {
			octolegsize = (filesize - i*8872) / 8;
			if ((filesize - i*8872) % 8 != 0)
				octolegsize++;
		}
		int bytesreceived = 0;

		// While not all octolegs received
		while (!arrayAllTrue(octolegACK, 8)) {
			memset(buffer, 0, 1111);
			// read content into buffer from an incoming client
			int len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address, &client_address_len);
			if (len < 0) {
				printf("Read error!\n");
		    return -1;
			}

			// Decode octoleg metadata
			int octoleg_id = buffer[0];
			octolegACK[octoleg_id] = true;
			bytesreceived += len-1;
			strncpy(octoblock+(octolegsize*octoleg_id), buffer+1, len-1);

			// inet_ntoa prints user friendly representation of the
			// ip address
			buffer[len] = '\0';
			printf("received: '%s' (%d bytes) from client %s on port %d\n", buffer+1,len-1,
			       inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

			// Send ACK back to client
			char ACK[1] = {(char)octoleg_id};
			int sent_len = sendto(sock, ACK, 1, 0, (struct sockaddr *)&client_address,
			      client_address_len);
			printf("server sent back octoleg ACK: %d\n\n",ACK[0]);
		} // End of octoleg loop

		// Save completed octoblock into vector
		// cout << "Completed Octoblock:\n" << octoblock << endl;
		datavec.insert(datavec.end(), octoblock, octoblock+bytesreceived);
	} // End of octoblock loop

	// Write to file
	ofstream outfile(filename);
	int count = 1;
	for (char&c : datavec) {
		if (count > filesize) break;
		outfile << c;
		count++;
	}
	outfile.close();
	cout << "\nTransfer complete, data written to file!" << endl;

	close(sock);
	return 0;
}
