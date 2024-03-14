#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <string>
#include <map>
#include <iostream>
#include <time.h>
#include <pthread.h>


using namespace std;


//#include "hash.h"
#include "duckchat.h"



//typedef map<string,string> channel_type; //<username, ip+port in string>
typedef map<string,struct sockaddr_in> channel_type; //<username, sockaddr_in of user>

int s; //socket for listening
struct sockaddr_in server;

//server info
char hostname[HOSTNAME_MAX];
int PORT;
int total_channels=0;

//used for S2S
//routing table
struct route_table *rt=NULL;
//keep remember of unique keys
struct unique_list *ul=NULL;
map<string,struct sockaddr_in> usernames; //<username, sockaddr_in of user>
map<string,int> active_usernames; //0-inactive , 1-active
//map<struct sockaddr_in,string> rev_usernames;
map<string,string> rev_usernames; //<ip+port in string, username>
map<string,channel_type> channels;

int num_of_adjacent_servers;
struct adjacent_servers *adjsrv=NULL;

void handle_socket_input();
void handle_login_message(void *data, struct sockaddr_in sock);
void handle_logout_message(struct sockaddr_in sock);
void handle_join_message(void *data, struct sockaddr_in sock);
void handle_leave_message(void *data, struct sockaddr_in sock);
void handle_say_message(void *data, struct sockaddr_in sock);
void handle_list_message(struct sockaddr_in sock);
void handle_who_message(void *data, struct sockaddr_in sock);
void handle_keep_alive_message(struct sockaddr_in sock);
void send_error_message(struct sockaddr_in sock, string error_msg);

//Function used for S2S communication 
void handle_s2s_join(void *data,struct sockaddr_in sock);
void handle_s2s_recv_join(void *data, struct sockaddr_in sock);
void handle_s2s_say(void *daya,struct sockaddr_in sock);
void handle_s2s_leave(void *data, struct sockaddr_in sock);
void join_servers(char *channel);
void display_table(struct route_table *ptr);
int check_for_channel(char *cname);
int if_active(char *cname);
//thread useful function
void *renew_joins(void *p);
void *evidence_joins(void *p);
void create_list_of_adjacent_servers(int argc,char *argv[]);
long get_id();
void set_activity_on_channel(char *cname);

int main(int argc, char *argv[])
{
	
	//Check for valid input of arguments	
	if( ((argc-1)%2) != 0 ){
		fprintf(stderr,"Bad input of arguments!\n");
		exit(1);
	}

	//store adjacent servers into linked list
	create_list_of_adjacent_servers(argc,argv);

	//calculate number of adjacent servers
	num_of_adjacent_servers=(argc-3)/2;

	strcpy(hostname, argv[1]);
	PORT = atoi(argv[2]);
	

	
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0)
	{
		perror ("socket() failed\n");
		exit(1);
	}

	//struct sockaddr_in server;

	struct hostent     *he;

	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);

	if ((he = gethostbyname(hostname)) == NULL) {
		puts("error resolving hostname..");
		exit(1);
	}
	memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);

	int err;

	err = bind(s, (struct sockaddr*)&server, sizeof server);

	if (err < 0)
	{
		perror("bind failed\n");
	}
	else
	{
		//printf("bound socket\n");
	}


	


	//testing maps end

	//create default channel Common
	/*string default_channel = "Common";
	map<string,struct sockaddr_in> default_channel_users;
	channels[default_channel] = default_channel_users;
*/

	
	
	pthread_t thread_id;
	pthread_create(&thread_id,NULL, renew_joins,NULL);
	pthread_t thread_id2;
	pthread_create(&thread_id2, NULL, evidence_joins,NULL);
	while(1) //server runs for ever
	{

		//use a file descriptor with a timer to handle timeouts
		int rc;
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(s, &fds);
		


		rc = select(s+1, &fds, NULL, NULL, NULL);
		

		
		if (rc < 0){
			printf("error in select\n");
            		getchar();
		}
		else{
			int socket_data = 0;

			if (FD_ISSET(s,&fds))
			{
               
				//reading from socket
				handle_socket_input();
				socket_data = 1;

			}

			

			

			


		}

		
	}



	return 0;

}

void handle_socket_input()
{

	struct sockaddr_in recv_client;
	ssize_t bytes;
	void *data;
	size_t len;
	socklen_t fromlen;
	fromlen = sizeof(recv_client);
	char recv_text[MAX_MESSAGE_LEN];
	data = &recv_text;
	len = sizeof recv_text;


	bytes = recvfrom(s, data, len, 0, (struct sockaddr*)&recv_client, &fromlen);


	if (bytes < 0)
	{
		perror ("recvfrom failed\n");
	}
	else
	{
		//printf("received message\n");

		struct request* request_msg;
		request_msg = (struct request*)data;

		//printf("Message type:");
		request_t message_type = request_msg->req_type;
		//printf("%d\n", message_type);

		if (message_type == REQ_LOGIN)
			handle_login_message(data, recv_client); //some methods would need recv_client
		else if (message_type == REQ_LOGOUT)
			handle_logout_message(recv_client);
		else if (message_type == REQ_JOIN)
			handle_join_message(data, recv_client);
		else if (message_type == REQ_LEAVE)
			handle_leave_message(data, recv_client);
		else if (message_type == REQ_SAY)
			handle_say_message(data, recv_client);
		else if (message_type == REQ_LIST)
			handle_list_message(recv_client);
		else if (message_type == REQ_WHO)
			handle_who_message(data, recv_client);
		else if (message_type == REQ_S2S_JOIN)
			handle_s2s_join(data,recv_client);
		else if (message_type == REQ_S2S_RECV_JOIN)
			handle_s2s_recv_join(data,recv_client);
		else if (message_type == REQ_S2S_SAY)
			handle_s2s_say(data, recv_client);
		else if (message_type == REQ_S2S_LEAVE )
			handle_s2s_leave(data, recv_client);
		else
		{
			//send error message to client
			send_error_message(recv_client, "*Unknown command");
		}




	}


}

