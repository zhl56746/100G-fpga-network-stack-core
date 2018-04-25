/*********************************************
Author: Mario Ruiz
HPCN research Group
*********************************************/
#include <stdio.h>
#include <stdlib.h>
#include "pcap.h"
#include "pcap2stream.hpp"

stream<axiWord> input_data("Data_Read_from_pcap");
axiWord transaction;

using namespace std;
using namespace hls;

#define DEBUG
//#define DEBUG1

void pcapPacketHandler (unsigned char *userData,  struct pcap_pkthdr* pkthdr, unsigned char *packet) {

	ap_uint<16> bytes_sent=0, bytes2send,last_trans;
	ap_uint<ETH_INTERFACE_WIDTH/8> aux_keep;
	ap_uint<ETH_INTERFACE_WIDTH> data_value;
	static ap_uint<16> pkt=0;

	for (int h=0 ; h< ETH_INTERFACE_WIDTH/8 ; h++){
		transaction.keep[h]=1;
	}
	transaction.last=0;

	bytes2send = (pkthdr->len);
#ifdef DEBUG1
	cout << "Bytes to send " << bytes2send << endl;
#endif
	for (int h=0; h < bytes2send ; h+= ETH_INTERFACE_WIDTH/8){
		bytes_sent += ETH_INTERFACE_WIDTH/8;
		if (bytes_sent < bytes2send){
			data_value= *((ap_uint<ETH_INTERFACE_WIDTH> *)packet);
			packet+=ETH_INTERFACE_WIDTH/8;
		}
		else if (bytes_sent < (bytes2send + ETH_INTERFACE_WIDTH/8)){
			last_trans= ETH_INTERFACE_WIDTH/8 - bytes_sent + bytes2send;

			for (int s=0 ; s< ETH_INTERFACE_WIDTH/8; s++){
				if (s < last_trans)
					aux_keep[s]=1;
				else
					aux_keep[s]=0;
			}
			transaction.last=1;
			transaction.keep=aux_keep;
			data_value=0;
			for (int s=0; s < last_trans*8 ; s+=8){
				data_value.range(s+7,s)= *((ap_uint<8> *)packet);
				packet++;
			}
		}
#ifdef CHANGE_ENDIANESS
		for (int s=0; s< ETH_INTERFACE_WIDTH; s+=8){
			transaction.data.range(ETH_INTERFACE_WIDTH-1-s,ETH_INTERFACE_WIDTH-s-8)=data_value.range(s+7,s);
		}
#else
		transaction.data = data_value;
#endif
#ifdef DEBUG1
		cout << "Test Data [" << dec << pkt << "] Transaction [" << dec << h/(ETH_INTERFACE_WIDTH/8) << "]";
		cout	<< "\tComplete Data " << hex << transaction.data << "\t\tkeep " << transaction.keep << "\tlast " << transaction.last << endl;
		pkt = pkt + transaction.last;
#endif

		input_data.write(transaction);
	}
}


