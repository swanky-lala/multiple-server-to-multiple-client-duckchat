
****************************** DuckChat: multiple client and multiple server chat API using UDP protocol *****************************************************
NAME: Kenneth Nnadi
      ID :951967580
      Programming assignment 2
      Intro to Networking

ABOUT:

        - Implementing simple server to client and server to server communication

PRERQUISITES:
        - Unix/Linux like OS
        - GCC compiler
        - POSIX pthread library
        - server,client and header file
        - Makefile

GOAL: 
        Implement options for clients to chart in channels. Provide option for message forwarding between servers.
        So users at other servers could chat in the same channel. 

        There are say command and special commands.

        Special commands starts with / symbol.

        If client wants to say something in the channel, he just need to type that and press Enter.

NOTE: To start up the program,
	* open the start_servers and uncomment any Topology you want, start up the script for servers with (./start_servers.sh)
	* start up different clients and connect to different servers. (./client local_address port_number username)        

Special commands are:

                * /exit- Logs out user and exit client program
                * /join [channel name] - Join user in specific channel,create channel if doesn't exists
                * /leave [channel name] - User will leave the current channel
                * /list -This command list all the channels in different servers even when a client is not connected to the server............This is the extra credit for the assignment programming 2. 
                * /who [channel name] - See all users in thesame channel
                * /switch [channel] - Change from current channel to already subscribed channel
        
        IMPORTANT: By default,when new user logs in,default channel is Common


NOTE:
        In case you don't have pthread library:

        For Ubuntu/Debian -> sudo apt-get install libpthread-stubs0-dev

        For Fedora -> sudo dnf install glibc-devel


NOTE: I implemented the List special command for the extra credit. This command /list will list all the channels in the servers. This includes channels in different server that the client is not connected to and displays it to the user seeking the information.


        

I implemented all the functionalities for the assignment and tested it with H topology given to us in the examples or guide. 