void handle_login_message(void *data, struct sockaddr_in sock)
{
	struct request_login* msg;
	msg = (struct request_login*)data;


	string username = msg->req_username;
	usernames[username]	= sock;
	active_usernames[username] = 1;

	//rev_usernames[sock] = username;

	//char *inet_ntoa(struct in_addr in);
	string ip = inet_ntoa(sock.sin_addr);
	//cout << "ip: " << ip <<endl;
	int port = sock.sin_port;
	//unsigned short short_port = sock.sin_port;
	//cout << "short port: " << short_port << endl;
	//cout << "port: " << port << endl;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	//cout << "port: " << port_str << endl;
	cout<<hostname<<":"<<PORT<<" "<<ip<<":"<<port<<" recv Request login "<<username<<endl;	
	string key = ip + ":" +port_str;
	//cout << "key: " << key <<endl;
	rev_usernames[key] = username;

	cout << "server: " << username << " logs in" << endl;

	//if there is no channel create default channel. This is first user on server
	if( total_channels == 0 ){
		char 	cname[CHANNEL_MAX];
		strcpy(cname,"Common");
		string default_channel = "Common";
		map<string,struct sockaddr_in> default_channel_users;
		channels[default_channel] = default_channel_users;
		cout<<hostname<<":"<<PORT<<" "<<key<<" recv Request Join "<<username<<" Common "<<endl;
		//Check to see if this channel is already in routing table
		int test=check_for_channel(cname);
		if(test==1){
			join_servers(cname);
		}else{
			int testactive=if_active(cname);
			if(testactive==0){
				set_activity_on_channel(cname);
				struct request_s2s_join join_msg;
				join_msg.req_type=REQ_S2S_JOIN;
				strcpy(join_msg.req_channel,cname);
				void *ptr_join;
				ptr_join=&join_msg;
				size_t lenjoin=sizeof join_msg;
				struct sockaddr_in foreign_server;
				struct route_table *temprt=rt;
				while(temprt!=NULL){
					if(! (strcmp(temprt->chann_name, cname))){
						struct forward_list *find=temprt->fw;
						while(find!=NULL){
							foreign_server.sin_family=AF_INET;
							foreign_server.sin_addr.s_addr=inet_addr(find->addr);
							foreign_server.sin_port=htons(find->port);	
							//sendto
							sendto(s, ptr_join, lenjoin, 0, (struct sockaddr*)&foreign_server, sizeof foreign_server);
							printf("%s:%d %s:%d send S2S Join %s\n", hostname,PORT, find->addr,find->port,cname);
							find=find->next;
						}
						return;
					}
					temprt=temprt->next_rt;
				}

			}
		}
		total_channels++;
	}else{
		printf("Total channels is not empty!\n");
	}

			
}

void 
handle_logout_message(struct sockaddr_in sock)
{

	//construct the key using sockaddr_in
	string ip = inet_ntoa(sock.sin_addr);
	//cout << "ip: " << ip <<endl;
	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	//cout << "port: " << port_str << endl;

	string key = ip + ":" +port_str;
	//cout << "key: " << key <<endl;

	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;

	/*
    for(iter = rev_usernames.begin(); iter != rev_usernames.end(); iter++)
    {
        cout << "key: " << iter->first << " username: " << iter->second << endl;
    }
	*/

	iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//send an error message saying not logged in
		send_error_message(sock, "Not logged in");
	}
	else
	{
		//cout << "key " << key << " found."<<endl;
		string username = rev_usernames[key];
		rev_usernames.erase(iter);

		//remove from usernames
		map<string,struct sockaddr_in>::iterator user_iter;
		user_iter = usernames.find(username);
		usernames.erase(user_iter);

		//remove from all the channels if found
		map<string,channel_type>::iterator channel_iter;
		for(channel_iter = channels.begin(); channel_iter != channels.end(); channel_iter++)
		{
			//cout << "key: " << iter->first << " username: " << iter->second << endl;
			//channel_type current_channel = channel_iter->second;
			map<string,struct sockaddr_in>::iterator within_channel_iterator;
			within_channel_iterator = channel_iter->second.find(username);
			if (within_channel_iterator != channel_iter->second.end())
			{
				channel_iter->second.erase(within_channel_iterator);
			}

		}


		//remove entry from active usernames also
		//active_usernames[username] = 1;
		map<string,int>::iterator active_user_iter;
		active_user_iter = active_usernames.find(username);
		active_usernames.erase(active_user_iter);


		cout << "server: " << username << " logs out" << endl;
	}


	/*
    for(iter = rev_usernames.begin(); iter != rev_usernames.end(); iter++)
    {
        cout << "key: " << iter->first << " username: " << iter->second << endl;
    }
	*/


	//if so delete it and delete username from usernames
	//if not send an error message - later

}