void remove_ethernet (
						stream<axiWord>&			dataIn,
					  	stream<axiWord>&			dataOut)
{

	enum mwState {WAIT_PKT, WRITE_TRANSACTION , WRITE_EXTRA_LAST_WORD};
	static mwState mw_state = WAIT_PKT;

	axiWord currWord;
	static axiWord prevWord;
	static axiWord sendWord;


	static int wordCount = 0;

	if (!dataIn.empty()){
		do {
			switch (mw_state){
				case WAIT_PKT:
					wordCount = 0;
					if (!dataIn.empty()){
						dataIn.read(currWord);
						prevWord = {0,0,0};									// initialize
						prevWord.data(399,0) = currWord.data(511 , 112);
						prevWord.keep(49 ,0) = currWord.keep( 63 ,  14);
						prevWord.last = currWord.last;
						if (currWord.last == 1){
							sendWord = prevWord;
							dataOut.write(sendWord);
#ifdef DEBUG1
		cout << "Test Data Transaction [" << dec << wordCount/(ETH_INTERFACE_WIDTH/8) << "]";
		cout	<< "\tShaved off Data " << hex << sendWord.data << "\t\tkeep " << sendWord.keep << "\tlast " << sendWord.last << endl;
#endif							
						}
						else{
							mw_state = WRITE_TRANSACTION;
						}
	
					}	
				break;
				case WRITE_TRANSACTION : 
					if (!dataIn.empty()){
						dataIn.read(currWord);
						wordCount++;
	
	
						sendWord.data(399,0) 	= prevWord.data(399,0);
						sendWord.keep(49 ,0) 	= prevWord.keep(49 ,0);
						sendWord.data(511,400) 	= currWord.data(111 ,  0);
						sendWord.keep( 63 ,50) 	= currWord.keep( 13 ,  0);
	
						prevWord = {0,0,0};									// initialize
						prevWord.data(399,0) 	= currWord.data(511 , 112);
						prevWord.keep(49 ,0) 	= currWord.keep( 63 ,  14);
						prevWord.last 		 	= currWord.last;

						sendWord.last 			= 0;
	
						if (currWord.last == 1) {
							if (currWord.keep.bit(14)){
								mw_state = WRITE_EXTRA_LAST_WORD;
							}
							else{
								wordCount = 0;
								sendWord.last 	= 1;
								mw_state = WAIT_PKT;
							}
						}
	
						dataOut.write(sendWord);
#ifdef DEBUG1
		cout << "Test Data Transaction [" << dec << wordCount/(ETH_INTERFACE_WIDTH/8) << "]";
		cout	<< "\tShaved off Data " << hex << sendWord.data << "\t\tkeep " << sendWord.keep << "\tlast " << sendWord.last << endl;
#endif
					}
				break;
	
				case WRITE_EXTRA_LAST_WORD : 
					prevWord.last 		 	= 1;
					sendWord = prevWord;
					dataOut.write(sendWord);
					mw_state = WAIT_PKT;
#ifdef DEBUG1
		cout << "Test Data Transaction [" << dec << wordCount/(ETH_INTERFACE_WIDTH/8) << "]";
		cout	<< "\tShaved off Data " << hex << sendWord.data << "\t\tkeep " << sendWord.keep << "\tlast " << sendWord.last << endl;
#endif
				break;
	
				default :
					mw_state = WAIT_PKT;
				break;
			}
		} while (sendWord.last == 0);
	}


}


int open_file (
				char 								*file2load) {


	cout << "File 2 load " << file2load << endl;

	/* Read packets from pcapfile */
	if(pcap_open (file2load)) {
		cout << "Error opening the input file"<< endl;
		return -1;
	}
	else
		cout <<"File open correct" << endl << endl;

	if (pcap_loop (0,                  /* Iterate over all the packets */
	                 pcapPacketHandler,  /* Routine invoked */
	                 NULL) < 0) {        /* Argument passed to the handler function (the buffer) */


	    pcap_close();
	    cout << "Error opening the input file"<< endl;
	    return -1;
  	}

	pcap_close();
	return 0;
}


void pcap2stream(
				char 								*file2load, 		// pcapfilename
				bool 								ethernet,			// 0: No ethernet in the packet, 1: ethernet include
				stream<axiWord>&					output_data			// output data
) {

	axiWord 	currWord;

	if (open_file(file2load)==0){

		while(!input_data.empty()){
			if (ethernet){
				input_data.read(currWord);
				output_data.write(currWord);
			}
			else {
				remove_ethernet(input_data , output_data);
			}
		}

	}

}

/* It returns one complete packet each time is called
*
*/

void pcap2stream_step(
				char 								*file2load, 		// pcapfilename
				bool 								ethernet,			// 0: No ethernet in the packet, 1: ethernet include
				stream<axiWord>&					output_data			// output data
	){

	static bool error_opening_file = false;
	static bool file_open = false;

	stream<axiWord> currWord_Stream("data_to_remove_ethernet");

	axiWord 	currWord;

	if (!file_open) {
		file_open = true;
		if (open_file(file2load)!=0){
			error_opening_file = true;
		}
	}


	if (!error_opening_file) {
		if (!input_data.empty()){
			do {
				input_data.read(currWord);
				if (ethernet){
					output_data.write(currWord);
				}
				else {
					currWord_Stream.write(currWord);
				}
			} while (!currWord.last);

			if (!ethernet){
				remove_ethernet(currWord_Stream , output_data);
			}
		}
	}


}