void handle_join_message(void *data, struct sockaddr_in sock)
{
	int create=0;	
	//get message fields
	struct request_join* msg;
	msg = (struct request_join*)data;

	string channel = msg->req_channel;

	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;
 	char port_str[6];
 	sprintf(port_str, "%d", port);
	string key = ip + ":" +port_str;
	/*
	 * First,check if user is subscribed to this channel.
	 * If yes,do nothing on S2S.
	 * If no, You should signal other servers to subscribe to this channel.	
	*/

	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;


	iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//ip+port not recognized - send an error message
		send_error_message(sock, "Not logged in");
	}
	else
	{
		string username = rev_usernames[key];

		map<string,channel_type>::iterator channel_iter;

		channel_iter = channels.find(channel);

		active_usernames[username] = 1;

		if (channel_iter == channels.end())
		{
			/*
			 * Channel not found, we shoudl create channel
			 * Then:
			 * -joing user
			 * -send S2S for other servers
			*/
			map<string,struct sockaddr_in> new_channel_users;
			char	cname[CHANNEL_MAX];
			strcpy(cname,channel.c_str() );
			new_channel_users[username] = sock;
			channels[channel] = new_channel_users;

			cout<<hostname<<":"<<PORT<<" "<<ip<<":"<<port<<" recv Request join "<<username<<" "<<channel<<endl;
			//Done creating new channel,user is in channel

			cout << "server: " << username << " joins channel " << channel << endl;
			//Locate servers
			create=check_for_channel(cname);
			if(create == 1){
				join_servers(cname);
			}
			if(create==0){
				int test=if_active(cname);
				if(test==0)
				{
					set_activity_on_channel(cname);
					//send all channels from routing table request to join
					struct route_table *temp_rt;
					struct forward_list *f;
					struct sockaddr_in foreign_server;
					if(rt!=NULL){
						temp_rt=rt;
						while(temp_rt!=NULL){
							if(!(strcmp(temp_rt->chann_name,cname))){
								//send to this one
								struct request_s2s_join msg;
								strcpy(msg.req_channel,cname);
								msg.req_type=REQ_S2S_JOIN;
								void *data_prt=&msg;
								size_t sizesend=sizeof msg;
								f=temp_rt->fw;
								while(f!=NULL){
									
									foreign_server.sin_addr.s_addr=inet_addr(f->addr);
									foreign_server.sin_port=f->port;
									//send
									sendto(s,data_prt,sizesend,0,(struct sockaddr*)&foreign_server, sizeof foreign_server);

									printf("%s:%d %s:%d send Request S2S Join %s\n",hostname,PORT,f->addr,f->port,cname);
									f=f->next;
								}
								break;

							}
							temp_rt=temp_rt->next_rt;
						}
					}
				}
			}
			//cout << "creating new channel and joining" << endl;
			total_channels++;
		}
		else
		{
			//channel already exits
			//map<string,struct sockaddr_in>* existing_channel_users;
			//existing_channel_users = &channels[channel];
			//*existing_channel_users[username] = sock;
			channels[channel][username] = sock;
			
			char cname[CHANNEL_MAX];
			strcpy(cname, msg->req_channel);
			cout << "server: " << username << " joins channel " << channel << endl;
			//cout << "joining exisitng channel" << endl;

			int test=if_active(cname);
			if(test==0)
			{
				set_activity_on_channel(cname);
				//send all channels from routing table request to join
				struct route_table *temp_rt;
				struct forward_list *f;
				struct sockaddr_in foreign_server;
				if(rt!=NULL){
					temp_rt=rt;
					while(temp_rt!=NULL){
						if(!(strcmp(temp_rt->chann_name,cname))){
							//send to this one
							struct request_s2s_join msg;
							strcpy(msg.req_channel,cname);
							msg.req_type=REQ_S2S_JOIN;
							void *data_prt=&msg;
							size_t sizesend=sizeof msg;
							f=temp_rt->fw;
							while(f!=NULL){
								
								foreign_server.sin_addr.s_addr=inet_addr(f->addr);
								foreign_server.sin_port=f->port;
								//send
								sendto(s,data_prt,sizesend,0,(struct sockaddr*)&foreign_server, sizeof foreign_server);

								printf("%s:%d %s:%d send Request S2S Join %s\n",hostname,PORT,f->addr,f->port,cname);
								f=f->next;
							}
							break;

						}
						temp_rt=temp_rt->next_rt;
					}
				}
			}

		}



	}

	//check whether the user is in usernames
	//if yes check whether channel is in channels
	//if channel is there add user to the channel
	//if channel is not there add channel and add user to the channel

}


void handle_leave_message(void *data, struct sockaddr_in sock)
{


	//check whether the user is in usernames
	//if yes check whether channel is in channels
	//check whether the user is in the channel
	//if yes, remove user from channel
	//if not send an error message to the user


	//get message fields
	struct request_leave* msg;
	msg = (struct request_leave*)data;

	string channel = msg->req_channel;

	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	string key = ip + ":" +port_str;


	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;


	iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//ip+port not recognized - send an error message
		send_error_message(sock, "Not logged in");
	}
	else
	{
		string username = rev_usernames[key];

		map<string,channel_type>::iterator channel_iter;

		channel_iter = channels.find(channel);

		active_usernames[username] = 1;

		if (channel_iter == channels.end())
		{
			//channel not found
			send_error_message(sock, "No channel by the name " + channel);
			cout << "server: " << username << " trying to leave non-existent channel " << channel << endl;

		}
		else
		{
			//channel already exits
			//map<string,struct sockaddr_in> existing_channel_users;
			//existing_channel_users = channels[channel];
			map<string,struct sockaddr_in>::iterator channel_user_iter;
			channel_user_iter = channels[channel].find(username);

			if (channel_user_iter == channels[channel].end())
			{
				//user not in channel
				send_error_message(sock, "You are not in channel " + channel);
				cout << "server: " << username << " trying to leave channel " << channel  << " where he/she is not a member" << endl;
			}
			else
			{
				channels[channel].erase(channel_user_iter);
				//existing_channel_users.erase(channel_user_iter);
				cout << "server: " << username << " leaves channel " << channel <<endl;

				//delete channel if no more users
				if (channels[channel].empty() && (channel != "Common"))
				{
					channels.erase(channel_iter);
					cout << "server: " << "removing empty channel " << channel <<endl;
				}

			}


		}




	}



}




void handle_say_message(void *data, struct sockaddr_in sock)
{

	//check whether the user is in usernames
	//if yes check whether channel is in channels
	//check whether the user is in the channel
	//if yes send the message to all the members of the channel
	//if not send an error message to the user
	//check if this channel has entry in routing table
	char uname[USERNAME_MAX];
	//get message fields
	struct request_say* msg;
	msg = (struct request_say*)data;
	char message_from_channel[CHANNEL_MAX];
	string channel = msg->req_channel;
	strcpy(message_from_channel, channel.c_str());
	string text = msg->req_text;


	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	string key = ip + ":" +port_str;


	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;


	iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//ip+port not recognized - send an error message
		send_error_message(sock, "Not logged in ");
	}
	else
	{
		string username = rev_usernames[key];
		strcpy(uname,username.c_str());

		map<string,channel_type>::iterator channel_iter;

		channel_iter = channels.find(channel);

		active_usernames[username] = 1;

		if (channel_iter == channels.end())
		{
			//channel not found
			send_error_message(sock, "No channel by the name " + channel);
			cout << "server: " << username << " trying to send a message to non-existent channel " << channel << endl;

		}
		else
		{
			//channel already exits
			//map<string,struct sockaddr_in> existing_channel_users;
			//existing_channel_users = channels[channel];
			map<string,struct sockaddr_in>::iterator channel_user_iter;
			channel_user_iter = channels[channel].find(username);

			if (channel_user_iter == channels[channel].end())
			{
				//user not in channel
				send_error_message(sock, "You are not in channel " + channel);
				cout << "server: " << username << " trying to send a message to channel " << channel  << " where he/she is not a member" << endl;
			}
			else
			{
				map<string,struct sockaddr_in> existing_channel_users;
				existing_channel_users = channels[channel];
				for(channel_user_iter = existing_channel_users.begin(); channel_user_iter != existing_channel_users.end(); channel_user_iter++)
				{
					//cout << "key: " << iter->first << " username: " << iter->second << endl;

					ssize_t bytes;
					void *send_data;
					size_t len;

					struct text_say send_msg;
					send_msg.txt_type = TXT_SAY;

					const char* str = channel.c_str();
					strcpy(send_msg.txt_channel, str);
					str = username.c_str();
					strcpy(send_msg.txt_username, str);
					str = text.c_str();
					strcpy(send_msg.txt_text, str);
					//send_msg.txt_username, *username.c_str();
					//send_msg.txt_text,*text.c_str();
					send_data = &send_msg;

					len = sizeof send_msg;

					//cout << username <<endl;
					struct sockaddr_in send_sock = channel_user_iter->second;

					//bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, fromlen);
					bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);

					if (bytes < 0)
					{
						perror("Message failed\n"); //error
					}
					else
					{
						//printf("Message sent\n");

					}

				}
				cout << "server: " << username << " sends say message in " << channel <<endl;

				//Check for channel name in routing table
				int check=check_for_channel(message_from_channel);
				if(check==0){
					//forward to others
					struct sockaddr_in srv;
					struct route_table *rt_index=rt;
					struct forward_list *fw_index;
					void *say_send;
					struct request_s2s_say msgsrv;
					size_t len;
					ssize_t bytes;
					srv.sin_family=AF_INET;
					//get unique id
					long unique=get_id();
					char id[32];
					sprintf(id,"%ld", unique);
					//prepare s2s message
					msgsrv.req_type=REQ_S2S_SAY;
					strcpy(msgsrv.channel, msg->req_channel);
					strcpy(msgsrv.unique_id, id);
					strcpy(msgsrv.username,uname);
					strcpy(msgsrv.request_text, msg->req_text);

					say_send=(struct request_s2s_say*)&msgsrv;

					len=sizeof(msgsrv);
					rt_index=rt;
					while(rt_index!=NULL){
						if(! (strcmp(rt_index->chann_name, message_from_channel))){
							
							fw_index=rt_index->fw;
							if(fw_index==NULL){
								return;
							}else{
								while(fw_index!=NULL){
									if(fw_index->active==1){
										srv.sin_port=htons(fw_index->port);								
										srv.sin_addr.s_addr=inet_addr(fw_index->addr);

										sendto(s, say_send, len,0,(struct sockaddr*)&srv,sizeof srv);

										printf("%s:%d %s:%d send S2S Say %s %s \" %s \"\n",hostname,PORT,fw_index->addr,fw_index->port, uname,message_from_channel,msgsrv.request_text);
									}
									fw_index=fw_index->next;
								}
								break;
							}
						}

						rt_index=rt_index->next_rt;
					}	

				}

			}


		}




	}



}


void handle_list_message(struct sockaddr_in sock)
{

	//check whether the user is in usernames
	//if yes, send a list of channels
	//if not send an error message to the user

	/*
	 * ------------------------------------------------------------------
	 * Operations on S2S
	 * 1-Send to user channels from routing table that are not 	
	*/



	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	string key = ip + ":" +port_str;


	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;


	iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//ip+port not recognized - send an error message
		send_error_message(sock, "Not logged in ");
	}
	else
	{
		string username = rev_usernames[key];
		//cout << "size: " << size << endl;

		active_usernames[username] = 1;

		ssize_t bytes;
		void *send_data;
		size_t len;

		//struct text_list temp;





		map<string,channel_type>::iterator channel_iter;



		//struct channel_info current_channels[size];
		//send_msg.txt_channels = new struct channel_info[size];
		int pos=0;	
		int size = channels.size();
		//get additional channels
		//one that are not in channels but in routing table
		int count_aditional=0;
		int flag=1;
		struct route_table *temp_rt=rt;
		while(temp_rt!=NULL){
			flag=1;
			for(channel_iter=channels.begin(); channel_iter!=channels.end(); channel_iter++){
				if( !(strcmp(channel_iter->first.c_str(), temp_rt->chann_name))){
					flag=0;
					break;
				}
			}
			if(flag==1)
				count_aditional++;
			temp_rt=temp_rt->next_rt;
		}
		int total_size=size+count_aditional;
		//printf("Total num of channels: %d\n",total_size);
		struct text_list *send_msg = (struct text_list*)malloc(sizeof (struct text_list) + (total_size * sizeof(struct channel_info)));
		send_msg->txt_type = TXT_LIST;
		send_msg->txt_nchannels = total_size;
		for(channel_iter = channels.begin(); channel_iter != channels.end(); channel_iter++)
		{
			string current_channel = channel_iter->first;
			const char* str = current_channel.c_str();
			//strcpy(current_channels[pos].ch_channel, str);
			//cout << "channel " << str <<endl;
			strcpy(((send_msg->txt_channels)+pos)->ch_channel, str);
			//strcpy(((send_msg->txt_channels)+pos)->ch_channel, "hello");
			//cout << ((send_msg->txt_channels)+pos)->ch_channel << endl;

			pos++;
		}
		temp_rt=rt;
		while(temp_rt!=NULL){
			flag=1;
			for(channel_iter=channels.begin(); channel_iter!=channels.end();channel_iter++){
				if(! (strcmp(channel_iter->first.c_str(), temp_rt->chann_name))){
					flag=0;
					break;
				}
			}
			if(flag==1){
				strcpy(((send_msg->txt_channels)+pos)->ch_channel, temp_rt->chann_name);
				pos++;
			}
			temp_rt=temp_rt->next_rt;
		}
		send_data = send_msg;

		len = sizeof (struct text_list) + (total_size * sizeof(struct channel_info));

		struct sockaddr_in send_sock = sock;
		
		bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);
		
		cout << "server: " << username << " lists channels"<<endl;
	}

		//send_msg.txt_channels =
		//send_msg.txt_channels = current_channels;

					//cout << username <<endl;


		//bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, fromlen);

		//Done sending your channels, take a look at list of subscribed channels
		/*struct route_table *rt_index=tr;
		while(rt_index!=NULL){

			if( ! (strncmp))

			rt_index=rt_index->next;
		}*/


	



}


void handle_who_message(void *data, struct sockaddr_in sock)
{


	//check whether the user is in usernames
	//if yes check whether channel is in channels
	//if yes, send user list in the channel
	//if not send an error message to the user


	//get message fields
	struct request_who* msg;
	msg = (struct request_who*)data;

	string channel = msg->req_channel;

	string ip = inet_ntoa(sock.sin_addr);

	int port = sock.sin_port;

 	char port_str[6];
 	sprintf(port_str, "%d", port);
	string key = ip + ":" +port_str;


	//check whether key is in rev_usernames
	map <string,string> :: iterator iter;


	iter = rev_usernames.find(key);
	if (iter == rev_usernames.end() )
	{
		//ip+port not recognized - send an error message
		send_error_message(sock, "Not logged in ");
	}
	else
	{
		string username = rev_usernames[key];

		active_usernames[username] = 1;

		map<string,channel_type>::iterator channel_iter;

		channel_iter = channels.find(channel);

		if (channel_iter == channels.end())
		{
			//channel not found
			send_error_message(sock, "No channel by the name " + channel);
			cout << "server: " << username << " trying to list users in non-existing channel " << channel << endl;

		}
		else
		{
			//channel exits
			map<string,struct sockaddr_in> existing_channel_users;
			existing_channel_users = channels[channel];
			int size = existing_channel_users.size();

			ssize_t bytes;
			void *send_data;
			size_t len;


			//struct text_list temp;
			struct text_who *send_msg = (struct text_who*)malloc(sizeof (struct text_who) + (size * sizeof(struct user_info)));


			send_msg->txt_type = TXT_WHO;

			send_msg->txt_nusernames = size;

			const char* str = channel.c_str();

			strcpy(send_msg->txt_channel, str);



			map<string,struct sockaddr_in>::iterator channel_user_iter;

			int pos = 0;

			for(channel_user_iter = existing_channel_users.begin(); channel_user_iter != existing_channel_users.end(); channel_user_iter++)
			{
				string username = channel_user_iter->first;

				str = username.c_str();

				strcpy(((send_msg->txt_users)+pos)->us_username, str);


				pos++;



			}

			send_data = send_msg;
			len = sizeof(struct text_who) + (size * sizeof(struct user_info));

						//cout << username <<endl;
			struct sockaddr_in send_sock = sock;


			//bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, fromlen);
			bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);

			if (bytes < 0)
			{
				perror("Message failed\n"); //error
			}
			else
			{
				//printf("Message sent\n");

			}

			cout << "server: " << username << " lists users in channnel "<< channel << endl;




			}




	}




}
void join_servers(char *channel)
{
	//You need to alarm other adjacent servers to subscribe to your channel

	//prepare request
	void *data;
	size_t len=0;
	ssize_t bytes=0;
	struct request_s2s_join msg;
	msg.req_type=REQ_S2S_JOIN;
	strcpy(msg.req_channel,channel);
	data=&msg;
	len=sizeof(msg);
	//send it
	struct sockaddr_in srvrecv;
	//always same	
	srvrecv.sin_family=AF_INET;
	if(num_of_adjacent_servers > 0 )
	{
		struct adjacent_servers *temp=adjsrv;
		while(temp!=NULL)
		{
			srvrecv.sin_port=htons(temp->port);
			srvrecv.sin_addr.s_addr=inet_addr(temp->addr);
			bytes=sendto(s,data,len,0,(struct sockaddr*)&srvrecv,sizeof srvrecv );
			
			//print info
			cout<<hostname<<":"<<PORT<<" "<<temp->addr<<":"<<temp->port<<" send S2S JOIN "<<channel<<endl;

			//increment
			temp=temp->next;
		}
	}
}

void handle_s2s_join(void *data, struct sockaddr_in sock)
{
	/*
	 * ----------------------------------------------------------------------------------------
	 * You got S2S join from other server
	 * Respond:
	 * - Extract all data from message
	 * - Check in your routing table  if 
	 * 	you are subscribed already
	 * 	if you have channel with same name from request you are already subscribed
	 * - But, you can accept join if it is comming from leaf node
	 * - Test that
	 * - JUst add him into existing routing table for that name and send him recv-ack
	 * - If no, subscribe to that channel 
	 * - Update your routing table
	 * - Send recv(you accepted) to server that called you to join on this channel	
	 * - Call your other servers to join
	 * ------------------------------------------------------------------------------------------
	*/

	//Extract data
	struct request_s2s_join *msg;
	msg=(struct request_s2s_join*)data;
	struct sockaddr_in 	remote_srv;
	remote_srv.sin_family=AF_INET;
	char	channel_to_join[CHANNEL_MAX];
	char	foreign_addr[HOSTNAME_MAX];
	char	s_port[6];
	int	port;
	strcpy(channel_to_join,msg->req_channel );
	strcpy(foreign_addr,inet_ntoa(sock.sin_addr));
	sprintf(s_port,"%d", htons(sock.sin_port));
	port=atoi(s_port);
	struct route_table *add,*temp_rt,*prev;
	struct forward_list *new_fw,*temp_fw;	
	int flag=0;

	//Check if you are already joined 
	//iF you have that channel in subs
	//do not subs again
	//if you are joined, check for server caller in your routing table and say active =1
	//subscribe
	int have_channel=0;
	//Go through list
	//When you fin channel with that name, say active=1
	temp_rt=rt;
	while(temp_rt!=NULL){
		//Find in forward list this server that send join
		//active = 1	
		if( ! (strcmp(temp_rt->chann_name, channel_to_join))){
			struct forward_list *f=temp_rt->fw;
			have_channel=1;
			while(f!=NULL){
				if( (! (strcmp(f->addr, foreign_addr)) && (f->port==port))){
					//printf("Get again old server!\n");
					f->active=1;
					//update time stamp
					struct tm *timestamp;
					time_t timer;
					time(&timer);
					timestamp=localtime(&timer);
					f->last_min=timestamp->tm_min;
					return;
				}

				f=f->next;
			}
		}


		temp_rt=temp_rt->next_rt;
	}//leaf node is added again
	//You are getting ner channel
	//We are adding new route table
	if(have_channel==1)
		return;
	//create new entry in routing table
	new_fw=(struct forward_list*)malloc(sizeof(struct forward_list));
	new_fw->port=port;
	strcpy(new_fw->addr,foreign_addr);
	new_fw->next=NULL;
	new_fw->authority=1;
	new_fw->active=1;
	//new rt item

	add=(struct route_table *)malloc(sizeof(struct route_table));
	strcpy(add->chann_name,channel_to_join);
	add->next_rt=NULL;
	add->fw=new_fw;
	add->mystate=1;
	if(rt==NULL)
		rt=add;
	else{
		temp_rt=rt;
		while(temp_rt->next_rt!=NULL){
			temp_rt=temp_rt->next_rt;
		}
		temp_rt->next_rt=add;
	}
	//send recv confirm to the server that called you
	struct request_s2s_recv send_recv;
	strcpy(send_recv.chann_name, channel_to_join);
	send_recv.req_type=REQ_S2S_RECV_JOIN;
	void *data_send;
	size_t lensend;
	data_send=&send_recv;
	lensend=sizeof send_recv;
	sendto(s, data_send,lensend,0,(struct sockaddr*)&sock, sizeof sock);
		
	/*
	 * LAst part: Call adjacent servers to join this channel
	 * Do not call server that called you
	 * 	
	*/ 
	struct adjacent_servers *temp_adj=adjsrv;
	remote_srv.sin_family=AF_INET;
	while(temp_adj!=NULL){
		//do not send join to server that sent join to you
		if( (temp_adj->port != port ) || ( strncmp(foreign_addr, temp_adj->addr,strlen(foreign_addr) ) ) ){
			//send join to this one
			struct request_s2s_join msg;
			msg.req_type=REQ_S2S_JOIN;
			strcpy(msg.req_channel,channel_to_join);
			void	*send_join=&msg;
			size_t	len=sizeof(msg);
			remote_srv.sin_port=htons(temp_adj->port);
			remote_srv.sin_addr.s_addr=inet_addr(temp_adj->addr);
			ssize_t bytes=sendto(s, send_join,len,0,(struct sockaddr*)&remote_srv,sizeof remote_srv );
			printf("%s:%d %s:%d send S2S Join %s\n", hostname, PORT, temp_adj->addr,temp_adj->port,channel_to_join);
		}
		temp_adj=temp_adj->next;	
	}
	
}

void handle_s2s_recv_join(void *data, struct sockaddr_in sock)
{
	/*
	 * ---------------------------------------------------------------------
	 * Function used to handle recv on Join S2S
	 * Server will respond with recv if it will go in the routing table
	 * By default he is active in routing table
	 * ---------------------------------------------------------------------	
	 *
	*/
	int cond=0;
	struct request_s2s_recv *msg;
	//extract data
	msg=(struct request_s2s_recv*)data;
	int 	fport=htons(sock.sin_port);
	char	foreign_server[HOSTNAME_MAX];
	char	channel_name[CHANNEL_MAX];
	strcpy(channel_name, msg->chann_name);	
	strcpy(foreign_server,inet_ntoa(sock.sin_addr) );
	struct forward_list *new_srv,*f;
	struct route_table *temp_rt,*add;
	new_srv=(struct forward_list *)malloc(sizeof(struct forward_list));
	strcpy(new_srv->addr,foreign_server);
	new_srv->port=fport;
	new_srv->authority=0;
	new_srv->active=1;
	struct tm *timestamp;
	time_t timer;
	time(&timer);
	timestamp=localtime(&timer);
	new_srv->last_min=timestamp->tm_min;
	new_srv->next=NULL;

	if(rt==NULL){
		//There are no route tables
		rt=(struct route_table*)malloc(sizeof(struct route_table));
		rt->next_rt=NULL;
		strcpy(rt->chann_name, channel_name);
		rt->fw=new_srv;
		rt->mystate=1;

	}else{
		temp_rt=rt;
		while( temp_rt!=NULL ){
			if( ! ( strncmp(channel_name,temp_rt->chann_name,strlen(channel_name)))){

				//channel exists
				//we found routing table for specific channel
				//now add info about server
				//update forward list
				f=temp_rt->fw;
				if(f==NULL)
					f=new_srv;
				else{
					while(f->next!=NULL){
						f=f->next;
					}
					f->next=new_srv;
				}
				cond=1;
				break;
			}//Done adding new server into your route table
			temp_rt=temp_rt->next_rt;
		}
		if(cond==0){
			//new channel to create
			add=(struct route_table*)malloc(sizeof(route_table));
			add->next_rt=NULL;
			strcpy(add->chann_name, channel_name);
			add->fw=new_srv;
			add->mystate=1;
			//place add
			temp_rt=rt;
			while(temp_rt->next_rt!=NULL)
				temp_rt=temp_rt->next_rt;
			temp_rt->next_rt=add;
		}

	}
	printf("%s:%d %s:%d recv S2S Join %s\n",foreign_server,fport,hostname,PORT,channel_name);
}

void handle_s2s_say(void *data, struct sockaddr_in sock)
{
	/*
	 * ------------------------------------------------------------------
	 * 0- Check if it is duplicate
	 * 	if yes,then Leave
	 * 1- Display it to your users in channel(if you have them)
	 * 2- Forward further
	 * 4- If 1 AND 2 failed, send leave message
	 * ------------------------------------------------------------------
	*/
	char	unique[32];
	int 	count_forwards=0;
	struct request_s2s_say *say_msg;
	say_msg=(struct request_s2s_say*)data;
	size_t len=sizeof(say_msg);
	char	get_from[HOSTNAME_MAX];
	struct sockaddr_in other_servers;
	other_servers.sin_family=AF_INET;
	strcpy(get_from,inet_ntoa(sock.sin_addr));
	char 	cname[CHANNEL_MAX];
	strcpy(cname, say_msg->channel);
	int from_port=htons(sock.sin_port);
	
	//checking  for duplicates first
	if(ul!=NULL){
		struct unique_list *temp_ul=ul;
		while(temp_ul !=NULL){

			if( ! (strcmp(temp_ul->unique_id, say_msg->unique_id)))
			{
				//leave message. Specify channel name - You find duplicate


				//Signale server from that channel route table
				struct route_table *temp;
				temp=rt;
				while(temp!=NULL){
					if(!(strcmp(temp->chann_name,cname))){
						temp->mystate=0;
						break;
					}
					temp=temp->next_rt;
				}
				struct request_s2s_leave leave_msg;
				leave_msg.req_type=REQ_S2S_LEAVE;
				strcpy(leave_msg.req_channel, cname);
				size_t send_size=sizeof leave_msg;
				void *data_ptr;
				data_ptr=&leave_msg;
				sendto(s, data_ptr, send_size, 0, (struct sockaddr*)&sock,sizeof sock );
				printf("%s:%d %s:%d send Request Leave %s\n",hostname,PORT, inet_ntoa(sock.sin_addr), htons(sock.sin_port),cname);


				//then exit
				return;
			}
			temp_ul=temp_ul->next;
		}
	}
	//get channel name
	struct route_table *temp_rt=rt;
	count_forwards=0;
	//check if you have that channel at your server
	//display first to your users
	map<string, map<string,struct sockaddr_in>>::iterator channel_it=channels.find(cname);
	if(channel_it==channels.end()){
		count_forwards=0;
	}else{
		map<string,struct sockaddr_in>::iterator it;
		for(it=channel_it->second.begin(); it!=channel_it->second.end(); it++){
			//send to this user
			ssize_t bytes_size;
			void *send_to_client;
			size_t lensend;

			struct text_say send_msg;
			send_msg.txt_type=TXT_SAY;
			strcpy(send_msg.txt_channel, say_msg->channel);
			strcpy(send_msg.txt_text,say_msg->request_text);
			strcpy(send_msg.txt_username, say_msg->username);

			struct sockaddr_in sendtoclient=it->second;
			sendtoclient.sin_family=AF_INET;
			send_to_client=&send_msg;
			lensend=sizeof send_msg;
			ssize_t checking=sendto(s,send_to_client,lensend,0,(struct sockaddr*)&sendtoclient, sizeof sendtoclient);
		}
		count_forwards=1;
	}//finished sending to your clients
	//send to your servers 
	temp_rt=rt;
	strcpy(cname,say_msg->channel);
	while(temp_rt!=NULL){
		//find route table with channel name
		if(! (strcmp(cname, temp_rt->chann_name))){
			//we found channel 
			//start sending daata
			struct forward_list *f=temp_rt->fw;

			while(f!=NULL){
				//dont change anything about message
				//just setup info about foreign server
				struct request_s2s_say forward_say;
				strcpy(forward_say.channel,say_msg->channel);
				strcpy(forward_say.request_text,say_msg->request_text);
				forward_say.req_type=say_msg->req_type;
				strcpy(forward_say.unique_id, say_msg->unique_id);
				strcpy(forward_say.username, say_msg->username);

				void *data_forward=&forward_say;
				size_t lenfor=sizeof forward_say;

				//Condition- Do not send to server that sent to you already
				if( (! (strcmp(f->addr,get_from )) ) && (from_port==f->port) )
					;
				else{
					if(f->active==1){
						other_servers.sin_port=htons(f->port);
						other_servers.sin_addr.s_addr=inet_addr(f->addr);
						sendto(s, data_forward, lenfor,0, (struct sockaddr*)&other_servers, sizeof other_servers);	
						printf("%s:%d %s:%d send S2S Say %s \"%s\" \n",hostname,PORT,f->addr,f->port,cname, forward_say.request_text);
						count_forwards++;
					}
				}	
				f=f->next;			
			}
			break;
		}
		temp_rt=temp_rt->next_rt;
	}
	if(count_forwards==0 ){
		//I don't have users subscribed in that channel and I don't have that channel in routing table

		//I think i should leave now
		struct route_table *temp=rt;
		while(temp!=NULL){

			if(! (strcmp(temp->chann_name,cname))){
				temp->mystate=0;
				break;
			}	
			temp=temp->next_rt;
		}

		//prepare DS
		struct request_s2s_leave leave_msg;
		leave_msg.req_type=REQ_S2S_LEAVE;
		strcpy(leave_msg.req_channel, cname);
		struct sockaddr_in buddy;
		buddy=sock;
		size_t send_size=sizeof leave_msg;
		void *data_ptr;
		data_ptr=&leave_msg;
		int flag=0;
		sendto(s, data_ptr, send_size, 0, (struct sockaddr*)&sock,sizeof sock );
		printf("%s:%d %s:%d send Request Leave %s\n",hostname,PORT, inet_ntoa(buddy.sin_addr), htons(buddy.sin_port),cname);

	}
	//no duplicates-make record of this message
	struct unique_list *add,*temp;
	add=(struct unique_list*)malloc(sizeof(struct unique_list));
	add->next=NULL;
	strcpy(add->unique_id, say_msg->unique_id);
	//store it
	if(ul==NULL)
		ul=add;
	else{
		temp=ul;
		while(temp->next!=NULL)
			temp=temp->next;
		temp->next=add;
	}
	//printf("I added new message in history of message!\n");


}
void handle_s2s_leave(void *data, struct sockaddr_in sock)
{

	//This server is going to leave your routing table for specific channel
	//just say active = 0 
	struct request_s2s_leave *leave_msg;
	leave_msg=(struct request_s2s_leave*)data;
	char	cname[CHANNEL_MAX];
	strcpy(cname, leave_msg->req_channel);
	int port=htons(sock.sin_port);
	char	srv_addr[HOSTNAME_MAX];
	strcpy(srv_addr, inet_ntoa(sock.sin_addr));
	//printf("S2S LEAVE HANDLE: %s:%d\n",srv_addr, port);
	struct route_table *temp_rt=rt;
	while(temp_rt!=NULL){

		if(! (strcmp(temp_rt->chann_name, cname))){
			struct forward_list *f=temp_rt->fw;
			while(f!=NULL){

				if(  (!(strcmp(f->addr, srv_addr))) && ( port==f->port)){
					

					f->active=0;
					printf("%s:%d %s:%d recv Request S2S Leave %s\n",hostname,PORT,srv_addr,port,cname);
					return;

				}
				f=f->next;
			}
		}

		temp_rt=temp_rt->next_rt;
	}



}

void send_error_message(struct sockaddr_in sock, string error_msg)
{
	ssize_t bytes;
	void *send_data;
	size_t len;

	struct text_error send_msg;
	send_msg.txt_type = TXT_ERROR;

	const char* str = error_msg.c_str();
	strcpy(send_msg.txt_error, str);

	send_data = &send_msg;

	len = sizeof send_msg;


	struct sockaddr_in send_sock = sock;



	bytes = sendto(s, send_data, len, 0, (struct sockaddr*)&send_sock, sizeof send_sock);

	if (bytes < 0)
	{
		perror("Message failed\n"); //error
	}
	else
	{
		//printf("Message sent\n");

	}





}

void create_list_of_adjacent_servers(int argc,char *argv[])
{
	int i;
	struct adjacent_servers *nnew,*temp;
	for(i=3;i<argc;i=i+2){
		nnew=(struct adjacent_servers*)malloc(sizeof(struct adjacent_servers));
		strncpy(nnew->addr, argv[i],strlen(argv[i]));
		nnew->port=atoi(argv[i+1]);
		nnew->next=NULL;

		if(adjsrv==NULL)
			adjsrv=nnew;
		else{
			temp=adjsrv;
			while(temp->next!=NULL)
				temp=temp->next;
			temp->next=nnew;
		}
	}
}//Done adding adjacent servers to linked list

void display_table(struct route_table *ptr)
{
	printf("DISPLAY TABLE: ");
	struct forward_list *f=ptr->fw;
	while(f!=NULL){
		printf("C: %s : %s:%d\n",ptr->chann_name, f->addr,f->port);
		f=f->next;
	}

}
int check_for_channel(char *cname)
{
	// Return 1 if sever doesnt have this channel in his subscribes(routing table)
	struct route_table *temp;
	temp=rt;
	if(temp==NULL)
		return 1;
	while(temp!=NULL){
		if( ! (strcmp(temp->chann_name, cname)) )
			return 0;
		temp=temp->next_rt;
	}
	return 1;
}
long get_id()
{
	long ret;
	int fd=open("/dev/random", O_RDONLY);
	read(fd,&ret,sizeof(ret));

	printf("Createad unique for message ID: %ld\n",ret);

	return ret;

}
void *renew_joins(void *p)
{
	struct route_table *temp_rt;
	struct forward_list *f;
	while(1){
		sleep(60);
		if(rt!=NULL){
			temp_rt=rt;
			while(temp_rt!=NULL){
				if(temp_rt->mystate==1){
					struct sockaddr_in renew_server;
					f=temp_rt->fw;
					while(f!=NULL){
						if( (f->active == 1) && (f->authority==1) ){
							struct request_s2s_join renew_msg;
							renew_msg.req_type=REQ_S2S_JOIN;
							strcpy(renew_msg.req_channel, temp_rt->chann_name);

							renew_server.sin_family=AF_INET;
							renew_server.sin_addr.s_addr=inet_addr(f->addr);
							renew_server.sin_port=htons(f->port);

							void *data=&renew_msg;
							size_t len=sizeof renew_msg;
							sendto(s,data,len,0,(struct sockaddr*)&renew_server, sizeof renew_server);
							printf("%s:%d %s:%d S2S renew Join %s\n", hostname,PORT, f->addr,f->port, temp_rt->chann_name);
						}
						f=f->next;
					}
				}
				temp_rt=temp_rt->next_rt;
			}
		}
	}


}
void *evidence_joins(void *p)
{

	struct route_table *temp_rt;
	struct forward_list *f;
	while(1){

		sleep(110);
		if(rt==NULL)
			;
		else{
			//go thorugh every channel
			//through every list
			//find their last minute and compare with current
			temp_rt=rt;
			while(temp_rt!=NULL)
			{
				f=temp_rt->fw;
				while(f!=NULL){
					if( (f->active==1) && (f->authority==0)){
						//get current time
						struct tm *timestamp;
						time_t timer;

						time(&timer);
						timestamp=localtime(&timer);
						if( f->last_min + 2 < timestamp->tm_min ){
							//kick him
							f->active=0;
							printf("%s:%d removed %s:%d from %s-inactive\n", hostname,PORT, f->addr,f->port, temp_rt->chann_name);
						}

					}
					f=f->next;
				}

				temp_rt=temp_rt->next_rt;
			}

		}


	}


}

void set_activity_on_channel(char *cname){

	struct route_table *temp_rt=rt;
	while(temp_rt!=NULL){
		if(! (strcmp(cname,temp_rt->chann_name))){
			temp_rt->mystate=1;
			return;
		}
		temp_rt=temp_rt->next_rt;
	}
}

int if_active(char *cname)
{
	struct route_table *temp=rt;
	if(rt!=NULL){
		while(temp!=NULL){
			if(! (strcmp(temp->chann_name,cname)))
			{
				if( temp->mystate==1 )
					return 1;
				else
					return 0;
			}
			temp=temp->next_rt;
		}
	}
	return 0;
}